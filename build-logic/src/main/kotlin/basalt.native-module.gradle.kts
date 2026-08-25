import org.jetbrains.kotlin.gradle.ExperimentalKotlinGradlePluginApi
import org.jetbrains.kotlin.gradle.dsl.ExplicitApiMode
import org.jetbrains.kotlin.gradle.plugin.mpp.KotlinNativeTarget
import java.io.File

plugins {
    id("org.jetbrains.kotlin.multiplatform")
    id("org.jlleitschuh.gradle.ktlint")
}

// Generated sources are not ours to format: a generator writes its own style and reformatting it
// would be undone on the next build anyway.
ktlint {
    filter {
        exclude { it.file.absolutePath.replace(File.separatorChar, '/').contains("/build/generated/") }
    }
}

kotlin {
    explicitApi = ExplicitApiMode.Strict

    val hostOs = System.getProperty("os.name")
    val isMacOs = hostOs == "Mac OS X"
    val isLinux = hostOs == "Linux"
    val isWindows = hostOs.startsWith("Windows")

    val nativeTargets = buildList<KotlinNativeTarget> {
        if (isMacOs) {
            add(macosArm64())
            add(macosX64())
        }
        if (isLinux) {
            add(linuxX64())
            add(linuxArm64())
        }
        if (isWindows) {
            add(mingwX64())
        }
    }

    check(nativeTargets.isNotEmpty()) {
        "No supported Kotlin/Native target for host '$hostOs'. " +
            "Basalt builds on macOS, Linux or Windows."
    }

    @OptIn(ExperimentalKotlinGradlePluginApi::class)
    applyDefaultHierarchyTemplate()

    sourceSets {
        commonMain {
            dependencies {
            }
        }
        commonTest {
            dependencies {
                implementation(kotlin("test"))
            }
        }
    }

}

// Kotlin/Native cannot link a test binary with zero test sources: the module still compiles, but
// a module without a single packet codec or binary format has nothing left to round-trip test
// against, and the link task fails outright rather than producing an empty runner. Skip the
// native test compile/link/run tasks entirely when this module currently has no test source.
afterEvaluate {
    val hasTestSource =
        kotlin.sourceSets
            .filter { it.name.contains("Test", ignoreCase = true) }
            .any { it.kotlin.files.isNotEmpty() }
    if (!hasTestSource) {
        tasks
            .matching { task ->
                task.name.matches(Regex("compileTestKotlin\\w+")) ||
                    task.name.matches(Regex("link(Debug|Release)Test\\w+")) ||
                    (task.group == "verification" && task.name.matches(Regex("\\w+Test")))
            }.configureEach {
                onlyIf { false }
            }
    }
}
