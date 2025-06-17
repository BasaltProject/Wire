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

        val food = parseFoodComponents(File(reportsDir, "minecraft/components/item"))
        writeFoodTable(File(registryDir, "VanillaItemFood.kt"), food)

        val support = extractBlockSupport(workDir)
        writeSupportTable(File(registryDir, "VanillaBlockSupport.kt"), support)

        println(
            "Regenerated ${blockStates.size} block default states, ${items.size} items, " +
                "${hardness.size} block hardness entries, ${toolRules.size} tool(s), " +
                "${components.size} data component type(s), ${durability.size} damageable item(s), " +
                "${shapes.second.size} block state shape(s), ${definitions.size} state definition(s), " +
                "${armour.size} piece(s) of armour, ${food.size} edible item(s) and " +
                "${support.rules.size} support rule(s) from $jarPath.",
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
): List<BlockHardnessRow> {
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
                            + block.defaultBlockState().requiresCorrectToolForDrops() + "\t"
                            + block.getExplosionResistance());
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
            val columns = line.split('\t')
            val id = columns[0].substring(columns[0].indexOf("minecraft:"))
            BlockHardnessRow(id, columns[1].toFloat(), columns[2].toBoolean(), columns[3].toFloat())
        }.sortedBy { it.id }
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

/** One block's destroy time, whether it needs the right tool, and what it costs an explosion. */
class BlockHardnessRow(
    val id: String,
    val destroyTime: Float,
    val needsTool: Boolean,
    val resistance: Float,
)

fun writeHardnessTable(
    file: File,
    entries: List<BlockHardnessRow>,
) {
    val body =
        entries.joinToString(",\n") { row ->
            "        BlockHardnessEntry(\"" + row.id.removePrefix("minecraft:") + "\", " +
                row.destroyTime + "f, " + row.needsTool + ", " + row.resistance + "f)"
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
        internal val ALL_BLOCK_HARDNESS: List<BlockHardnessEntry> =
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
            columns[11]
                .split(';')
                .filter { it.isNotEmpty() }
                .joinToString(";") { box -> box.split(',').joinToString(",") { toThirtySeconds(it) } }
        val entry =
            "${columns[3]}|${columns[4]}|${columns[5]}|${columns[6]}|${columns[8]}|${columns[9]}|${columns[10]}|$boxes"
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
    import net.minecraft.world.level.block.StairBlock;
    import net.minecraft.world.level.block.SupportType;
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
                    StringBuilder centre = new StringBuilder();
                    StringBuilder rigid = new StringBuilder();
                    for (Direction direction : Direction.values()) {
                        sturdy.append(supports(state, direction, SupportType.FULL) ? '1' : '0');
                        centre.append(supports(state, direction, SupportType.CENTER) ? '1' : '0');
                        rigid.append(supports(state, direction, SupportType.RIGID) ? '1' : '0');
                    }
                    out.append("SHAPE\t").append(Block.getId(state)).append('\t')
                       .append(BuiltInRegistries.BLOCK.getKey(block)).append('\t')
                       .append(state.getLightDampening()).append('\t')
                       .append(state.blocksMotion() ? 1 : 0).append('\t')
                       .append(state.canBeReplaced() ? 1 : 0).append('\t')
                       .append(state.isSolid() ? 1 : 0).append('\t')
                       .append(state.propagatesSkylightDown() ? 1 : 0).append('\t')
                       .append(sturdy).append('\t')
                       .append(centre).append('\t')
                       .append(rigid).append('\t')
                       .append(boxes).append('\n');
                }
            }
            System.out.print(out);
        }

        /** A face that cannot be asked supports nothing, which is what an empty shape answers anyway. */
        private static boolean supports(BlockState state, Direction direction, SupportType support) {
            try {
                return state.isFaceSturdy(EmptyBlockGetter.INSTANCE, BlockPos.ZERO, direction, support);
            } catch (Throwable t) {
                return false;
            }
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
         * dampening, whether it blocks movement, whether something placed there replaces it,
         * whether it counts as solid, which faces are sturdy, which support a centre and which
         * support a rigid block (down, up, north, south, west, east each time) and the boxes of the
         * collision shape in thirty-seconds of a block.
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


/**
 * Every edible item: what it feeds, what it saturates, whether it goes down on a full stomach and
 * how long it takes, from the item component reports' `minecraft:food` and `minecraft:consumable`.
 *
 * The effects an item applies when eaten are left alone here: they are a list of effect instances,
 * which is a different shape and a different problem.
 */
fun parseFoodComponents(componentsDir: File): List<String> {
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
            val consumable = components["minecraft:consumable"] as? Map<String, Any?> ?: return@mapNotNull null
            @Suppress("UNCHECKED_CAST")
            val food = components["minecraft:food"] as? Map<String, Any?> ?: emptyMap()
            // Milk is the reason this is not simply "has a food component": it feeds nobody and
            // clears every effect. An item that neither feeds nor does anything is left out.
            if (food.isEmpty() && consumable["on_consume_effects"] == null) return@mapNotNull null
            val nutrition = (food["nutrition"] as? Number)?.toInt() ?: 0
            val saturation = (food["saturation"] as? Number)?.toFloat() ?: 0f
            val alwaysEat = food["can_always_eat"] as? Boolean ?: false
            // parity: `Consumable.DEFAULT_CONSUME_SECONDS`, the 1.6 seconds eating takes.
            val seconds = (consumable["consume_seconds"] as? Number)?.toFloat() ?: 1.6f
            @Suppress("UNCHECKED_CAST")
            val onConsume = consumable["on_consume_effects"] as? List<Map<String, Any?>> ?: emptyList()
            @Suppress("UNCHECKED_CAST")
            val remainder = (components["minecraft:use_remainder"] as? Map<String, Any?>)?.get("id") as? String
            "${file.nameWithoutExtension}|$nutrition|$saturation|$alwaysEat|$seconds|" +
                "${encodeConsumeEffects(onConsume)}|${remainder?.removePrefix("minecraft:").orEmpty()}"
        }
}

