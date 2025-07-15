package org.basaltmc.wire.multiversion

/** What to do with a client whose protocol this build does not know. */
public enum class UnknownProtocolPolicy {
    ACCEPT,
    WARN,
    REFUSE,
    ;

    public companion object {
        /** Null rather than a default, so a typo in the file is reported instead of guessed. */
        public fun parse(name: String): UnknownProtocolPolicy? =
            entries.firstOrNull { it.name.equals(name.trim(), ignoreCase = true) }
    }
}

/**
 * The answer to "may this client in", and why.
 *
 * [Translated] and [Unknown] both let someone in, and the difference matters to whoever reads the
 * log: one is a version this build was built to speak, the other is a bet.
 */
public sealed interface ProtocolVerdict {
    /** Speaks what the server speaks. Nothing is translated. */
    public data object Native : ProtocolVerdict

    public data class Translated(
        public val version: ProtocolVersion,
    ) : ProtocolVerdict

    /** Let in without a translator: newer than this build, or a snapshot. */
    public data class Unknown(
        public val protocol: Int,
        public val warn: Boolean,
    ) : ProtocolVerdict

    /** Kicked at the handshake, with what the player reads. */
    public data class Refused(
        public val protocol: Int,
        public val reason: Reason,
    ) : ProtocolVerdict {
        public enum class Reason { TOO_OLD, TOO_NEW, SNAPSHOT, NOT_TRANSLATED }
    }
}

/**
 * The door. It knows which protocols this build can translate and what the operator decided about
 * the ones it cannot, and it answers before a player exists.
 *
 * It deliberately has no idea how translation works: [supported] is a set of numbers handed to it,
 * so the day a link is added the gate changes by itself.
 */
public class ProtocolGate(
    /** Every protocol this door lets in, the native one included. */
    public val supported: Set<Int>,
    private val futurePolicy: UnknownProtocolPolicy,
    private val snapshotPolicy: UnknownProtocolPolicy,
    private val native: Int = ProtocolVersions.NATIVE,
) {
    public fun verdictFor(protocol: Int): ProtocolVerdict {
        if (protocol == native) return ProtocolVerdict.Native
        if (ProtocolVersions.isSnapshot(protocol)) return unknown(protocol, snapshotPolicy, Refusal.SNAPSHOT)
        if (protocol > native) return unknown(protocol, futurePolicy, Refusal.TOO_NEW)
        val version = ProtocolVersions.of(protocol)
        if (version == null || protocol < ProtocolVersions.oldest) {
            return ProtocolVerdict.Refused(protocol, ProtocolVerdict.Refused.Reason.TOO_OLD)
        }
        if (protocol !in supported) {
            return ProtocolVerdict.Refused(protocol, ProtocolVerdict.Refused.Reason.NOT_TRANSLATED)
        }
        return ProtocolVerdict.Translated(version)
    }

    private fun unknown(
        protocol: Int,
        policy: UnknownProtocolPolicy,
        refusal: Refusal,
    ): ProtocolVerdict =
        when (policy) {
            UnknownProtocolPolicy.ACCEPT -> ProtocolVerdict.Unknown(protocol, warn = false)
            UnknownProtocolPolicy.WARN -> ProtocolVerdict.Unknown(protocol, warn = true)
            UnknownProtocolPolicy.REFUSE ->
                ProtocolVerdict.Refused(
                    protocol,
                    when (refusal) {
                        Refusal.SNAPSHOT -> ProtocolVerdict.Refused.Reason.SNAPSHOT
                        Refusal.TOO_NEW -> ProtocolVerdict.Refused.Reason.TOO_NEW
                    },
                )
        }

    private enum class Refusal { SNAPSHOT, TOO_NEW }
}
