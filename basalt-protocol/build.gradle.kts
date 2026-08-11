plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":basalt-common"))
            api(project(":basalt-nbt"))
            api(project(":basalt-text"))
            api(project(":basalt-io"))
            implementation(libs.kotlinx.serialization.json)
        }
    }
}
