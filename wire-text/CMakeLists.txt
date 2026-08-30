basalt_module(wire-text
    SOURCES
        src/Component.cpp
        src/ComponentJson.cpp
        src/ComponentNbt.cpp
        src/LangTable.cpp
        src/MiniMessage.cpp
        src/Serializers.cpp
        src/TranslationResolver.cpp
        src/TextColor.cpp
    DEPENDS basalt::wire-common basalt::wire-nbt
)

basalt_test(wire-text-test
    SOURCES
        test/TextTest.cpp
    DEPENDS basalt::wire-text
)
