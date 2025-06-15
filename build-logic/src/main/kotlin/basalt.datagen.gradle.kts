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

        val components =
            parseRegistryEntries(File(reportsDir, "registries.json").readText(), "minecraft:data_component_type")
        writeIdTable(
            File(registryDir, "VanillaDataComponents.kt"),
            "ALL_DATA_COMPONENT_IDS",
            DATA_COMPONENTS_KDOC,
            components,
        )

        val durability = parseDurabilityComponents(File(reportsDir, "minecraft/components/item"))
        writeDurabilityTable(File(registryDir, "VanillaItemDurability.kt"), durability)

        val shapes = extractBlockShapes(workDir)
        writeShapeTable(File(registryDir, "VanillaBlockShapes.kt"), shapes)

        val definitions = parseStateDefinitions(File(reportsDir, "blocks.json").readText())
        writeStateDefinitionTable(File(registryDir, "VanillaBlockStateDefinitions.kt"), definitions)

        val armour = parseArmourComponents(File(reportsDir, "minecraft/components/item"))
        writeArmourTable(File(registryDir, "VanillaItemArmour.kt"), armour)

        println(
            "Regenerated ${blockStates.size} block default states, ${items.size} items, " +
                "${hardness.size} block hardness entries, ${toolRules.size} tool(s), " +
                "${components.size} data component type(s), ${durability.size} damageable item(s), " +
                "${shapes.second.size} block state shape(s), ${definitions.size} state definition(s) " +
                "and ${armour.size} piece(s) of armour from $jarPath.",
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
    val classpath = harnessClasspath(workDir)

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

val DATA_COMPONENTS_KDOC =
    """
    /**
     * Every data component type's id in the `minecraft:data_component_type` registry, which is what
     * an item stack's component patch carries on the wire.
     *
     * The names are what `/give diamond_sword[custom_name=...]` writes, so both directions of that
     * translation come from the real registry rather than from a hand-kept list.
     */
    """.trimIndent()

// `max_damage` is how much wear an item survives; `damage_per_block` and `item_damage_per_attack`
// are what one use costs, zero meaning that use does not wear it at all.
fun parseDurabilityComponents(componentsDir: File): List<Triple<String, Int, Pair<Int, Int>>> {
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
            val maxDamage = (components["minecraft:max_damage"] as? Number)?.toInt() ?: return@mapNotNull null
            @Suppress("UNCHECKED_CAST")
            val tool = components["minecraft:tool"] as? Map<String, Any?>
            @Suppress("UNCHECKED_CAST")
            val weapon = components["minecraft:weapon"] as? Map<String, Any?>
            // Zero where the item has no `tool`/`weapon` component at all: vanilla's `Item.mineBlock`
            // returns early for one, so armour and a bow take no wear from mining or from swinging.
            val perBlock = if (tool == null) 0 else (tool["damage_per_block"] as? Number)?.toInt() ?: 1
            val perAttack = if (weapon == null) 0 else (weapon["item_damage_per_attack"] as? Number)?.toInt() ?: 1
            Triple(file.nameWithoutExtension, maxDamage, perBlock to perAttack)
        }
}

fun writeDurabilityTable(
    file: File,
    entries: List<Triple<String, Int, Pair<Int, Int>>>,
) {
    val body =
        entries.joinToString(",\n") { (item, maxDamage, cost) ->
            "        \"$item\" to ItemDurabilityEntry($maxDamage, ${cost.first}, ${cost.second})"
        }
    file.writeText(
        """
        package org.basaltmc.server.registry

        /**
         * Every damageable item, from the real server jar's own item component reports: how much
         * wear it survives (`minecraft:max_damage`), what breaking one block costs it
         * (`minecraft:tool`'s `damage_per_block`) and what one attack costs it
         * (`minecraft:weapon`'s `item_damage_per_attack`).
         *
         * An item missing from here has no durability at all, which is not the same as having a
         * large one: it never wears and never breaks.
         */
        internal val ALL_ITEM_DURABILITY: List<Pair<String, ItemDurabilityEntry>> =
            listOf(
        BODY,
            )
        """.trimIndent().replace("BODY", body) + "\n",
    )
}


