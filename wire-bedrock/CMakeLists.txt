basalt_module(wire-bedrock
    SOURCES
        src/BedrockBatch.cpp
        src/BedrockCipher.cpp
        src/BedrockConnection.cpp
        src/BedrockGate.cpp
        src/BedrockKeys.cpp
        src/BedrockPacketNames.cpp
        src/BedrockPackets.cpp
        src/BedrockTypes.cpp
        src/RakFrame.cpp
        src/RakNet.cpp
        src/RakServer.cpp
        src/RakSession.cpp
    DEPENDS basalt::wire-common basalt::wire-io
    PRIVATE_DEPENDS basalt::mbedtls
)

basalt_test(wire-bedrock-test
    SOURCES
        test/BedrockTest.cpp
    DEPENDS basalt::wire-bedrock
)
