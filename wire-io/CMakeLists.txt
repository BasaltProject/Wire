basalt_module(wire-io
    SOURCES
        src/BudgetMemoryResource.cpp
        src/Compression.cpp
        src/ConfigFiles.cpp
        src/ConsoleInput.cpp
        src/DnsResolver.cpp
        src/EventLoop.cpp
        src/FileIO.cpp
        src/LogFile.cpp
        src/Platform.cpp
        src/ResourceBudget.cpp
        src/TcpSocket.cpp
        src/TaskQueue.cpp
        src/TimerQueue.cpp
        src/WorkerPool.cpp
        src/UdpSocket.cpp
        src/ZipReader.cpp
        src/Zstd.cpp
    DEPENDS basalt::wire-common
    PRIVATE_DEPENDS basalt::zlib basalt::zstd basalt::cares
)

option(BASALT_IO_FORCE_POLL "Use portable poll readiness instead of the native backend" OFF)
if(BASALT_IO_FORCE_POLL)
    target_compile_definitions(wire-io PRIVATE BASALT_IO_FORCE_POLL=1)
endif()

basalt_test(wire-io-test
    SOURCES
        test/IoTest.cpp
        test/ConfigFilesTest.cpp
        test/ConsoleInputTest.cpp
        test/DnsResolverTest.cpp
        test/ResourceBudgetTest.cpp
        test/BudgetMemoryResourceTest.cpp
        test/SchedulingTest.cpp
        test/WorkerPoolTest.cpp
    DEPENDS basalt::wire-io
)

if(BASALT_BUILD_TESTS)
    set_tests_properties(wire-io-test PROPERTIES TIMEOUT 30)
endif()
