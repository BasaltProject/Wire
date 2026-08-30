basalt_module(wire-io
    SOURCES
        src/Compression.cpp
        src/EventLoop.cpp
        src/FileIO.cpp
        src/LogFile.cpp
        src/Platform.cpp
        src/TcpSocket.cpp
        src/ZipReader.cpp
        src/Zstd.cpp
    DEPENDS basalt::wire-common
    PRIVATE_DEPENDS basalt::zlib basalt::zstd
)

basalt_test(wire-io-test
    SOURCES
        test/IoTest.cpp
    DEPENDS basalt::wire-io
)
