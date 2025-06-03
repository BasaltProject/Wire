import kotlinx.serialization.json.Json
import kotlinx.serialization.json.JsonArray
import kotlinx.serialization.json.JsonObject
import kotlinx.serialization.json.JsonPrimitive
import kotlinx.serialization.json.jsonPrimitive
import java.io.File

// This module never ships in the server binary (see the module's own root package doc):
// it only runs `java` against a locally supplied jar at build time, so it needs no Kotlin/Native
// target of its own, just the `base` lifecycle tasks (`build`, `check`, `clean`) for consistency
// with every other subproject.
plugins {
    base
}

tasks.register("generate") {
    group = "basalt"
    description = "Regenerates server-registry's real vanilla ID tables from the official server jar's own data generator."

    doLast {
        val jarPath =
            project.findProperty("mc.jar") as String?
                ?: error("Missing -Pmc.jar=/path/to/server.jar (the bundler jar, e.g. server-2.jar)")
        val jarFile = File(jarPath)
        check(jarFile.isFile) { "No such file: $jarPath" }

        val workDir = project.layout.buildDirectory.dir("datagen").get().asFile
        workDir.mkdirs()

        // The bundler needs its own classpath unpacked first, so this must run against the full
        // bundler jar (e.g. `server-2.jar`), not the nested `META-INF/versions/*/server-*.jar` alone.
        val process =
            ProcessBuilder(
                "java",
                "-DbundlerMainClass=net.minecraft.data.Main",
                "-jar",
                jarFile.absolutePath,
                "--reports",
            ).directory(workDir)
                .redirectOutput(ProcessBuilder.Redirect.INHERIT)
                .redirectError(ProcessBuilder.Redirect.INHERIT)
                .start()
        val exitCode = process.waitFor()
        check(exitCode == 0) { "Data generator exited with code $exitCode" }

        val reportsDir = File(workDir, "generated/reports")
        val blockStates = parseDefaultBlockStates(File(reportsDir, "blocks.json").readText())
        val items = parseRegistryEntries(File(reportsDir, "registries.json").readText(), "minecraft:item")
        val blocks = parseRegistryEntries(File(reportsDir, "registries.json").readText(), "minecraft:block")

        val registryDir = project.rootProject.file("server-registry/src/commonMain/kotlin/org/basaltmc/server/registry")
        writeIdTable(File(registryDir, "VanillaBlockStates.kt"), "ALL_DEFAULT_BLOCK_STATES", BLOCK_STATES_KDOC, blockStates)
        writeIdTable(File(registryDir, "VanillaItems.kt"), "ALL_ITEMS", ITEMS_KDOC, items)
        writeIdTable(File(registryDir, "VanillaBlockIds.kt"), "ALL_BLOCK_IDS", BLOCK_IDS_KDOC, blocks)

        val hardness = extractBlockHardness(jarFile, workDir)
        writeHardnessTable(File(registryDir, "VanillaBlockHardness.kt"), hardness)

        val toolRules = parseToolComponents(File(reportsDir, "minecraft/components/item"))
        writeToolTable(File(registryDir, "VanillaItemTools.kt"), toolRules)

        println(
            "Regenerated ${blockStates.size} block default states, ${items.size} items, " +
                "${hardness.size} block hardness entries and ${toolRules.size} tool(s) from $jarPath.",
        )
    }
}

// Each block's `states` array marks exactly one entry `"default": true`, even blocks with a single,
// property-less state: that entry's `id` is the real numeric default block state id.
fun parseDefaultBlockStates(json: String): List<Pair<Int, String>> {
    val root = Json.parseToJsonElement(json) as JsonObject
    return root.entries
        .map { (key, value) ->
            val states = (value as JsonObject).getValue("states") as JsonArray
            val default = states.map { it as JsonObject }.first { (it["default"] as? JsonPrimitive)?.content == "true" }
            default.getValue("id").jsonPrimitive.content.toInt() to key.substringAfter(':')
        }.sortedBy { it.first }
}

// `registries.json` is keyed by registry id (`minecraft:item`, `minecraft:block`...), each with an
// `entries` map of `namespace:path` to `{"protocol_id": N}`.
fun parseRegistryEntries(
    json: String,
    registryId: String,
): List<Pair<Int, String>> {
    val root = Json.parseToJsonElement(json) as JsonObject
    val entries = (root.getValue(registryId) as JsonObject).getValue("entries") as JsonObject
    return entries.entries
        .map { (key, value) ->
            (value as JsonObject).getValue("protocol_id").jsonPrimitive.content.toInt() to key.substringAfter(':')
        }.sortedBy { it.first }
}

