package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.io.ByteBuffer
import org.basaltmc.wire.multiversion.IdMap
import org.basaltmc.wire.multiversion.ProtocolVersions
import org.basaltmc.wire.protocol.ProtocolState
import org.basaltmc.wire.protocol.play.AttackPacket
import org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket
import org.basaltmc.wire.protocol.play.InteractPacket
import org.basaltmc.wire.protocol.play.LoginPlayPacket
import org.basaltmc.wire.protocol.play.SetTimePacket
import org.basaltmc.wire.protocol.readAnonymousNbt
import org.basaltmc.wire.protocol.readProtocolString
import org.basaltmc.wire.protocol.readUuid
import org.basaltmc.wire.protocol.readVarInt
import org.basaltmc.wire.protocol.writeAnonymousNbt
import org.basaltmc.wire.protocol.writeBoolean
import org.basaltmc.wire.protocol.writeProtocolString
import org.basaltmc.wire.protocol.writeVarInt
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/** Several links at once, which is the thing a chain is for. */
class ChainTest {
    @Test
    fun `every supported version is reachable from the one above it`() {
        val supported = JavaTranslators.supported.sorted()
        val known = ProtocolVersions.known.map { it.number }.filter { it in supported }
        assertEquals(known, supported, "a version is supported with a gap above it")
        assertTrue(ProtocolVersions.NATIVE in supported)
    }

    @Test
    fun `a chain of three links renumbers a block twice`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_9))
        val body = ByteBuffer().also { ClientboundBlockUpdatePacket(POS, DEEPSLATE_776).write(it) }.toByteArray()
        var outId = -1
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, ClientboundBlockUpdatePacket.ID, body) { id, bytes ->
            outId = id
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        assertEquals(DEEPSLATE_773, ClientboundBlockUpdatePacket.read(out).blockStateId)
        // 1.21.9 numbers the packet itself differently too.
        assertEquals(BLOCK_UPDATE_773, outId)
    }

    @Test
    fun `the clock a client without named clocks understands`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_11))
        val body =
            ByteBuffer()
                .also {
                    SetTimePacket(
                        gameTime = 1000L,
                        clocks =
                            listOf(
                                org.basaltmc.wire.protocol.play
                                    .ClockUpdate(SetTimePacket.OVERWORLD_CLOCK, 6000L, 0f, 1f),
                                org.basaltmc.wire.protocol.play
                                    .ClockUpdate(SetTimePacket.END_CLOCK, 0L, 0f, 0f),
                            ),
                    ).write(it)
                }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, SetTimePacket.ID, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        assertEquals(1000L, out.readLong())
        assertEquals(6000L, out.readLong(), "the overworld clock is the day time")
        assertEquals(1, out.readByte().toInt(), "a running clock is a running day")
    }

    @Test
    fun `a stopped clock reaches an older client as a stopped day`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_11))
        val body =
            ByteBuffer()
                .also {
                    SetTimePacket(
                        gameTime = 50L,
                        clocks =
                            listOf(
                                org.basaltmc.wire.protocol.play
                                    .ClockUpdate(SetTimePacket.OVERWORLD_CLOCK, 18000L, 0f, 0f),
                            ),
                    ).write(it)
                }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, SetTimePacket.ID, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        out.readLong()
        assertEquals(18000L, out.readLong())
        assertEquals(0, out.readByte().toInt())
    }

    @Test
    fun `hitting something on 1_21_11 arrives as the attack packet`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_11))
        // 1.21.11's interact, action 1: an attack, with no hand and no position.
        val body =
            ByteBuffer()
                .apply {
                    writeVarInt(42)
                    writeVarInt(1)
                    writeBoolean(true)
                }.toByteArray()
        var outId = -1
        var out = ByteBuffer()
        chain.serverbound(ProtocolState.PLAY, INTERACT_774, body) { id, bytes ->
            outId = id
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        assertEquals(AttackPacket.ID, outId, "an old attack has to become the packet 26.2 split out")
        assertEquals(42, AttackPacket.read(out).entityId)
    }

    @Test
    fun `right clicking a spot on an entity keeps where it was clicked`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_11))
        val body =
            ByteBuffer()
                .apply {
                    writeVarInt(7)
                    writeVarInt(2)
                    writeFloat(0.5f)
                    writeFloat(1.25f)
                    writeFloat(-0.25f)
                    writeVarInt(InteractPacket.OFF_HAND)
                    writeBoolean(false)
                }.toByteArray()
        var outId = -1
        var out = ByteBuffer()
        chain.serverbound(ProtocolState.PLAY, INTERACT_774, body) { id, bytes ->
            outId = id
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        assertEquals(InteractPacket.ID, outId)
        val interact = InteractPacket.read(out)
        assertEquals(7, interact.entityId)
        assertEquals(InteractPacket.OFF_HAND, interact.hand)
        // The position travels quantized on the modern side, so it comes back close rather than equal.
        assertTrue(kotlin.math.abs(interact.x - 0.5) < 0.01, "x was lost: ${interact.x}")
        assertTrue(kotlin.math.abs(interact.y - 1.25) < 0.01, "y was lost: ${interact.y}")
    }

    @Test
    fun `a packet a version does not have is dropped rather than sent as another one`() {
        // 1.21.11 added the spear; 1.21.10 has no name for it, so it must come back as nothing
        // rather than as whatever item happens to sit at that number over there.
        assertEquals(IdMap.NONE, TABLES_773.item.map(COPPER_SPEAR_774))
    }

    private companion object {
        val POS =
            org.basaltmc.wire.protocol.play
                .BlockPos(4, 5, 6)
        const val DEEPSLATE_776 = 30417
        const val DEEPSLATE_773 = 27722
        const val BLOCK_UPDATE_773 = 8
        const val INTERACT_774 = 0x19
        const val COPPER_SPEAR_774 = 1298
    }
}

/** The three serverbound packets a bot sends, checked against every supported version. */
class ServerboundIdsTest {
    @Test
    fun `what an older client sends arrives as the packet this server reads`() {
        // Accepting a teleport, answering a keep-alive, and moving: what every client does.
        val sends =
            mapOf(
                ProtocolVersions.V26_1 to listOf(0, 28, 31),
                ProtocolVersions.V1_21_11 to listOf(0, 27, 30),
                ProtocolVersions.V1_21_9 to listOf(0, 27, 30),
                ProtocolVersions.V1_21_5 to listOf(0, 26, 29),
                ProtocolVersions.V1_21_4 to listOf(0, 26, 29),
                ProtocolVersions.V1_21_2 to listOf(0, 26, 29),
                ProtocolVersions.V1_17 to listOf(0, 15, 18),
                ProtocolVersions.V1_16_4 to listOf(0, 16, 19),
            )
        for ((protocol, ids) in sends) {
            val chain = checkNotNull(JavaTranslators.chainFor(protocol))
            val seen = mutableListOf<Int>()
            // Bodies of the right size: a link on the way up may read what the client sent.
            val bodies =
                listOf(
                    ByteBuffer().apply { writeVarInt(1) }.toByteArray(),
                    ByteBuffer().apply { writeLong(1L) }.toByteArray(),
                    ByteBuffer()
                        .apply {
                            writeDouble(1.0)
                            writeDouble(2.0)
                            writeDouble(3.0)
                            writeFloat(0f)
                            writeFloat(0f)
                            writeByte(1)
                        }.toByteArray(),
                )
            for ((index, id) in ids.withIndex()) {
                chain.serverbound(ProtocolState.PLAY, id, bodies[index]) { native, _ -> seen += native }
            }
            assertEquals(listOf(0, 28, 31), seen, "protocol $protocol translated its own ids wrongly")
        }
    }
}

/** The link where a velocity stopped being three shorts. */
class Link772Test {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_7))

    @Test
    fun `a velocity reaches 1_21_8 as the three shorts it expects`() {
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .SetEntityMotionPacket(9, 0.5, -0.25, 1.0)
                        .write(it)
                }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.SetEntityMotionPacket.ID,
            body,
        ) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        assertEquals(9, out.readVarInt())
        assertEquals(4000, out.readShort().toInt())
        assertEquals(-2000, out.readShort().toInt())
        assertEquals(8000, out.readShort().toInt())
        assertEquals(0, out.readableBytes, "the old velocity is exactly three shorts")
    }

    @Test
    fun `a velocity past what a short can hold is clamped rather than wrapped`() {
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .SetEntityMotionPacket(1, 100.0, 0.0, 0.0)
                        .write(it)
                }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.SetEntityMotionPacket.ID,
            body,
        ) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        out.readVarInt()
        assertEquals(31200, out.readShort().toInt(), "3.9 blocks a tick is the ceiling, not an overflow")
    }

    @Test
    fun `the spawn point loses the dimension a 1_21_8 client has no field for`() {
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .SpawnPositionPacket(
                            org.basaltmc.wire.protocol.play.GlobalPos(
                                "minecraft:overworld",
                                org.basaltmc.wire.protocol.play
                                    .BlockPos(8, 64, -8),
                            ),
                            90f,
                            0f,
                        ).write(it)
                }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.SpawnPositionPacket.ID,
            body,
        ) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        assertEquals(
            org.basaltmc.wire.protocol.play
                .BlockPos(8, 64, -8),
            org.basaltmc.wire.protocol.play.BlockPos
                .unpack(out.readLong()),
        )
        assertEquals(90f, out.readFloat())
        assertEquals(0, out.readableBytes)
    }
}

/** An explosion, which is both renumbered and reshaped on the way to 1.21.8. */
class ExplosionTest {
    private fun explosion() =
        org.basaltmc.wire.protocol.play.ExplodePacket(
            x = 10.0,
            y = 65.0,
            z = -20.0,
            radius = 3f,
            blockCount = 7,
            knockbackX = 0.25,
            knockbackY = 0.5,
            knockbackZ = 0.0,
            particleId = FLAME_776,
            sound = "minecraft:entity.generic.explode",
            blockParticles =
                listOf(
                    org.basaltmc.wire.protocol.play
                        .ExplosionParticle(FLAME_776, 0.5f, 1f, 1),
                ),
        )

