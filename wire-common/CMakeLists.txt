basalt_module(wire-common
    SOURCES
        src/Base64.cpp
        src/Identifier.cpp
        src/Json.cpp
        src/Logger.cpp
        src/LogOutput.cpp
        src/Md5.cpp
        src/Sha256.cpp
        src/Mth.cpp
        src/Panic.cpp
        src/PlaceholderCondition.cpp
        src/Placeholders.cpp
        src/PlaceholderText.cpp
        src/TickDurations.cpp
        src/Timings.cpp
        src/Toml.cpp
        src/Uuid.cpp
    PRIVATE_DEPENDS basalt::simdjson
)

basalt_test(wire-common-placeholder-test
    SOURCES
        test/PlaceholderTest.cpp
    DEPENDS basalt::wire-common
)

basalt_test(wire-common-test
    SOURCES
        test/CommonTest.cpp
    DEPENDS basalt::wire-common
)