/**
 * Every block state's collision shape, light dampening and sturdy faces.
 *
 * None of this is reported by the data generator: shapes live in the block classes, so a harness
 * asks the real ones. Coordinates come out as multiples of 1/32 and are stored as thirty-seconds,
 * which is exact for every vanilla block and lets a face be rasterised into a 32x32 mask.
 *
 * Returns the distinct entries and one index per block state id, in id order.
 */
fun extractBlockShapes(workDir: File): Pair<List<String>, List<Int>> {
    val classpath = harnessClasspath(workDir)
    val harnessDir = File(workDir, "harness")
    harnessDir.mkdirs()
    File(harnessDir, "DumpShapes.java").writeText(DUMP_SHAPES_SOURCE)
    runHarness(
        listOf("javac", "-nowarn", "-cp", classpath, "-d", harnessDir.absolutePath, File(harnessDir, "DumpShapes.java").absolutePath),
        workDir,
    )
    val output = File(harnessDir, "shapes.tsv")
    runHarness(listOf("java", "-cp", "${harnessDir.absolutePath}${File.pathSeparator}$classpath", "DumpShapes"), workDir, output)

    val entries = LinkedHashMap<String, Int>()
    val index = mutableListOf<Pair<Int, Int>>()
    for (line in output.readLines()) {
        if (!line.startsWith("SHAPE\t")) continue
        val columns = line.split('\t')
        val stateId = columns[1].toInt()
        val boxes =
            columns[7]
                .split(';')
                .filter { it.isNotEmpty() }
                .joinToString(";") { box -> box.split(',').joinToString(",") { toThirtySeconds(it) } }
        val entry = "${columns[3]}|${columns[4]}|${columns[6]}|$boxes"
        index += stateId to entries.getOrPut(entry) { entries.size }
    }
    check(index.isNotEmpty()) { "The shape harness printed nothing" }
    val byState = IntArray(index.maxOf { it.first } + 1)
    for ((stateId, entryIndex) in index) byState[stateId] = entryIndex
    return entries.keys.toList() to byState.toList()
}

fun toThirtySeconds(coordinate: String): String {
    val scaled = coordinate.toDouble() * 32.0
    val rounded = Math.round(scaled).toInt()
    check(Math.abs(scaled - rounded) < 1.0E-6) { "Shape coordinate $coordinate is not a multiple of 1/32" }
    return rounded.toString()
}

/**
 * The unpacked server jar plus its libraries, which every harness here compiles against.
 *
 * The bundler unpacks the real server jar next to itself the first time it runs, so a harness has
 * to build against that, not against the bundler.
 */
fun harnessClasspath(workDir: File): String {
    val versionsDir = File(workDir, "versions")
    val serverJar =
        versionsDir.walkTopDown().firstOrNull { it.isFile && it.name.startsWith("server-") && it.extension == "jar" }
            ?: error("Could not find the unpacked server jar under ${versionsDir.absolutePath}")
    val librariesDir = File(workDir, "libraries")
    return (listOf(serverJar) + librariesDir.walkTopDown().filter { it.extension == "jar" })
        .joinToString(File.pathSeparator) { it.absolutePath }
}

