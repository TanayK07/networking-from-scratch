#ifndef NFS_RTO_H
#define NFS_RTO_H

/* ---------------------------------------------------------------
 * TCP Retransmission Timeout (RTO) calculator per RFC 6298.
 *
 * The RTO determines how long TCP waits before retransmitting a
 * segment. It is computed from smoothed RTT (SRTT) and RTT variance
 * (RTTVAR) measurements.
 *
 * Algorithm summary:
 *   First sample:
 *     SRTT   = R
 *     RTTVAR = R / 2
 *     RTO    = SRTT + max(G, K * RTTVAR)    where K=4, G=granularity
 *
 *   Subsequent samples:
 *     RTTVAR = (1 - beta) * RTTVAR + beta * |SRTT - R|   beta  = 1/4
 *     SRTT   = (1 - alpha) * SRTT  + alpha * R           alpha = 1/8
 *     RTO    = SRTT + max(G, K * RTTVAR)
 *
 *   Bounds:
 *     RTO_MIN = 1.0 second
 *     RTO_MAX = 60.0 seconds
 *
 *   Backoff:
 *     On retransmit timeout, RTO = min(RTO * 2, RTO_MAX)
 * --------------------------------------------------------------- */

#define NFS_RTO_MIN   1.0    /* seconds (RFC 6298 Section 2.4) */
#define NFS_RTO_MAX  60.0    /* seconds (RFC 6298 Section 2.5) */
#define NFS_RTO_K     4.0    /* variance multiplier */
#define NFS_RTO_ALPHA 0.125  /* 1/8 — SRTT smoothing factor */
#define NFS_RTO_BETA  0.25   /* 1/4 — RTTVAR smoothing factor */
#define NFS_RTO_G     0.0    /* clock granularity (0 = fine-grained) */

struct nfs_rto_state {
    double srtt;        /* smoothed RTT */
    double rttvar;      /* RTT variance */
    double rto;         /* current retransmission timeout */
    int    initialized; /* has at least one RTT measurement? */
};

/* Initialize the RTO state. RTO starts at 1.0 second (RFC 6298 2.1). */
void nfs_rto_init(struct nfs_rto_state *s);

/* Update SRTT/RTTVAR/RTO with a new RTT sample (in seconds).
 * Must not be called with Karn's algorithm exception (retransmitted
 * segments — the caller is responsible for filtering those out). */
void nfs_rto_update(struct nfs_rto_state *s, double rtt_sample);

/* Exponential backoff: double the RTO, clamp at RTO_MAX. */
void nfs_rto_backoff(struct nfs_rto_state *s);

/* Get current RTO value (seconds). */
double nfs_rto_get(const struct nfs_rto_state *s);

/* Get current smoothed RTT (seconds). Returns 0.0 if not initialized. */
double nfs_rto_srtt(const struct nfs_rto_state *s);

/* Get current RTT variance (seconds). Returns 0.0 if not initialized. */
double nfs_rto_rttvar(const struct nfs_rto_state *s);

#endif /* NFS_RTO_H */
