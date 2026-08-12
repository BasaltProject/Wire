plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":wire-common"))
            api(project(":wire-nbt"))
            implementation(libs.kotlinx.serialization.json)
        }
    }
}
