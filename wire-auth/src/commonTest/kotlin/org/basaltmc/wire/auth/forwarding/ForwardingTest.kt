package org.basaltmc.wire.auth.forwarding

import org.basaltmc.wire.auth.SessionProfileProperty
import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertIs
import kotlin.test.assertTrue
import kotlin.uuid.ExperimentalUuidApi
import kotlin.uuid.Uuid

/**
 * The forwarded data is what decides who a connection claims to be, so what matters here is not
 * that a well-formed login is read back but that every malformed or unsigned one is refused.
 */
@OptIn(ExperimentalUuidApi::class)
class ForwardingTest {
    private val playerId = Uuid.parse("d8f4a1e0-0f1b-4c3a-9f2e-1a2b3c4d5e6f")
    private val skin = SessionProfileProperty("textures", "a-skin", "a-signature")

    private fun bungeeGuard(vararg tokens: String) =
        ProxyForwarding(ForwardingMode.BUNGEE_GUARD, tokens = tokens.toSet())

    private fun forwarded(
        forwarding: ProxyForwarding,
        token: String? = null,
        properties: List<SessionProfileProperty> = listOf(skin),
    ) = LegacyForwarding.encode(
        host = "mc.example.com",
        address = "192.0.2.10",
        profileId = playerId,
        properties = properties,
        tokenProperty = token?.let { forwarding.tokenProperty },
        token = token,
    )

    @Test
    fun readsBackWhatABungeeProxyWrote() {
        val forwarding = ProxyForwarding(ForwardingMode.BUNGEECORD)
        val accepted = assertIs<ForwardingResult.Accepted>(LegacyForwarding.decode(forwarded(forwarding), forwarding))

        assertEquals("mc.example.com", accepted.login.host)
        assertEquals("192.0.2.10", accepted.login.address)
        assertEquals(playerId, accepted.login.profileId)
        assertEquals(listOf(skin), accepted.login.properties)
    }

    @Test
    fun stripsTheTokenFromTheProfileItAccepts() {
        val forwarding = bungeeGuard("a-token")
        val accepted =
            assertIs<ForwardingResult.Accepted>(
                LegacyForwarding.decode(forwarded(forwarding, token = "a-token"), forwarding),
            )

        // The token is a password: the other players are sent this profile.
        assertEquals(listOf(skin), accepted.login.properties)
    }

    @Test
    fun refusesTheWrongTokenAndNoTokenAtAll() {
        val forwarding = bungeeGuard("a-token")

        assertIs<ForwardingResult.Rejected>(LegacyForwarding.decode(forwarded(forwarding, "another-token"), forwarding))
        assertIs<ForwardingResult.Rejected>(LegacyForwarding.decode(forwarded(forwarding), forwarding))
        // Somebody who reached the port without going through the proxy at all.
        assertIs<ForwardingResult.Rejected>(LegacyForwarding.decode("mc.example.com", forwarding))
    }

    @Test
    fun refusesTwoTokensInOneLogin() {
        val forwarding = bungeeGuard("a-token")
        val doubled =
            LegacyForwarding.encode(
                host = "mc.example.com",
                address = "192.0.2.10",
                profileId = playerId,
                properties =
                    listOf(
                        SessionProfileProperty(forwarding.tokenProperty, "a-guess"),
                        SessionProfileProperty(forwarding.tokenProperty, "a-token"),
                    ),
            )

        assertIs<ForwardingResult.Rejected>(LegacyForwarding.decode(doubled, forwarding))
    }

    @Test
    fun refusesEveryLoginWhileNoTokenIsConfigured() {
        val unconfigured = ProxyForwarding(ForwardingMode.BUNGEE_GUARD)

        assertTrue(unconfigured.misconfiguration() != null)
        assertIs<ForwardingResult.Rejected>(LegacyForwarding.decode(forwarded(unconfigured, "a-token"), unconfigured))
    }

    @Test
    fun readsSafeNetsOwnPropertyName() {
        val safeNet = ProxyForwarding(ForwardingMode.SAFE_NET, tokens = setOf("a-passphrase"))
        assertEquals(ProxyForwarding.SAFE_NET_PROPERTY, safeNet.tokenProperty)

        val accepted =
            assertIs<ForwardingResult.Accepted>(
                LegacyForwarding.decode(forwarded(safeNet, token = "a-passphrase"), safeNet),
            )
        assertEquals(listOf(skin), accepted.login.properties)
    }

    @Test
    fun readsThroughTheChunksFloodgateInserts() {
        val forwarding = ProxyForwarding(ForwardingMode.BUNGEECORD)
        val separator = '\u0000'
        val withFloodgate =
            "mc.example.com$separator" + "Geyser-Floodgate${separator}floodgate-data$separator" +
                "192.0.2.10$separator${playerId.toHexString()}$separator[]"

        val accepted = assertIs<ForwardingResult.Accepted>(LegacyForwarding.decode(withFloodgate, forwarding))
        assertEquals("192.0.2.10", accepted.login.address)
        assertEquals(playerId, accepted.login.profileId)
    }

    @Test
    fun readsBackWhatVelocitySigned() {
        val forwarding = ProxyForwarding(ForwardingMode.VELOCITY, secret = "a-secret")
        val signed =
            VelocityForwarding.sign("a-secret", "192.0.2.10", playerId, "Steve", listOf(skin))

        val accepted = assertIs<ForwardingResult.Accepted>(VelocityForwarding.decode(signed, forwarding))
        assertEquals("192.0.2.10", accepted.login.address)
        assertEquals(playerId, accepted.login.profileId)
        assertEquals("Steve", accepted.login.name)
        assertEquals(listOf(skin), accepted.login.properties)
    }

    @Test
    fun refusesAPayloadSignedWithAnotherSecret() {
        val forwarding = ProxyForwarding(ForwardingMode.VELOCITY, secret = "a-secret")
        val signed =
            VelocityForwarding.sign("another-secret", "192.0.2.10", playerId, "Steve", emptyList())

        assertIs<ForwardingResult.Rejected>(VelocityForwarding.decode(signed, forwarding))
        // And one nobody signed at all, which is the shape a guess takes.
        assertIs<ForwardingResult.Rejected>(VelocityForwarding.decode(ByteArray(64), forwarding))
        assertIs<ForwardingResult.Rejected>(VelocityForwarding.decode(ByteArray(8), forwarding))
    }

    @Test
    fun refusesAForwardingVersionItCannotRead() {
        val forwarding = ProxyForwarding(ForwardingMode.VELOCITY, secret = "a-secret")
        val signed =
            VelocityForwarding.sign(
                "a-secret",
                "192.0.2.10",
                playerId,
                "Steve",
                emptyList(),
                version = VelocityForwarding.MAX_SUPPORTED_VERSION + 1,
            )

        assertIs<ForwardingResult.Rejected>(VelocityForwarding.decode(signed, forwarding))
    }
}