/** parity: `Consumable.onConsumeEffects`, of which only these three shapes act on the eater. */
fun encodeConsumeEffects(effects: List<Map<String, Any?>>): String =
    effects
        .mapNotNull { effect ->
            when (effect["type"] as? String) {
                "minecraft:apply_effects" -> {
                    @Suppress("UNCHECKED_CAST")
                    val applied = effect["effects"] as? List<Map<String, Any?>> ?: return@mapNotNull null
                    val probability = (effect["probability"] as? Number)?.toFloat() ?: 1f
                    applied.joinToString(";") { instance ->
                        val id = (instance["id"] as? String)?.removePrefix("minecraft:") ?: ""
                        val duration = (instance["duration"] as? Number)?.toInt() ?: 0
                        val amplifier = (instance["amplifier"] as? Number)?.toInt() ?: 0
                        "apply:$id,$duration,$amplifier,$probability"
                    }
                }
                "minecraft:remove_effects" -> "remove:${(effect["effects"] as? String)?.removePrefix("minecraft:")}"
                "minecraft:clear_all_effects" -> "clear"
                else -> null
            }
        }.joinToString(";")

fun writeFoodTable(
    file: File,
    entries: List<String>,
) {
    val body = entries.joinToString(",\n") { "        \"$it\"" }
    file.writeText(
        """
        package org.basaltmc.server.registry

        /**
         * Every edible item: `<item>|<nutrition>|<saturation>|<can always eat>|<seconds to eat>|
         * <what eating it does>|<what it leaves behind>`, from the real item component reports.
         *
         * The last field is `apply:<effect>,<ticks>,<amplifier>,<probability>`, `remove:<effect>`
         * or `clear`, several separated by `;`: what a golden apple, a bottle of honey and a bucket
         * of milk respectively do.
         */
        internal val ALL_ITEM_FOOD: List<String> =
            listOf(
        FOOD_BODY,
            )
        """.trimIndent().replace("FOOD_BODY", body) + "\n",
    )
}

/** What each block state needs around it to stay where it is, plus which states tick at random. */
class BlockSupportData(
    val sets: List<String>,
    val rules: List<String>,
    val index: List<Int>,
    val randomTick: List<Int>,
    val conductors: List<Int>,
    val families: List<Pair<String, String>>,
    val fireOdds: List<Triple<Int, Int, Int>>,
    val weathering: List<Pair<String, String>>,
    val fuels: List<Pair<String, Int>>,
)