val DUMP_SHAPES_SOURCE =
    """
    import net.minecraft.SharedConstants;
    import net.minecraft.core.BlockPos;
    import net.minecraft.core.Direction;
    import net.minecraft.core.registries.BuiltInRegistries;
    import net.minecraft.server.Bootstrap;
    import net.minecraft.world.level.EmptyBlockGetter;
    import net.minecraft.world.level.block.Block;
    import net.minecraft.world.level.block.state.BlockState;
    import net.minecraft.world.phys.AABB;
    import net.minecraft.world.phys.shapes.CollisionContext;
    import net.minecraft.world.phys.shapes.Shapes;
    import net.minecraft.world.phys.shapes.VoxelShape;

    public final class DumpShapes {
        public static void main(String[] args) {
            SharedConstants.tryDetectVersion();
            Bootstrap.bootStrap();
            StringBuilder out = new StringBuilder();
            for (Block block : BuiltInRegistries.BLOCK) {
                for (BlockState state : block.getStateDefinition().getPossibleStates()) {
                    VoxelShape collision;
                    try {
                        collision = state.getCollisionShape(EmptyBlockGetter.INSTANCE, BlockPos.ZERO, CollisionContext.empty());
                    } catch (Throwable t) {
                        collision = Shapes.block();
                    }
                    StringBuilder boxes = new StringBuilder();
                    for (AABB box : collision.toAabbs()) {
                        if (boxes.length() > 0) boxes.append(';');
                        boxes.append(box.minX).append(',').append(box.minY).append(',').append(box.minZ).append(',')
                             .append(box.maxX).append(',').append(box.maxY).append(',').append(box.maxZ);
                    }
                    StringBuilder sturdy = new StringBuilder();
                    for (Direction direction : Direction.values()) {
                        boolean value;
                        try {
                            value = state.isFaceSturdy(EmptyBlockGetter.INSTANCE, BlockPos.ZERO, direction);
                        } catch (Throwable t) {
                            value = false;
                        }
                        sturdy.append(value ? '1' : '0');
                    }
                    out.append("SHAPE\t").append(Block.getId(state)).append('\t')
                       .append(BuiltInRegistries.BLOCK.getKey(block)).append('\t')
                       .append(state.getLightDampening()).append('\t')
                       .append(state.blocksMotion() ? 1 : 0).append('\t')
                       .append(state.propagatesSkylightDown() ? 1 : 0).append('\t')
                       .append(sturdy).append('\t')
                       .append(boxes).append('\n');
                }
            }
            System.out.print(out);
        }
    }
    """.trimIndent()

val SHAPE_INDEX_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"

fun writeShapeTable(
    file: File,
    shapes: Pair<List<String>, List<Int>>,
) {
    val (entries, index) = shapes
    val entryBody = entries.joinToString(",\n") { "        \"$it\"" }
    val encoded =
        buildString {
            for (entryIndex in index) {
                append(SHAPE_INDEX_ALPHABET[entryIndex / SHAPE_INDEX_ALPHABET.length])
                append(SHAPE_INDEX_ALPHABET[entryIndex % SHAPE_INDEX_ALPHABET.length])
            }
        }
    val indexBody = encoded.chunked(100).joinToString(" +\n") { "        \"$it\"" }
    file.writeText(
        """
        package org.basaltmc.server.registry

        /**
         * Every distinct block shape, from the real server jar's own block classes: light
         * dampening, whether it blocks movement, which faces are sturdy (down, up, north, south,
         * west, east) and the boxes of the collision shape in thirty-seconds of a block.
         */
        internal val ALL_BLOCK_SHAPES: List<String> =
            listOf(
        SHAPE_ENTRIES_BODY,
            )

        /**
         * Which of [ALL_BLOCK_SHAPES] each block state uses, two characters per state id in id
         * order. A table of one entry per state would be thirty thousand lines of source for what
         * fits here in a few hundred.
         */
        internal val BLOCK_SHAPE_INDEX: String =
        SHAPE_INDEX_BODY
        """.trimIndent().replace("SHAPE_ENTRIES_BODY", entryBody).replace("SHAPE_INDEX_BODY", indexBody) + "\n",
    )
}


/**
 * Every block's state definition: its first state id and each property with its values, in the
 * order the ids run through them.
 *
 * The report lists a block's properties in its own order, which for a dozen blocks (every chest,
 * `moving_piston`) is not the order the state ids vary in, so the order is worked out from the
 * states themselves rather than trusted. With this, any block state id turns into its properties
 * and back without a hand-written table per family.
 */
