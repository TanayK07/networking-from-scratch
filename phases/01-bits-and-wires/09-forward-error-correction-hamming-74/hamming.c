#include "hamming.h"

/* ---------------------------------------------------------------
 * Hamming(7,4) implementation.
 *
 * We store the codeword in the lower 7 bits of a uint8_t:
 *   bit 6 = position 7 (d4)
 *   bit 5 = position 6 (d3)
 *   bit 4 = position 5 (d2)
 *   bit 3 = position 4 (p3)
 *   bit 2 = position 3 (d1)
 *   bit 1 = position 2 (p2)
 *   bit 0 = position 1 (p1)
 *
 * Data bits from the input nibble (d1=MSB, d4=LSB of nibble):
 *   d1 = nibble bit 3
 *   d2 = nibble bit 2
 *   d3 = nibble bit 1
 *   d4 = nibble bit 0
 * --------------------------------------------------------------- */

/* Helper: get bit at 1-indexed position from codeword stored in lower 7 bits.
 * Position 1 is bit 0, position 7 is bit 6. */
static inline int get_pos(uint8_t cw, int pos) {
    return (cw >> (pos - 1)) & 1;
}

/* Helper: set bit at 1-indexed position */
static inline uint8_t set_pos(uint8_t cw, int pos, int val) {
    if (val)
        return cw | (uint8_t)(1 << (pos - 1));
    else
        return cw & (uint8_t)~(1 << (pos - 1));
}

/* Helper: flip bit at 1-indexed position */
static inline uint8_t flip_pos(uint8_t cw, int pos) {
    return cw ^ (uint8_t)(1 << (pos - 1));
}

uint8_t nfs_hamming74_encode(uint8_t data) {
    /* Extract 4 data bits */
    int d1 = (data >> 3) & 1; /* nibble MSB → position 3 */
    int d2 = (data >> 2) & 1; /* → position 5 */
    int d3 = (data >> 1) & 1; /* → position 6 */
    int d4 = (data >> 0) & 1; /* → position 7 */

    /* Compute parity bits (even parity) */
    int p1 = d1 ^ d2 ^ d4; /* covers positions 1,3,5,7 */
    int p2 = d1 ^ d3 ^ d4; /* covers positions 2,3,6,7 */
    int p3 = d2 ^ d3 ^ d4; /* covers positions 4,5,6,7 */

    /* Assemble codeword */
    uint8_t cw = 0;
    cw = set_pos(cw, 1, p1);
    cw = set_pos(cw, 2, p2);
    cw = set_pos(cw, 3, d1);
    cw = set_pos(cw, 4, p3);
    cw = set_pos(cw, 5, d2);
    cw = set_pos(cw, 6, d3);
    cw = set_pos(cw, 7, d4);

    return cw;
}

uint8_t nfs_hamming74_syndrome(uint8_t codeword) {
    /* Syndrome bit s1: XOR of positions 1,3,5,7 */
    int s1 =
        get_pos(codeword, 1) ^ get_pos(codeword, 3) ^ get_pos(codeword, 5) ^ get_pos(codeword, 7);

    /* Syndrome bit s2: XOR of positions 2,3,6,7 */
    int s2 =
        get_pos(codeword, 2) ^ get_pos(codeword, 3) ^ get_pos(codeword, 6) ^ get_pos(codeword, 7);

    /* Syndrome bit s3: XOR of positions 4,5,6,7 */
    int s3 =
        get_pos(codeword, 4) ^ get_pos(codeword, 5) ^ get_pos(codeword, 6) ^ get_pos(codeword, 7);

    return (uint8_t)((s3 << 2) | (s2 << 1) | s1);
}

int nfs_hamming74_decode(uint8_t codeword, uint8_t *data_out) {
    if (!data_out)
        return -1;

    uint8_t syndrome = nfs_hamming74_syndrome(codeword);
    int corrected = 0;

    if (syndrome != 0) {
        /* Syndrome gives the 1-indexed position of the error */
        if (syndrome > 7)
            return -1; /* shouldn't happen with 7-bit codeword */
        codeword = flip_pos(codeword, syndrome);
        corrected = 1;
    }

    /* Extract data bits from corrected codeword */
    int d1 = get_pos(codeword, 3);
    int d2 = get_pos(codeword, 5);
    int d3 = get_pos(codeword, 6);
    int d4 = get_pos(codeword, 7);

    *data_out = (uint8_t)((d1 << 3) | (d2 << 2) | (d3 << 1) | d4);
    return corrected;
}

size_t nfs_hamming_encode_buf(const uint8_t *data, size_t len, uint8_t *out, size_t out_sz) {
    if (!data || !out)
        return 0;

    size_t needed = len * 2; /* 2 codewords per byte */
    if (out_sz < needed)
        return 0;

    size_t idx = 0;
    for (size_t i = 0; i < len; i++) {
        uint8_t hi = (data[i] >> 4) & 0x0F;
        uint8_t lo = data[i] & 0x0F;
        out[idx++] = nfs_hamming74_encode(hi);
        out[idx++] = nfs_hamming74_encode(lo);
    }
    return idx;
}

int nfs_hamming_decode_buf(const uint8_t *codes, size_t code_len, uint8_t *out, size_t out_sz) {
    if (!codes || !out)
        return -1;

    if (code_len % 2 != 0)
        return -1;

    size_t nbytes = code_len / 2;
    if (out_sz < nbytes)
        return -1;

    for (size_t i = 0; i < nbytes; i++) {
        uint8_t hi_data, lo_data;

        int r1 = nfs_hamming74_decode(codes[i * 2], &hi_data);
        if (r1 < 0)
            return -1;

        int r2 = nfs_hamming74_decode(codes[i * 2 + 1], &lo_data);
        if (r2 < 0)
            return -1;

        out[i] = (uint8_t)((hi_data << 4) | lo_data);
    }
    return (int)nbytes;
}
