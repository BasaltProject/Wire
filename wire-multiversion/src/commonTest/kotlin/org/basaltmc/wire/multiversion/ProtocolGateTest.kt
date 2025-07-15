package org.basaltmc.wire.multiversion

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue

class ProtocolGateTest {
    private fun gate(
        supported: Set<Int> = setOf(776),
        future: UnknownProtocolPolicy = UnknownProtocolPolicy.WARN,
        snapshots: UnknownProtocolPolicy = UnknownProtocolPolicy.REFUSE,
    ) = ProtocolGate(supported, future, snapshots)

    @Test
    fun `the native protocol needs no translation`() {
        assertEquals(ProtocolVerdict.Native, gate().verdictFor(776))
    }

    @Test
    fun `a version with a link is translated`() {
        val verdict = gate(supported = setOf(776, 775)).verdictFor(775)
        assertEquals(ProtocolVerdict.Translated(ProtocolVersion(775, "26.1-26.1.2")), verdict)
    }

    @Test
    fun `a known version without a link is refused as not translated`() {
        val verdict = gate().verdictFor(775)
        assertEquals(ProtocolVerdict.Refused(775, ProtocolVerdict.Refused.Reason.NOT_TRANSLATED), verdict)
    }

    @Test
    fun `1_7 and older are below the floor`() {
        assertEquals(ProtocolVerdict.Refused(5, ProtocolVerdict.Refused.Reason.TOO_OLD), gate().verdictFor(5))
    }

    @Test
    fun `the future policy decides what a newer client gets`() {
        assertEquals(ProtocolVerdict.Unknown(999, warn = true), gate().verdictFor(999))
        assertEquals(
            ProtocolVerdict.Unknown(999, warn = false),
            gate(future = UnknownProtocolPolicy.ACCEPT).verdictFor(999),
        )
        assertEquals(
            ProtocolVerdict.Refused(999, ProtocolVerdict.Refused.Reason.TOO_NEW),
            gate(future = UnknownProtocolPolicy.REFUSE).verdictFor(999),
        )
    }

    @Test
    fun `a snapshot is judged by its own policy and not by its number`() {
        val snapshot = ProtocolVersions.SNAPSHOT_BIT or 329
        assertTrue(ProtocolVersions.isSnapshot(snapshot))
        // Its number is enormous, but it is not "the future": it is a development build.
        assertEquals(
            ProtocolVerdict.Refused(snapshot, ProtocolVerdict.Refused.Reason.SNAPSHOT),
            gate().verdictFor(snapshot),
        )
        assertIs<ProtocolVerdict.Unknown>(gate(snapshots = UnknownProtocolPolicy.ACCEPT).verdictFor(snapshot))
    }

    @Test
    fun `a snapshot of an older version is still a snapshot`() {
        // The counter under the bit has nothing to do with the release numbers, so a small one is
        // not "older than 1.8".
        val early = ProtocolVersions.SNAPSHOT_BIT or 1
        assertEquals(
            ProtocolVerdict.Refused(early, ProtocolVerdict.Refused.Reason.SNAPSHOT),
            gate().verdictFor(early),
        )
    }

    @Test
    fun `the table covers 1_8 to the native version without holes`() {
        val numbers = ProtocolVersions.known.map { it.number }
        assertEquals(47, numbers.first())
        assertEquals(ProtocolVersions.NATIVE, numbers.last())
        assertEquals(numbers.sorted(), numbers, "the table has to be oldest first")
        assertEquals(numbers.toSet().size, numbers.size, "a protocol number appears twice")
        assertEquals(49, numbers.size)
    }

    @Test
    fun `describe names what it can and admits what it cannot`() {
        assertEquals("26.2 (776)", ProtocolVersions.describe(776))
        assertEquals("1.8-1.8.9 (47)", ProtocolVersions.describe(47))
        assertEquals("unknown (999)", ProtocolVersions.describe(999))
        assertEquals("snapshot 329 (1073742153)", ProtocolVersions.describe(ProtocolVersions.SNAPSHOT_BIT or 329))
    }
}