fun parseStateDefinitions(json: String): List<String> {
    val root = Json.parseToJsonElement(json) as JsonObject
    return root.entries
        .mapNotNull { (block, value) ->
            val definition = value as JsonObject
            val states =
                (definition.getValue("states") as JsonArray)
                    .map { it as JsonObject }
                    .associate { state ->
                        val properties =
                            (state["properties"] as? JsonObject)
                                .orEmpty()
                                .mapValues { (_, property) -> property.jsonPrimitive.content }
                        state.getValue("id").jsonPrimitive.content.toInt() to properties
                    }
            val base = states.keys.min()
            val values =
                (definition["properties"] as? JsonObject)
                    ?.mapValues { (_, property) -> (property as JsonArray).map { it.jsonPrimitive.content } }
                    ?: return@mapNotNull null

            // A property's stride is how far apart two states differing only in it are; the one
            // that moves the ids furthest is the outermost of the nested loops that built them.
            val baseProperties = states.getValue(base)
            val strides =
                values.keys.associateWith { name ->
                    val wanted = baseProperties + (name to values.getValue(name)[1])
                    (states.entries.first { it.value == wanted }.key - base)
                }
            val ordered = values.keys.sortedByDescending { strides.getValue(it) }
            "$base|${block.substringAfter(':')}|" +
                ordered.joinToString(";") { name -> "$name=${values.getValue(name).joinToString(",")}" }
        }.sortedBy { it.substringBefore('|').toInt() }
}

fun writeStateDefinitionTable(
    file: File,
    definitions: List<String>,
) {
    val body = definitions.joinToString(",\n") { "        \"$it\"" }
    file.writeText(
        """
        package org.basaltmc.server.registry

        /**
         * Every block with more than one state: `<first state id>|<block>|<property>=<values>;...`.
         *
         * The ids of a block run through its properties as nested loops, the first property here
         * being the outermost, so a state id and a set of property values convert into each other
         * with arithmetic instead of a table of thirty thousand entries.
         */
        internal val ALL_BLOCK_STATE_DEFINITIONS: List<String> =
            listOf(
        STATE_DEFINITIONS_BODY,
            )
        """.trimIndent().replace("STATE_DEFINITIONS_BODY", body) + "\n",
    )
}


/**
 * Every item that protects whoever wears it: its armour, its toughness, the knockback it resists
 * and the slot it goes in, from the item component reports' own `minecraft:attribute_modifiers`.
 *
 * Only modifiers that apply to an armour slot count. A weapon's attack damage lives in the same
 * component and is somebody else's business.
 */
fun parseArmourComponents(componentsDir: File): List<String> {
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
            val modifiers = components["minecraft:attribute_modifiers"] as? List<Map<String, Any?>> ?: return@mapNotNull null
            var armour = 0.0
            var toughness = 0.0
            var knockbackResistance = 0.0
            var slot = ""
            for (modifier in modifiers) {
                val where = modifier["slot"] as? String ?: continue
                if (where !in ARMOUR_SLOTS) continue
                val amount = (modifier["amount"] as? Number)?.toDouble() ?: continue
                slot = where
                when (modifier["type"] as? String) {
                    "minecraft:armor" -> armour = amount
                    "minecraft:armor_toughness" -> toughness = amount
                    "minecraft:knockback_resistance" -> knockbackResistance = amount
                }
            }
            if (slot.isEmpty() || (armour == 0.0 && toughness == 0.0 && knockbackResistance == 0.0)) {
                null
            } else {
                "${file.nameWithoutExtension}|$slot|$armour|$toughness|$knockbackResistance"
            }
        }
}

val ARMOUR_SLOTS = setOf("head", "chest", "legs", "feet", "body")

fun writeArmourTable(
    file: File,
    entries: List<String>,
) {
    val body = entries.joinToString(",\n") { "        \"$it\"" }
    file.writeText(
        """
        package org.basaltmc.server.registry

        /**
         * Every piece of armour: `<item>|<slot>|<armour>|<toughness>|<knockback resistance>`, from
         * the real item component reports.
         */
        internal val ALL_ITEM_ARMOUR: List<String> =
            listOf(
        ARMOUR_BODY,
            )
        """.trimIndent().replace("ARMOUR_BODY", body) + "\n",
    )
}