/**
 * Runs [DUMP_BLOCK_SUPPORT_SOURCE] and folds its answers into distinct rules.
 *
 * `canSurvive` is code, not data, so the harness calls it for real, one neighbour at a time. What
 * comes back per direction is either "any block with a sturdy face", "any block that supports a
 * centre", or a list of blocks; identical tuples of those six answers collapse into one rule.
 */
fun extractBlockSupport(workDir: File): BlockSupportData {
    val classpath = harnessClasspath(workDir)
    val harnessDir = File(workDir, "harness")
    harnessDir.mkdirs()
    File(harnessDir, "DumpBlockSupport.java").writeText(DUMP_BLOCK_SUPPORT_SOURCE)
    runHarness(
        listOf(
            "javac", "-nowarn", "-cp", classpath, "-d", harnessDir.absolutePath,
            File(harnessDir, "DumpBlockSupport.java").absolutePath,
        ),
        workDir,
    )
    val output = File(harnessDir, "support.tsv")
    runHarness(
        listOf("java", "-cp", "${harnessDir.absolutePath}${File.pathSeparator}$classpath", "DumpBlockSupport"),
        workDir,
        output,
    )

    val wetByState = HashMap<Int, List<String>>()
    val supportLines = mutableListOf<List<String>>()
    val randomTick = mutableListOf<Int>()
    val conductors = mutableListOf<Int>()
    val families = mutableListOf<Pair<String, String>>()
    val fireOdds = mutableListOf<Triple<Int, Int, Int>>()
    val weathering = mutableListOf<Pair<String, String>>()
    val fuels = mutableListOf<Pair<String, Int>>()
    for (line in output.readLines()) {
        val columns = line.split('\t')
        when (columns.firstOrNull()) {
            "RANDOMTICK" -> randomTick += columns[1].toInt()
            "CONDUCTOR" -> conductors += columns[1].toInt()
            "WET" -> wetByState[columns[1].toInt()] = columns.drop(2).map { it.substringAfter('=') }
            "SUPPORT" -> supportLines += columns
            "FAMILY" -> families += columns[1] to columns[2].trimEnd(',')
            "FIRE" -> fireOdds += Triple(columns[1].toInt(), columns[2].toInt(), columns[3].toInt())
            "COPPER" -> weathering += columns[1].substringAfter(':') to columns[2].substringAfter(':')
            "FUEL" -> fuels += columns[1] to columns[2].toInt()
        }
    }
    check(supportLines.isNotEmpty()) { "The support harness printed nothing" }

    val sets = LinkedHashMap<String, Int>()
    val rules = LinkedHashMap<String, Int>()
    val index = IntArray(supportLines.maxOf { it[1].toInt() } + 1)
    for (columns in supportLines) {
        val stateId = columns[1].toInt()
        // No single neighbour holds it up, or something must *not* be beside it: either way this
        // build cannot express the rule, and a rule it cannot express must never break a block.
        if (columns[3] == "0" || columns[4] == "0") {
            index[stateId] = UNKNOWN_SUPPORT
            continue
        }
        val dry = columns.drop(5).map { it.substringAfter('=') }
        val wet = wetByState[stateId] ?: List(dry.size) { "" }
        val rule = (dry + wet).joinToString("|") { token -> encodeSupportToken(token, sets) }
        index[stateId] = FIRST_SUPPORT_RULE + rules.getOrPut(rule) { rules.size }
    }
    return BlockSupportData(
        sets.keys.map { encodeBlockSet(it) },
        rules.keys.toList(),
        index.toList(),
        randomTick,
        conductors,
        families,
        fireOdds,
        weathering,
        fuels,
    )
}

/** 0 means nothing holds this state up; 1 means the rule is one this build does not model. */
val UNKNOWN_SUPPORT = 1
val FIRST_SUPPORT_RULE = 2

fun encodeSupportToken(
    token: String,
    sets: LinkedHashMap<String, Int>,
): String =
    when (token) {
        "", "*", "c", "r" -> token
        else -> "#" + sets.getOrPut(token) { sets.size }
    }

/** A block-id list as a base64 bitset, which is far shorter than the list once a set is large. */
fun encodeBlockSet(members: String): String {
    val ids = members.split(',').filter { it.isNotEmpty() }.map { it.toInt() }
    val bytes = ByteArray(ids.max() / 8 + 1)
    for (id in ids) bytes[id / 8] = (bytes[id / 8].toInt() or (1 shl (id % 8))).toByte()
    return java.util.Base64.getEncoder().encodeToString(bytes)
}

