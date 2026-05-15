/* TCP Sequence Number Arithmetic (RFC 793, RFC 9293).
 *
 * TCP sequence numbers are unsigned 32-bit integers.  They wrap around
 * at 2^32.  The comparison rule from RFC 793:
 *
 *   "A new acknowledgment (called an "acceptable ack"), is one for which
 *    the inequality below holds:
 *      SND.UNA < SEG.ACK =< SND.NXT"
 *
 * Comparisons use modular arithmetic:  a < b iff (int32_t)(a - b) < 0.
 *
 * This works because casting the unsigned difference to a signed 32-bit
 * integer gives a negative result when a is "before" b in the circular
 * sequence space (within 2^31 distance).
 */

#include "seqnum.h"

/* ------------------------------------------------------------------ */
/*  Comparison functions                                               */
/* ------------------------------------------------------------------ */

int nfs_seq_lt(nfs_seq_t a, nfs_seq_t b) {
    return (int32_t)(a - b) < 0;
}

int nfs_seq_le(nfs_seq_t a, nfs_seq_t b) {
    return (int32_t)(a - b) <= 0;
}

int nfs_seq_gt(nfs_seq_t a, nfs_seq_t b) {
    return (int32_t)(a - b) > 0;
}

int nfs_seq_ge(nfs_seq_t a, nfs_seq_t b) {
    return (int32_t)(a - b) >= 0;
}

int nfs_seq_between(nfs_seq_t seq, nfs_seq_t lo, nfs_seq_t hi) {
    /* lo <= seq <= hi  in sequence space. */
    return nfs_seq_ge(seq, lo) && nfs_seq_le(seq, hi);
}

/* ------------------------------------------------------------------ */
/*  Arithmetic functions                                               */
/* ------------------------------------------------------------------ */

nfs_seq_t nfs_seq_add(nfs_seq_t seq, uint32_t delta) {
    /* Natural unsigned 32-bit addition wraps at 2^32. */
    return seq + delta;
}

int32_t nfs_seq_diff(nfs_seq_t a, nfs_seq_t b) {
    return (int32_t)(a - b);
}

int nfs_seq_in_window(nfs_seq_t seq, nfs_seq_t win_start, uint32_t win_size) {
    /* Half-open interval: [win_start, win_start + win_size).
     *
     * Equivalent to: win_start <= seq < win_start + win_size
     * Using: (uint32_t)(seq - win_start) < win_size
     *
     * This works because if seq is in the window, the unsigned
     * difference (seq - win_start) will be a small positive number
     * less than win_size.  If seq is outside the window, the
     * unsigned difference wraps to a large number >= win_size.
     */
    if (win_size == 0)
        return 0;
    return (uint32_t)(seq - win_start) < win_size;
}
