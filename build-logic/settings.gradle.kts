// Not plain "build-logic": consumers include this build alongside their own, and two builds of the
// same name cannot both be substituted for.
rootProject.name = "wire-build-logic"

dependencyResolutionManagement {
    repositories {
        gradlePluginPortal()
        mavenCentral()
    }
}