val SUPPORT_KDOC =
    """
    /**
     * What each block state needs around it to stay put, asked of the real block classes.
     *
     * One rule per distinct answer. Each rule is twelve fields: six directions dry, then the same
     * six with water at and around the position, and a field is empty when that side holds nothing
     * up, `*` for any block whose face towards it is sturdy, `c` for one that supports a centre,
     * and `#n` for a member of set n. A state survives when any one of its fields is satisfied.
     */
    """.trimIndent()

/** Writes the rules, their sets, one index per block state and the states that tick at random. */
fun writeSupportTable(
    file: File,
    support: BlockSupportData,
) {
    val setsBody = support.sets.joinToString(",\n") { "        \"$it\"" }
    val rulesBody = support.rules.joinToString(",\n") { "        \"$it\"" }
    val encoded =
        buildString {
            for (rule in support.index) {
                append(SHAPE_INDEX_ALPHABET[rule / SHAPE_INDEX_ALPHABET.length])
                append(SHAPE_INDEX_ALPHABET[rule % SHAPE_INDEX_ALPHABET.length])
            }
        }
    val indexBody = encoded.chunked(100).joinToString(" +\n") { "        \"$it\"" }
    val tickBody = support.randomTick.chunked(20).joinToString(",\n") { row -> "        " + row.joinToString(", ") }
    val conductorBody =
        support.conductors.chunked(20).joinToString(",\n") { row -> "        " + row.joinToString(", ") }
    val byFamily = LinkedHashMap<String, MutableList<String>>()
    for ((blockId, names) in support.families) {
        for (name in names.split(',').filter { it.isNotEmpty() }) byFamily.getOrPut(name) { mutableListOf() } += blockId
    }
    val fuelBody =
        support.fuels.joinToString(",\n") { (item, ticks) -> "        \"" + item + "\" to " + ticks }
    val copperBody =
        support.weathering.joinToString(",\n") { (from, to) ->
            "        \"" + from + "\" to \"" + to + "\""
        }
    val fireBody =
        support.fireOdds.joinToString(",\n") { (block, ignite, burn) ->
            "        Triple(" + block + ", " + ignite + ", " + burn + ")"
        }
    val familyBody =
        byFamily.entries.joinToString(",\n") { (name, ids) ->
            val encoded = encodeBlockSet(ids.joinToString(","))
            "        \"" + name + "\" to \"" + encoded + "\""
        }
    file.writeText(
        """
        package org.basaltmc.server.registry

        SUPPORT_KDOC
        internal val ALL_SUPPORT_RULES: List<String> =
            listOf(
        RULES_BODY,
            )

        /** Each rule's block sets, as base64 bitsets over block registry ids (not state ids). */
        internal val ALL_SUPPORT_SETS: List<String> =
            listOf(
        SETS_BODY,
            )

        /** Two characters per block state id: 0 for "nothing holds it up", 1 for a rule not modelled here. */
        internal const val BLOCK_SUPPORT_INDEX: String =
        INDEX_BODY

        /** The states vanilla ticks at random, which is what makes crops grow and ice melt. */
        internal val RANDOM_TICKING_STATES: IntArray =
            intArrayOf(
        TICK_BODY,
            )

        /**
         * How long each item burns in a furnace, in ticks. parity: `FuelValues.vanillaBurnTimes`,
         * which vanilla builds in code rather than in data, so this is that call's answer.
         */
        internal val ALL_FUEL_VALUES: List<Pair<String, Int>> =
            listOf(
        FUEL_BODY,
            )

        /**
         * The states redstone treats as solid: a signal reaches through them and a wire climbs over
         * them. parity: `BlockStateBase.isRedstoneConductor`.
         */
        internal val REDSTONE_CONDUCTOR_STATES: IntArray =
            intArrayOf(
        CONDUCTOR_BODY,
            )

        /** What each copper block turns into as it weathers. parity: `WeatheringCopper.NEXT_BY_BLOCK`. */
        internal val ALL_WEATHERING_STEPS: List<Pair<String, String>> =
            listOf(
        COPPER_BODY,
            )

        /**
         * How readily each block catches fire and how readily it burns away, straight from the two
         * tables `FireBlock` builds at bootstrap: block id, ignite odds, burn odds.
         */
        internal val ALL_FIRE_ODDS: List<Triple<Int, Int, Int>> =
            listOf(
        FIRE_BODY,
            )

        /**
         * Which blocks are a fence, a gate, panes, leaves or a wall, and which refuse to be
         * connected to at all: what `connectsTo` and `attachsTo` ask about, as base64 bitsets over
         * block registry ids.
         */
        internal val BLOCK_FAMILIES: List<Pair<String, String>> =
            listOf(
        FAMILY_BODY,
            )
        """.trimIndent()
            .replace("SUPPORT_KDOC", SUPPORT_KDOC)
            .replace("RULES_BODY", rulesBody)
            .replace("SETS_BODY", setsBody)
            .replace("INDEX_BODY", indexBody)
            .replace("TICK_BODY", tickBody)
            .replace("CONDUCTOR_BODY", conductorBody)
            .replace("FAMILY_BODY", familyBody)
            .replace("FIRE_BODY", fireBody)
            .replace("COPPER_BODY", copperBody)
            .replace("FUEL_BODY", fuelBody) + "\n",
    )
}

