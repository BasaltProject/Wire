package org.basaltmc.text

/** What happens when a player clicks a piece of text. Mirrors Adventure's `ClickEvent` model. */
public data class ClickEvent(
    val action: Action,
    val value: String,
) {
    public enum class Action(
        public val id: String,
    ) {
        OPEN_URL("open_url"),
        RUN_COMMAND("run_command"),
        SUGGEST_COMMAND("suggest_command"),
        CHANGE_PAGE("change_page"),
        COPY_TO_CLIPBOARD("copy_to_clipboard"),
        ;

        public companion object {
            public fun fromId(id: String): Action? = entries.find { it.id == id }
        }
    }

    public companion object {
        public fun openUrl(url: String): ClickEvent = ClickEvent(Action.OPEN_URL, url)

        public fun runCommand(command: String): ClickEvent = ClickEvent(Action.RUN_COMMAND, command)

        public fun suggestCommand(command: String): ClickEvent = ClickEvent(Action.SUGGEST_COMMAND, command)

        public fun changePage(page: Int): ClickEvent = ClickEvent(Action.CHANGE_PAGE, page.toString())

        public fun copyToClipboard(text: String): ClickEvent = ClickEvent(Action.COPY_TO_CLIPBOARD, text)
    }
}
