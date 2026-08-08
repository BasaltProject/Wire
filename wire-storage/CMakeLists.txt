basalt_module(wire-storage
    SOURCES
        src/AccessRepository.cpp
        src/AntibotVerifiedRepository.cpp
        src/PlayerIdentityRepository.cpp
        src/PluginDataRepository.cpp
        src/Schema.cpp
    DEPENDS basalt::wire-sqlite
)

