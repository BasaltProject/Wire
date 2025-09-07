basalt_module(wire-protocol
    SOURCES
        src/PacketFraming.cpp
        src/HandshakePacket.cpp
        src/LoginPackets.cpp
        src/LowPrecisionVec3.cpp
        src/NibbleArray.cpp
        src/PalettedContainer.cpp
        src/StatusPackets.cpp
        src/ProtocolBuffer.cpp
    DEPENDS basalt::wire-common basalt::wire-io basalt::wire-nbt basalt::wire-text
)

basalt_test(wire-protocol-test
    SOURCES
        test/ProtocolTest.cpp
    DEPENDS basalt::wire-protocol
)
