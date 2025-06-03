plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":basalt-common"))
            api(project(":basalt-protocol"))
            api(project(":basalt-multiversion"))
        }
    }
}
