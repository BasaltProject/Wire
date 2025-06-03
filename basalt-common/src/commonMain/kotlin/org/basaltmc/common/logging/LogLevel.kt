package org.basaltmc.common.logging

/** Ordered from most to least verbose; [ordinal] drives level filtering in [Logger]. */
public enum class LogLevel(
    public val label: String,
) {
    TRACE("TRACE"),
    DEBUG("DEBUG"),
    INFO("INFO"),
    WARN("WARN"),
    ERROR("ERROR"),
}
