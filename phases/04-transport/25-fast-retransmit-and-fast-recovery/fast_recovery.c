#include "fast_recovery.h"

/* ---------------------------------------------------------------
 * TCP Reno fast retransmit / fast recovery.
 * --------------------------------------------------------------- */

void nfs_fr_init(struct nfs_fr_state *s, uint32_t mss, uint32_t initial_cwnd) {
    if (!s)
        return;
    s->cwnd = initial_cwnd;
    s->ssthresh = 65535; /* RFC 5681: arbitrary large value */
    s->mss = mss;
    s->dup_ack_count = 0;
    s->last_ack = 0;
    s->in_recovery = 0;
    s->recovery_point = 0;
}

int nfs_fr_on_ack(struct nfs_fr_state *s, uint32_t ack_num) {
    if (!s)
        return NFS_FR_NORMAL;

    /* ---- New ACK (ack > last_ack) ---- */
    if (ack_num > s->last_ack) {
        s->last_ack = ack_num;
        s->dup_ack_count = 0;

        if (s->in_recovery) {
            if (ack_num >= s->recovery_point) {
                /* Full ACK: exit fast recovery */
                s->cwnd = s->ssthresh;
                s->in_recovery = 0;
                return NFS_FR_RECOVERY_EXIT;
            }
            /* Partial ACK during recovery:
             * Deflate cwnd by the amount of new data acked,
             * then add back one MSS (RFC 5681 partial ACK handling).
             * Simplified: keep cwnd as-is for Reno baseline. */
        }
        return NFS_FR_NORMAL;
    }

    /* ---- Duplicate ACK (ack == last_ack) ---- */
    if (ack_num == s->last_ack) {
        s->dup_ack_count++;

        if (!s->in_recovery && s->dup_ack_count == 3) {
            /* Enter fast retransmit */
            s->ssthresh = s->cwnd / 2;
            if (s->ssthresh < 2 * s->mss)
                s->ssthresh = 2 * s->mss;
            s->cwnd = s->ssthresh + 3 * s->mss;
            s->in_recovery = 1;
            /* recovery_point should be set to snd_nxt by the caller.
             * We use last_ack + cwnd as approximation, but the
             * caller is expected to set recovery_point explicitly
             * after receiving the FAST_RETRANSMIT signal.
             * For testing, we set it to last_ack so any new ACK
             * above last_ack exits recovery -- caller should override. */
            s->recovery_point = s->last_ack;
            return NFS_FR_FAST_RETRANSMIT;
        }

        if (s->in_recovery) {
            /* Additional dup ACK during recovery: inflate cwnd */
            s->cwnd += s->mss;
        }
    }

    return NFS_FR_NORMAL;
}

int nfs_fr_in_recovery(const struct nfs_fr_state *s) {
    if (!s)
        return 0;
    return s->in_recovery;
}

uint32_t nfs_fr_cwnd(const struct nfs_fr_state *s) {
    if (!s)
        return 0;
    return s->cwnd;
}
