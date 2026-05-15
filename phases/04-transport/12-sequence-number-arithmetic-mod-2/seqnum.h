#ifndef NFS_SEQNUM_H
#define NFS_SEQNUM_H

#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP Sequence Number Arithmetic (RFC 793, RFC 9293).
 *
 * TCP sequence numbers are 32-bit unsigned integers that wrap around
 * at 2^32.  Comparison uses modular arithmetic:
 *
 *   a < b  iff  (int32_t)(a - b) < 0
 *
 * This means that 0xFFFFFFF0 < 0x00000010 is TRUE (the former is
 * "before" the latter in sequence space), even though as unsigned
 * integers the relationship is reversed.
 *
 * The "window" of valid comparisons is 2^31 in each direction.
 * --------------------------------------------------------------- */

typedef uint32_t nfs_seq_t;

/* ---------------------------------------------------------------
 * Comparison functions
 *
 * All comparisons use the modular arithmetic rule:
 *   a < b in sequence space  iff  (int32_t)(a - b) < 0
 * --------------------------------------------------------------- */

/* a < b in sequence space. */
int nfs_seq_lt(nfs_seq_t a, nfs_seq_t b);

/* a <= b in sequence space. */
int nfs_seq_le(nfs_seq_t a, nfs_seq_t b);

/* a > b in sequence space. */
int nfs_seq_gt(nfs_seq_t a, nfs_seq_t b);

/* a >= b in sequence space. */
int nfs_seq_ge(nfs_seq_t a, nfs_seq_t b);

/* lo <= seq <= hi in sequence space.
 * Returns 1 if seq is in the closed interval [lo, hi]. */
int nfs_seq_between(nfs_seq_t seq, nfs_seq_t lo, nfs_seq_t hi);

/* ---------------------------------------------------------------
 * Arithmetic functions
 * --------------------------------------------------------------- */

/* seq + delta (mod 2^32, natural unsigned wrap). */
nfs_seq_t nfs_seq_add(nfs_seq_t seq, uint32_t delta);

/* Signed difference a - b in sequence space.
 * Returns (int32_t)(a - b). */
int32_t nfs_seq_diff(nfs_seq_t a, nfs_seq_t b);

/* Is seq within [win_start, win_start + win_size)?
 * Returns 1 if seq is in the half-open window. */
int nfs_seq_in_window(nfs_seq_t seq, nfs_seq_t win_start, uint32_t win_size);

#endif /* NFS_SEQNUM_H */
