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

// Only the modules that still have Kotlin. A module whose C++ port is finished has had its Kotlin
// deleted: what it was checked against lives on in the branch this one grew from.
include(
    ":wire-common",
    ":wire-nbt",
    ":wire-text",
    ":wire-io",
    ":wire-protocol",
    ":wire-auth",
)
