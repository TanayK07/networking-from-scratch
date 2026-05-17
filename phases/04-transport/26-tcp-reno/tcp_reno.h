#ifndef NFS_TCP_RENO_H
#define NFS_TCP_RENO_H

#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP Reno congestion controller.
 *
 * Combines slow start, congestion avoidance, fast retransmit,
 * and fast recovery — the classic congestion control algorithm
 * defined in RFC 5681.
 * --------------------------------------------------------------- */

struct nfs_reno {
    uint32_t cwnd;        /* congestion window (bytes)            */
    uint32_t ssthresh;    /* slow-start threshold                 */
    uint32_t mss;         /* maximum segment size                 */
    int dup_ack_count;    /* consecutive duplicate ACKs received  */
    int in_recovery;      /* 1 if currently in fast recovery      */
    uint32_t recover_seq; /* sequence number that exits recovery  */
};

/* Initialise a Reno controller: cwnd = mss, ssthresh = 65535. */
void nfs_reno_init(struct nfs_reno *r, uint32_t mss);

/* Called on each new ACK that advances the window.
 * Slow start: cwnd += mss per ACK.
 * Congestion avoidance: cwnd += mss*mss / cwnd per ACK.
 * Returns 0. */
int nfs_reno_on_ack(struct nfs_reno *r, uint32_t bytes_acked);

/* Called on each duplicate ACK.  At 3 dup ACKs: enter fast
 * retransmit / fast recovery.  During recovery, each additional
 * dup ACK inflates cwnd by mss.
 * Returns 1 when entering fast retransmit (dup_ack_count == 3),
 * 0 otherwise. */
int nfs_reno_on_dup_ack(struct nfs_reno *r);

/* Called on retransmission timeout.
 * ssthresh = max(cwnd/2, 2*mss), cwnd = mss, back to slow start. */
void nfs_reno_on_timeout(struct nfs_reno *r);

/* Exit fast recovery (called when the retransmitted segment is
 * ACK'd).  cwnd = ssthresh, dup_ack_count = 0, in_recovery = 0. */
void nfs_reno_exit_recovery(struct nfs_reno *r);

/* Return the current congestion window. */
uint32_t nfs_reno_cwnd(const struct nfs_reno *r);

/* Return a human-readable phase string:
 * "slow_start", "congestion_avoidance", or "fast_recovery". */
const char *nfs_reno_phase(const struct nfs_reno *r);

#endif /* NFS_TCP_RENO_H */