    private fun through(protocol: Int): ByteBuffer {
        val chain = checkNotNull(JavaTranslators.chainFor(protocol))
        val body = ByteBuffer().also { explosion().write(it) }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.ExplodePacket.ID, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        return out
    }

    @Test
    fun `its particles are renumbered for a version that still has the same shape`() {
        val explode =
            org.basaltmc.wire.protocol.play.ExplodePacket
                .read(through(ProtocolVersions.V26_1))
        assertEquals(FLAME_775, explode.particleId)
        assertEquals(FLAME_775, explode.blockParticles.single().particleId)
        assertEquals(7, explode.blockCount, "everything else came through unchanged")
    }

    @Test
    fun `1_21_8 gets the shorter explosion it knows`() {
        val out = through(ProtocolVersions.V1_21_7)
        assertEquals(10.0, out.readDouble())
        assertEquals(65.0, out.readDouble())
        assertEquals(-20.0, out.readDouble())
        assertEquals(1, out.readByte().toInt(), "the knockback is there")
        assertEquals(0.25, out.readDouble())
        out.readDouble()
        out.readDouble()
        assertTrue(out.readableBytes > 0, "the particle and the sound follow")
    }

    private companion object {
        const val FLAME_776 = 39
        const val FLAME_775 = 32
    }
}

/** The two things a link fixes that are not numbering: units, and effects that did not exist. */
class Link773Test {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_9))

    private fun through(
        id: Int,
        write: (ByteBuffer) -> Unit,
    ): ByteBuffer {
        val body = ByteBuffer().also(write).toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, id, body) { _, bytes -> out = ByteBuffer().apply { writeBytes(bytes) } }
        return out
    }

    @Test
    fun `the border is told milliseconds by a client that counts in milliseconds`() {
        val out =
            through(org.basaltmc.wire.protocol.play.SetBorderLerpSizePacket.ID) {
                org.basaltmc.wire.protocol.play
                    .SetBorderLerpSizePacket(100.0, 50.0, 200L)
                    .write(it)
            }
        val border =
            org.basaltmc.wire.protocol.play.SetBorderLerpSizePacket
                .read(out)
        assertEquals(10_000L, border.lerpTimeTicks, "ten seconds of ticks is ten seconds of milliseconds")
        assertEquals(100.0, border.oldSize)
    }

    @Test
    fun `an effect 1_21_9 has no name for is not sent at all`() {
        val out =
            through(org.basaltmc.wire.protocol.play.UpdateMobEffectPacket.ID) {
                org.basaltmc.wire.protocol.play
                    .UpdateMobEffectPacket(1, BREATH_OF_THE_NAUTILUS, 0, 100)
                    .write(it)
            }
        assertEquals(0, out.readableBytes, "an effect that does not exist over there went out anyway")
    }

    @Test
    fun `an effect both versions have keeps its own number`() {
        val out =
            through(org.basaltmc.wire.protocol.play.UpdateMobEffectPacket.ID) {
                org.basaltmc.wire.protocol.play
                    .UpdateMobEffectPacket(1, SPEED, 1, 200)
                    .write(it)
            }
        assertEquals(
            SPEED,
            org.basaltmc.wire.protocol.play.UpdateMobEffectPacket
                .read(out)
                .effect,
        )
    }

    private companion object {
        const val SPEED = 1
        const val BREATH_OF_THE_NAUTILUS = 39
    }
}

/** A named, enchanted item, which is the case where component numbering shows. */
class ItemComponentTest {
    /**
     * The component ids of the stack as it leaves for [protocol].
     *
     * Read by hand rather than with the packet's own reader: the whole point is that the bytes are
     * no longer in this server's numbering, so this server's reader would refuse them.
     */
    private fun componentIdsFor(protocol: Int): List<Int> {
        val stack =
            org.basaltmc.wire.protocol.play.WireItemStack(
                itemId = DIAMOND_SWORD_776,
                count = 1,
                components =
                    listOf(
                        org.basaltmc.wire.protocol.play.ItemComponent.Text(
                            org.basaltmc.wire.protocol.play.ItemComponent.CUSTOM_NAME,
                            org.basaltmc.wire.text.Component
                                .text("Sting"),
                        ),
                        org.basaltmc.wire.protocol.play.ItemComponent
                            .Enchantments(mapOf(2 to 3)),
                    ),
            )
        val chain = checkNotNull(JavaTranslators.chainFor(protocol))
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .ContainerSetSlotPacket(0, 1, 5, stack)
                        .write(it)
                }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.ContainerSetSlotPacket.ID, body) { _, b ->
            out = ByteBuffer().apply { writeBytes(b) }
        }
        out.readVarInt() // container id
        out.readVarInt() // state id
        out.readShort() // slot
        out.readVarInt() // count
        out.readVarInt() // item id
        val added = out.readVarInt()
        out.readVarInt() // removed count
        return List(added) {
            val id = out.readVarInt()
            skipPayload(out, id)
            id
        }
    }

    /** Walks past a payload this test knows the shape of: a name is one nbt tag, levels are pairs. */
    private fun skipPayload(
        buffer: ByteBuffer,
        id: Int,
    ) {
        if (id == NAME_ID_1_21_10 || id == org.basaltmc.wire.protocol.play.ItemComponent.CUSTOM_NAME) {
            buffer.readAnonymousNbt()
        } else {
            repeat(buffer.readVarInt()) {
                buffer.readVarInt()
                buffer.readVarInt()
            }
        }
    }

    @Test
    fun `its components keep their numbers where the version did not move them`() {
        assertEquals(listOf(6, 13), componentIdsFor(ProtocolVersions.V1_21_11), "1.21.11 numbers these the same way")
    }

    @Test
    fun `its components are renumbered where the version moved them`() {
        // 1.21.10 calls the custom name 5 and the enchantments 10.
        assertEquals(listOf(5, 10), componentIdsFor(ProtocolVersions.V1_21_9))
    }

    private companion object {
        const val DIAMOND_SWORD_776 = 892
        const val NAME_ID_1_21_10 = 5
    }
}

/** The link where the chunk itself changed shape. */
class Link769Test {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_4))

    @Test
    fun `a chunk reaches 1_21_4 with named heightmaps and counted palettes`() {
        val blocks = IntArray(4096) { STONE_776 }
        val sections =
            ByteBuffer()
                .apply {
                    writeShort(4096)
                    writeShort(0)
                    org.basaltmc.wire.protocol.play.PalettedContainer
                        .write(this, blocks, 4, 8, 15)
                    org.basaltmc.wire.protocol.play.PalettedContainer
                        .write(this, IntArray(64), 0, 3, 6)
                }.toByteArray()
        val packet =
            org.basaltmc.wire.protocol.play.MapChunkPacket(
                x = 3,
                z = -4,
                heightmaps =
                    listOf(
                        org.basaltmc.wire.protocol.play.Heightmap(
                            org.basaltmc.wire.protocol.play.HeightmapType.MOTION_BLOCKING,
                            longArrayOf(1L, 2L),
                        ),
                    ),
                chunkData = sections,
                blockEntities = emptyList(),
                skyLightMask = longArrayOf(),
                blockLightMask = longArrayOf(),
                emptySkyLightMask = longArrayOf(),
                emptyBlockLightMask = longArrayOf(),
                skyLight = emptyList(),
                blockLight = emptyList(),
            )
        val body = ByteBuffer().also { packet.write(it) }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.MapChunkPacket.ID, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }

        assertEquals(3, out.readInt())
        assertEquals(-4, out.readInt())
        val heightmaps = out.readAnonymousNbt()
        assertTrue(heightmaps is org.basaltmc.wire.nbt.NbtCompound)
        assertTrue((heightmaps as org.basaltmc.wire.nbt.NbtCompound).keys.contains("MOTION_BLOCKING"))

        val data = ByteBuffer().apply { writeBytes(out.readBytes(out.readVarInt())) }
        assertEquals(4096, data.readShort().toInt())
        // No fluid count: 26.1 added that short and 1.21.4 has no field for it.
        // A single-value palette follows: the width, the value, and the count of longs behind it,
        // which is zero and which this version still reads.
        assertEquals(0, data.readByte().toInt())
        assertEquals(STONE_769, data.readVarInt())
        assertEquals(0, data.readVarInt(), "a container of one value still counts its longs")
    }

    private companion object {
        const val STONE_776 = 1
        const val STONE_769 = 1
    }
}

/** The two small fields 1.21.4 changed. */
class Link768Test {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_2))

    private fun through(
        id: Int,
        write: (ByteBuffer) -> Unit,
    ): ByteBuffer {
        val body = ByteBuffer().also(write).toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, id, body) { _, bytes -> out = ByteBuffer().apply { writeBytes(bytes) } }
        return out
    }

    @Test
    fun `the slot in the hand is one byte again`() {
        val out =
            through(org.basaltmc.wire.protocol.play.SetHeldSlotPacket.ID) {
                org.basaltmc.wire.protocol.play
                    .SetHeldSlotPacket(7)
                    .write(it)
            }
        assertEquals(7, out.readByte().toInt())
        assertEquals(0, out.readableBytes)
    }

    @Test
    fun `a particle loses the flag 1_21_3 has no field for`() {
        val out =
            through(org.basaltmc.wire.protocol.play.LevelParticlesPacket.ID) {
                org.basaltmc.wire.protocol.play
                    .LevelParticlesPacket(
                        particleTypeId = FLAME_776,
                        x = 1.0,
                        y = 2.0,
                        z = 3.0,
                        overrideLimiter = true,
                        alwaysShow = true,
                        count = 4,
                    ).write(it)
            }
        assertEquals(1, out.readByte().toInt(), "override limiter stays")
        assertEquals(1.0, out.readDouble(), "and the position follows it straight away")
    }

    private companion object {
        const val FLAME_776 = 39
    }
}

