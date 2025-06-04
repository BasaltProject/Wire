plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":wire-common"))
            // For JsonToNbt: the registries this server sends are datapack JSON turned into NBT.
            implementation(libs.kotlinx.serialization.json)
        }
    }
}
