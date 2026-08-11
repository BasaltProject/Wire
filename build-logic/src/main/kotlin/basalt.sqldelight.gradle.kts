// SQLDelight applied through a convention plugin rather than from the module's own `plugins` block:
// the plugin ships its own Kotlin Gradle Plugin, and resolving it from the plugin portal loads it in
// a second classloader, which clashes with the one this included build already provides (a real
// failure, not a precaution: "SwiftPMLockTaskAggregationBuildService cannot be cast to itself").
plugins {
    id("basalt.native-module")
    id("app.cash.sqldelight")
}