/** The login packet, which is the one an older client reads last before the game starts. */
class LoginFinishedTest {
    @Test
    fun `an older client is not sent the session id 26_2 added`() {
        val profile =
            org.basaltmc.wire.protocol.login.LoginFinishedPacket(
                org.basaltmc.wire.protocol.login.GameProfile(
                    id = kotlin.uuid.Uuid.fromLongs(1L, 2L),
                    name = "Casey",
                    properties = emptyList(),
                ),
                sessionId = kotlin.uuid.Uuid.fromLongs(3L, 4L),
            )
        val body = ByteBuffer().also { profile.write(it) }.toByteArray()
        for (protocol in JavaTranslators.supported.filter { it != ProtocolVersions.NATIVE }) {
            val chain = checkNotNull(JavaTranslators.chainFor(protocol))
            var out = ByteBuffer()
            chain.clientbound(
                ProtocolState.LOGIN,
                org.basaltmc.wire.protocol.login.LoginFinishedPacket.ID,
                body,
            ) { _, b ->
                out = ByteBuffer().apply { writeBytes(b) }
            }
            // 1.21 and below read one flag more than the versions above them, so those are one
            // byte longer than the sixteen the session id took.
            val expected = if (protocol <= ProtocolVersions.V1_21) body.size - 15 else body.size - 16
            assertEquals(
                expected,
                out.readableBytes,
                "protocol $protocol was sent a session id it has no field for",
            )
        }
    }
}

/** The step where three registries stop existing. */
class Link766Test {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_20_5))

    private fun sent(
        state: ProtocolState,
        id: Int,
        write: (ByteBuffer) -> Unit,
    ): Boolean {
        val body = ByteBuffer().also(write).toByteArray()
        var arrived = false
        chain.clientbound(state, id, body) { _, _ -> arrived = true }
        return arrived
    }

    @Test
    fun `a registry 1_20_5 does not have is not sent`() {
        assertFalse(
            sent(ProtocolState.CONFIGURATION, org.basaltmc.wire.protocol.configuration.RegistryDataPacket.ID) {
                org.basaltmc.wire.protocol.configuration
                    .RegistryDataPacket(
                        "minecraft:enchantment",
                        emptyList(),
                    ).write(it)
            },
        )
    }

    @Test
    fun `a registry it does have still goes out`() {
        assertTrue(
            sent(ProtocolState.CONFIGURATION, org.basaltmc.wire.protocol.configuration.RegistryDataPacket.ID) {
                org.basaltmc.wire.protocol.configuration
                    .RegistryDataPacket(
                        "minecraft:dimension_type",
                        emptyList(),
                    ).write(it)
            },
        )
    }

    @Test
    fun `a jukebox song has no name over there so the record does not play`() {
        assertFalse(
            sent(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.LevelEventPacket.ID) {
                org.basaltmc.wire.protocol.play
                    .LevelEventPacket(
                        1010,
                        org.basaltmc.wire.protocol.play
                            .BlockPos(1, 2, 3),
                        7,
                    ).write(it)
            },
        )
    }

    @Test
    fun `using an item carries the rotation the client last moved with`() {
        // Written by hand: a client sends this one, so the server only has a reader for it.
        val move =
            ByteBuffer()
                .apply {
                    writeDouble(1.0)
                    writeDouble(2.0)
                    writeDouble(3.0)
                    writeFloat(90f)
                    writeFloat(-20f)
                    writeByte(1)
                }.toByteArray()
        chain.serverbound(ProtocolState.PLAY, MOVE_POS_ROT_766, move) { _, _ -> }

        val use =
            ByteBuffer()
                .apply {
                    writeVarInt(0)
                    writeVarInt(5)
                }.toByteArray()
        var out = ByteBuffer()
        chain.serverbound(ProtocolState.PLAY, USE_ITEM_766, use) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        out.readVarInt()
        out.readVarInt()
        assertEquals(90f, out.readFloat(), "the yaw the server needs was filled in")
        assertEquals(-20f, out.readFloat())
    }

    private companion object {
        const val MOVE_POS_ROT_766 = 27
        const val USE_ITEM_766 = 57
    }
}

/** The step where an item goes back to being a tag. */
class Link765Test {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_20_3))

    private fun slotSentTo1204(item: org.basaltmc.wire.protocol.play.WireItemStack?): ByteBuffer {
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .ContainerSetSlotPacket(0, 1, 5, item)
                        .write(it)
                }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.ContainerSetSlotPacket.ID, body) { _, b ->
            out = ByteBuffer().apply { writeBytes(b) }
        }
        out.readVarInt()
        out.readVarInt()
        out.readShort()
        return out
    }

    @Test
    fun `an empty slot is a stack that is not there`() {
        assertEquals(0, slotSentTo1204(null).readByte().toInt())
    }

    @Test
    fun `a plain stack keeps its item and its count and carries no tag`() {
        val out =
            slotSentTo1204(
                org.basaltmc.wire.protocol.play
                    .WireItemStack(DIAMOND_776, 5),
            )
        assertEquals(1, out.readByte().toInt(), "the stack is there")
        assertEquals(DIAMOND_765, out.readVarInt())
        assertEquals(5, out.readByte().toInt())
        assertEquals(0, out.readByte().toInt(), "no tag is a single zero")
        assertEquals(0, out.readableBytes)
    }

    @Test
    fun `damage travels in the tag it used to travel in`() {
        val out =
            slotSentTo1204(
                org.basaltmc.wire.protocol.play.WireItemStack(
                    DIAMOND_776,
                    1,
                    components =
                        listOf(
                            org.basaltmc.wire.protocol.play.ItemComponent.Number(
                                org.basaltmc.wire.protocol.play.ItemComponent.DAMAGE,
                                42,
                            ),
                        ),
                ),
            )
        out.readByte()
        out.readVarInt()
        out.readByte()
        val tag = out.readAnonymousNbt()
        assertTrue(tag is org.basaltmc.wire.nbt.NbtCompound)
        assertEquals(42, (tag as org.basaltmc.wire.nbt.NbtCompound).getInt("Damage", 0))
    }

    private companion object {
        const val DIAMOND_776 = 926
        const val DIAMOND_765 = 801
    }
}

/** The step where text goes back to json. */
class Link764Test {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_20_2))

    @Test
    fun `a chat message arrives as a string of json`() {
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .SystemChatPacket(
                            org.basaltmc.wire.text.Component
                                .text("hello"),
                            false,
                        ).write(it)
                }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.SystemChatPacket.ID, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        val json = out.readProtocolString()
        assertTrue(json.contains("hello"), "the text did not survive: $json")
        assertEquals(0, out.readByte().toInt(), "the action bar flag follows it")
    }

    @Test
    fun `a boss bar is not sent at all rather than sent unreadable`() {
        val body =
            ByteBuffer()
                .apply {
                    writeLong(0L)
                    writeLong(0L)
                    writeVarInt(0)
                }.toByteArray()
        var arrived = false
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.BossEventPacket.ID, body) { _, _ ->
            arrived = true
        }
        assertFalse(arrived)
    }
}

/** The packet that starts the game, which loses a field at almost every step down. */
class LoginPacketTest {
    private fun loginFor(protocol: Int): ByteBuffer {
        val packet =
            org.basaltmc.wire.protocol.play.LoginPlayPacket(
                entityId = 7,
                isHardcore = false,
                worldNames = listOf("minecraft:overworld"),
                maxPlayers = 20,
                viewDistance = 10,
                simulationDistance = 10,
                reducedDebugInfo = false,
                enableRespawnScreen = true,
                doLimitedCrafting = false,
                spawnInfo =
                    org.basaltmc.wire.protocol.play.SpawnInfo(
                        dimension = 0,
                        dimensionName = "minecraft:overworld",
                        hashedSeed = 1234L,
                        gamemode = org.basaltmc.wire.protocol.play.Gamemode.SURVIVAL,
                        previousGamemode = -1,
                        isDebug = false,
                        isFlat = false,
                        death = null,
                        portalCooldown = 0,
                        seaLevel = 63,
                    ),
                onlineMode = true,
                enforcesSecureChat = false,
            )
        val body = ByteBuffer().also { packet.write(it) }.toByteArray()
        val chain = checkNotNull(JavaTranslators.chainFor(protocol))
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.LoginPlayPacket.ID, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        return out
    }

    @Test
    fun `26_1 is not sent the online mode flag 26_2 added`() {
        val out = loginFor(ProtocolVersions.V26_1)
        val login = LoginFields.read(out, withOnlineMode = false, withSeaLevel = true)
        assertEquals(7, login.entityId)
        assertEquals(63, login.spawn.seaLevel)
        assertEquals(0, out.readableBytes, "something was left over")
    }

    @Test
    fun `1_21 is sent neither that flag nor the sea level`() {
        val out = loginFor(ProtocolVersions.V1_21)
        val login = LoginFields.read(out, withOnlineMode = false, withSeaLevel = false)
        assertEquals(7, login.entityId)
        assertEquals("minecraft:overworld", login.spawn.dimensionName)
        assertEquals(0, out.readableBytes)
    }

    @Test
    fun `1_20_1 is reachable at all`() {
        assertTrue(ProtocolVersions.V1_20 in JavaTranslators.supported, JavaTranslators.supported.toString())
    }

    @Test
    fun `1_20_1 gets the codec inside the packet instead of a configuration phase`() {
        val out = loginFor(ProtocolVersions.V1_20)
        assertTrue(out.readableBytes > 80, "the packet was not rewritten: ${out.readableBytes} bytes")
        assertEquals(7, out.readInt())
        assertEquals(0, out.readByte().toInt(), "hardcore")
        assertEquals(0, out.readByte().toInt(), "game mode")
        out.readByte()
        assertEquals(1, out.readVarInt(), "one world")
        assertEquals("minecraft:overworld", out.readProtocolString())
        val codec = out.readAnonymousNbt()
        assertTrue(codec is org.basaltmc.wire.nbt.NbtCompound)
        assertTrue(
            (codec as org.basaltmc.wire.nbt.NbtCompound).keys.contains("minecraft:chat_type"),
            "a client this old will not start without chat types",
        )
    }
}

