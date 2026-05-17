#include "encoding.h"

/* ---------------------------------------------------------------
 * Manchester encoding (IEEE 802.3 convention):
 *   bit 1 → symbols (0, 1)  — low-to-high transition
 *   bit 0 → symbols (1, 0)  — high-to-low transition
 *
 * Bits are processed MSB-first within each byte.
 * --------------------------------------------------------------- */

size_t nfs_manchester_encode(const uint8_t *data, size_t len, uint8_t *out, size_t out_sz) {
    if (!data || !out)
        return 0;

    size_t needed = len * 16; /* 2 symbols per bit, 8 bits per byte */
    if (out_sz < needed)
        return 0;

    size_t idx = 0;
    for (size_t i = 0; i < len; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            int b = (data[i] >> bit) & 1;
            if (b) {
                /* 1 → low-high */
                out[idx++] = 0;
                out[idx++] = 1;
            } else {
                /* 0 → high-low */
                out[idx++] = 1;
                out[idx++] = 0;
            }
        }
    }
    return idx;
}

int nfs_manchester_decode(const uint8_t *symbols, size_t sym_len, uint8_t *out, size_t out_sz) {
    if (!symbols || !out)
        return -1;

    /* Must be a multiple of 16 symbols (1 byte = 8 bits = 16 symbols) */
    if (sym_len % 16 != 0)
        return -1;

    size_t nbytes = sym_len / 16;
    if (out_sz < nbytes)
        return -1;

    size_t idx = 0;
    for (size_t i = 0; i < nbytes; i++) {
        uint8_t byte = 0;
        for (int bit = 7; bit >= 0; bit--) {
            uint8_t s0 = symbols[idx++];
            uint8_t s1 = symbols[idx++];

            if (s0 == 0 && s1 == 1) {
                /* low-high → 1 */
                byte |= (uint8_t)(1 << bit);
            } else if (s0 == 1 && s1 == 0) {
                /* high-low → 0 */
                /* bit is already 0 */
            } else {
                /* Invalid Manchester transition */
                return -1;
            }
        }
        out[i] = byte;
    }
    return (int)nbytes;
}

/* ---------------------------------------------------------------
 * NRZI encoding:
 *   1 → toggle signal level
 *   0 → keep same signal level
 *   Initial level = 0. Bits processed MSB-first.
 * --------------------------------------------------------------- */

size_t nfs_nrzi_encode(const uint8_t *data, size_t len, uint8_t *out, size_t out_sz) {
    if (!data || !out)
        return 0;

    size_t needed = len * 8;
    if (out_sz < needed)
        return 0;

    uint8_t level = 0;
    size_t idx = 0;
    for (size_t i = 0; i < len; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            int b = (data[i] >> bit) & 1;
            if (b) {
                level ^= 1; /* toggle on 1 */
            }
            out[idx++] = level;
        }
    }
    return idx;
}

int nfs_nrzi_decode(const uint8_t *symbols, size_t sym_len, uint8_t *out, size_t out_sz) {
    if (!symbols || !out)
        return -1;

    if (sym_len % 8 != 0)
        return -1;

    size_t nbytes = sym_len / 8;
    if (out_sz < nbytes)
        return -1;

    uint8_t prev_level = 0;
    size_t idx = 0;
    for (size_t i = 0; i < nbytes; i++) {
        uint8_t byte = 0;
        for (int bit = 7; bit >= 0; bit--) {
            uint8_t cur = symbols[idx++];
            if (cur != prev_level) {
                /* Transition → data bit 1 */
                byte |= (uint8_t)(1 << bit);
            }
            /* else no transition → data bit 0 */
            prev_level = cur;
        }
        out[i] = byte;
    }
    return (int)nbytes;
}

/* ---------------------------------------------------------------
 * 4B/5B lookup table (FDDI / 100BASE-TX standard).
 *
 * Nibble → 5-bit code:
 *   0x0 → 11110 (0x1E)    0x8 → 10010 (0x12)
 *   0x1 → 01001 (0x09)    0x9 → 10011 (0x13)
 *   0x2 → 10100 (0x14)    0xA → 10110 (0x16)
 *   0x3 → 10101 (0x15)    0xB → 10111 (0x17)
 *   0x4 → 01010 (0x0A)    0xC → 11010 (0x1A)
 *   0x5 → 01011 (0x0B)    0xD → 11011 (0x1B)
 *   0x6 → 01110 (0x0E)    0xE → 11100 (0x1C)
 *   0x7 → 01111 (0x0F)    0xF → 11101 (0x1D)
 * --------------------------------------------------------------- */

static const uint8_t encode_4b5b[16] = {
    0x1E, /* 0: 11110 */
    0x09, /* 1: 01001 */
    0x14, /* 2: 10100 */
    0x15, /* 3: 10101 */
    0x0A, /* 4: 01010 */
    0x0B, /* 5: 01011 */
    0x0E, /* 6: 01110 */
    0x0F, /* 7: 01111 */
    0x12, /* 8: 10010 */
    0x13, /* 9: 10011 */
    0x16, /* A: 10110 */
    0x17, /* B: 10111 */
    0x1A, /* C: 11010 */
    0x1B, /* D: 11011 */
    0x1C, /* E: 11100 */
    0x1D, /* F: 11101 */
};

/* Reverse lookup: 5-bit code → nibble. -1 means invalid/unused code. */
static const int8_t decode_4b5b[32] = {
    -1, -1, -1,  -1,  -1,  -1,  -1,  -1,  /* 00000..00111 */
    -1, 1,  4,   5,   -1,  -1,  6,   7,   /* 01000..01111 */
    -1, -1, 8,   9,   2,   3,   0xA, 0xB, /* 10000..10111 */
    -1, -1, 0xC, 0xD, 0xE, 0xF, 0,   -1   /* 11000..11111 */
};

int nfs_4b5b_encode(uint8_t nibble) {
    if (nibble > 0x0F)
        return -1;
    return encode_4b5b[nibble];
}

int nfs_4b5b_decode(uint8_t code) {
    if (code > 0x1F)
        return -1;
    return decode_4b5b[code];
}
