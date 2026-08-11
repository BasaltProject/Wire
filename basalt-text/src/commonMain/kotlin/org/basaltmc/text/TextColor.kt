package org.basaltmc.text

/** An RGB color, either named (one of the 16 legacy colors) or arbitrary truecolor. */
public data class TextColor(
    val rgb: Int,
) {
    public val red: Int get() = (rgb shr 16) and 0xFF
    public val green: Int get() = (rgb shr 8) and 0xFF
    public val blue: Int get() = rgb and 0xFF

    public fun toHexString(): String = "#" + rgb.toString(16).padStart(6, '0')

    public companion object {
        public fun of(
            red: Int,
            green: Int,
            blue: Int,
        ): TextColor = TextColor((red and 0xFF shl 16) or (green and 0xFF shl 8) or (blue and 0xFF))

        public fun fromHex(hex: String): TextColor? {
            val cleaned = hex.removePrefix("#")
            if (cleaned.length != 6) return null
            val value = cleaned.toIntOrNull(16) ?: return null
            return TextColor(value)
        }

        public fun lerp(
            from: TextColor,
            to: TextColor,
            fraction: Float,
        ): TextColor {
            val t = fraction.coerceIn(0f, 1f)

            fun mix(
                a: Int,
                b: Int,
            ): Int = (a + (b - a) * t).toInt().coerceIn(0, 255)
            return of(mix(from.red, to.red), mix(from.green, to.green), mix(from.blue, to.blue))
        }

        public val BLACK: TextColor = TextColor(0x000000)
        public val DARK_BLUE: TextColor = TextColor(0x0000AA)
        public val DARK_GREEN: TextColor = TextColor(0x00AA00)
        public val DARK_AQUA: TextColor = TextColor(0x00AAAA)
        public val DARK_RED: TextColor = TextColor(0xAA0000)
        public val DARK_PURPLE: TextColor = TextColor(0xAA00AA)
        public val GOLD: TextColor = TextColor(0xFFAA00)
        public val GRAY: TextColor = TextColor(0xAAAAAA)
        public val DARK_GRAY: TextColor = TextColor(0x555555)
        public val BLUE: TextColor = TextColor(0x5555FF)
        public val GREEN: TextColor = TextColor(0x55FF55)
        public val AQUA: TextColor = TextColor(0x55FFFF)
        public val RED: TextColor = TextColor(0xFF5555)
        public val LIGHT_PURPLE: TextColor = TextColor(0xFF55FF)
        public val YELLOW: TextColor = TextColor(0xFFFF55)
        public val WHITE: TextColor = TextColor(0xFFFFFF)

        private val NAMED: Map<String, TextColor> =
            mapOf(
                "black" to BLACK,
                "dark_blue" to DARK_BLUE,
                "dark_green" to DARK_GREEN,
                "dark_aqua" to DARK_AQUA,
                "dark_red" to DARK_RED,
                "dark_purple" to DARK_PURPLE,
                "gold" to GOLD,
                "gray" to GRAY,
                "dark_gray" to DARK_GRAY,
                "blue" to BLUE,
                "green" to GREEN,
                "aqua" to AQUA,
                "red" to RED,
                "light_purple" to LIGHT_PURPLE,
                "yellow" to YELLOW,
                "white" to WHITE,
            )

        private val NAMES_BY_COLOR: Map<TextColor, String> = NAMED.entries.associate { (name, color) -> color to name }

        public fun named(name: String): TextColor? = NAMED[name]

        public fun nameOf(color: TextColor): String? = NAMES_BY_COLOR[color]

        public fun parse(text: String): TextColor? = named(text) ?: fromHex(text)
    }
}