/** The short that only 26.1 and above read. */
class FluidCountTest {
    private fun sectionsFor(protocol: Int): ByteBuffer {
        val blocks = IntArray(4096) { 1 }
        val sections =
            ByteBuffer()
                .apply {
                    writeShort(4096)
                    writeShort(7)
                    org.basaltmc.wire.protocol.play.PalettedContainer
                        .write(this, blocks, 4, 8, 15)
                    org.basaltmc.wire.protocol.play.PalettedContainer
                        .write(this, IntArray(64), 0, 3, 6)
                }.toByteArray()
        val packet =
            org.basaltmc.wire.protocol.play.MapChunkPacket(
                x = 0,
                z = 0,
                heightmaps = emptyList(),
                chunkData = sections,
                blockEntities = emptyList(),
                skyLightMask = longArrayOf(),
                blockLightMask = longArrayOf(),
                emptySkyLightMask = longArrayOf(),
                emptyBlockLightMask = longArrayOf(),
                skyLight = emptyList(),
                blockLight = emptyList(),
            )
        val body = ByteBuffer().also { packet.write(it) }.toByteArray()
        val chain = checkNotNull(JavaTranslators.chainFor(protocol))
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.MapChunkPacket.ID, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        return out
    }

    @Test
    fun `26_1 still reads the fluid count`() {
        val out = sectionsFor(ProtocolVersions.V26_1)
        val chunk =
            org.basaltmc.wire.protocol.play.MapChunkPacket
                .read(out)
        val data = ByteBuffer().apply { writeBytes(chunk.chunkData) }
        assertEquals(4096, data.readShort().toInt())
        assertEquals(7, data.readShort().toInt(), "the fluid count is still there")
    }

    @Test
    fun `1_21_11 does not`() {
        val out = sectionsFor(ProtocolVersions.V1_21_11)
        val chunk =
            org.basaltmc.wire.protocol.play.MapChunkPacket
                .read(out)
        val data = ByteBuffer().apply { writeBytes(chunk.chunkData) }
        assertEquals(4096, data.readShort().toInt())
        // The palette starts right here: one value for the whole section, not a count of fluids.
        assertEquals(0, data.readByte().toInt())
        assertEquals(1, data.readVarInt(), "and the value is the stone it was filled with")
    }
}

/** The step where the world stops fitting. */
class Link754Test {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_16_4))

    @Test
    fun `a chunk of twenty four sections reaches 1_16_5 as sixteen`() {
        val sections =
            ByteBuffer()
                .apply {
                    repeat(24) {
                        writeShort(4096)
                        writeShort(0)
                        org.basaltmc.wire.protocol.play.PalettedContainer
                            .write(this, IntArray(4096) { 1 }, 4, 8, 15)
                        org.basaltmc.wire.protocol.play.PalettedContainer
                            .write(this, IntArray(64), 0, 3, 6)
                    }
                }.toByteArray()
        val packet =
            org.basaltmc.wire.protocol.play.MapChunkPacket(
                x = 2,
                z = 3,
                heightmaps =
                    listOf(
                        org.basaltmc.wire.protocol.play.Heightmap(
                            org.basaltmc.wire.protocol.play.HeightmapType.MOTION_BLOCKING,
                            LongArray(37),
                        ),
                    ),
                chunkData = sections,
                blockEntities = emptyList(),
                skyLightMask = longArrayOf(),
                blockLightMask = longArrayOf(),
                emptySkyLightMask = longArrayOf(),
                emptyBlockLightMask = longArrayOf(),
                skyLight = emptyList(),
                blockLight = emptyList(),
            )
        val body = ByteBuffer().also { packet.write(it) }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.MapChunkPacket.ID, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }

        assertEquals(2, out.readInt())
        assertEquals(3, out.readInt())
        assertEquals(1, out.readByte().toInt(), "a full chunk")
        assertEquals(0xFFFF, out.readVarInt(), "all sixteen sections it has room for")
        out.readAnonymousNbt()
        assertEquals(1024, out.readVarInt(), "the biomes of those sixteen and no more")
        repeat(1024) { out.readVarInt() }
        val data = out.readBytes(out.readVarInt())
        assertTrue(data.isNotEmpty())
        assertEquals(0, out.readVarInt(), "no block entities")
        assertEquals(0, out.readableBytes, "the packet ends there")
    }

    @Test
    fun `a block below the floor is not sent at all`() {
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .ClientboundBlockUpdatePacket(
                            org.basaltmc.wire.protocol.play
                                .BlockPos(0, -20, 0),
                            1,
                        ).write(it)
                }.toByteArray()
        var arrived = false
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.ClientboundBlockUpdatePacket.ID,
            body,
        ) { _, _ -> arrived = true }
        assertFalse(arrived)
    }
}

/** The light that comes with a chunk, cut to the band 1.16 lights. */
class Link754LightTest {
    @Test
    fun `the masks become one number each and only the sections 1_16 lights survive`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_16_4))
        // Sections four to nine of the twenty six a 1.17 world lights, which are the first six a
        // 1.16 client has.
        val mask = longArrayOf(0b1111110000L)
        val arrays = List(6) { ByteArray(2048) }
        val packet =
            org.basaltmc.wire.protocol.play.UpdateLightPacket(
                chunkX = 1,
                chunkZ = 2,
                skyLightMask = mask,
                blockLightMask = mask,
                emptySkyLightMask = longArrayOf(),
                emptyBlockLightMask = longArrayOf(),
                skyLight = arrays,
                blockLight = arrays,
            )
        val body = ByteBuffer().also { packet.write(it) }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.UpdateLightPacket.ID, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        assertEquals(1, out.readVarInt())
        assertEquals(2, out.readVarInt())
        assertEquals(1, out.readByte().toInt(), "trust edges, added on the way down")
        assertEquals(0b111111, out.readVarInt(), "six sections, moved to the bottom of the mask")
        assertEquals(0b111111, out.readVarInt())
        assertEquals(0, out.readVarInt())
        assertEquals(0, out.readVarInt())
        repeat(12) {
            assertEquals(2048, out.readVarInt())
            out.readBytes(2048)
        }
        assertEquals(0, out.readableBytes)
    }
}

/** 1.16.1, the last version that is told what its world is rather than shown. */
class Link736Test {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_16_1))

    /** The registries reach a client this old inside the login packet, so they go past first. */
    private fun sendDimensionRegistry() {
        val element =
            org.basaltmc.wire.nbt
                .NbtCompound()
        element.putBoolean("has_skylight", true)
        element.putDouble("coordinate_scale", 1.0)
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.configuration
                        .RegistryDataPacket(
                            "minecraft:dimension_type",
                            listOf(
                                org.basaltmc.wire.protocol.configuration
                                    .RegistryEntry("minecraft:overworld", element),
                            ),
                        ).write(it)
                }.toByteArray()
        chain.clientbound(
            ProtocolState.CONFIGURATION,
            org.basaltmc.wire.protocol.configuration.RegistryDataPacket.ID,
            body,
        ) { _, _ -> }
    }

    private fun login(hardcore: Boolean): ByteBuffer {
        sendDimensionRegistry()
        val packet =
            org.basaltmc.wire.protocol.play.LoginPlayPacket(
                entityId = 7,
                isHardcore = hardcore,
                worldNames = listOf("minecraft:overworld"),
                maxPlayers = 20,
                viewDistance = 10,
                simulationDistance = 10,
                reducedDebugInfo = false,
                enableRespawnScreen = true,
                doLimitedCrafting = false,
                spawnInfo =
                    org.basaltmc.wire.protocol.play.SpawnInfo(
                        dimension = 0,
                        dimensionName = "minecraft:overworld",
                        hashedSeed = 1234L,
                        gamemode = org.basaltmc.wire.protocol.play.Gamemode.SURVIVAL,
                        previousGamemode = -1,
                        isDebug = false,
                        isFlat = false,
                        death = null,
                        portalCooldown = 0,
                        seaLevel = 63,
                    ),
                onlineMode = true,
                enforcesSecureChat = false,
            )
        val body = ByteBuffer().also { packet.write(it) }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, LoginPlayPacket.ID, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        return out
    }

    @Test
    fun `a hardcore world arrives as a bit of the gamemode`() {
        val out = login(hardcore = true)
        assertEquals(7, out.readInt(), "the entity id")
        assertEquals(0x08 or 0, out.readByte().toInt() and 0xFF, "survival, and hardcore with it")
    }

    @Test
    fun `the dimension is named rather than described`() {
        val out = login(hardcore = false)
        out.readInt()
        out.readByte()
        out.readByte()
        repeat(out.readVarInt()) { out.readProtocolString() }
        val registry = out.readAnonymousNbt() as org.basaltmc.wire.nbt.NbtCompound
        val dimensions = checkNotNull(registry.getList("dimension"))
        assertEquals(1, dimensions.size, "the one registry a client this old is given")
        assertEquals("minecraft:overworld", out.readProtocolString())
        assertEquals("minecraft:overworld", out.readProtocolString(), "the world it is in")
        assertEquals(1234L, out.readLong())
        assertEquals(20, out.readByte().toInt() and 0xFF, "the player limit, in one byte")
    }

    @Test
    fun `a line above the hotbar is sent as a title because that is the only way it shows`() {
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .SystemChatPacket(
                            org.basaltmc.wire.text.Component
                                .text("hello"),
                            isActionBar = true,
                        ).write(it)
                }.toByteArray()
        var outId = -1
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.SystemChatPacket.ID, body) { id, bytes ->
            outId = id
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        assertEquals(0x4F, outId, "the title packet of 1.16.1")
        assertEquals(2, out.readVarInt(), "its action bar")
        assertTrue(out.readProtocolString().contains("hello"))
    }
}

/** The five packets 1.17 split a title into, back as the one packet 1.16 had. */
class TitleFoldTest {
    @Test
    fun `a title and its subtitle and its timings become one packet with an action`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_16_4))
        val actions = mutableListOf<Pair<Int, Int>>()
        for (packet in listOf(
            org.basaltmc.wire.protocol.play.SetTitleTextPacket.ID,
            org.basaltmc.wire.protocol.play.SetSubtitleTextPacket.ID,
            org.basaltmc.wire.protocol.play.SetActionBarTextPacket.ID,
        )) {
            val body =
                ByteBuffer()
                    .also {
                        it.writeAnonymousNbt(
                            org.basaltmc.wire.text.ComponentNbt
                                .serialize(
                                    org.basaltmc.wire.text.Component
                                        .text("hi"),
                                ),
                        )
                    }.toByteArray()
            chain.clientbound(ProtocolState.PLAY, packet, body) { id, bytes ->
                actions += id to ByteBuffer().apply { writeBytes(bytes) }.readVarInt()
            }
        }
        assertEquals(listOf(0x4F to 0, 0x4F to 1, 0x4F to 2), actions)

        val times =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .SetTitlesAnimationPacket(1, 2, 3)
                        .write(it)
                }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.SetTitlesAnimationPacket.ID,
            times,
        ) { _, bytes -> out = ByteBuffer().apply { writeBytes(bytes) } }
        assertEquals(3, out.readVarInt(), "the action that sets the timings")
        assertEquals(1, out.readInt())
        assertEquals(2, out.readInt())
        assertEquals(3, out.readInt())
    }
}

