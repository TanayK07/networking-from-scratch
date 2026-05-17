/* tcp_cubic.c — TCP CUBIC congestion controller (RFC 8312). */

#include "tcp_cubic.h"
#include <math.h>
#include <string.h>

void nfs_cubic_init(struct nfs_cubic *c, uint32_t mss) {
    memset(c, 0, sizeof(*c));
    c->mss = mss;
    c->cwnd = mss;
    c->ssthresh = 65535;
    c->C = 0.4;
    c->beta = 0.7;
    c->epoch_start = -1.0; /* no epoch yet */
    c->wmax = 0.0;
    c->K = 0.0;
}

void nfs_cubic_on_ack(struct nfs_cubic *c, double now) {
    c->last_time = now;

    if (c->cwnd < c->ssthresh) {
        /* Slow start: exponential growth. */
        c->cwnd += c->mss;
        return;
    }

    /* Congestion avoidance — CUBIC window function. */

    /* If this is a fresh epoch, record epoch start time and
     * compute K = cbrt(Wmax * beta / C). */
    if (c->epoch_start < 0.0) {
        c->epoch_start = now;
        if (c->wmax > 0.0) {
            /* K = cbrt(Wmax * (1 - beta) / C)
             * This is the time it takes to grow back to Wmax. */
            c->K = cbrt(c->wmax * (1.0 - c->beta) / c->C);
        } else {
            c->K = 0.0;
        }
    }

    double t = now - c->epoch_start;
    double dt = t - c->K;

    /* W_cubic(t) = C * (t - K)^3 + Wmax */
    double w_cubic = c->C * dt * dt * dt + c->wmax;

    /* Convert to bytes (w_cubic is in units of MSS-sized segments
     * in the original RFC, but we keep everything in bytes for
     * simplicity — Wmax is already in bytes). */
    if (w_cubic < (double)c->mss)
        w_cubic = (double)c->mss;

    uint32_t target = (uint32_t)w_cubic;

    /* Only increase: compute increment per ACK. */
    if (target > c->cwnd) {
        /* Increase cwnd towards target.  We get one ACK per ~cwnd/mss
         * segments, so spread the increase across all ACKs in the
         * window.  Increment = (target - cwnd) / (cwnd / mss). */
        uint32_t segs = c->cwnd / c->mss;
        if (segs == 0)
            segs = 1;
        uint32_t inc = (target - c->cwnd) / segs;
        if (inc == 0)
            inc = 1;
        c->cwnd += inc;
    } else {
        /* TCP-friendliness: at minimum grow by 1 byte per ACK
         * to avoid complete stalls. */
        c->cwnd += 1;
    }
}

void nfs_cubic_on_loss(struct nfs_cubic *c, double now) {
    /* Record Wmax. */
    c->wmax = (double)c->cwnd;

    /* Multiplicative decrease. */
    uint32_t new_ssthresh = (uint32_t)((double)c->cwnd * c->beta);
    if (new_ssthresh < 2 * c->mss)
        new_ssthresh = 2 * c->mss;

    c->ssthresh = new_ssthresh;
    c->cwnd = new_ssthresh;
    c->epoch_start = now;

    /* Compute K for the new epoch. */
    c->K = cbrt(c->wmax * (1.0 - c->beta) / c->C);
}

uint32_t nfs_cubic_cwnd(const struct nfs_cubic *c) {
    return c->cwnd;
}

const char *nfs_cubic_phase(const struct nfs_cubic *c) {
    if (c->cwnd < c->ssthresh)
        return "slow_start";
    return "congestion_avoidance";
}
