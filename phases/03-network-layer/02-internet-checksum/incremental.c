/* RFC 1624 incremental checksum update.
 *
 * When a single 16-bit word `m` in a checksummed packet is replaced
 * with `m'`, you can update the checksum in O(1) instead of re-summing
 * the whole packet. NAT boxes do this on every translated flow.
 */

#include <stdint.h>

uint16_t internet_checksum_update(uint16_t old_check,
                                  uint16_t old_word,
                                  uint16_t new_word) {
    /* RFC 1624 eq. 3:  HC' = ~(~HC + ~m + m')
     * where ~ is one's-complement and addition is one's-complement
     * (i.e. with carry folding). */
    uint32_t s = (uint16_t)~old_check;
    s += (uint16_t)~old_word;
    s += new_word;
    while (s >> 16) {
        s = (s & 0xFFFF) + (s >> 16);
    }
    return (uint16_t)~s;
}

/* Update a checksum when an N-byte field changes. The old and new
 * fields must be the same length, and N must be even. */
uint16_t internet_checksum_update_n(uint16_t old_check,
                                    const void *old_data,
                                    const void *new_data,
                                    unsigned int n) {
    const uint8_t *o = (const uint8_t *)old_data;
    const uint8_t *n_ = (const uint8_t *)new_data;
    uint32_t s = (uint16_t)~old_check;
    for (unsigned int i = 0; i < n; i += 2) {
        uint16_t old_word = (uint16_t)((o[i] << 8) | o[i + 1]);
        uint16_t new_word = (uint16_t)((n_[i] << 8) | n_[i + 1]);
        s += (uint16_t)~old_word;
        s += new_word;
    }
    while (s >> 16) {
        s = (s & 0xFFFF) + (s >> 16);
    }
    return (uint16_t)~s;
}