fun writeIdTable(
    file: File,
    varName: String,
    kdoc: String,
    pairs: List<Pair<Int, String>>,
) {
    val content =
        buildString {
            appendLine("package org.basaltmc.server.registry")
            appendLine()
            appendLine(kdoc)
            appendLine("internal val $varName: List<Pair<Int, String>> =")
            appendLine("    listOf(")
            for ((id, path) in pairs) appendLine("        $id to \"$path\",")
            appendLine("    )")
        }
    file.writeText(content)
}

val BLOCK_STATES_KDOC =
    """
    /**
     * Every block's default state id, straight from the real 26.2 server jar's own data
     * generator (${'`'}net.minecraft.data.Main --reports${'`'}, the ${'`'}blocks.json${'`'} report): authoritative,
     * not decompiled/guessed. Non-default states (block properties: facing, waterlogged, age...)
     * aren't included, since server-world doesn't model block properties yet; every block here
     * is registered by its default state only, same shape [Blocks] already used for its five
     * hand-picked named constants.
     */
    """.trimIndent()

val BLOCK_IDS_KDOC =
    """
    /**
     * Every block's id in the ${'`'}minecraft:block${'`'} registry, which is **not** its block *state* id.
     *
     * Both exist and they are different numbers: chunk data and block updates carry state ids,
     * while anything naming a block itself (a tag's members, most notably) carries these. Sending
     * one where the other belongs is silently wrong rather than an error, which is why they are
     * two tables and not one.
     */
    """.trimIndent()

val ITEMS_KDOC =
    """
    /**
     * Every item's real protocol id, straight from the real 26.2 server jar's own data
     * generator (${'`'}net.minecraft.data.Main --reports${'`'}, the ${'`'}registries.json${'`'} report):
     * authoritative, not decompiled/guessed.
     */
    """.trimIndent()


/**
 * Block destroy times, which the vanilla data generator does not report: hardness lives in the
 * block definitions, so a small harness bootstraps the registries, walks `BuiltInRegistries.BLOCK`
 * and prints each block's `defaultDestroyTime` and whether drops need the right tool.
 *
 * One float and one flag per block id, the same kind of fact as the numeric ids above.
 */
fun extractBlockHardness(
    jarFile: File,
    workDir: File,
): List<Triple<String, Float, Boolean>> {
    // The bundler unpacks the real server jar next to itself the first time it runs; the harness
    // has to compile and run against that, not against the bundler.
    val versionsDir = File(workDir, "versions")
    val serverJar =
        versionsDir.walkTopDown().firstOrNull { it.isFile && it.name.startsWith("server-") && it.extension == "jar" }
            ?: error("Could not find the unpacked server jar under ${versionsDir.absolutePath}")
    val librariesDir = File(workDir, "libraries")
    val classpath =
        (listOf(serverJar) + librariesDir.walkTopDown().filter { it.extension == "jar" })
            .joinToString(File.pathSeparator) { it.absolutePath }

    val harnessDir = File(workDir, "harness")
    harnessDir.mkdirs()
    File(harnessDir, "DumpHardness.java").writeText(
        """
        import net.minecraft.SharedConstants;
        import net.minecraft.core.registries.BuiltInRegistries;
        import net.minecraft.resources.Identifier;
        import net.minecraft.server.Bootstrap;
        import net.minecraft.world.level.block.Block;

        public final class DumpHardness {
            public static void main(String[] args) {
                SharedConstants.tryDetectVersion();
                Bootstrap.bootStrap();
                for (Block block : BuiltInRegistries.BLOCK) {
                    Identifier id = BuiltInRegistries.BLOCK.getKey(block);
                    System.out.println(
                        id + "\t"
                            + block.defaultDestroyTime() + "\t"
                            + block.defaultBlockState().requiresCorrectToolForDrops());
                }
            }
        }
        """.trimIndent(),
    )

    runHarness(listOf("javac", "-cp", classpath, "-d", harnessDir.absolutePath, File(harnessDir, "DumpHardness.java").absolutePath), workDir)
    val output = File(harnessDir, "hardness.tsv")
    runHarness(
        listOf("java", "-cp", "${harnessDir.absolutePath}${File.pathSeparator}$classpath", "DumpHardness"),
        workDir,
        output,
    )

    // Minecraft installs its own logger over stdout, so every line the harness prints arrives
    // wrapped in a timestamp and a `[STDOUT]:` marker. The id is whatever follows that.
    return output
        .readLines()
        .filter { it.contains('\t') && it.contains("minecraft:") }
        .map { line ->
            val (rawId, destroyTime, needsTool) = line.split('\t')
            val id = rawId.substring(rawId.indexOf("minecraft:"))
            Triple(id, destroyTime.toFloat(), needsTool.toBoolean())
        }.sortedBy { it.first }
}

