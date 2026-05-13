basalt_module(wire-config
    SOURCES
        src/BasicPlaceholders.cpp
        src/ConfigDirectory.cpp
        src/ConfigUpgrade.cpp
        src/ProfileSelection.cpp
        src/ServerListConfig.cpp
        src/ServerListStatus.cpp
    DEPENDS basalt::wire-common basalt::wire-io basalt::wire-protocol basalt::wire-text
)

