#include "framing.h"
#include <string.h>

/* ---------------------------------------------------------------
 * Byte stuffing (PPP, RFC 1662)
 * --------------------------------------------------------------- */

int nfs_byte_stuff(const uint8_t *data, size_t len, uint8_t *out, size_t out_sz) {
    if (!out)
        return -1;

    size_t idx = 0;

    /* Leading flag */
    if (idx >= out_sz)
        return -1;
    out[idx++] = NFS_PPP_FLAG;

    /* Escaped payload */
    for (size_t i = 0; i < len; i++) {
        if (data[i] == NFS_PPP_FLAG || data[i] == NFS_PPP_ESCAPE) {
            if (idx + 2 > out_sz)
                return -1;
            out[idx++] = NFS_PPP_ESCAPE;
            out[idx++] = data[i] ^ NFS_PPP_XOR;
        } else {
            if (idx + 1 > out_sz)
                return -1;
            out[idx++] = data[i];
        }
    }

    /* Trailing flag */
    if (idx >= out_sz)
        return -1;
    out[idx++] = NFS_PPP_FLAG;

    return (int)idx;
}

int nfs_byte_unstuff(const uint8_t *frame, size_t frame_len, uint8_t *out, size_t out_sz) {
    if (!frame || !out)
        return -1;

    /* Minimum frame: flag + flag = 2 bytes (empty payload) */
    if (frame_len < 2)
        return -1;

    /* Must start and end with flags */
    if (frame[0] != NFS_PPP_FLAG || frame[frame_len - 1] != NFS_PPP_FLAG)
        return -1;

    size_t idx = 0;
    size_t i = 1;               /* skip leading flag */
    size_t end = frame_len - 1; /* stop before trailing flag */

    while (i < end) {
        if (frame[i] == NFS_PPP_FLAG) {
            /* Unexpected flag in middle of frame */
            return -1;
        } else if (frame[i] == NFS_PPP_ESCAPE) {
            i++;
            if (i >= end)
                return -1; /* Escape at end of payload */
            if (idx >= out_sz)
                return -1;
            out[idx++] = frame[i] ^ NFS_PPP_XOR;
        } else {
            if (idx >= out_sz)
                return -1;
            out[idx++] = frame[i];
        }
        i++;
    }

    return (int)idx;
}

/* ---------------------------------------------------------------
 * Bit stuffing (HDLC-style)
 *
 * Bits are packed MSB-first in each byte.
 * --------------------------------------------------------------- */

/* Helper: get bit n from a bit-packed buffer (MSB-first) */
static inline int get_bit(const uint8_t *data, size_t n) {
    return (data[n / 8] >> (7 - (n % 8))) & 1;
}

/* Helper: set bit n in a bit-packed buffer (MSB-first) */
static inline void set_bit(uint8_t *data, size_t n, int val) {
    size_t byte_idx = n / 8;
    int bit_idx = 7 - (int)(n % 8);
    if (val)
        data[byte_idx] |= (uint8_t)(1 << bit_idx);
    else
        data[byte_idx] &= (uint8_t) ~(1 << bit_idx);
}

int nfs_bit_stuff(const uint8_t *data, size_t nbits, uint8_t *out, size_t out_sz) {
    if (!data || !out)
        return -1;

    /* Worst case: every 5 bits we add 1, so output ≤ nbits * 6/5 + 1 */
    size_t max_out_bits = nbits + nbits / 5 + 1;
    if (out_sz < (max_out_bits + 7) / 8)
        return -1;

    memset(out, 0, out_sz);

    size_t out_idx = 0;
    int consecutive_ones = 0;

    for (size_t i = 0; i < nbits; i++) {
        int b = get_bit(data, i);
        set_bit(out, out_idx++, b);

        if (b == 1) {
            consecutive_ones++;
            if (consecutive_ones == 5) {
                /* Insert a stuffed 0 */
                set_bit(out, out_idx++, 0);
                consecutive_ones = 0;
            }
        } else {
            consecutive_ones = 0;
        }
    }

    return (int)out_idx;
}

int nfs_bit_unstuff(const uint8_t *data, size_t nbits, uint8_t *out, size_t out_sz) {
    if (!data || !out)
        return -1;

    memset(out, 0, out_sz);

    size_t out_idx = 0;
    int consecutive_ones = 0;

    for (size_t i = 0; i < nbits; i++) {
        int b = get_bit(data, i);

        if (consecutive_ones == 5) {
            /* This bit must be a stuffed 0 — skip it */
            if (b != 0)
                return -1; /* Invalid: 6+ consecutive 1s */
            consecutive_ones = 0;
            continue;
        }

        if ((out_idx + 7) / 8 >= out_sz && out_idx / 8 >= out_sz)
            return -1;

        set_bit(out, out_idx++, b);

        if (b == 1)
            consecutive_ones++;
        else
            consecutive_ones = 0;
    }

    return (int)out_idx;
}

/* ---------------------------------------------------------------
 * COBS (Consistent Overhead Byte Stuffing)
 *
 * Algorithm:
 *   Walk through data. Maintain a "code pointer" that says how
 *   far until the next zero. When we encounter a zero (or every
 *   254 bytes), we write the distance and start a new block.
 * --------------------------------------------------------------- */

int nfs_cobs_encode(const uint8_t *data, size_t len, uint8_t *out, size_t out_sz) {
    if (!data || !out)
        return -1;

    /* COBS output is at most len + 1 + len/254 bytes */
    if (out_sz < len + 1)
        return -1;

    size_t code_idx = 0;  /* where to write the distance byte */
    size_t write_idx = 1; /* next position to write data */
    uint8_t code = 1;     /* distance counter */

    for (size_t i = 0; i < len; i++) {
        if (data[i] == 0x00) {
            /* Write the distance at the code position */
            if (code_idx >= out_sz || write_idx > out_sz)
                return -1;
            out[code_idx] = code;
            code_idx = write_idx++;
            code = 1;
        } else {
            if (write_idx >= out_sz)
                return -1;
            out[write_idx++] = data[i];
            code++;
            if (code == 0xFF) {
                /* Block full at 254 bytes — emit and start new block */
                out[code_idx] = code;
                code_idx = write_idx++;
                code = 1;
            }
        }
    }

    /* Write final distance */
    if (code_idx >= out_sz)
        return -1;
    out[code_idx] = code;

    return (int)write_idx;
}

int nfs_cobs_decode(const uint8_t *data, size_t len, uint8_t *out, size_t out_sz) {
    if (!data || !out)
        return -1;

    if (len == 0)
        return 0;

    size_t read_idx = 0;
    size_t write_idx = 0;

    while (read_idx < len) {
        uint8_t code = data[read_idx++];
        if (code == 0)
            return -1; /* 0x00 should never appear in COBS data */

        for (uint8_t i = 1; i < code; i++) {
            if (read_idx >= len)
                return -1;
            if (write_idx >= out_sz)
                return -1;
            if (data[read_idx] == 0x00)
                return -1; /* 0x00 in data position is invalid COBS */
            out[write_idx++] = data[read_idx++];
        }

        /* If code < 0xFF and we're not at the end, insert a zero */
        if (code < 0xFF && read_idx < len) {
            if (write_idx >= out_sz)
                return -1;
            out[write_idx++] = 0x00;
        }
    }

    return (int)write_idx;
}
