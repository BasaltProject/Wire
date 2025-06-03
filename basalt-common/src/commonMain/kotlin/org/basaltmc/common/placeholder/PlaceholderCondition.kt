package org.basaltmc.common.placeholder

/** Whether a condition held, or why it could not be decided. */
public sealed interface ConditionResult {
    public data object True : ConditionResult

    public data object False : ConditionResult

    /**
     * The condition could not be decided here: an unknown placeholder, one this situation cannot
     * answer, or a comparison of things that are not numbers.
     *
     * Kept apart from [False] because they mean opposite things to whoever wrote the file. A false
     * condition worked and said no; an unresolvable one never ran, and the operator needs telling.
     */
    public data class Unresolvable(
        val reason: String,
    ) : ConditionResult
}

/**
 * One line of an operator-written condition, e.g. `%basalt_player_protocol% < 735`. Nothing here
 * knows what a protocol version or a player is: it compares whatever the placeholders resolve to,
 * so a new thing to condition on is a registered placeholder, not a parser edit.
 *
 * Comparisons are numeric when both sides are numbers and case-insensitive text otherwise, and a
 * condition with no operator is true when it resolves to `true`.
 */
public class PlaceholderCondition(
    public val raw: String,
) {
    private val left: String
    private val operator: Operator?
    private val right: String

    init {
        val trimmed = raw.trim()
        val found = findOperator(trimmed)
        if (found == null) {
            left = trimmed
            operator = null
            right = ""
        } else {
            val (op, at) = found
            left = trimmed.substring(0, at).trim()
            operator = op
            right = trimmed.substring(at + op.token.length).trim()
        }
    }

    public fun evaluate(
        registry: PlaceholderRegistry,
        context: PlaceholderContext = PlaceholderContext.EMPTY,
    ): ConditionResult {
        var failure: ConditionResult.Unresolvable? = null
        val report = { name: String, value: PlaceholderValue ->
            if (failure == null) {
                failure =
                    ConditionResult.Unresolvable(
                        when {
                            value is PlaceholderValue.Unavailable && value.reason != null ->
                                "%$name% cannot be used here: ${value.reason}"
                            value is PlaceholderValue.Unknown -> "%$name% is not a placeholder this server knows"
                            else -> "%$name% cannot be answered here"
                        },
                    )
            }
            null
        }
        val leftText = PlaceholderText.substitute(left, registry, context, report)
        val rightText = PlaceholderText.substitute(right, registry, context, report)
        failure?.let { return it }

        if (operator == null) {
            return if (leftText.equals("true", ignoreCase = true)) ConditionResult.True else ConditionResult.False
        }
        return operator.apply(leftText, rightText)
    }

    override fun toString(): String = raw

    private enum class Operator(
        val token: String,
        val wordy: Boolean = false,
    ) {
        NOT_EQUAL("!="),
        LESS_OR_EQUAL("<="),
        GREATER_OR_EQUAL(">="),
        EQUAL_STRICT("=="),
        EQUAL("="),
        LESS("<"),
        GREATER(">"),
        MATCHES("matches", wordy = true),
        CONTAINS("contains", wordy = true),
        ;

        fun apply(
            left: String,
            right: String,
        ): ConditionResult {
            if (this == MATCHES) return globMatches(right, left).asResult()
            if (this == CONTAINS) return left.contains(right, ignoreCase = true).asResult()

            val leftNumber = left.toDoubleOrNull()
            val rightNumber = right.toDoubleOrNull()
            if (this == EQUAL || this == EQUAL_STRICT || this == NOT_EQUAL) {
                val equal =
                    if (leftNumber != null && rightNumber != null) {
                        leftNumber == rightNumber
                    } else {
                        left.equals(right, ignoreCase = true)
                    }
                return (if (this == NOT_EQUAL) !equal else equal).asResult()
            }
            if (leftNumber == null || rightNumber == null) {
                val offender = if (leftNumber == null) left else right
                return ConditionResult.Unresolvable("'$offender' is not a number, so '$token' cannot compare it")
            }
            return when (this) {
                LESS -> leftNumber < rightNumber
                LESS_OR_EQUAL -> leftNumber <= rightNumber
                GREATER -> leftNumber > rightNumber
                else -> leftNumber >= rightNumber
            }.asResult()
        }

        private fun Boolean.asResult(): ConditionResult = if (this) ConditionResult.True else ConditionResult.False
    }

    private companion object {
        /**
         * The first operator outside a `%...%`, longest token first.
         *
         * Outside, because a placeholder's argument is free text and can contain anything: the
         * separator in `%basalt_server_countdown_dd.MM.yyyy_01.01.2020%` must never be read as an
         * operator. Longest first, so `<=` is not read as `<` followed by a stray `=`.
         */
        fun findOperator(text: String): Pair<Operator, Int>? {
            var index = 0
            var inPlaceholder = false
            while (index < text.length) {
                val char = text[index]
                if (char == '%') {
                    inPlaceholder = !inPlaceholder
                    index++
                    continue
                }
                if (!inPlaceholder) {
                    for (operator in Operator.entries) {
                        if (!text.startsWith(operator.token, index)) continue
                        // A word operator has to be a word: "matches" inside "rematches" is not one.
                        if (operator.wordy && !isWord(text, index, operator.token.length)) continue
                        return operator to index
                    }
                }
                index++
            }
            return null
        }

        fun isWord(
            text: String,
            start: Int,
            length: Int,
        ): Boolean {
            val before = text.getOrNull(start - 1)
            val after = text.getOrNull(start + length)
            return (before == null || !before.isLetterOrDigit()) && (after == null || !after.isLetterOrDigit())
        }
    }
}