/** Attributes, which stop being numbers below 1.21 and stop being a varint count below 1.17. */
class AttributeTest {
    private fun attributes(protocol: Int): ByteBuffer {
        val packet =
            org.basaltmc.wire.protocol.play.UpdateAttributesPacket(
                entityId = 7,
                attributes =
                    listOf(
                        org.basaltmc.wire.protocol.play.UpdateAttributesPacket.AttributeSnapshot(
                            attribute = MAX_HEALTH_776,
                            base = 20.0,
                            modifiers =
                                listOf(
                                    org.basaltmc.wire.protocol.play.UpdateAttributesPacket.Modifier(
                                        org.basaltmc.wire.common.Identifier
                                            .parse("minecraft:effect.health_boost"),
                                        4.0,
                                        org.basaltmc.wire.protocol.play.UpdateAttributesPacket.Operation.ADD_VALUE,
                                    ),
                                ),
                        ),
                    ),
            )
        val body = ByteBuffer().also { packet.write(it) }.toByteArray()
        val chain = checkNotNull(JavaTranslators.chainFor(protocol))
        var out = ByteBuffer()
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.UpdateAttributesPacket.ID,
            body,
        ) { _, bytes -> out = ByteBuffer().apply { writeBytes(bytes) } }
        return out
    }

    @Test
    fun `1_20_6 reads the modifier as a uuid rather than a name`() {
        val out = attributes(ProtocolVersions.V1_20_5)
        assertEquals(7, out.readVarInt())
        assertEquals(1, out.readVarInt())
        out.readVarInt()
        assertEquals(20.0, out.readDouble())
        assertEquals(1, out.readVarInt())
        out.readUuid()
        assertEquals(4.0, out.readDouble())
        assertEquals(0, out.readByte().toInt())
        assertEquals(0, out.readableBytes, "the packet ends there")
    }

    @Test
    fun `1_20_4 reads the attribute itself by name`() {
        val out = attributes(ProtocolVersions.V1_20_3)
        assertEquals(7, out.readVarInt())
        assertEquals(1, out.readVarInt())
        assertEquals("minecraft:generic.max_health", out.readProtocolString())
        assertEquals(20.0, out.readDouble())
    }

    @Test
    fun `1_16_5 counts them in four bytes`() {
        val out = attributes(ProtocolVersions.V1_16_4)
        assertEquals(7, out.readVarInt())
        assertEquals(1, out.readInt(), "the count, as an int")
        assertEquals("minecraft:generic.max_health", out.readProtocolString())
    }

    private companion object {
        /** `minecraft:attribute` in 26.2, where the prefixes are gone from the names. */
        const val MAX_HEALTH_776 = 23
    }
}

/** 1.15.2, the last version before a dimension had a name. */
class Link578Test {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_15_2))

    /** The dimension only has a name because the registry said so, so it goes past first. */
    private fun sendDimensions() {
        val entries =
            listOf("minecraft:overworld", "minecraft:the_nether").map { name ->
                val element =
                    org.basaltmc.wire.nbt
                        .NbtCompound()
                element.putBoolean("has_skylight", name.endsWith("overworld"))
                org.basaltmc.wire.protocol.configuration
                    .RegistryEntry(name, element)
            }
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.configuration
                        .RegistryDataPacket("minecraft:dimension_type", entries)
                        .write(it)
                }.toByteArray()
        chain.clientbound(
            ProtocolState.CONFIGURATION,
            org.basaltmc.wire.protocol.configuration.RegistryDataPacket.ID,
            body,
        ) { _, _ -> }
    }

    @Test
    fun `the nether arrives as minus one and the world generator is named`() {
        sendDimensions()
        val body =
            ByteBuffer()
                .also { output ->
                    output.writeInt(7)
                    output.writeBoolean(false)
                    output.writeVarInt(1)
                    output.writeProtocolString("minecraft:overworld")
                    output.writeVarInt(20)
                    output.writeVarInt(10)
                    output.writeVarInt(10)
                    output.writeBoolean(false)
                    output.writeBoolean(true)
                    output.writeBoolean(false)
                    output.writeVarInt(1)
                    output.writeProtocolString("minecraft:the_nether")
                    output.writeLong(99L)
                    output.writeByte(1)
                    output.writeByte(-1)
                    output.writeBoolean(false)
                    output.writeBoolean(true)
                    output.writeBoolean(false)
                    output.writeVarInt(0)
                    output.writeVarInt(64)
                    output.writeBoolean(false)
                    output.writeBoolean(false)
                }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, LoginPlayPacket.ID, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        assertEquals(7, out.readInt())
        assertEquals(1, out.readByte().toInt(), "creative, and no hardcore bit")
        assertEquals(-1, out.readInt(), "the nether")
        assertEquals(99L, out.readLong())
        assertEquals(20, out.readByte().toInt())
        assertEquals("flat", out.readProtocolString())
        assertEquals(10, out.readVarInt())
        assertEquals(0, out.readByte().toInt())
        assertEquals(1, out.readByte().toInt())
        assertEquals(0, out.readableBytes, "the packet ends there")
    }

    @Test
    fun `every slot of a suit of armour arrives as its own packet`() {
        val packet =
            org.basaltmc.wire.protocol.play.SetEquipmentPacket(
                entityId = 3,
                slots =
                    listOf(
                        org.basaltmc.wire.protocol.play.SetEquipmentPacket.Slot(
                            org.basaltmc.wire.protocol.play.SetEquipmentPacket.Type.MAINHAND,
                            org.basaltmc.wire.protocol.play.WireItemStack.EMPTY,
                        ),
                        org.basaltmc.wire.protocol.play.SetEquipmentPacket.Slot(
                            org.basaltmc.wire.protocol.play.SetEquipmentPacket.Type.HEAD,
                            org.basaltmc.wire.protocol.play.WireItemStack.EMPTY,
                        ),
                    ),
            )
        val body = ByteBuffer().also { packet.write(it) }.toByteArray()
        val sent = mutableListOf<Int>()
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.SetEquipmentPacket.ID,
            body,
        ) { id, _ -> sent += id }
        assertEquals(2, sent.size, "one packet per slot")
        assertEquals(sent[0], sent[1], "both of them the same packet")
    }
}

/** The two ways a chunk has been packed, and the step between them. */
class ChunkPackingTest {
    @Test
    fun `a section of one block reaches 1_17_1 as a palette it can read`() {
        val out = chunkFor(ProtocolVersions.V1_17_1)
        out.readInt()
        out.readInt()
        val mask = LongArray(out.readVarInt()) { out.readLong() }
        assertTrue(mask.isNotEmpty())
        out.readAnonymousNbt()
        repeat(out.readVarInt()) { out.readVarInt() }
        val data = ByteBuffer().apply { writeBytes(out.readBytes(out.readVarInt())) }
        data.readShort()
        assertEquals(4, data.readByte().toInt(), "four bits, not the single value 1.18 invented")
        assertEquals(1, data.readVarInt(), "one entry in the palette")
        assertEquals(1, data.readVarInt(), "which is the block the section is made of")
        assertEquals(256, data.readVarInt(), "and the longs that go with four bits")
    }

    @Test
    fun `1_15_2 is sent the packing that spans longs`() {
        val out = chunkFor(ProtocolVersions.V1_15_2)
        out.readInt()
        out.readInt()
        out.readByte()
        out.readVarInt()
        out.readAnonymousNbt()
        repeat(1024) { out.readInt() }
        val data = ByteBuffer().apply { writeBytes(out.readBytes(out.readVarInt())) }
        data.readShort()
        assertEquals(4, data.readByte().toInt())
        assertEquals(1, data.readVarInt())
        assertEquals(1, data.readVarInt())
        assertEquals(256, data.readVarInt(), "four bits across sixteen values a long, either way")
    }

    /** One section of a single block, which is what a chunk of anything uniform is made of. */
    private fun chunkFor(protocol: Int): ByteBuffer {
        val sections =
            ByteBuffer()
                .apply {
                    repeat(24) {
                        writeShort(4096)
                        writeShort(0)
                        // A container of one value: no long array follows it, and none is counted.
                        writeByte(0)
                        writeVarInt(1)
                        writeByte(0)
                        writeVarInt(1)
                    }
                }.toByteArray()
        val packet =
            org.basaltmc.wire.protocol.play.MapChunkPacket(
                x = 0,
                z = 0,
                heightmaps =
                    listOf(
                        org.basaltmc.wire.protocol.play.Heightmap(
                            org.basaltmc.wire.protocol.play.HeightmapType.MOTION_BLOCKING,
                            LongArray(37),
                        ),
                    ),
                chunkData = sections,
                blockEntities = emptyList(),
                skyLightMask = longArrayOf(),
                blockLightMask = longArrayOf(),
                emptySkyLightMask = longArrayOf(),
                emptyBlockLightMask = longArrayOf(),
                skyLight = emptyList(),
                blockLight = emptyList(),
            )
        val body = ByteBuffer().also { packet.write(it) }.toByteArray()
        val chain = checkNotNull(JavaTranslators.chainFor(protocol))
        var out = ByteBuffer()
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.MapChunkPacket.ID,
            body,
        ) { _, bytes -> out = ByteBuffer().apply { writeBytes(bytes) } }
        return out
    }
}

