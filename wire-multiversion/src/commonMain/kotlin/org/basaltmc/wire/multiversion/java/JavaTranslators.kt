package org.basaltmc.wire.multiversion.java

import org.basaltmc.wire.multiversion.ComposedTables
import org.basaltmc.wire.multiversion.ProtocolChain
import org.basaltmc.wire.multiversion.ProtocolLink
import org.basaltmc.wire.multiversion.ProtocolVersions

/**
 * The links this build has, and the chain one client needs.
 *
 * A version is only reachable when every link between it and the native one exists: the chain is
 * built by walking down from 26.2 and stopping at the first gap, so half a road is no road and the
 * gate refuses it rather than letting somebody in to a broken game.
 */
public object JavaTranslators {
    /** Newest first. Each entry is what translates *into* that version from the one above it. */
    private val factories: List<Pair<Int, (ComposedTables) -> ProtocolLink>> =
        listOf(
            ProtocolVersions.V26_1 to { _ -> Link775(TABLES_775) },
            ProtocolVersions.V1_21_11 to { _ -> Link774(TABLES_774) },
            ProtocolVersions.V1_21_9 to { _ -> Link773(TABLES_773) },
            ProtocolVersions.V1_21_7 to { _ -> Link772(TABLES_772) },
            ProtocolVersions.V1_21_6 to { _ -> RenumberingLink(TABLES_771) },
            // 1.21.6 changed nothing a packet carries: the dialogs and the waypoint it added are
            // simply not sent to a client that has no id for them.
            ProtocolVersions.V1_21_5 to { _ -> RenumberingLink(TABLES_770) },
            ProtocolVersions.V1_21_4 to { _ -> Link769(TABLES_769) },
            ProtocolVersions.V1_21_2 to { _ -> Link768(TABLES_768) },
            ProtocolVersions.V1_21 to { _ -> Link767(TABLES_767) },
            ProtocolVersions.V1_20_5 to { _ -> Link766(TABLES_766) },
            ProtocolVersions.V1_20_3 to { composed -> Link765(TABLES_765, composed) },
            ProtocolVersions.V1_20_2 to { _ -> Link764(TABLES_764) },
            ProtocolVersions.V1_20 to { _ -> Link763(TABLES_763) },
            ProtocolVersions.V1_19_4 to { _ -> Link762(TABLES_762) },
            ProtocolVersions.V1_19_3 to { composed -> Link761(TABLES_761, composed) },
            ProtocolVersions.V1_19_1 to { _ -> Link760(TABLES_760) },
            ProtocolVersions.V1_19 to { _ -> Link759(TABLES_759) },
            ProtocolVersions.V1_18_2 to { _ -> Link758(TABLES_758) },
            ProtocolVersions.V1_18 to { _ -> Link757(TABLES_757) },
            ProtocolVersions.V1_17_1 to { _ -> Link756(TABLES_756) },
            ProtocolVersions.V1_17 to { _ -> Link755(TABLES_755) },
            ProtocolVersions.V1_16_4 to { _ -> Link754(TABLES_754) },
            // Nothing changed on the wire in these two: 1.16.3 and 1.16.2 are 1.16.4 with other
            // numbers, so they are tables and no code.
            ProtocolVersions.V1_16_3 to { _ -> RenumberingLink(TABLES_753) },
            ProtocolVersions.V1_16_2 to { _ -> RenumberingLink(TABLES_751) },
            ProtocolVersions.V1_16_1 to { composed -> Link736(TABLES_736, composed) },
            ProtocolVersions.V1_16 to { _ -> RenumberingLink(TABLES_735) },
            ProtocolVersions.V1_15_2 to { _ -> Link578(TABLES_578) },
            ProtocolVersions.V1_15_1 to { _ -> RenumberingLink(TABLES_575) },
            ProtocolVersions.V1_15 to { _ -> RenumberingLink(TABLES_573) },
            ProtocolVersions.V1_14_4 to { _ -> Link498(TABLES_498) },
            ProtocolVersions.V1_14_3 to { _ -> RenumberingLink(TABLES_490) },
            ProtocolVersions.V1_14_2 to { _ -> RenumberingLink(TABLES_485) },
            ProtocolVersions.V1_14_1 to { _ -> RenumberingLink(TABLES_480) },
            ProtocolVersions.V1_14 to { _ -> RenumberingLink(TABLES_477) },
            ProtocolVersions.V1_13_2 to { _ -> Link404(TABLES_404) },
            ProtocolVersions.V1_13_1 to { _ -> RenumberingLink(TABLES_401) },
            ProtocolVersions.V1_13 to { _ -> RenumberingLink(TABLES_393) },
            ProtocolVersions.V1_12_2 to { _ -> Link340(TABLES_340) },
            ProtocolVersions.V1_12_1 to { _ -> RenumberingLink(TABLES_338) },
            ProtocolVersions.V1_12 to { _ -> RenumberingLink(TABLES_335) },
            ProtocolVersions.V1_11_1 to { _ -> RenumberingLink(TABLES_316) },
            ProtocolVersions.V1_11 to { _ -> RenumberingLink(TABLES_315) },
            ProtocolVersions.V1_10 to { _ -> Link210(TABLES_210) },
            ProtocolVersions.V1_9_4 to { _ -> Link110(TABLES_110) },
            ProtocolVersions.V1_9_2 to { _ -> Link109(TABLES_109) },
            ProtocolVersions.V1_9_1 to { _ -> RenumberingLink(TABLES_108) },
            ProtocolVersions.V1_9 to { _ -> Link107(TABLES_107) },
            ProtocolVersions.V1_8 to { _ -> Link47(TABLES_47) },
        )

