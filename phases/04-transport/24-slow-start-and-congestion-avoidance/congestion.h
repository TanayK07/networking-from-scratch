#ifndef NFS_CONGESTION_H
#define NFS_CONGESTION_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP Congestion Control — Reno-style Slow Start + Congestion
 * Avoidance (RFC 5681).
 *
 * Phases:
 *   Slow Start:  cwnd grows by MSS per ACK (exponential growth
 *                per RTT) until cwnd >= ssthresh.
 *   Congestion Avoidance:  cwnd grows by ~MSS per RTT (additive
 *                increase, via cwnd += MSS*MSS/cwnd per ACK).
 *
 * On timeout loss:  ssthresh = max(cwnd/2, 2*MSS), cwnd = MSS,
 *                   return to slow start.
 * On triple dup ACK:  fast retransmit/recovery entry:
 *                   ssthresh = max(cwnd/2, 2*MSS),
 *                   cwnd = ssthresh + 3*MSS.
 * --------------------------------------------------------------- */

#define NFS_CONG_SLOW_START  0
#define NFS_CONG_AVOIDANCE   1

struct nfs_cong_state {
    uint32_t cwnd;      /* congestion window (bytes) */
    uint32_t ssthresh;  /* slow start threshold (bytes) */
    uint32_t mss;       /* maximum segment size (bytes) */
    int      phase;     /* NFS_CONG_SLOW_START or NFS_CONG_AVOIDANCE */
};

/* Initialize: cwnd = MSS (1 segment), ssthresh = 65535.
 * Phase starts as slow start. */
void nfs_cong_init(struct nfs_cong_state *c, uint32_t mss);

/* Called for each ACK received.
 * Slow start: cwnd += mss.
 * Congestion avoidance: cwnd += mss * mss / cwnd.
 * Transitions from slow start to avoidance when cwnd >= ssthresh. */
void nfs_cong_on_ack(struct nfs_cong_state *c);

/* Called on timeout (loss detection).
 * Multiplicative decrease: ssthresh = max(cwnd/2, 2*mss),
 * cwnd = mss, phase = slow start. */
void nfs_cong_on_loss(struct nfs_cong_state *c);

/* Called on triple duplicate ACK (fast retransmit).
 * ssthresh = max(cwnd/2, 2*mss), cwnd = ssthresh + 3*mss. */
void nfs_cong_on_triple_dup_ack(struct nfs_cong_state *c);

/* Return the current congestion window in bytes. */
uint32_t nfs_cong_window(const struct nfs_cong_state *c);

/* Return a string name for the current phase. */
const char *nfs_cong_phase_str(int phase);

#endif /* NFS_CONGESTION_H */
