#ifndef NFS_FAST_RECOVERY_H
#define NFS_FAST_RECOVERY_H

#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP Fast Retransmit & Fast Recovery (Reno).
 *
 * RFC 5681 describes the algorithm:
 *   - On receiving 3 duplicate ACKs, enter fast retransmit:
 *       ssthresh = cwnd / 2
 *       cwnd     = ssthresh + 3*MSS  (inflate for 3 dup ACKs in flight)
 *       Retransmit the lost segment.
 *   - Each additional duplicate ACK during recovery:
 *       cwnd += MSS  (inflate window)
 *   - When a new ACK arrives covering recovery_point:
 *       cwnd = ssthresh  (deflate)
 *       Exit recovery.
 * --------------------------------------------------------------- */

/* Return codes from nfs_fr_on_ack */
#define NFS_FR_NORMAL         0
#define NFS_FR_FAST_RETRANSMIT 1
#define NFS_FR_RECOVERY_EXIT   2

struct nfs_fr_state {
    uint32_t cwnd;           /* congestion window (bytes) */
    uint32_t ssthresh;       /* slow-start threshold */
    uint32_t mss;            /* maximum segment size */
    int      dup_ack_count;  /* consecutive duplicate ACKs */
    uint32_t last_ack;       /* last ACK number received */
    int      in_recovery;    /* 1 if in fast recovery */
    uint32_t recovery_point; /* snd_nxt at time of entering recovery */
};

/* Initialise fast-recovery state. */
void nfs_fr_init(struct nfs_fr_state *s, uint32_t mss,
                 uint32_t initial_cwnd);

/* Process an incoming ACK.
 * Returns:
 *   NFS_FR_NORMAL          (0) - normal processing
 *   NFS_FR_FAST_RETRANSMIT (1) - 3rd dup ACK, retransmit now
 *   NFS_FR_RECOVERY_EXIT   (2) - recovery complete
 */
int nfs_fr_on_ack(struct nfs_fr_state *s, uint32_t ack_num);

/* Return 1 if currently in fast recovery. */
int nfs_fr_in_recovery(const struct nfs_fr_state *s);

/* Return current congestion window. */
uint32_t nfs_fr_cwnd(const struct nfs_fr_state *s);

#endif /* NFS_FAST_RECOVERY_H */
