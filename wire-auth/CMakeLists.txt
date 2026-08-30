basalt_module(wire-auth
    SOURCES
        src/Cfb8Cipher.cpp
        src/HttpsClient.cpp
        src/MojangSessionService.cpp
        src/LegacyForwarding.cpp
        src/ProxyForwarding.cpp
        src/SessionKeyPair.cpp
        src/VelocityForwarding.cpp
    DEPENDS basalt::wire-common basalt::wire-io basalt::wire-protocol
    PRIVATE_DEPENDS basalt::mbedtls
)

basalt_test(wire-auth-test
    SOURCES
        test/AuthTest.cpp
    DEPENDS basalt::wire-auth
)
