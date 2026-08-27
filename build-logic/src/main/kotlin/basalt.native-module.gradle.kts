import org.jetbrains.kotlin.gradle.ExperimentalKotlinGradlePluginApi
import org.jetbrains.kotlin.gradle.dsl.ExplicitApiMode
import java.io.File

plugins {
    id("org.jetbrains.kotlin.multiplatform")
    id("org.jlleitschuh.gradle.ktlint")
}

// Generated sources are not ours to format: a generator writes its own style and reformatting it
// would be undone on the next build anyway.
ktlint {
    filter {
        exclude { it.file.absolutePath.replace(File.separatorChar, '/').contains("/build/generated/") }
    }
}

// Kotlin/Native does not cross-compile between families, so a host builds only its own, and of
// those only the target it runs: building from source should cost one binary. `-Pbasalt.targets`
// asks for the rest, `all` or a comma-separated list.
val hostOs: String = System.getProperty("os.name")
val hostIsArm: Boolean = System.getProperty("os.arch") in listOf("aarch64", "arm64")

val hostFamily: List<String> =
    when {
        hostOs == "Mac OS X" -> listOf("macosArm64", "macosX64")
        hostOs == "Linux" -> listOf("linuxX64", "linuxArm64")
        hostOs.startsWith("Windows") -> listOf("mingwX64")
        else ->
            error(
                "No supported Kotlin/Native target for host '$hostOs'. " +
                    "Basalt builds on macOS, Linux or Windows.",
            )
    }

val hostTarget: String =
    when {
        hostOs == "Mac OS X" -> if (hostIsArm) "macosArm64" else "macosX64"
        hostOs == "Linux" -> if (hostIsArm) "linuxArm64" else "linuxX64"
        else -> "mingwX64"
    }

val requestedTargets: String = providers.gradleProperty("basalt.targets").getOrElse("host")

val selectedTargets: List<String> =
    when (requestedTargets) {
        "host" -> listOf(hostTarget)
        "all" -> hostFamily
        else -> requestedTargets.split(',').map { it.trim() }.filter { it.isNotEmpty() }
    }

check(selectedTargets.isNotEmpty()) { "-Pbasalt.targets names no target at all." }

for (name in selectedTargets) {
    check(name in hostFamily) {
        "Unknown target '$name' in -Pbasalt.targets. This host builds ${hostFamily.joinToString()}, " +
            "or 'host' ($hostTarget), or 'all'."
    }
}

// For whoever links an executable: the target this machine can run.
extra["basaltHostTarget"] = if (hostTarget in selectedTargets) hostTarget else selectedTargets.first()

kotlin {
    explicitApi = ExplicitApiMode.Strict

    for (name in selectedTargets) {
        when (name) {
            "macosArm64" -> macosArm64()
            "macosX64" -> macosX64()
            "linuxX64" -> linuxX64()
            "linuxArm64" -> linuxArm64()
            "mingwX64" -> mingwX64()
        }
    }

    @OptIn(ExperimentalKotlinGradlePluginApi::class)
    applyDefaultHierarchyTemplate()

    sourceSets {
        commonMain {
            dependencies {
            }
        }
        commonTest {
            dependencies {
                implementation(kotlin("test"))
            }
        }
    }

}

// Kotlin/Native cannot link a test binary with zero test sources: the module still compiles, but
// a module without a single packet codec or binary format has nothing left to round-trip test
// against, and the link task fails outright rather than producing an empty runner. Skip the
// native test compile/link/run tasks entirely when this module currently has no test source.
afterEvaluate {
    val hasTestSource =
        kotlin.sourceSets
            .filter { it.name.contains("Test", ignoreCase = true) }
            .any { it.kotlin.files.isNotEmpty() }
    if (!hasTestSource) {
        tasks
            .matching { task ->
                task.name.matches(Regex("compileTestKotlin\\w+")) ||
                    task.name.matches(Regex("link(Debug|Release)Test\\w+")) ||
                    (task.group == "verification" && task.name.matches(Regex("\\w+Test")))
            }.configureEach {
                onlyIf { false }
            }
    }
}
