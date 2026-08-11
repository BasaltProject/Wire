rootProject.name = "wire"

pluginManagement {
    includeBuild("build-logic")
    repositories {
        gradlePluginPortal()
        mavenCentral()
    }
}

dependencyResolutionManagement {
    repositories {
        mavenCentral()
    }
}

include(
    ":basalt-common",
    ":basalt-nbt",
    ":basalt-text",
    ":basalt-io",
    ":basalt-protocol",
    ":basalt-auth",
    ":basalt-multiversion",
    ":basalt-bedrock",
)
