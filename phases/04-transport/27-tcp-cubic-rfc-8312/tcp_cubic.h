#ifndef NFS_TCP_CUBIC_H
#define NFS_TCP_CUBIC_H

#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP CUBIC congestion controller (RFC 8312).
 *
 * CUBIC is the default congestion control algorithm in Linux.
 * It uses a cubic function of elapsed time since the last loss
 * event to compute the congestion window:
 *
 *   W(t) = C * (t - K)^3 + Wmax
 *
 * where K = cbrt(Wmax * beta / C) and beta = 0.7.
 *
 * The cubic function gives a concave region (below Wmax) and a
 * convex region (above Wmax), providing both TCP-friendliness
 * and aggressive probing.
 * --------------------------------------------------------------- */

struct nfs_cubic {
    uint32_t cwnd;      /* congestion window (bytes)          */
    uint32_t ssthresh;  /* slow-start threshold               */
    uint32_t mss;       /* maximum segment size               */
    double wmax;        /* W_max: cwnd at last loss event     */
    double K;           /* time to reach Wmax from epoch      */
    double epoch_start; /* timestamp of last loss / epoch     */
    double last_time;   /* timestamp of last on_ack call      */
    double C;           /* CUBIC scaling constant (0.4)       */
    double beta;        /* multiplicative decrease factor     */
};

/* Initialise CUBIC: cwnd=mss, ssthresh=65535, C=0.4, beta=0.7. */
void nfs_cubic_init(struct nfs_cubic *c, uint32_t mss);

/* Called on each new ACK.  `now` is the current time in seconds.
 * In slow start: cwnd += mss per ACK (exponential).
 * In congestion avoidance: apply the CUBIC window function. */
void nfs_cubic_on_ack(struct nfs_cubic *c, double now);

/* Called on packet loss.  Record Wmax, reduce cwnd by beta,
 * start new CUBIC epoch.  `now` is the current time. */
void nfs_cubic_on_loss(struct nfs_cubic *c, double now);

/* Return the current congestion window. */
uint32_t nfs_cubic_cwnd(const struct nfs_cubic *c);

/* Return "slow_start" or "congestion_avoidance". */
const char *nfs_cubic_phase(const struct nfs_cubic *c);

#endif /* NFS_TCP_CUBIC_H */
