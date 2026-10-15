basalt_module(wire-auth
    SOURCES
        src/Cfb8Cipher.cpp
        src/HttpsClient.cpp
        src/MojangSessionService.cpp
        src/LegacyForwarding.cpp
        src/ProxyForwarding.cpp
        src/SessionKeyPair.cpp
        src/SignatureValidator.cpp
        src/VelocityForwarding.cpp
    DEPENDS basalt::wire-common basalt::wire-io basalt::wire-protocol
    PRIVATE_DEPENDS basalt::mbedtls
)

if(BASALT_BUILD_TESTS)
    basalt_embed_text(wire-auth-tls-fixtures
        NAMESPACE basalt::wire::auth::test
        HEADER basalt/wire/auth/test/TlsFixtures.hpp
        SUFFIX Pem
        FILES ${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/localhost_cert.pem
              ${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures/localhost_key.pem
    )
endif()

basalt_test(wire-auth-test
    SOURCES
        test/AuthTest.cpp
        test/HttpsResponseTest.cpp
        test/HttpsTransportTest.cpp
    DEPENDS basalt::wire-auth basalt::wire-auth-tls-fixtures basalt::mbedtls
)

if(BASALT_BUILD_TESTS)
    set_tests_properties(wire-auth-test PROPERTIES TIMEOUT 30)
endif()
