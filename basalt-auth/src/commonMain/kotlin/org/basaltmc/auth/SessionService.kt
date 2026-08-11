package org.basaltmc.auth

import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

@OptIn(ExperimentalUuidApi::class)
public data class SessionProfile(
    val id: Uuid,
    val name: String,
    val properties: List<SessionProfileProperty> = emptyList(),
)

public data class SessionProfileProperty(
    val name: String,
    val value: String,
    val signature: String? = null,
)

/**
 * Verifies a client's claimed username against Mojang's session server after key exchange.
 *
 * // parity: `MinecraftSessionService.hasJoinedServer`.
 *
 * [MojangSessionService] is the real implementation. Tests can supply a fake one instead.
 */
public fun interface SessionService {
    public fun hasJoined(
        username: String,
        serverHash: String,
        clientAddress: String?,
    ): SessionProfile?
}
