plugins {
    `kotlin-dsl`
}

repositories {
    gradlePluginPortal()
    mavenCentral()
}

// Every Basalt repository composes this one build rather than keeping a second one of its own.
// Two included builds would each load their own Kotlin Gradle Plugin, and a project applying a
// convention from both then fails on a class cast between the two classloaders (a real failure,
// not a precaution: "SwiftPMLockTaskAggregationBuildService cannot be cast to itself"). That is
// also why the SQLDelight plugin is resolved here instead of from a module's own `plugins` block:
// it ships a Kotlin Gradle Plugin too.
dependencies {
    implementation("org.jetbrains.kotlin:kotlin-gradle-plugin:2.4.10")
    implementation("org.jlleitschuh.gradle:ktlint-gradle:13.0.0")
    implementation("app.cash.sqldelight:gradle-plugin:2.3.2")
    implementation("org.jetbrains.kotlinx:kotlinx-serialization-json:1.9.0")
}
