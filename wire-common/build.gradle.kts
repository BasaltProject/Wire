plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            // Only for the local time zone of log timestamps. The standard library's Instant is
            // UTC-only, and the alternative (`localtime` through cinterop) is either not
            // thread-safe or needs per-platform code written blind for two of the five targets.
            api(libs.kotlinx.datetime)
        }
    }
}
