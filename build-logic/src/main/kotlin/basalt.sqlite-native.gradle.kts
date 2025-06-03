import org.gradle.api.DefaultTask
import org.gradle.api.tasks.CacheableTask
import org.gradle.api.tasks.Input
import org.gradle.api.tasks.OutputDirectory
import org.gradle.api.tasks.TaskAction
import org.jetbrains.kotlin.gradle.plugin.mpp.KotlinNativeTarget
import java.io.File
import java.net.URI
import java.security.MessageDigest
import java.util.Properties
import java.util.zip.ZipInputStream

plugins {
    // Applied so this script sees the `kotlin` extension; applying it twice alongside
    // basalt.sqldelight is a no-op, Gradle plugins are idempotent.
    id("basalt.native-module")
}

/**
 * Compiles SQLite's official amalgamation for every native target this host builds and hands the
 * static library to a cinterop, so it ends up inside the binary.
 *
 * Linking `-lsqlite3` instead, as SQLDelight's driver does, fails outright: Kotlin/Native links
 * Linux and Windows against its own bundled sysroots and neither ships a SQLite.
 *
 * The triple, the sysroot and the `$variable` indirections all come from `konan.properties`, the
 * file the compiler itself reads: a hardcoded path would be a guess about a platform this host
 * cannot even build for.
 */
@CacheableTask
abstract class DownloadSqliteAmalgamation : DefaultTask() {
    @get:Input
    abstract val url: org.gradle.api.provider.Property<String>

    /** SHA3-256, which is the digest sqlite.org publishes for its own downloads. */
    @get:Input
    abstract val sha3: org.gradle.api.provider.Property<String>

    @get:OutputDirectory
    abstract val outputDirectory: org.gradle.api.file.DirectoryProperty

    @TaskAction
    fun run() {
        val target = outputDirectory.get().asFile
        target.mkdirs()
        val bytes = URI(url.get()).toURL().openStream().use { it.readBytes() }
        val digest = MessageDigest.getInstance("SHA3-256").digest(bytes).joinToString("") { "%02x".format(it) }
        check(digest == sha3.get()) {
            "SQLite amalgamation checksum mismatch: expected ${sha3.get()}, got $digest"
        }
        // Flattened deliberately: the archive nests everything under one versioned directory, and
        // the version is already pinned by the checksum above.
        ZipInputStream(bytes.inputStream()).use { zip ->
            while (true) {
                val entry = zip.nextEntry ?: break
                if (entry.isDirectory) continue
                val name = entry.name.substringAfterLast('/')
                if (name != "sqlite3.c" && name != "sqlite3.h") continue
                File(target, name).writeBytes(zip.readBytes())
            }
        }
        check(File(target, "sqlite3.c").exists()) { "sqlite3.c missing from ${url.get()}" }
    }
}

private val sqliteRoot = layout.buildDirectory.dir("sqlite")

private val downloadSqlite =
    tasks.register<DownloadSqliteAmalgamation>("downloadSqliteAmalgamation") {
        description = "Downloads and verifies the official SQLite amalgamation sources."
        url.set("https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip")
        sha3.set("628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e")
        outputDirectory.set(sqliteRoot.map { it.dir("src") })
    }

/** Reads `konan.properties`, resolving the `$variable` indirections its own values are written with. */
private class KonanToolchain(
    val home: File,
) {
    private val properties =
        Properties().apply { File(home, "konan/konan.properties").inputStream().use { load(it) } }

    val dependencies: File = File(System.getProperty("user.home"), ".konan/dependencies")

    /** Values chain: `targetSysRoot` points at `$gccToolchain`, which points at `$toolchainDependency`. */
    fun resolve(
        key: String,
        target: String,
    ): String? {
        var value = properties.getProperty("$key.$target") ?: return null
        repeat(RESOLVE_DEPTH) {
            if (!value.contains('$')) return value
            value =
                VARIABLE.replace(value) { match ->
                    properties.getProperty("${match.groupValues[1]}.$target") ?: match.value
                }
        }
        check(!value.contains('$')) { "Unresolved variable in $key.$target: $value" }
        return value
    }

    val clang: File
        get() =
            dependencies
                .listFiles()
                ?.filter { it.name.startsWith("llvm-") }
                ?.firstNotNullOfOrNull { tool(it, "clang") }
                ?: error("No Kotlin/Native LLVM toolchain under $dependencies. Build once so it is downloaded.")

    /** Windows carries the `.exe` suffix, so the name alone never locates the binary there. */
    fun tool(
        llvm: File,
        name: String,
    ): File? = sequenceOf(name, "$name.exe").map { File(llvm, "bin/$it") }.firstOrNull { it.exists() }

    companion object {
        private const val RESOLVE_DEPTH = 4
        private val VARIABLE = Regex("""\$([A-Za-z0-9_.]+?)\.[a-z0-9_]+""")

        fun locate(): KonanToolchain {
            val root = File(System.getProperty("user.home"), ".konan")
            val prebuilt =
                root
                    .listFiles()
                    ?.filter { it.name.startsWith("kotlin-native-prebuilt-") }
                    ?.maxByOrNull { it.name }
                    ?: error("No Kotlin/Native distribution under $root. Build once so it is downloaded.")
            return KonanToolchain(prebuilt)
        }
    }
}

