plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":wire-common"))
        }
        nativeMain.dependencies {
            implementation(libs.zstd.kmp)
        }
    }
}
