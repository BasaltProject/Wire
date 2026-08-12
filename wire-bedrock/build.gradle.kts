plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":wire-common"))
            api(project(":wire-protocol"))
            api(project(":wire-multiversion"))
        }
    }
}