val DUMP_BLOCK_SUPPORT_SOURCE =
    """
    import java.lang.reflect.InvocationHandler;
    import java.lang.reflect.Method;
    import java.lang.reflect.Proxy;
    import java.util.ArrayList;
    import java.util.HashMap;
    import java.util.List;
    import java.util.Map;

    import net.minecraft.SharedConstants;
    import net.minecraft.core.BlockPos;
    import net.minecraft.core.Direction;
    import net.minecraft.core.Registry;
    import net.minecraft.core.RegistryAccess;
    import net.minecraft.core.registries.BuiltInRegistries;
    import net.minecraft.server.Bootstrap;
    import net.minecraft.server.packs.PackType;
    import net.minecraft.server.packs.repository.ServerPacksSource;
    import net.minecraft.server.packs.resources.MultiPackResourceManager;
    import net.minecraft.tags.TagLoader;
    import net.minecraft.world.level.LevelReader;
    import net.minecraft.world.level.block.Block;
    import net.minecraft.tags.BlockTags;
    import net.minecraft.world.level.block.Blocks;
    import net.minecraft.world.flag.FeatureFlags;
    import net.minecraft.world.item.Item;
    import net.minecraft.world.level.block.entity.FuelValues;
    import net.minecraft.world.level.block.Fallable;
    import net.minecraft.world.level.block.FallingBlock;
    import net.minecraft.world.level.block.FireBlock;
    import net.minecraft.world.level.block.FenceBlock;
    import net.minecraft.world.level.block.FenceGateBlock;
    import net.minecraft.world.level.block.IronBarsBlock;
    import net.minecraft.world.level.block.LeavesBlock;
    import net.minecraft.world.level.block.StairBlock;
    import net.minecraft.world.level.block.SupportType;
    import net.minecraft.world.level.block.WallBlock;
    import net.minecraft.world.level.block.WeatheringCopper;
    import net.minecraft.world.level.block.state.BlockState;
    import net.minecraft.world.level.material.Fluids;

    /**
     * Asks the real block classes what each state needs under or beside it to stay put.
     *
     * canSurvive is code, not data, so the only honest way to get it is to call it. The level it is
     * called against is a proxy holding a handful of positions: default methods run their own bodies,
     * the few abstract ones answer from that map.
     */
    public final class DumpBlockSupport {
        private static final Map<Long, BlockState> WORLD = new HashMap<>();
        private static final BlockPos ORIGIN = new BlockPos(0, 64, 0);

        public static void main(String[] args) {
            SharedConstants.tryDetectVersion();
            Bootstrap.bootStrap();
            bindVanillaTags();

            LevelReader level = (LevelReader) Proxy.newProxyInstance(
                    DumpBlockSupport.class.getClassLoader(),
                    new Class<?>[] { LevelReader.class },
                    DumpBlockSupport::handle);

            List<Block> blocks = new ArrayList<>();
            for (Block block : BuiltInRegistries.BLOCK) blocks.add(block);

            StringBuilder out = new StringBuilder();
            for (Block block : blocks) {
                StringBuilder families = new StringBuilder();
                BlockState state = block.defaultBlockState();
                if (block instanceof FenceBlock) families.append("fence,");
                if (block instanceof FenceGateBlock) families.append("gate,");
                if (block instanceof IronBarsBlock) families.append("bars,");
                if (block instanceof LeavesBlock) families.append("leaves,");
                if (block instanceof WallBlock) families.append("wall,");
                if (state.is(BlockTags.FENCES)) families.append("fences_tag,");
                if (state.is(BlockTags.WOODEN_FENCES)) families.append("wooden_fences_tag,");
                if (state.is(BlockTags.WALLS)) families.append("walls_tag,");
                if (state.is(BlockTags.PREVENTS_NEARBY_LEAF_DECAY)) families.append("prevents_decay,");
                if (state.is(BlockTags.MAINTAINS_FARMLAND)) families.append("maintains_farmland,");
                if (state.is(BlockTags.SUPPORTS_CACTUS)) families.append("supports_cactus,");
                if (state.is(BlockTags.SNIFFER_EGG_HATCH_BOOST)) families.append("hatch_boost,");
                if (state.is(BlockTags.CAUSES_CONTINUOUS_GEYSER_ERUPTIONS)) families.append("geyser_continuous,");
                if (state.is(BlockTags.CAUSES_PERIODIC_GEYSER_ERUPTIONS)) families.append("geyser_periodic,");
                // Fallable, not FallingBlock: suspicious sand and gravel fall the same way
                // and are the only two that implement it without extending it.
                if (block instanceof Fallable) families.append("falling,");
                if (block instanceof StairBlock) families.append("stairs,");
                if (state.is(BlockTags.SOUL_FIRE_BASE_BLOCKS)) families.append("soul_fire_base,");
                    if (Block.isExceptionForConnection(state)) families.append("exception,");
                if (families.length() > 0) {
                    out.append("FAMILY\t").append(BuiltInRegistries.BLOCK.getId(block)).append('\t')
                       .append(families).append('\n');
                }
            }
            // The two tables `FireBlock` keeps to itself: what catches, and how readily it burns away.
            try {
                java.lang.reflect.Field igniteField = FireBlock.class.getDeclaredField("igniteOdds");
                java.lang.reflect.Field burnField = FireBlock.class.getDeclaredField("burnOdds");
                igniteField.setAccessible(true);
                burnField.setAccessible(true);
                Object ignite = igniteField.get(Blocks.FIRE);
                Object burn = burnField.get(Blocks.FIRE);
                java.lang.reflect.Method getInt = ignite.getClass().getMethod("getInt", Object.class);
                for (Block block : blocks) {
                    int igniteOdds = (Integer) getInt.invoke(ignite, block);
                    int burnOdds = (Integer) getInt.invoke(burn, block);
                    if (igniteOdds == 0 && burnOdds == 0) continue;
                    out.append("FIRE\t").append(BuiltInRegistries.BLOCK.getId(block)).append('\t')
                       .append(igniteOdds).append('\t').append(burnOdds).append('\n');
                }
            } catch (Throwable t) {
                System.err.println("fire odds unavailable: " + t);
            }
            // What each copper block turns into as it weathers, from `WeatheringCopper.NEXT_BY_BLOCK`.
            for (Block block : blocks) {
                java.util.Optional<Block> next = WeatheringCopper.getNext(block);
                if (next.isEmpty()) continue;
                out.append("COPPER\t").append(BuiltInRegistries.BLOCK.getKey(block)).append('\t')
                   .append(BuiltInRegistries.BLOCK.getKey(next.get())).append('\n');
            }
            int always = 0;
            int composite = 0;
            for (Block block : blocks) {
                for (BlockState state : block.getStateDefinition().getPossibleStates()) {
                    int id = Block.getId(state);
                    if (state.isRandomlyTicking()) out.append("RANDOMTICK\t").append(id).append('\n');
                    // What redstone treats as solid: the states a signal travels through and that a
                    // wire climbs over. parity: `BlockBehaviour.Properties.isRedstoneConductor`.
                    if (state.isRedstoneConductor(level, ORIGIN)) {
                        out.append("CONDUCTOR\t").append(id).append('\n');
                    }
                    WORLD.clear();
                    if (survives(state, level)) {
                        always++;
                        continue;
                    }
                    StringBuilder line = new StringBuilder();
                    StringBuilder wetLine = new StringBuilder();
                    boolean any = false;
                    boolean crowded = true;
                    for (Direction direction : Direction.values()) {
                        StringBuilder members = new StringBuilder();
                        StringBuilder wetMembers = new StringBuilder();
                        Block first = null;
                        for (Block candidate : blocks) {
                            WORLD.clear();
                            WORLD.put(ORIGIN.relative(direction).asLong(), candidate.defaultBlockState());
                            if (survives(state, level)) {
                                if (members.length() > 0) members.append(',');
                                members.append(BuiltInRegistries.BLOCK.getId(candidate));
                                any = true;
                                if (first == null) first = candidate;
                            }
                            // The same question in water: sugar cane wants a river beside it and kelp
                            // wants to be under one, and a probe of dry neighbours says they can live
                            // nowhere at all. A block that answers here and not above is never broken
                            // for standing in the wrong place; it is only kept.
                            flood(direction, candidate);
                            if (survives(state, level)) {
                                if (wetMembers.length() > 0) wetMembers.append(',');
                                wetMembers.append(BuiltInRegistries.BLOCK.getId(candidate));
                                any = true;
                            }
                        }
                        if (wetLine.length() > 0) wetLine.append('\t');
                        wetLine.append(direction.getName()).append('=')
                               .append(wetMembers.toString().contentEquals(members) ? "" : wetMembers);
                        // Whether that same support still holds with the other five sides walled in: a
                        // block that says no here has a condition about what must *not* be next to it,
                        // and one neighbour at a time cannot see it.
                        if (first != null) {
                            WORLD.clear();
                            for (Direction other : Direction.values()) {
                                WORLD.put(ORIGIN.relative(other).asLong(),
                                        (other == direction ? first : Blocks.STONE).defaultBlockState());
                            }
                            if (!survives(state, level)) crowded = false;
                        }
                        if (line.length() > 0) line.append('\t');
                        line.append(direction.getName()).append('=').append(shorten(members.toString(), direction, blocks, level));
                    }
                    if (!any) composite++;
                    out.append("SUPPORT\t").append(id).append('\t')
                       .append(BuiltInRegistries.BLOCK.getKey(block)).append('\t')
                       .append(any ? "1" : "0").append('\t')
                       .append(crowded ? "1" : "0").append('\t')
                       .append(line).append('\n');
                    if (wetLine.toString().replace("=", "").replace("\t", "")
                            .replaceAll("[a-z]", "").length() > 0) {
                        out.append("WET\t").append(id).append('\t').append(wetLine).append('\n');
                    }
                }
            }
            // What a furnace burns and for how long. Vanilla builds this in code, not in data, so
            // the only honest source is calling it.
            FuelValues fuels = FuelValues.vanillaBurnTimes(
                    RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY), FeatureFlags.VANILLA_SET);
            // Straight off the map it keeps: asking `burnDuration` wants an ItemStack, and a stack
            // cannot be built here because the item components are not bound in this harness.
            try {
                java.lang.reflect.Field valuesField = FuelValues.class.getDeclaredField("values");
                valuesField.setAccessible(true);
                for (Map.Entry<?, ?> entry : ((Map<?, ?>) valuesField.get(fuels)).entrySet()) {
                    int ticks = ((Number) entry.getValue()).intValue();
                    if (ticks <= 0) continue;
                    out.append("FUEL\t").append(BuiltInRegistries.ITEM.getKey((Item) entry.getKey()).getPath())
                       .append('\t').append(ticks).append('\n');
                }
            } catch (ReflectiveOperationException failure) {
                throw new IllegalStateException("FuelValues.values moved: " + failure, failure);
            }
            System.err.println("always=" + always + " composite=" + composite);
            System.out.print(out);
        }

        /**
         * Binds the block tags from the built-in datapack. Without them every `state.is(SOME_TAG)` is
         * false, and a sapling comes out unable to stand on anything at all.
         */
        private static void bindVanillaTags() {
            MultiPackResourceManager manager = new MultiPackResourceManager(
                    PackType.SERVER_DATA, List.of(ServerPacksSource.createVanillaPackSource()));
            RegistryAccess.Frozen registries = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);
            for (Registry.PendingTags<?> pending : TagLoader.loadTagsForExistingRegistries(manager, registries)) {
                pending.apply();
            }
        }

        /**
         * Replaces a member list with `*` or `c` when it is exactly the blocks whose face towards this
         * one is sturdy, or supports its centre. That is the condition most blocks really ask about,
         * and naming it keeps the answer right for a state the probe never placed: an upside-down slab
         * holds a torch, its default state does not, and a list of blocks cannot tell them apart.
         */
        private static String shorten(String members, Direction direction, List<Block> blocks, LevelReader level) {
            if (members.isEmpty()) return members;
            StringBuilder full = new StringBuilder();
            StringBuilder centre = new StringBuilder();
            StringBuilder rigid = new StringBuilder();
            Direction facing = direction.getOpposite();
            BlockPos pos = ORIGIN.relative(direction);
            for (Block candidate : blocks) {
                WORLD.clear();
                BlockState support = candidate.defaultBlockState();
                WORLD.put(pos.asLong(), support);
                int id = BuiltInRegistries.BLOCK.getId(candidate);
                if (support.isFaceSturdy(level, pos, facing, SupportType.FULL)) {
                    if (full.length() > 0) full.append(',');
                    full.append(id);
                }
                if (support.isFaceSturdy(level, pos, facing, SupportType.CENTER)) {
                    if (centre.length() > 0) centre.append(',');
                    centre.append(id);
                }
                if (support.isFaceSturdy(level, pos, facing, SupportType.RIGID)) {
                    if (rigid.length() > 0) rigid.append(',');
                    rigid.append(id);
                }
            }
            if (members.contentEquals(full)) return "*";
            if (members.contentEquals(centre)) return "c";
            if (members.contentEquals(rigid)) return "r";
            return members;
        }

        /** The candidate in place, water at this position and around it: the wet half of every probe. */
        private static void flood(Direction direction, Block candidate) {
            WORLD.clear();
            WORLD.put(ORIGIN.asLong(), Blocks.WATER.defaultBlockState());
            for (Direction side : Direction.Plane.HORIZONTAL) {
                WORLD.put(ORIGIN.relative(side).asLong(), Blocks.WATER.defaultBlockState());
                // Sugar cane looks beside the block it stands on, not beside itself.
                WORLD.put(ORIGIN.below().relative(side).asLong(), Blocks.WATER.defaultBlockState());
            }
            WORLD.put(ORIGIN.relative(direction).asLong(), candidate.defaultBlockState());
        }

        private static boolean survives(BlockState state, LevelReader level) {
            try {
                return state.canSurvive(level, ORIGIN);
            } catch (Throwable t) {
                return true;
            }
        }

        private static Object handle(Object proxy, Method method, Object[] args) throws Throwable {
            String name = method.getName();
            if (name.equals("getBlockState") && args != null && args.length == 1) {
                BlockState state = WORLD.get(((BlockPos) args[0]).asLong());
                return state == null ? Blocks.AIR.defaultBlockState() : state;
            }
            if (name.equals("getFluidState") && args != null && args.length == 1) {
                BlockState state = WORLD.get(((BlockPos) args[0]).asLong());
                return state == null ? Fluids.EMPTY.defaultFluidState() : state.getFluidState();
            }
            if (name.equals("getBlockEntity")) return null;
            if (name.equals("getMinY")) return 0;
            if (name.equals("getHeight") && method.getParameterCount() == 0) return 384;
            if (name.equals("isClientSide")) return false;
            // Full daylight, not the zero an unanswered int method gave: a crop asks for light in
            // the same canSurvive this probe is measuring support with, so darkness made every
            // direction fail and the state came out as if nothing held it up at all.
            if (name.equals("getRawBrightness") || name.equals("getBrightness")
                    || name.equals("getMaxLocalRawBrightness")) {
                return 15;
            }
            if (method.isDefault()) return InvocationHandler.invokeDefault(proxy, method, args);
            Class<?> returnType = method.getReturnType();
            if (returnType == boolean.class) return false;
            if (returnType == int.class) return 0;
            if (returnType == long.class) return 0L;
            if (returnType == float.class) return 0f;
            if (returnType == double.class) return 0d;
            if (returnType == void.class) return null;
            return null;
        }
    }
    """.trimIndent()
