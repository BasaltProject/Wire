basalt_module(wire-protocol
    SOURCES
        src/PacketFraming.cpp
        src/BlockEntityPackets.cpp
        src/BorderPackets.cpp
        src/ConfigurationPackets.cpp
        src/HandshakePacket.cpp
        src/HudPackets.cpp
        src/ItemComponent.cpp
        src/LoginPackets.cpp
        src/LoginPluginPackets.cpp
        src/LowPrecisionVec3.cpp
        src/NibbleArray.cpp
        src/PalettedContainer.cpp
        src/ParticleOptions.cpp
        src/SimplePackets.cpp
        src/StatusPackets.cpp
        src/PlayerStatePackets.cpp
        src/ProtocolBuffer.cpp
    DEPENDS basalt::wire-common basalt::wire-io basalt::wire-nbt basalt::wire-text
)

basalt_test(wire-protocol-test
    SOURCES
        test/ProtocolTest.cpp
    DEPENDS basalt::wire-protocol
)
