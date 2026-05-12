/* RFC 1071 Internet checksum.
 *
 * The algorithm: sum all 16-bit words, fold the carry back in,
 * take the one's-complement.
 *
 * If the buffer has odd length, pad with a zero byte on the right
 * (RFC 1071, §1).
 */

#include "checksum.h"

uint32_t internet_checksum_partial(const void *data, size_t len, uint32_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = seed;

    /* Sum 16 bits at a time. */
    while (len >= 2) {
        /* Wire byte order: high byte first. We accumulate in host order
         * and the result must be byte-swapped consistently across calls,
         * so we read big-endian here. */
        sum += ((uint32_t)p[0] << 8) | (uint32_t)p[1];
        p += 2;
        len -= 2;
    }

    /* Odd length: pad with a zero byte on the right. */
    if (len == 1) {
        sum += (uint32_t)p[0] << 8;
    }

    return sum;
}

uint16_t internet_checksum_fold(uint32_t partial) {
    /* Fold the upper 16 bits into the lower 16, repeating
     * until the upper half is zero. RFC 1071 §1. */
    while (partial >> 16) {
        partial = (partial & 0xFFFF) + (partial >> 16);
    }
    return (uint16_t)~partial;
}

uint16_t internet_checksum(const void *data, size_t len) {
    return internet_checksum_fold(internet_checksum_partial(data, len, 0));
}