/** 1.14.4, where a biome is a column and lives inside the chunk data. */
class Link498Test {
    @Test
    fun `the biomes end up after the sections and there are two hundred and fifty six`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_14_4))
        val sections =
            ByteBuffer()
                .apply {
                    repeat(24) {
                        writeShort(4096)
                        writeShort(0)
                        writeByte(0)
                        writeVarInt(1)
                        writeByte(0)
                        writeVarInt(1)
                    }
                }.toByteArray()
        val packet =
            org.basaltmc.wire.protocol.play.MapChunkPacket(
                x = 1,
                z = 2,
                heightmaps =
                    listOf(
                        org.basaltmc.wire.protocol.play.Heightmap(
                            org.basaltmc.wire.protocol.play.HeightmapType.MOTION_BLOCKING,
                            LongArray(37),
                        ),
                    ),
                chunkData = sections,
                blockEntities = emptyList(),
                skyLightMask = longArrayOf(),
                blockLightMask = longArrayOf(),
                emptySkyLightMask = longArrayOf(),
                emptyBlockLightMask = longArrayOf(),
                skyLight = emptyList(),
                blockLight = emptyList(),
            )
        val body = ByteBuffer().also { packet.write(it) }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.MapChunkPacket.ID,
            body,
        ) { _, bytes -> out = ByteBuffer().apply { writeBytes(bytes) } }

        assertEquals(1, out.readInt())
        assertEquals(2, out.readInt())
        assertEquals(1, out.readByte().toInt())
        assertEquals(0xFFFF, out.readVarInt(), "the sixteen sections it has room for")
        out.readAnonymousNbt()
        val data = ByteBuffer().apply { writeBytes(out.readBytes(out.readVarInt())) }
        assertEquals(0, out.readVarInt(), "no block entities")
        assertEquals(0, out.readableBytes)
        // The biomes are the last thousand and twenty four bytes of the data, and nothing else is.
        val sectionBytes = data.readableBytes - 256 * 4
        data.readBytes(sectionBytes)
        repeat(256) { data.readInt() }
        assertEquals(0, data.readableBytes, "the data ends with the biomes")
    }
}

/** 1.13.2, where the light is part of the chunk and the chunk waits for it. */
class Link404Test {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_13_2))

    @Test
    fun `the chunk is held back until its light arrives and then goes as one packet`() {
        val sections =
            ByteBuffer()
                .apply {
                    repeat(24) {
                        writeShort(4096)
                        writeShort(0)
                        writeByte(0)
                        writeVarInt(1)
                        writeByte(0)
                        writeVarInt(1)
                    }
                }.toByteArray()
        val chunk =
            org.basaltmc.wire.protocol.play.MapChunkPacket(
                x = 0,
                z = 0,
                heightmaps =
                    listOf(
                        org.basaltmc.wire.protocol.play.Heightmap(
                            org.basaltmc.wire.protocol.play.HeightmapType.MOTION_BLOCKING,
                            LongArray(37),
                        ),
                    ),
                chunkData = sections,
                blockEntities = emptyList(),
                skyLightMask = longArrayOf(),
                blockLightMask = longArrayOf(),
                emptySkyLightMask = longArrayOf(),
                emptyBlockLightMask = longArrayOf(),
                skyLight = emptyList(),
                blockLight = emptyList(),
            )
        var arrived = 0
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.MapChunkPacket.ID,
            ByteBuffer().also { chunk.write(it) }.toByteArray(),
        ) { _, _ -> arrived++ }
        assertEquals(0, arrived, "nothing goes out until the light does")

        val light =
            org.basaltmc.wire.protocol.play.UpdateLightPacket(
                chunkX = 0,
                chunkZ = 0,
                skyLightMask = longArrayOf(-1L),
                blockLightMask = longArrayOf(-1L),
                emptySkyLightMask = longArrayOf(),
                emptyBlockLightMask = longArrayOf(),
                skyLight = List(26) { ByteArray(2048) },
                blockLight = List(26) { ByteArray(2048) },
            )
        var outId = -1
        var out = ByteBuffer()
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.UpdateLightPacket.ID,
            ByteBuffer().also { light.write(it) }.toByteArray(),
        ) { id, bytes ->
            outId = id
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        assertEquals(0x22, outId, "the chunk packet of 1.13.2")
        assertEquals(0, out.readInt())
        assertEquals(0, out.readInt())
        assertEquals(1, out.readByte().toInt())
        assertEquals(0xFFFF, out.readVarInt())
        val data = ByteBuffer().apply { writeBytes(out.readBytes(out.readVarInt())) }
        val whole = data.readableBytes
        data.readByte()
        assertEquals(1, data.readVarInt(), "one entry in the palette")
        data.readVarInt()
        val longs = data.readVarInt()
        repeat(longs) { data.readLong() }
        val section = whole - data.readableBytes
        // Sixteen sections of blocks and their two lights, and the biomes of the column after them.
        assertEquals(16 * (section + 2048 + 2048) + 256 * Int.SIZE_BYTES, whole)
    }
}

/** The container a packet is about, which stopped fitting in a byte in 1.21.2. */
class ContainerIdTest {
    @Test
    fun `the cursor slot reaches 1_21_1 as the one byte it reads`() {
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .ContainerSetSlotPacket(containerId = -1, stateId = 0, slot = -1, item = null)
                        .write(it)
                }.toByteArray()
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21))
        var out = ByteBuffer()
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.ContainerSetSlotPacket.ID,
            body,
        ) { _, bytes -> out = ByteBuffer().apply { writeBytes(bytes) } }
        assertEquals(-1, out.readByte().toInt(), "the player's own screen, in one byte")
        assertEquals(0, out.readVarInt())
        assertEquals(-1, out.readShort().toInt())
    }

    @Test
    fun `and 1_20_4 still reads it as one byte with the old stack behind it`() {
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .ContainerSetSlotPacket(containerId = -1, stateId = 3, slot = 5, item = null)
                        .write(it)
                }.toByteArray()
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_20_3))
        var out = ByteBuffer()
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.ContainerSetSlotPacket.ID,
            body,
        ) { _, bytes -> out = ByteBuffer().apply { writeBytes(bytes) } }
        assertEquals(-1, out.readByte().toInt())
        assertEquals(3, out.readVarInt())
        assertEquals(5, out.readShort().toInt())
        assertEquals(0, out.readByte().toInt(), "an empty slot, which is a flag and nothing else")
        assertEquals(0, out.readableBytes)
    }
}

/** 1.12.2, the version below the flattening. */
class Link340Test {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_12_2))

    @Test
    fun `a block becomes an id and four bits of metadata`() {
        val body =
            ByteBuffer()
                .also {
                    ClientboundBlockUpdatePacket(
                        org.basaltmc.wire.protocol.play
                            .BlockPos(0, 64, 0),
                        GRANITE_776,
                    ).write(it)
                }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, ClientboundBlockUpdatePacket.ID, body) { _, bytes ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        val update = ClientboundBlockUpdatePacket.read(out)
        assertEquals(STONE_ID_1_12 shl 4 or GRANITE_META, update.blockStateId, "stone, with granite's metadata")
    }

    @Test
    fun `a stack carries its variant as a damage value`() {
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .ContainerSetSlotPacket(
                            containerId = 0,
                            stateId = 0,
                            slot = 9,
                            item =
                                org.basaltmc.wire.protocol.play
                                    .WireItemStack(GRANITE_ITEM_776, 3),
                        ).write(it)
                }.toByteArray()
        var out = ByteBuffer()
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.ContainerSetSlotPacket.ID, body) {
            _,
            bytes,
            ->
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        assertEquals(0, out.readByte().toInt())
        assertEquals(9, out.readShort().toInt())
        assertEquals(STONE_ID_1_12, out.readShort().toInt(), "the item is stone")
        assertEquals(3, out.readByte().toInt())
        assertEquals(GRANITE_META, out.readShort().toInt(), "and granite is its damage")
    }

    private companion object {
        /** `minecraft:granite` in 26.2, which 1.12 says as stone with a metadata of one. */
        const val GRANITE_776 = 2
        const val GRANITE_ITEM_776 = 2
        const val STONE_ID_1_12 = 1
        const val GRANITE_META = 1
    }
}

/** 1.8, where a chunk is two bytes per block and the light comes behind all of them. */
class Link47Test {
    private val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_8))

    @Test
    fun `a chunk arrives flat and its light and biomes come behind it`() {
        val sections =
            ByteBuffer()
                .apply {
                    repeat(24) {
                        writeShort(4096)
                        writeShort(0)
                        writeByte(0)
                        writeVarInt(1)
                        writeByte(0)
                        writeVarInt(1)
                    }
                }.toByteArray()
        val packet =
            org.basaltmc.wire.protocol.play.MapChunkPacket(
                x = 4,
                z = 5,
                heightmaps =
                    listOf(
                        org.basaltmc.wire.protocol.play.Heightmap(
                            org.basaltmc.wire.protocol.play.HeightmapType.MOTION_BLOCKING,
                            LongArray(37),
                        ),
                    ),
                chunkData = sections,
                blockEntities = emptyList(),
                skyLightMask = longArrayOf(),
                blockLightMask = longArrayOf(),
                emptySkyLightMask = longArrayOf(),
                emptyBlockLightMask = longArrayOf(),
                skyLight = emptyList(),
                blockLight = emptyList(),
            )
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.MapChunkPacket.ID,
            ByteBuffer().also { packet.write(it) }.toByteArray(),
        ) { _, _ -> }
        val light =
            org.basaltmc.wire.protocol.play.UpdateLightPacket(
                chunkX = 4,
                chunkZ = 5,
                skyLightMask = longArrayOf(-1L),
                blockLightMask = longArrayOf(-1L),
                emptySkyLightMask = longArrayOf(),
                emptyBlockLightMask = longArrayOf(),
                skyLight = List(26) { ByteArray(2048) },
                blockLight = List(26) { ByteArray(2048) },
            )
        var out = ByteBuffer()
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.UpdateLightPacket.ID,
            ByteBuffer().also { light.write(it) }.toByteArray(),
        ) { _, bytes -> out = ByteBuffer().apply { writeBytes(bytes) } }

        assertEquals(4, out.readInt())
        assertEquals(5, out.readInt())
        assertEquals(1, out.readByte().toInt(), "a full chunk")
        assertEquals(0xFFFF, out.readShort().toInt() and 0xFFFF, "the sixteen sections, in a short")
        val data = out.readVarInt()
        // Sixteen sections of 4096 blocks at two bytes, their block light and their sky light,
        // and the two hundred and fifty six biomes of the column at the end.
        assertEquals(16 * (4096 * 2 + 2048 + 2048) + 256, data)
        assertEquals(data, out.readableBytes)
    }

    @Test
    fun `a player spawns at a position measured in thirty seconds of a block`() {
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .SpawnEntityPacket(
                            entityId = 9,
                            uuid = kotlin.uuid.Uuid.fromLongs(1L, 2L),
                            entityType = PLAYER_776,
                            x = 8.5,
                            y = 64.0,
                            z = -3.25,
                            pitch = 0f,
                            yaw = 0f,
                            headYaw = 0f,
                            objectData = 0,
                        ).write(it)
                }.toByteArray()
        var out = ByteBuffer()
        var outId = -1
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.SpawnEntityPacket.ID, body) { id, bytes ->
            outId = id
            out = ByteBuffer().apply { writeBytes(bytes) }
        }
        assertEquals(0x0C, outId, "the packet 1.8 spawns a player with")
        assertEquals(
            34,
            out.readableBytes,
            "the body 1.8 reads: an id, a uuid, three ints, two angles, an item and the end",
        )
        assertEquals(9, out.readVarInt())
        out.readLong()
        out.readLong()
        assertEquals(272, out.readInt(), "eight and a half blocks, in thirty-seconds")
        assertEquals(2048, out.readInt())
        assertEquals(-104, out.readInt())
    }

    private companion object {
        /** `minecraft:player` in 26.2, which the links above turn into the spawn 1.8 reads. */
        const val PLAYER_776 = 156
    }
}