/**
 * One `libsqlite3.a` per target. The compile flags are SQLite's own recommended server set: no
 * shared-cache (it is deprecated and we open one connection), thread-safe in serialized mode
 * because the tick loop and connection threads share the database, and the OMITs drop features a
 * server never uses out of the binary.
 */
@CacheableTask
abstract class CompileSqliteAmalgamation : DefaultTask() {
    @get:org.gradle.api.tasks.InputDirectory
    @get:org.gradle.api.tasks.PathSensitive(org.gradle.api.tasks.PathSensitivity.RELATIVE)
    abstract val sources: org.gradle.api.file.DirectoryProperty

    @get:Input
    abstract val konanTarget: org.gradle.api.provider.Property<String>

    @get:OutputDirectory
    abstract val outputDirectory: org.gradle.api.file.DirectoryProperty

    @TaskAction
    fun run() {
        val toolchain = KonanToolchain.locate()
        val clang = toolchain.clang
        val target = konanTarget.get()
        val triple = toolchain.resolve("targetTriple", target) ?: error("No target triple for $target")
        val sysRoot = toolchain.resolve("targetSysRoot", target)?.let { File(toolchain.dependencies, it) }

        val outputDir = outputDirectory.get().asFile
        outputDir.mkdirs()
        val objectFile = File(outputDir, "sqlite3.o")
        val archive = File(outputDir, "libsqlite3.a")

        val command =
            buildList {
                add(clang.absolutePath)
                add("-c")
                add("-O2")
                add("-target")
                add(triple)
                if (target.startsWith("macos")) {
                    // Apple targets link against the installed SDK, which is what Kotlin/Native
                    // itself does on this host; the konan.properties sysroot is an Xcode dependency
                    // that is not materialised as a directory here.
                    add("-isysroot")
                    add(xcodeSdkPath())
                } else {
                    // Never fall back to the host headers: that silently builds against the wrong libc.
                    checkNotNull(sysRoot) { "No sysroot for $target in konan.properties" }
                    check(sysRoot.exists()) { "Kotlin/Native sysroot for $target is missing: $sysRoot" }
                    add("--sysroot")
                    add(sysRoot.absolutePath)
                }
                addAll(COMPILE_FLAGS)
                add(File(sources.get().asFile, "sqlite3.c").absolutePath)
                add("-o")
                add(objectFile.absolutePath)
            }
        execute(command)
        // `ar` from the same LLVM distribution, so the archive format always matches the linker
        // that is going to read it.
        val ar =
            toolchain.tool(clang.parentFile.parentFile, "llvm-ar")
                ?: error("No llvm-ar next to ${clang.absolutePath}")
        execute(listOf(ar.absolutePath, "rcs", archive.absolutePath, objectFile.absolutePath))
    }

    private fun xcodeSdkPath(): String {
        val process = ProcessBuilder("xcrun", "--show-sdk-path").redirectErrorStream(true).start()
        val output = process.inputStream.bufferedReader().readText().trim()
        check(process.waitFor() == 0) { "xcrun --show-sdk-path failed: $output" }
        return output
    }

    private fun execute(command: List<String>) {
        val process = ProcessBuilder(command).redirectErrorStream(true).start()
        val output = process.inputStream.bufferedReader().readText()
        check(process.waitFor() == 0) { "${command.first()} failed:\n$output" }
    }

    private companion object {
        val COMPILE_FLAGS =
            listOf(
                "-DSQLITE_THREADSAFE=1",
                "-DSQLITE_ENABLE_COLUMN_METADATA=1",
                "-DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1",
                "-DSQLITE_OMIT_DEPRECATED=1",
                "-DSQLITE_OMIT_SHARED_CACHE=1",
                "-DSQLITE_OMIT_LOAD_EXTENSION=1",
                "-DSQLITE_DQS=0",
            )
    }
}

kotlin {
    targets.withType(KotlinNativeTarget::class.java).configureEach {
        val nativeTarget = this
        val targetName = nativeTarget.konanTarget.name
        val compileSqlite =
            tasks.register<CompileSqliteAmalgamation>(
                "compileSqlite${targetName.split('_').joinToString("") { part -> part.replaceFirstChar { it.uppercase() } }}",
            ) {
                description = "Compiles SQLite into a static library for $targetName."
                dependsOn(downloadSqlite)
                // Downloads the LLVM this task shells out to; without it, the two race on a clean machine.
                dependsOn(tasks.matching { it.name == "downloadKotlinNativeDistribution" })
                sources.set(downloadSqlite.flatMap { it.outputDirectory })
                konanTarget.set(targetName)
                outputDirectory.set(sqliteRoot.map { it.dir("lib/$targetName") })
            }

        compilations.getByName("main").cinterops.create("sqlite3") {
            defFile(project.file("src/nativeInterop/cinterop/sqlite3.def"))
            includeDirs(sqliteRoot.map { it.dir("src") })
            // The archive travels inside the cinterop klib, so anything that depends on this module
            // links SQLite in without having to know it exists.
            extraOpts(
                "-staticLibrary",
                "libsqlite3.a",
                "-libraryPath",
                sqliteRoot.get().dir("lib/$targetName").asFile.absolutePath,
            )
        }

        tasks.named("cinteropSqlite3${nativeTarget.name.replaceFirstChar { it.uppercase() }}") {
            dependsOn(compileSqlite)
        }
    }
}
