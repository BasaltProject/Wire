/* Basalt plugin ABI: the structured value encoding. Normative; the prose and the limit table are in ABI.md, "Values".
 *
 * Whatever does not fit a handful of scalars (a recipe, a list of effects, an attribute table, a form definition, an
 * event payload) crosses the boundary as ONE encoded value in a buffer: `(const char *ptr, int32_t len)` going in,
 * `(char *out, int32_t cap)` coming out with the usual "length the full answer needs" result. An import never takes
 * a struct pointer and never invents its own layout.
 *
 * A value is a tag byte followed by its payload. Integers that are lengths, counts or zigzag numbers are LEB128
 * (7 bits per byte, low group first, at most 10 bytes). A document is exactly one value with nothing after it.
 *
 *   BASALT_VALUE_NULL    0x00
 *   BASALT_VALUE_FALSE   0x01
 *   BASALT_VALUE_TRUE    0x02
 *   BASALT_VALUE_INT     0x03  zigzag LEB128 of an i64
 *   BASALT_VALUE_FLOAT   0x04  8 bytes, an IEEE 754 double, little-endian
 *   BASALT_VALUE_STRING  0x05  LEB128 byte length, then that many UTF-8 bytes
 *   BASALT_VALUE_BYTES   0x06  LEB128 byte length, then that many bytes
 *   BASALT_VALUE_LIST    0x07  LEB128 count, then `count` values
 *   BASALT_VALUE_MAP     0x08  LEB128 count, then `count` pairs of (LEB128 key length, key UTF-8 bytes, value);
 *                              keys are unique and keep the order they were written in
 */
#ifndef BASALT_VALUES_H
#define BASALT_VALUES_H

#define BASALT_VALUE_NULL 0x00
#define BASALT_VALUE_FALSE 0x01
#define BASALT_VALUE_TRUE 0x02
#define BASALT_VALUE_INT 0x03
#define BASALT_VALUE_FLOAT 0x04
#define BASALT_VALUE_STRING 0x05
#define BASALT_VALUE_BYTES 0x06
#define BASALT_VALUE_LIST 0x07
#define BASALT_VALUE_MAP 0x08

/* Limits both sides enforce; a document over any of them is BASALT_ERR_INVALID. */
#define BASALT_VALUE_MAX_BYTES (1 << 20)   /* the whole encoded document */
#define BASALT_VALUE_MAX_DEPTH 16          /* nesting of lists and maps, a scalar at the top is depth 0 */
#define BASALT_VALUE_MAX_NODES 65536       /* values in the document, containers included */
#define BASALT_VALUE_MAX_KEY_BYTES 256     /* one map key */
#define BASALT_VALUE_MAX_STRING_BYTES (1 << 20) /* one string or bytes payload, within the document limit */

#endif /* BASALT_VALUES_H */
