basalt_module(wire-nbt
    SOURCES
        src/JsonToNbt.cpp
        src/ModifiedUtf8.cpp
        src/NbtIO.cpp
        src/NbtTag.cpp
        src/NbtType.cpp
        src/Snbt.cpp
    DEPENDS basalt::wire-common
)

basalt_test(wire-nbt-test
    SOURCES
        test/NbtTest.cpp
    DEPENDS basalt::wire-nbt
)
