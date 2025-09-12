basalt_module(wire-auth
    SOURCES
        src/Cfb8Cipher.cpp
        src/SessionKeyPair.cpp
    DEPENDS basalt::wire-common
    PRIVATE_DEPENDS basalt::mbedtls
)

basalt_test(wire-auth-test
    SOURCES
        test/AuthTest.cpp
    DEPENDS basalt::wire-auth
)
