basalt_module(wire-config
    SOURCES
        src/BasicPlaceholders.cpp
        src/AnimationPlaceholders.cpp
        src/AnimationsConfig.cpp
        src/ConfigDirectory.cpp
        src/ConfigUpgrade.cpp
        src/PlayerUi.cpp
        src/ProfileSelection.cpp
        src/ServerListConfig.cpp
        src/ServerListStatus.cpp
        src/UiConfigs.cpp
    DEPENDS basalt::wire-common basalt::wire-io basalt::wire-protocol basalt::wire-text
)