/** A player spawning, at every depth: it is the one entity with no number of its own down there. */
class PlayerSpawnTest {
    @Test
    fun `a player reaches every version this server translates`() {
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .SpawnEntityPacket(
                            entityId = 9,
                            uuid = kotlin.uuid.Uuid.fromLongs(1L, 2L),
                            entityType = PLAYER_776,
                            x = 8.5,
                            y = 64.0,
                            z = -3.25,
                        ).write(it)
                }.toByteArray()
        val missing = mutableListOf<String>()
        for (protocol in JavaTranslators.supported.sorted()) {
            if (protocol == ProtocolVersions.NATIVE) continue
            val chain = checkNotNull(JavaTranslators.chainFor(protocol))
            var arrived = false
            chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.SpawnEntityPacket.ID, body) { _, _ ->
                arrived = true
            }
            if (!arrived) missing += protocol.toString()
        }
        assertEquals(emptyList(), missing, "versions a player never spawns for")
    }

    @Test
    fun `everything a joining player sends out reaches every version`() {
        val packets =
            listOf(
                // A mob and a thrown thing, which are the two other shapes a spawn has.
                org.basaltmc.wire.protocol.play.SpawnEntityPacket.ID to
                    ByteBuffer()
                        .also {
                            org.basaltmc.wire.protocol.play
                                .SpawnEntityPacket(
                                    entityId = 10,
                                    uuid = kotlin.uuid.Uuid.fromLongs(3L, 4L),
                                    entityType = ZOMBIE_776,
                                    x = 1.0,
                                    y = 2.0,
                                    z = 3.0,
                                ).write(it)
                        }.toByteArray(),
                org.basaltmc.wire.protocol.play.SpawnEntityPacket.ID to
                    ByteBuffer()
                        .also {
                            org.basaltmc.wire.protocol.play
                                .SpawnEntityPacket(
                                    entityId = 11,
                                    uuid = kotlin.uuid.Uuid.fromLongs(5L, 6L),
                                    entityType = ARROW_776,
                                    x = 1.0,
                                    y = 2.0,
                                    z = 3.0,
                                    objectData = 7,
                                ).write(it)
                        }.toByteArray(),
                org.basaltmc.wire.protocol.play.SetEquipmentPacket.ID to
                    ByteBuffer()
                        .also {
                            org.basaltmc.wire.protocol.play
                                .SetEquipmentPacket(
                                    9,
                                    org.basaltmc.wire.protocol.play.SetEquipmentPacket.Type.entries.map { type ->
                                        org.basaltmc.wire.protocol.play.SetEquipmentPacket
                                            .Slot(type, org.basaltmc.wire.protocol.play.WireItemStack.EMPTY)
                                    },
                                ).write(it)
                        }.toByteArray(),
                org.basaltmc.wire.protocol.play.EntityPositionSyncPacket.ID to
                    ByteBuffer()
                        .also {
                            org.basaltmc.wire.protocol.play
                                .EntityPositionSyncPacket(9, 1.0, 2.0, 3.0, 0.0, 0.0, 0.0, 0f, 0f, false)
                                .write(it)
                        }.toByteArray(),
                org.basaltmc.wire.protocol.play.RotateHeadPacket.ID to
                    ByteBuffer()
                        .also {
                            org.basaltmc.wire.protocol.play
                                .RotateHeadPacket(9, 0f)
                                .write(it)
                        }.toByteArray(),
                org.basaltmc.wire.protocol.play.SetEntityDataPacket.ID to
                    ByteBuffer()
                        .also {
                            org.basaltmc.wire.protocol.play
                                .SetEntityDataPacket(
                                    9,
                                    listOf(
                                        org.basaltmc.wire.protocol.play.SetEntityDataPacket.Entry
                                            .Bytes(0, 0),
                                    ),
                                ).write(it)
                        }.toByteArray(),
            )
        for (protocol in JavaTranslators.supported.sorted()) {
            if (protocol == ProtocolVersions.NATIVE) continue
            val chain = checkNotNull(JavaTranslators.chainFor(protocol))
            for ((id, body) in packets) {
                try {
                    chain.clientbound(ProtocolState.PLAY, id, body) { _, _ -> }
                } catch (e: Exception) {
                    throw IllegalStateException("packet 0x${id.toString(16)} at $protocol: $e")
                }
            }
        }
    }

    private companion object {
        const val PLAYER_776 = 156
        const val ZOMBIE_776 = 151
        const val ARROW_776 = 6
    }
}

/** The registries, which are content and not numbers: what a version cannot read must not go. */
class RegistryDataTest {
    @Test
    fun `a biome and a song 26_2 invented do not reach 26_1`() {
        val out = registriesFor(ProtocolVersions.V26_1)
        assertEquals(listOf("minecraft:plains"), out["minecraft:worldgen/biome"], "the new biome is gone")
        assertEquals(listOf("minecraft:wait"), out["minecraft:jukebox_song"], "and so is the new song")
    }

    @Test
    fun `1_21_11 is not told about a registry it does not have`() {
        val out = registriesFor(ProtocolVersions.V1_21_11)
        assertEquals(null, out["minecraft:world_clock"], "a registry that version never had")
        assertEquals(listOf("minecraft:plains"), out["minecraft:worldgen/biome"])
    }

