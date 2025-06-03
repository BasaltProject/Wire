package org.basaltmc.wire.auth

/** Bundles what a connection needs to run the online-mode login key exchange. */
public class OnlineMode(
    public val keyPair: SessionKeyPair,
    public val sessionService: SessionService,
)
