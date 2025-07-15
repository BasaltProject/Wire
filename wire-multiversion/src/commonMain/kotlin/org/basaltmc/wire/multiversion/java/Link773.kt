package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.MutablePacket
import org.basaltmc.wire.multiversion.VersionTables
import org.basaltmc.wire.nbt.NbtCompound
import org.basaltmc.wire.nbt.NbtTag
import org.basaltmc.wire.nbt.Snbt
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.configuration.RegistryDataPacket
import org.basaltmc.wire.protocol.play.SetBorderLerpSizePacket

/**
 * 1.21.11 to 1.21.9.
 *
 * The world border takes ticks here and milliseconds there, which is fifty times the number.
 *
 * The bigger half is the registries: 1.21.11 gathered what a dimension and a biome are into named
 * attributes, and a client below it knows none of those names and refuses to load anything. So a
 * dimension is sent that version's own definition and a biome's colours go back into its effects.
 */
internal class Link773(
    tables: VersionTables,
) : RenumberingLink(tables) {
    override fun clientbound(
        state: ProtocolState,
        packet: MutablePacket,
    ): Boolean {
        if (!super.clientbound(state, packet)) return false
        if (state == ProtocolState.CONFIGURATION && packet.nativeId == RegistryDataPacket.ID) {
            rewriteRegistry(packet)
            return true
        }
        if (state == ProtocolState.PLAY && packet.nativeId == SetBorderLerpSizePacket.ID) {
            rewrite(packet) { input, output ->
                val border = SetBorderLerpSizePacket.read(input)
                border.copy(lerpTimeTicks = border.lerpTimeTicks * MILLIS_PER_TICK).write(output)
            }
        }
        return true
    }

    /** A dimension and a biome, as the version below the attributes reads them. */
    private fun rewriteRegistry(packet: MutablePacket) {
        val data = decode(packet) { RegistryDataPacket.read(it) }
        val rewritten =
            when (data.registryId) {
                DIMENSION_TYPE -> data.entries.map { it.copy(value = ownDimension(it.key)) }
                BIOME -> data.entries.map { it.copy(value = oldEffects(it.value)) }
                else -> return
            }
        encode(packet) { RegistryDataPacket(data.registryId, rewritten).write(it) }
    }

    /**
     * That version's own definition of the dimension, by name.
     *
     * Not a translation: what a dimension is was rearranged, and half of what the older one needs
     * (whether it is natural, whether it burns forever) is not in what arrives at all. A dimension
     * it has never heard of is shown the overworld, which is the one every client has.
     */
    private fun ownDimension(name: String): NbtTag? {
        val text = tables.dimensionTypes[name] ?: tables.dimensionTypes[OVERWORLD] ?: return null
        return Snbt.parse(text)
    }

    /** The colours and sounds of a biome, back in the one block of effects they used to live in. */
    private fun oldEffects(value: NbtTag?): NbtTag? {
        val biome = value as? NbtCompound ?: return value
        val attributes = biome["attributes"] as? NbtCompound
        val effects = biome["effects"] as? NbtCompound ?: NbtCompound()
        val rewritten = NbtCompound()
        for ((name, tag) in biome) if (name != "attributes" && name != "effects") rewritten[name] = tag

        val old = NbtCompound()
        for ((name, tag) in effects) {
            val colour = colourOf(tag)
            if (colour != null) old.putInt(name, colour) else old[name] = tag
        }
        old.putInt(FOG, colourFrom(attributes, VISUAL_FOG) ?: old.getIntOrNull(FOG) ?: DEFAULT_FOG)
        old.putInt(WATER, old.getIntOrNull(WATER) ?: DEFAULT_WATER)
        old.putInt(
            WATER_FOG,
            colourFrom(attributes, VISUAL_WATER_FOG) ?: old.getIntOrNull(WATER_FOG) ?: DEFAULT_WATER_FOG,
        )
        old.putInt(
            SKY,
            colourFrom(attributes, VISUAL_SKY) ?: old.getIntOrNull(SKY) ?: skyFor(biome.getFloat(TEMPERATURE, 0.8f)),
        )
        (attributes?.get(AUDIO_AMBIENT) as? NbtCompound)?.get("mood")?.let { old["mood_sound"] = it }
        (attributes?.get(AUDIO_MUSIC) as? NbtCompound)?.get("default")?.let { old["music"] = it }
        rewritten["effects"] = old
        return rewritten
    }

    private fun NbtCompound.getIntOrNull(key: String): Int? = (this[key] as? org.basaltmc.wire.nbt.NbtInt)?.value

    private fun colourFrom(
        attributes: NbtCompound?,
        key: String,
    ): Int? = colourOf(attributes?.get(key))

    /** 26.x writes a colour as `#rrggbb`; every version below it reads the number. */
    private fun colourOf(tag: NbtTag?): Int? {
        val text = (tag as? org.basaltmc.wire.nbt.NbtString)?.value ?: return null
        if (!text.startsWith("#")) return null
        val digits = text.substring(1)
        val value = digits.toLongOrNull(16) ?: return null
        return (value and 0xFFFFFF).toInt()
    }

    /**
     * The sky a temperature gives, which older versions are told outright and this one works out.
     *
     * parity: `OverworldBiomeBuilder.calculateSkyColor`, hue and saturation moving with the
     * temperature and full value.
     */
    private fun skyFor(temperature: Float): Int {
        val scaled = (temperature / 3f).coerceIn(-1f, 1f)
        return hsvToRgb(0.62222224f - scaled * 0.05f, 0.5f + scaled * 0.1f, 1f)
    }

    private fun hsvToRgb(
        hue: Float,
        saturation: Float,
        value: Float,
    ): Int {
        val sector = (hue * 6f).toInt() % 6
        val fraction = hue * 6f - sector
        val p = value * (1f - saturation)
        val q = value * (1f - fraction * saturation)
        val t = value * (1f - (1f - fraction) * saturation)
        val (red, green, blue) =
            when (sector) {
                0 -> Triple(value, t, p)
                1 -> Triple(q, value, p)
                2 -> Triple(p, value, t)
                3 -> Triple(p, q, value)
                4 -> Triple(t, p, value)
                else -> Triple(value, p, q)
            }
        return ((red * 255f).toInt() shl 16) or ((green * 255f).toInt() shl 8) or (blue * 255f).toInt()
    }

    private companion object {
        const val DIMENSION_TYPE = "minecraft:dimension_type"
        const val BIOME = "minecraft:worldgen/biome"
        const val OVERWORLD = "minecraft:overworld"
        const val TEMPERATURE = "temperature"

        const val VISUAL_FOG = "minecraft:visual/fog_color"
        const val VISUAL_SKY = "minecraft:visual/sky_color"
        const val VISUAL_WATER_FOG = "minecraft:visual/water_fog_color"
        const val AUDIO_AMBIENT = "minecraft:audio/ambient_sounds"
        const val AUDIO_MUSIC = "minecraft:audio/background_music"

        const val FOG = "fog_color"
        const val WATER = "water_color"
        const val WATER_FOG = "water_fog_color"
        const val SKY = "sky_color"

        /** parity: `BiomeSpecialEffects`' own defaults, which is what a biome that says nothing had. */
        const val DEFAULT_FOG = 12638463
        const val DEFAULT_WATER = 4159204
        const val DEFAULT_WATER_FOG = 329011

        const val MILLIS_PER_TICK = 50L
    }
}
