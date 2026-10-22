basalt_module(wire-config
    SOURCES
        src/AnimationPlaceholders.cpp
        src/AnimationsConfig.cpp
        src/BasicPlaceholders.cpp
        src/CommandsConfig.cpp
        src/ConfigDirectory.cpp
        src/ConfigUpgrade.cpp
        src/DefaultIcon.cpp
        src/MessageCatalog.cpp
        src/Permissions.cpp
        src/PlayerUi.cpp
        src/ProfileSelection.cpp
        src/ServerListConfig.cpp
        src/ServerListStatus.cpp
        src/UiConfigs.cpp
    DEPENDS basalt::wire-common basalt::wire-io basalt::wire-protocol basalt::wire-text
)

