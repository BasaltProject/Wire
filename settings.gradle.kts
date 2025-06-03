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
    ":wire-common",
    ":wire-nbt",
    ":wire-text",
    ":wire-io",
    ":wire-protocol",
    ":wire-auth",
    ":wire-multiversion",
    ":wire-bedrock",
)