fun runHarness(
    command: List<String>,
    workDir: File,
    output: File? = null,
) {
    val builder =
        ProcessBuilder(command)
            .directory(workDir)
            .redirectError(ProcessBuilder.Redirect.INHERIT)
    if (output != null) builder.redirectOutput(output) else builder.redirectOutput(ProcessBuilder.Redirect.INHERIT)
    val exitCode = builder.start().waitFor()
    check(exitCode == 0) { "${command.first()} exited with code $exitCode" }
}

fun writeHardnessTable(
    file: File,
    entries: List<Triple<String, Float, Boolean>>,
) {
    val body =
        entries.joinToString(",\n") { (id, destroyTime, needsTool) ->
            "        Triple(\"${id.removePrefix("minecraft:")}\", ${destroyTime}f, $needsTool)"
        }
    file.writeText(
        """
        package org.basaltmc.server.registry

        /**
         * Every block's destroy time and whether it needs the correct tool to drop anything,
         * extracted straight from the real server jar's own block definitions.
         *
         * The vanilla data generator does not report these, so unlike the id tables beside this one
         * they come from a harness run against the real classes (see `basalt.datagen`). A destroy
         * time of -1 means unbreakable, which is how bedrock and the portal frames are marked.
         */
        internal val ALL_BLOCK_HARDNESS: List<Triple<String, Float, Boolean>> =
            listOf(
        ${'$'}body,
            )
        """.trimIndent().replace("${'$'}body", body) + "\n",
    )
}


/**
 * Each tool item's `minecraft:tool` component: the rules deciding which blocks it mines properly
 * and how fast. Straight out of the data generator's own per-item component reports.
 *
 * A rule names a block tag, whether the tool counts as correct for that tag's drops, and optionally
 * a mining speed. Vanilla takes the first rule whose tag contains the block, which is why order is
 * preserved here: a pickaxe lists `incorrect_for_<tier>_tool` before `mineable/pickaxe`, so the
 * exclusion wins over the general case.
 */
fun parseToolComponents(componentsDir: File): List<Pair<String, List<Triple<String, Boolean, Float?>>>> {
    if (!componentsDir.isDirectory) return emptyList()
    val slurper = groovy.json.JsonSlurper()
    return componentsDir
        .listFiles { file -> file.extension == "json" }
        .orEmpty()
        .sortedBy { it.name }
        .mapNotNull { file ->
            @Suppress("UNCHECKED_CAST")
            val root = slurper.parse(file) as Map<String, Any?>
            @Suppress("UNCHECKED_CAST")
            val components = root["components"] as? Map<String, Any?> ?: return@mapNotNull null
            @Suppress("UNCHECKED_CAST")
            val tool = components["minecraft:tool"] as? Map<String, Any?> ?: return@mapNotNull null
            @Suppress("UNCHECKED_CAST")
            val rules = tool["rules"] as? List<Map<String, Any?>> ?: return@mapNotNull null
            val parsed =
                rules.mapNotNull { rule ->
                    // Only tag-shaped rules are taken. A rule can also name blocks directly, which
                    // no vanilla tool does, so one appearing means this needs revisiting rather
                    // than being silently half-read.
                    val blocks = rule["blocks"] as? String ?: return@mapNotNull null
                    if (!blocks.startsWith("#")) return@mapNotNull null
                    val correct = rule["correct_for_drops"] as? Boolean ?: false
                    val speed = (rule["speed"] as? Number)?.toFloat()
                    Triple(blocks.removePrefix("#"), correct, speed)
                }
            if (parsed.isEmpty()) null else file.nameWithoutExtension to parsed
        }
}

fun writeToolTable(
    file: File,
    tools: List<Pair<String, List<Triple<String, Boolean, Float?>>>>,
) {
    val body =
        tools.joinToString(",\n") { (item, rules) ->
            val ruleText =
                rules.joinToString(", ") { (tag, correct, speed) ->
                    "ToolRule(\"$tag\", $correct, ${speed?.let { "${it}f" } ?: "null"})"
                }
            "        \"$item\" to listOf($ruleText)"
        }
    file.writeText(
        """
        package org.basaltmc.server.registry

        /**
         * Every tool item's mining rules, from the real server jar's own item component reports
         * (`minecraft:tool`). Each rule is a block tag, whether the tool is correct for that tag's
         * drops, and how fast it mines it.
         *
         * Order matters and is preserved: vanilla applies the first rule whose tag contains the
         * block, and every tool lists its `incorrect_for_<tier>_tool` exclusion before the general
         * `mineable/<kind>` rule so the exclusion wins.
         */
        internal val ALL_ITEM_TOOLS: List<Pair<String, List<ToolRule>>> =
            listOf(
        BODY,
            )
        """.trimIndent().replace("BODY", body) + "\n",
    )
}