    @Test
    fun `an attribute key 26_x invented is taken out of the element that carries it`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_11))
        val element =
            org.basaltmc.wire.nbt
                .NbtCompound()
        val attributes =
            org.basaltmc.wire.nbt
                .NbtCompound()
        attributes.putString("minecraft:visual/fog_color", "#c0d8ff")
        attributes.putString("minecraft:visual/ambient_light_color", "#0a0a0a")
        element["attributes"] = attributes
        var kept: org.basaltmc.wire.nbt.NbtCompound? = null
        chain.clientbound(
            ProtocolState.CONFIGURATION,
            org.basaltmc.wire.protocol.configuration.RegistryDataPacket.ID,
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.configuration
                        .RegistryDataPacket(
                            "minecraft:dimension_type",
                            listOf(
                                org.basaltmc.wire.protocol.configuration
                                    .RegistryEntry("minecraft:overworld", element),
                            ),
                        ).write(it)
                }.toByteArray(),
        ) { _, bytes ->
            val data =
                org.basaltmc.wire.protocol.configuration.RegistryDataPacket
                    .read(ByteBuffer().apply { writeBytes(bytes) })
            kept =
                (data.entries.single().value as org.basaltmc.wire.nbt.NbtCompound)["attributes"]
                    as org.basaltmc.wire.nbt.NbtCompound
        }
        val keys = checkNotNull(kept).keys
        assertTrue("minecraft:visual/fog_color" in keys, "what it does know stays")
        assertFalse("minecraft:visual/ambient_light_color" in keys, "what it does not know goes")
    }

    @Test
    fun `a wolf's sounds arrive flat where a puppy has none of its own`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_11))
        val adult =
            org.basaltmc.wire.nbt
                .NbtCompound()
        adult.putString("ambient_sound", "minecraft:entity.wolf.ambient")
        adult.putString("step_sound", "minecraft:entity.wolf.step")
        val element =
            org.basaltmc.wire.nbt
                .NbtCompound()
        element["adult_sounds"] = adult
        element["baby_sounds"] = adult
        var kept: org.basaltmc.wire.nbt.NbtCompound? = null
        chain.clientbound(
            ProtocolState.CONFIGURATION,
            org.basaltmc.wire.protocol.configuration.RegistryDataPacket.ID,
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.configuration
                        .RegistryDataPacket(
                            "minecraft:wolf_sound_variant",
                            listOf(
                                org.basaltmc.wire.protocol.configuration
                                    .RegistryEntry("minecraft:classic", element),
                            ),
                        ).write(it)
                }.toByteArray(),
        ) { _, bytes ->
            val data =
                org.basaltmc.wire.protocol.configuration.RegistryDataPacket
                    .read(ByteBuffer().apply { writeBytes(bytes) })
            kept = data.entries.single().value as org.basaltmc.wire.nbt.NbtCompound
        }
        val keys = checkNotNull(kept).keys
        assertTrue("ambient_sound" in keys, "the adult's sounds are the element itself")
        assertFalse("adult_sounds" in keys)
        assertFalse("step_sound" in keys, "which that version has no field for")
    }

    @Test
    fun `a biome reaches 1_21_10 with the colours in the block of effects it reads`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_9))
        val attributes =
            org.basaltmc.wire.nbt
                .NbtCompound()
        attributes.putString("minecraft:visual/sky_color", "#78a7ff")
        val effects =
            org.basaltmc.wire.nbt
                .NbtCompound()
        effects.putString("water_color", "#3f76e4")
        val biome =
            org.basaltmc.wire.nbt
                .NbtCompound()
        biome["attributes"] = attributes
        biome["effects"] = effects
        biome.putFloat("temperature", 0.8f)
        biome.putBoolean("has_precipitation", true)
        var out: org.basaltmc.wire.nbt.NbtCompound? = null
        chain.clientbound(
            ProtocolState.CONFIGURATION,
            org.basaltmc.wire.protocol.configuration.RegistryDataPacket.ID,
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.configuration
                        .RegistryDataPacket(
                            "minecraft:worldgen/biome",
                            listOf(
                                org.basaltmc.wire.protocol.configuration
                                    .RegistryEntry("minecraft:plains", biome),
                            ),
                        ).write(it)
                }.toByteArray(),
        ) { _, bytes ->
            val data =
                org.basaltmc.wire.protocol.configuration.RegistryDataPacket
                    .read(ByteBuffer().apply { writeBytes(bytes) })
            out = data.entries.single().value as org.basaltmc.wire.nbt.NbtCompound
        }
        val element = checkNotNull(out)
        assertFalse("attributes" in element.keys, "the attributes are 26.x's and go")
        val kept = element["effects"] as org.basaltmc.wire.nbt.NbtCompound
        assertEquals(0x78a7ff, kept.getInt("sky_color"), "the colour as the number it reads")
        assertEquals(0x3f76e4, kept.getInt("water_color"))
        assertEquals(12638463, kept.getInt("fog_color"), "and the ones it needs and was not sent")
        assertEquals(329011, kept.getInt("water_fog_color"))
    }

    @Test
    fun `a dimension reaches 1_21_10 as its own definition of it`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_9))
        val element =
            org.basaltmc.wire.nbt
                .NbtCompound()
        element["attributes"] =
            org.basaltmc.wire.nbt
                .NbtCompound()
        var out: org.basaltmc.wire.nbt.NbtCompound? = null
        chain.clientbound(
            ProtocolState.CONFIGURATION,
            org.basaltmc.wire.protocol.configuration.RegistryDataPacket.ID,
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.configuration
                        .RegistryDataPacket(
                            "minecraft:dimension_type",
                            listOf(
                                org.basaltmc.wire.protocol.configuration
                                    .RegistryEntry("minecraft:overworld", element),
                            ),
                        ).write(it)
                }.toByteArray(),
        ) { _, bytes ->
            val data =
                org.basaltmc.wire.protocol.configuration.RegistryDataPacket
                    .read(ByteBuffer().apply { writeBytes(bytes) })
            out = data.entries.single().value as org.basaltmc.wire.nbt.NbtCompound
        }
        val kept = checkNotNull(out)
        assertTrue("natural" in kept.keys, "a field that version needs and 26.x does not send")
        assertTrue("effects" in kept.keys)
        assertFalse("attributes" in kept.keys)
        assertEquals(384, kept.getInt("height"))
    }

    /** The registries this server sends, as they come out the far end, by name. */
    private fun registriesFor(protocol: Int): Map<String, List<String>> {
        val chain = checkNotNull(JavaTranslators.chainFor(protocol))
        val sent =
            listOf(
                "minecraft:worldgen/biome" to listOf("minecraft:plains", "minecraft:sulfur_caves"),
                "minecraft:jukebox_song" to listOf("minecraft:wait", "minecraft:bounce"),
                "minecraft:world_clock" to listOf("minecraft:overworld"),
            )
        val out = mutableMapOf<String, List<String>>()
        for ((registry, names) in sent) {
            val body =
                ByteBuffer()
                    .also {
                        org.basaltmc.wire.protocol.configuration
                            .RegistryDataPacket(
                                registry,
                                names.map { name ->
                                    org.basaltmc.wire.protocol.configuration
                                        .RegistryEntry(
                                            name,
                                            org.basaltmc.wire.nbt
                                                .NbtCompound(),
                                        )
                                },
                            ).write(it)
                    }.toByteArray()
            chain.clientbound(
                ProtocolState.CONFIGURATION,
                org.basaltmc.wire.protocol.configuration.RegistryDataPacket.ID,
                body,
            ) { _, bytes ->
                val data =
                    org.basaltmc.wire.protocol.configuration.RegistryDataPacket
                        .read(ByteBuffer().apply { writeBytes(bytes) })
                out[data.registryId] = data.entries.map { it.key }
            }
        }
        return out
    }
}

/** The stonecutter's list, whose ids are two registries deep and both of them moved. */
class UpdateRecipesTest {
    @Test
    fun `an item and the kind of slot are both renumbered for 1_21_11`() {
        val out = recipesFor(ProtocolVersions.V1_21_11)
        assertEquals(ITEM_STACK_IN_1_21_11, out.first, "the slot display 1.21.11 calls a stack")
        assertEquals(listOf(PLANKS_774), out.second, "and the item as that version numbers it")
    }

    @Test
    fun `an item 26_2 added is shown as something 26_1 has and never as a number past its list`() {
        val out = recipesFor(ProtocolVersions.V26_1)
        val shown = out.second.single()
        assertTrue(shown < ITEMS_IN_26_1, "an id past the end is what took the client down: $shown")
        assertEquals(PLANKS_775, shown)
    }

    @Test
    fun `a tag says which blocks it means in the numbering of whoever reads it`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V26_1))
        val packet =
            org.basaltmc.wire.protocol.configuration.TagsPacket(
                listOf(
                    org.basaltmc.wire.protocol.configuration.TagGroup(
                        "minecraft:item",
                        listOf(
                            org.basaltmc.wire.protocol.configuration
                                .TagEntry("minecraft:planks", listOf(PLANKS_776)),
                        ),
                    ),
                ),
            )
        var ids = emptyList<Int>()
        chain.clientbound(
            ProtocolState.CONFIGURATION,
            org.basaltmc.wire.protocol.configuration.TagsPacket.ID,
            ByteBuffer().also { packet.write(it) }.toByteArray(),
        ) { _, bytes ->
            ids =
                org.basaltmc.wire.protocol.configuration.TagsPacket
                    .read(ByteBuffer().apply { writeBytes(bytes) })
                    .groups
                    .single()
                    .tags
                    .single()
                    .ids
        }
        assertEquals(listOf(PLANKS_775), ids)
    }

    /** The slot display kind and the result ids, as the far end reads them. */
    private fun recipesFor(protocol: Int): Pair<Int, List<Int>> {
        val chain = checkNotNull(JavaTranslators.chainFor(protocol))
        val packet =
            org.basaltmc.wire.protocol.play.UpdateRecipesPacket(
                listOf(
                    org.basaltmc.wire.protocol.play
                        .StonecutterOption(listOf(PLANKS_776), PLANKS_776, 1),
                ),
            )
        var kind = -1
        var results = emptyList<Int>()
        chain.clientbound(
            ProtocolState.PLAY,
            org.basaltmc.wire.protocol.play.UpdateRecipesPacket.ID,
            ByteBuffer().also { packet.write(it) }.toByteArray(),
        ) { _, bytes ->
            val body = ByteBuffer().apply { writeBytes(bytes) }
            body.readVarInt()
            val count = body.readVarInt()
            results =
                List(count) {
                    repeat(body.readVarInt() - 1) { body.readVarInt() }
                    kind = body.readVarInt()
                    val item = body.readVarInt()
                    body.readVarInt()
                    body.readVarInt()
                    body.readVarInt()
                    item
                }
        }
        return kind to results
    }

    private companion object {
        /** `minecraft:oak_planks`, which 26.2 numbers past where 1.21.11 does. */
        const val PLANKS_776 = 63
        const val PLANKS_774 = 36

        /** parity: `SlotDisplays`, where 26.1 put three new kinds in front of the stack. */
        const val ITEM_STACK_IN_1_21_11 = 3

        /** `minecraft:oak_planks` in 26.1, and how many items that version has at all. */
        const val PLANKS_775 = 36
        const val ITEMS_IN_26_1 = 1506
    }
}

/** The command tree, which names registries the client has to look up while it reads it. */
class CommandsTest {
    @Test
    fun `an argument naming a registry 1_21_11 does not have becomes a plain word`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V1_21_11))
        val parser = read(chain)
        assertEquals(
            org.basaltmc.wire.protocol.play.ArgumentParser
                .BrigadierString(org.basaltmc.wire.protocol.play.ArgumentParser.BrigadierString.Kind.SINGLE_WORD),
            parser,
            "a registry that version never had cannot be named in a command",
        )
    }

    @Test
    fun `and 26_1 has the registry so its command is left alone`() {
        val chain = checkNotNull(JavaTranslators.chainFor(ProtocolVersions.V26_1))
        assertEquals(
            org.basaltmc.wire.protocol.play.ArgumentParser
                .Resource("minecraft:world_clock"),
            read(chain),
        )
    }

    private fun read(chain: org.basaltmc.wire.multiversion.ProtocolChain): Any? {
        val nodes =
            listOf(
                org.basaltmc.wire.protocol.play.CommandNodeData(
                    kind = org.basaltmc.wire.protocol.play.CommandNodeData.Kind.ROOT,
                    executable = false,
                    children = intArrayOf(1),
                ),
                org.basaltmc.wire.protocol.play.CommandNodeData(
                    kind = org.basaltmc.wire.protocol.play.CommandNodeData.Kind.ARGUMENT,
                    executable = true,
                    children = intArrayOf(),
                    name = "clock",
                    parser =
                        org.basaltmc.wire.protocol.play.ArgumentParser
                            .Resource("minecraft:world_clock"),
                ),
            )
        val body =
            ByteBuffer()
                .also {
                    org.basaltmc.wire.protocol.play
                        .CommandsPacket(nodes, 0)
                        .write(it)
                }.toByteArray()
        var out: Any? = null
        chain.clientbound(ProtocolState.PLAY, org.basaltmc.wire.protocol.play.CommandsPacket.ID, body) { _, bytes ->
            out =
                org.basaltmc.wire.protocol.play.CommandsPacket
                    .read(ByteBuffer().apply { writeBytes(bytes) })
                    .nodes[1]
                    .parser
        }
        return out
    }
}
