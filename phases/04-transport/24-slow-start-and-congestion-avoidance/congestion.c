#include "congestion.h"

void nfs_cong_init(struct nfs_cong_state *c, uint32_t mss) {
    c->mss = mss;
    c->cwnd = mss; /* start with 1 segment */
    c->ssthresh = 65535;
    c->phase = NFS_CONG_SLOW_START;
}

void nfs_cong_on_ack(struct nfs_cong_state *c) {
    if (c->phase == NFS_CONG_SLOW_START) {
        /* Exponential growth: cwnd += MSS per ACK.
         * Over one RTT with N segments ACKed, cwnd roughly doubles. */
        c->cwnd += c->mss;

        /* Check for phase transition */
        if (c->cwnd >= c->ssthresh) {
            c->phase = NFS_CONG_AVOIDANCE;
        }
    } else {
        /* Congestion avoidance: additive increase.
         * Increase cwnd by approximately MSS per RTT:
         *   cwnd += MSS * MSS / cwnd  (per ACK) */
        uint32_t inc = (c->mss * c->mss) / c->cwnd;
        if (inc == 0)
            inc = 1; /* ensure at least 1-byte increase per ACK */
        c->cwnd += inc;
    }
}

/* Helper: max of two uint32_t values. */
static uint32_t max_u32(uint32_t a, uint32_t b) {
    return (a > b) ? a : b;
}

void nfs_cong_on_loss(struct nfs_cong_state *c) {
    /* Timeout: multiplicative decrease */
    c->ssthresh = max_u32(c->cwnd / 2, 2 * c->mss);
    c->cwnd = c->mss; /* reset to 1 segment */
    c->phase = NFS_CONG_SLOW_START;
}

void nfs_cong_on_triple_dup_ack(struct nfs_cong_state *c) {
    /* Fast retransmit / fast recovery entry */
    c->ssthresh = max_u32(c->cwnd / 2, 2 * c->mss);
    c->cwnd = c->ssthresh + 3 * c->mss;
}

uint32_t nfs_cong_window(const struct nfs_cong_state *c) {
    return c->cwnd;
}

const char *nfs_cong_phase_str(int phase) {
    switch (phase) {
    case NFS_CONG_SLOW_START:
        return "slow_start";
    case NFS_CONG_AVOIDANCE:
        return "congestion_avoidance";
    default:
        return "unknown";
    }
}
