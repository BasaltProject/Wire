package org.basaltmc.protocol.play

/** One of the six block faces. Ordinal order matches vanilla's own `Direction` enum exactly: both its varint id and its single-byte "3D data value" happen to equal the ordinal in this version. */
public enum class Direction {
    DOWN,
    UP,
    NORTH,
    SOUTH,
    WEST,
    EAST,
}

/** The unit step from a block towards its neighbour on this face. Mirrors `Direction.getUnitVec3i`. */
public val Direction.normalX: Int
    get() =
        when (this) {
            Direction.WEST -> -1
            Direction.EAST -> 1
            else -> 0
        }

public val Direction.normalY: Int
    get() =
        when (this) {
            Direction.DOWN -> -1
            Direction.UP -> 1
            else -> 0
        }

public val Direction.normalZ: Int
    get() =
        when (this) {
            Direction.NORTH -> -1
            Direction.SOUTH -> 1
            else -> 0
        }
