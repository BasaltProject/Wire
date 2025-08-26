plugins {
    id("basalt.native-module")
}

kotlin {
    sourceSets {
        commonMain.dependencies {
            api(project(":wire-common"))
            api(project(":wire-io"))
            api(project(":wire-protocol"))
            implementation(libs.cryptography.core)
            implementation(libs.cryptography.provider.optimal)
            implementation(libs.ktor.client.core)
            implementation(libs.kotlinx.coroutines.core)
        }
        commonTest.dependencies {
            implementation(libs.ktor.client.mock)
        }

        // Ktor needs a concrete native engine per platform family. Only the families the
        // convention plugin actually registered for the current host exist as source sets here
        // (macOS builds only ever see `macosMain`, never `linuxMain`/`mingwMain`), hence the
        // guarded lookups instead of the typed `linuxMain.dependencies { ... }` accessor, which
        // would fail to even configure on a host that never created that source set.
        sourceSets.findByName("macosMain")?.dependencies {
            implementation(libs.ktor.client.darwin)
        }
        sourceSets.findByName("linuxMain")?.dependencies {
            implementation(libs.ktor.client.curl)
        }
        sourceSets.findByName("mingwMain")?.dependencies {
            implementation(libs.ktor.client.winhttp)
        }
    }
}
