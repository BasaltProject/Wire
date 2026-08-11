plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":basalt-common"))
        }
        nativeMain.dependencies {
            implementation(libs.zstd.kmp)
        }
    }
}
