basalt_module(wire-common
    SOURCES
        src/Identifier.cpp
        src/Json.cpp
        src/Logger.cpp
        src/LogOutput.cpp
        src/Md5.cpp
        src/Mth.cpp
    PRIVATE_DEPENDS basalt::simdjson
)

basalt_test(wire-common-test
    SOURCES
        test/CommonTest.cpp
    DEPENDS basalt::wire-common
)
