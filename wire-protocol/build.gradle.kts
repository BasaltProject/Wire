plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":wire-common"))
            api(project(":wire-nbt"))
            api(project(":wire-text"))
            api(project(":wire-io"))
            implementation(libs.kotlinx.serialization.json)
        }
    }
}
