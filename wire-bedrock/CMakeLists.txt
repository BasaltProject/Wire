basalt_module(wire-bedrock
    SOURCES
        src/BedrockBatch.cpp
        src/BedrockCipher.cpp
        src/BedrockConnection.cpp
        src/BedrockGate.cpp
        src/BedrockKeys.cpp
        src/BedrockNbt.cpp
        src/BedrockPacketNames.cpp
        src/BedrockBanners.cpp
        src/BedrockItemFrames.cpp
        src/BedrockBlocks2630.cpp
        src/BedrockBlocks2640.cpp
        src/BedrockBlocks2650.cpp
        src/BedrockBlockTables.cpp
        src/BedrockEffectIds.cpp
        src/BedrockEntityIdentifiers.cpp
        src/BedrockEntityNames.cpp
        src/BedrockItemNames.cpp
        src/BedrockItems.cpp
        src/BedrockParticleNames.cpp
        src/BedrockParticles.cpp
        src/BedrockSoundEvents.cpp
        src/BedrockSoundNames.cpp
        src/BedrockSounds.cpp
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
        test/BedrockShapeTest.cpp
    DEPENDS basalt::wire-bedrock
)
