plugins {
    `kotlin-dsl`
}

// Consumed as a composite build by every repository that uses Wire, so it needs real coordinates
// to be substituted for.
group = "org.basaltmc"
version = "0.1.0-SNAPSHOT"

repositories {
    gradlePluginPortal()
    mavenCentral()
}

dependencies {
    implementation("org.jetbrains.kotlin:kotlin-gradle-plugin:2.4.10")
    implementation("org.jlleitschuh.gradle:ktlint-gradle:13.0.0")
}