    /** Every protocol a client may connect with, the native one included. */
    public val supported: Set<Int> =
        buildSet {
            add(ProtocolVersions.NATIVE)
            var expected = ProtocolVersions.NATIVE
            for ((version, _) in factories) {
                val below = ProtocolVersions.known.lastOrNull { it.number < expected }?.number ?: break
                if (version != below) break
                add(version)
                expected = version
            }
        }

    /**
     * How [protocol] numbers a block state this server calls [nativeState].
     *
     * The tables are otherwise private to the links. This is here because "what does the other side
     * call this" is a question worth answering from outside one: a test checks the wire against it,
     * and a proxy that rewrites a chunk of its own needs the same answer.
     */
    public fun blockStateAs(
        protocol: Int,
        nativeState: Int,
    ): Int =
        when (protocol) {
            ProtocolVersions.NATIVE -> nativeState
            ProtocolVersions.V26_1 -> TABLES_775.blockStates.map(nativeState)
            ProtocolVersions.V1_21_11 -> TABLES_774.blockStates.map(TABLES_775.blockStates.map(nativeState))
            else -> nativeState
        }

    /** Every link's tables, in the same order as [factories], for composing the numbering. */
    private val tablesByVersion: Map<Int, org.basaltmc.wire.multiversion.VersionTables> =
        mapOf(
            ProtocolVersions.V26_1 to TABLES_775,
            ProtocolVersions.V1_21_11 to TABLES_774,
            ProtocolVersions.V1_21_9 to TABLES_773,
            ProtocolVersions.V1_21_7 to TABLES_772,
            ProtocolVersions.V1_21_6 to TABLES_771,
            ProtocolVersions.V1_21_5 to TABLES_770,
            ProtocolVersions.V1_21_4 to TABLES_769,
            ProtocolVersions.V1_21_2 to TABLES_768,
            ProtocolVersions.V1_21 to TABLES_767,
            ProtocolVersions.V1_20_5 to TABLES_766,
            ProtocolVersions.V1_20_3 to TABLES_765,
            ProtocolVersions.V1_20_2 to TABLES_764,
            ProtocolVersions.V1_20 to TABLES_763,
            ProtocolVersions.V1_19_4 to TABLES_762,
            ProtocolVersions.V1_19_3 to TABLES_761,
            ProtocolVersions.V1_19_1 to TABLES_760,
            ProtocolVersions.V1_19 to TABLES_759,
            ProtocolVersions.V1_18_2 to TABLES_758,
            ProtocolVersions.V1_18 to TABLES_757,
            ProtocolVersions.V1_17_1 to TABLES_756,
            ProtocolVersions.V1_17 to TABLES_755,
            ProtocolVersions.V1_16_4 to TABLES_754,
            ProtocolVersions.V1_16_3 to TABLES_753,
            ProtocolVersions.V1_16_2 to TABLES_751,
            ProtocolVersions.V1_16_1 to TABLES_736,
            ProtocolVersions.V1_16 to TABLES_735,
            ProtocolVersions.V1_15_2 to TABLES_578,
            ProtocolVersions.V1_15_1 to TABLES_575,
            ProtocolVersions.V1_15 to TABLES_573,
            ProtocolVersions.V1_14_4 to TABLES_498,
            ProtocolVersions.V1_14_3 to TABLES_490,
            ProtocolVersions.V1_14_2 to TABLES_485,
            ProtocolVersions.V1_14_1 to TABLES_480,
            ProtocolVersions.V1_14 to TABLES_477,
            ProtocolVersions.V1_13_2 to TABLES_404,
            ProtocolVersions.V1_13_1 to TABLES_401,
            ProtocolVersions.V1_13 to TABLES_393,
            ProtocolVersions.V1_12_2 to TABLES_340,
            ProtocolVersions.V1_12_1 to TABLES_338,
            ProtocolVersions.V1_12 to TABLES_335,
            ProtocolVersions.V1_11_1 to TABLES_316,
            ProtocolVersions.V1_11 to TABLES_315,
            ProtocolVersions.V1_10 to TABLES_210,
            ProtocolVersions.V1_9_4 to TABLES_110,
            ProtocolVersions.V1_9_2 to TABLES_109,
            ProtocolVersions.V1_9_1 to TABLES_108,
            ProtocolVersions.V1_9 to TABLES_107,
            ProtocolVersions.V1_8 to TABLES_47,
        )

    /** A chain of fresh links for one connection, or null when nothing has to be translated. */
    public fun chainFor(protocol: Int): ProtocolChain? {
        if (protocol == ProtocolVersions.NATIVE || protocol !in supported) return null
        val used = factories.takeWhile { it.first >= protocol }
        val composed = ComposedTables(used.mapNotNull { tablesByVersion[it.first] })
        return ProtocolChain(used.map { it.second(composed) }, ContentRewriter(composed))
    }
}
