package org.basaltmc.wire.multiversion

/**
 * One protocol version of Java Edition: the number a client sends in its handshake, and the
 * releases that send it.
 *
 * Several releases share a number whenever a release changed nothing on the wire (1.16.4 and
 * 1.16.5 are both 754), so [label] is a range rather than a single name.
 */
public data class ProtocolVersion(
    public val number: Int,
    public val label: String,
) {
    override fun toString(): String = "$label ($number)"
}

/**
 * Every Java protocol from 1.8 to the native version, and what a number that is not in the table
 * means.
 *
 * Only netty protocols are here. 1.7.x and earlier spoke a different framing altogether, and their
 * numbers collide with the modern ones (1.4.2 is also 47), so the floor of the table is where the
 * numbers start being comparable at all.
 */
public object ProtocolVersions {
    /** The version this build speaks without translating anything. */
    public const val NATIVE: Int = 776

    public const val V1_8: Int = 47
    public const val V1_9: Int = 107
    public const val V1_9_1: Int = 108
    public const val V1_9_2: Int = 109
    public const val V1_9_4: Int = 110
    public const val V1_10: Int = 210
    public const val V1_11: Int = 315
    public const val V1_11_1: Int = 316
    public const val V1_12: Int = 335
    public const val V1_12_1: Int = 338
    public const val V1_12_2: Int = 340
    public const val V1_13: Int = 393
    public const val V1_13_1: Int = 401
    public const val V1_13_2: Int = 404
    public const val V1_14: Int = 477
    public const val V1_14_1: Int = 480
    public const val V1_14_2: Int = 485
    public const val V1_14_3: Int = 490
    public const val V1_14_4: Int = 498
    public const val V1_15: Int = 573
    public const val V1_15_1: Int = 575
    public const val V1_15_2: Int = 578
    public const val V1_16: Int = 735
    public const val V1_16_1: Int = 736
    public const val V1_16_2: Int = 751
    public const val V1_16_3: Int = 753
    public const val V1_16_4: Int = 754
    public const val V1_17: Int = 755
    public const val V1_17_1: Int = 756

    public const val V1_18: Int = 757
    public const val V1_18_2: Int = 758
    public const val V1_19: Int = 759
    public const val V1_19_1: Int = 760
    public const val V1_19_3: Int = 761
    public const val V1_19_4: Int = 762
    public const val V1_20: Int = 763
    public const val V1_20_2: Int = 764
    public const val V1_20_3: Int = 765

    public const val V1_20_5: Int = 766
    public const val V1_21: Int = 767
    public const val V1_21_2: Int = 768
    public const val V1_21_4: Int = 769
    public const val V1_21_5: Int = 770
    public const val V1_21_6: Int = 771
    public const val V1_21_7: Int = 772
    public const val V1_21_9: Int = 773
    public const val V1_21_11: Int = 774
    public const val V26_1: Int = 775

    /**
     * A snapshot sets bit 30 of its protocol number, and what is left is a counter that has been
     * climbing since 15w32a rather than anything comparable to a release number.
     *
     * parity: `SharedConstants.SNAPSHOT_PROTOCOL_BIT`.
     */
    public const val SNAPSHOT_BIT: Int = 1 shl 30

    private val all: List<ProtocolVersion> =
        listOf(
            ProtocolVersion(47, "1.8-1.8.9"),
            ProtocolVersion(107, "1.9"),
            ProtocolVersion(108, "1.9.1"),
            ProtocolVersion(109, "1.9.2"),
            ProtocolVersion(110, "1.9.3-1.9.4"),
            ProtocolVersion(210, "1.10-1.10.2"),
            ProtocolVersion(315, "1.11"),
            ProtocolVersion(316, "1.11.1-1.11.2"),
            ProtocolVersion(335, "1.12"),
            ProtocolVersion(338, "1.12.1"),
            ProtocolVersion(340, "1.12.2"),
            ProtocolVersion(393, "1.13"),
            ProtocolVersion(401, "1.13.1"),
            ProtocolVersion(404, "1.13.2"),
            ProtocolVersion(477, "1.14"),
            ProtocolVersion(480, "1.14.1"),
            ProtocolVersion(485, "1.14.2"),
            ProtocolVersion(490, "1.14.3"),
            ProtocolVersion(498, "1.14.4"),
            ProtocolVersion(573, "1.15"),
            ProtocolVersion(575, "1.15.1"),
            ProtocolVersion(578, "1.15.2"),
            ProtocolVersion(735, "1.16"),
            ProtocolVersion(736, "1.16.1"),
            ProtocolVersion(751, "1.16.2"),
            ProtocolVersion(753, "1.16.3"),
            ProtocolVersion(754, "1.16.4-1.16.5"),
            ProtocolVersion(755, "1.17"),
            ProtocolVersion(756, "1.17.1"),
            ProtocolVersion(757, "1.18-1.18.1"),
            ProtocolVersion(758, "1.18.2"),
            ProtocolVersion(759, "1.19"),
            ProtocolVersion(760, "1.19.1-1.19.2"),
            ProtocolVersion(761, "1.19.3"),
            ProtocolVersion(762, "1.19.4"),
            ProtocolVersion(763, "1.20-1.20.1"),
            ProtocolVersion(764, "1.20.2"),
            ProtocolVersion(765, "1.20.3-1.20.4"),
            ProtocolVersion(766, "1.20.5-1.20.6"),
            ProtocolVersion(767, "1.21-1.21.1"),
            ProtocolVersion(768, "1.21.2-1.21.3"),
            ProtocolVersion(769, "1.21.4"),
            ProtocolVersion(770, "1.21.5"),
            ProtocolVersion(771, "1.21.6"),
            ProtocolVersion(772, "1.21.7-1.21.8"),
            ProtocolVersion(773, "1.21.9-1.21.10"),
            ProtocolVersion(774, "1.21.11"),
            ProtocolVersion(775, "26.1-26.1.2"),
            ProtocolVersion(776, "26.2"),
        )

    private val byNumber: Map<Int, ProtocolVersion> = all.associateBy { it.number }

    /** Oldest first, which is the order a chain of translators runs in. */
    public val known: List<ProtocolVersion> get() = all

    public val oldest: Int get() = all.first().number

    public fun of(number: Int): ProtocolVersion? = byNumber[number]

    /** What to call a number in a log line, whether or not the table knows it. */
    public fun describe(number: Int): String =
        when {
            isSnapshot(number) -> "snapshot ${number and SNAPSHOT_BIT.inv()} ($number)"
            else -> byNumber[number]?.toString() ?: "unknown ($number)"
        }

    public fun isSnapshot(number: Int): Boolean = number and SNAPSHOT_BIT != 0

    /** Whether [number] is a release newer than what this build speaks natively. */
    public fun isFuture(number: Int): Boolean = !isSnapshot(number) && number > NATIVE

    /** Older than the floor of the table: 1.7.x and the pre-netty protocols. */
    public fun isBelowFloor(number: Int): Boolean = !isSnapshot(number) && number < oldest
}
