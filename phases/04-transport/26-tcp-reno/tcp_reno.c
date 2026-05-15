/* tcp_reno.c — TCP Reno congestion controller (RFC 5681). */

#include "tcp_reno.h"
#include <string.h>

/* ---- helpers ---- */

static uint32_t max_u32(uint32_t a, uint32_t b)
{
    return a > b ? a : b;
}

/* ---- public API ---- */

void nfs_reno_init(struct nfs_reno *r, uint32_t mss)
{
    memset(r, 0, sizeof(*r));
    r->mss      = mss;
    r->cwnd     = mss;
    r->ssthresh = 65535;
}

int nfs_reno_on_ack(struct nfs_reno *r, uint32_t bytes_acked)
{
    (void)bytes_acked;

    /* Exit recovery on new ACK if we're in recovery. */
    if (r->in_recovery) {
        nfs_reno_exit_recovery(r);
        return 0;
    }

    r->dup_ack_count = 0;

    if (r->cwnd < r->ssthresh) {
        /* Slow start: exponential growth — one mss per ACK. */
        r->cwnd += r->mss;
    } else {
        /* Congestion avoidance: approximately linear growth.
         * Increase by mss * mss / cwnd per ACK — over a full
         * window's worth of ACKs this adds roughly one mss. */
        uint32_t inc = (uint32_t)((uint64_t)r->mss * r->mss / r->cwnd);
        if (inc == 0) inc = 1;
        r->cwnd += inc;
    }

    return 0;
}

int nfs_reno_on_dup_ack(struct nfs_reno *r)
{
    r->dup_ack_count++;

    if (r->dup_ack_count == 3 && !r->in_recovery) {
        /* Fast retransmit / enter fast recovery. */
        r->ssthresh    = max_u32(r->cwnd / 2, 2 * r->mss);
        r->cwnd        = r->ssthresh + 3 * r->mss;
        r->in_recovery = 1;
        return 1;  /* signal: retransmit now */
    }

    if (r->dup_ack_count > 3 && r->in_recovery) {
        /* Inflate window during fast recovery. */
        r->cwnd += r->mss;
    }

    return 0;
}

void nfs_reno_on_timeout(struct nfs_reno *r)
{
    r->ssthresh    = max_u32(r->cwnd / 2, 2 * r->mss);
    r->cwnd        = r->mss;
    r->dup_ack_count = 0;
    r->in_recovery = 0;
}

void nfs_reno_exit_recovery(struct nfs_reno *r)
{
    r->cwnd        = r->ssthresh;
    r->dup_ack_count = 0;
    r->in_recovery = 0;
}

uint32_t nfs_reno_cwnd(const struct nfs_reno *r)
{
    return r->cwnd;
}

const char *nfs_reno_phase(const struct nfs_reno *r)
{
    if (r->in_recovery)
        return "fast_recovery";
    if (r->cwnd < r->ssthresh)
        return "slow_start";
    return "congestion_avoidance";
}
