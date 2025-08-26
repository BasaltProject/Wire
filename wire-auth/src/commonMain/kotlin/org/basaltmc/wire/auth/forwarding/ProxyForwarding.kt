package org.basaltmc.wire.auth.forwarding

import org.basaltmc.wire.auth.SessionProfileProperty
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * How a proxy tells this server who is really connecting. Every mode replaces the session exchange,
 * and what they differ in is what stops anybody else from claiming to be the proxy.
 */
public enum class ForwardingMode(
    /** What an operator writes in the configuration file. */
    public val configName: String,
) {
    /** Nobody is forwarding. The server authenticates its own players. */
    NONE("none"),

    /**
     * BungeeCord's `ip_forward`: the data is appended to the handshake host, unsigned.
     *
     * Nothing proves it came from the proxy, so the port has to be unreachable from outside.
     */
    BUNGEECORD("bungeecord"),

    /** BungeeCord forwarding plus a shared token in the profile properties, as BungeeGuard injects it. */
    BUNGEE_GUARD("bungeeguard"),

    /** The same idea as [BUNGEE_GUARD] with SafeNET's own property name and passphrase. */
    SAFE_NET("safenet"),

    /** Velocity's modern forwarding: an HMAC-SHA256 signed payload, asked for over a login query. */
    VELOCITY("velocity"),
    ;

    /** Whether this mode reads its data off the handshake host rather than a login query. */
    public val isLegacy: Boolean get() = this == BUNGEECORD || this == BUNGEE_GUARD || this == SAFE_NET

    public companion object {
        public fun parse(name: String): ForwardingMode? {
            val written = name.trim().lowercase()
            return entries.find { it.configName == written } ?: ALIASES[written]
        }

        // The names other software uses for the same thing, so a copied configuration line works.
        private val ALIASES =
            mapOf(
                "legacy" to BUNGEECORD,
                "bungee" to BUNGEECORD,
                "modern" to VELOCITY,
                "false" to NONE,
                "disabled" to NONE,
            )
    }
}

/** Who a proxy says is connecting. [name] is null for the legacy modes, which never carry one. */
@OptIn(ExperimentalUuidApi::class)
public data class ForwardedLogin(
    /** The host the player really typed, with the forwarded data stripped back off it. */
    val host: String,
    /** The player's own address, which is what bans, logs and plugins should see. */
    val address: String,
    val profileId: Uuid,
    val name: String?,
    val properties: List<SessionProfileProperty>,
)

/** What came of reading a proxy's forwarded data. */
public sealed interface ForwardingResult {
    public data class Accepted(
        val login: ForwardedLogin,
    ) : ForwardingResult

    /** [reason] is for the log. It is never shown to the player: it would say why the guess was wrong. */
    public data class Rejected(
        val reason: String,
    ) : ForwardingResult
}

/** The forwarding an operator configured: which mode, and the secret that mode needs. */
public class ProxyForwarding(
    public val mode: ForwardingMode,
    /** Velocity's shared HMAC secret. Unused by every other mode. */
    public val secret: String = "",
    /** The tokens BungeeGuard or SafeNET will accept. More than one so keys can be rotated. */
    public val tokens: Set<String> = emptySet(),
    /** The property SafeNET carries its passphrase in. Configurable there, so configurable here. */
    public val propertyName: String = SAFE_NET_PROPERTY,
) {
    public val enabled: Boolean get() = mode != ForwardingMode.NONE

    /** Whether this configuration proves the data came from the proxy, or merely trusts it. */
    public val authenticated: Boolean
        get() =
            when (mode) {
                ForwardingMode.NONE, ForwardingMode.BUNGEECORD -> false
                ForwardingMode.BUNGEE_GUARD, ForwardingMode.SAFE_NET -> tokens.isNotEmpty()
                ForwardingMode.VELOCITY -> secret.isNotEmpty()
            }

    /** Why this configuration cannot be used, or null when it can. */
    public fun misconfiguration(): String? =
        when (mode) {
            ForwardingMode.NONE, ForwardingMode.BUNGEECORD -> null
            ForwardingMode.BUNGEE_GUARD ->
                if (tokens.isEmpty()) "bungeeguard forwarding needs at least one token in tokens" else null
            ForwardingMode.SAFE_NET ->
                if (tokens.isEmpty()) "safenet forwarding needs at least one passphrase in tokens" else null
            ForwardingMode.VELOCITY ->
                if (secret.isEmpty()) "velocity forwarding needs the proxy's forwarding.secret" else null
        }

    /** The name of the property the token travels in, for the mode that uses one. */
    public val tokenProperty: String
        get() =
            when (mode) {
                ForwardingMode.SAFE_NET -> propertyName
                else -> BUNGEE_GUARD_PROPERTY
            }

    public companion object {
        /** // parity of name with BungeeGuard's own `bungeeguard-token`. */
        public const val BUNGEE_GUARD_PROPERTY: String = "bungeeguard-token"

        /** SafeNET's default `property-name.handshake`. */
        public const val SAFE_NET_PROPERTY: String = "safe_net_passphrase"

        public val DISABLED: ProxyForwarding = ProxyForwarding(ForwardingMode.NONE)
    }
}
