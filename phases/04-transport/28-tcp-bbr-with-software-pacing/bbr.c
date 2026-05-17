/*
 * bbr.c -- TCP BBR congestion control implementation
 */
#include "bbr.h"
#include <string.h>

/* PROBE_BW cycle gains (x1000) */
static const uint32_t probe_bw_gains[NFS_BBR_PROBE_BW_PHASES] = {
    1250,                              /* up: probe for more bandwidth */
    750,                               /* down: drain queue */
    1000, 1000, 1000, 1000, 1000, 1000 /* cruise */
};

uint32_t nfs_bbr_probe_bw_gain(uint8_t phase) {
    if (phase >= NFS_BBR_PROBE_BW_PHASES)
        return 1000;
    return probe_bw_gains[phase];
}

/* ---- Bandwidth filter ---- */

static void bw_filter_init(struct nfs_bbr_bw_filter *f) {
    memset(f, 0, sizeof(*f));
}

static void bw_filter_add(struct nfs_bbr_bw_filter *f, uint64_t rate, uint64_t timestamp) {
    size_t idx = f->head;
    f->samples[idx] = rate;
    f->timestamps[idx] = timestamp;
    f->head = (f->head + 1) % NFS_BBR_BW_FILTER_LEN;
    if (f->count < NFS_BBR_BW_FILTER_LEN)
        f->count++;
}

static uint64_t bw_filter_max(const struct nfs_bbr_bw_filter *f) {
    uint64_t max = 0;
    for (size_t i = 0; i < f->count; i++) {
        if (f->samples[i] > max)
            max = f->samples[i];
    }
    return max;
}

/* ---- State machine transitions ---- */

static void bbr_check_startup_done(struct nfs_bbr *bbr) {
    if (bbr->state != NFS_BBR_STARTUP)
        return;

    /* Check if bandwidth has plateaued (not growing >25% per round) */
    if (bbr->btl_bw > 0 && bbr->full_bw > 0) {
        if (bbr->btl_bw <= bbr->full_bw + bbr->full_bw / 4) {
            bbr->full_bw_count++;
        } else {
            bbr->full_bw = bbr->btl_bw;
            bbr->full_bw_count = 0;
        }
    } else {
        bbr->full_bw = bbr->btl_bw;
    }

    if (bbr->full_bw_count >= 3) {
        bbr->filled_pipe = 1;
        bbr->state = NFS_BBR_DRAIN;
    }
}

static void bbr_check_drain_done(struct nfs_bbr *bbr, uint64_t inflight) {
    if (bbr->state != NFS_BBR_DRAIN)
        return;

    uint64_t bdp = nfs_bbr_bdp(bbr);
    if (inflight <= bdp || bdp == 0) {
        bbr->state = NFS_BBR_PROBE_BW;
        bbr->probe_bw_phase = 0;
    }
}

static void bbr_check_probe_rtt(struct nfs_bbr *bbr, uint64_t now_us) {
    if (bbr->state == NFS_BBR_PROBE_RTT)
        return;

    /* probe_rtt_pending is set by the caller (on_ack) before rt_prop
     * gets refreshed, so we just check the flag here. */
    if (bbr->probe_rtt_pending && bbr->state == NFS_BBR_PROBE_BW) {
        bbr->state = NFS_BBR_PROBE_RTT;
        bbr->probe_rtt_pending = 0;
        /* Stay in PROBE_RTT for at least 200ms */
        bbr->probe_rtt_done_stamp = now_us + 200000;
    }
}

/* ---- Public API ---- */

void nfs_bbr_init(struct nfs_bbr *bbr, uint16_t mss) {
    if (!bbr)
        return;
    memset(bbr, 0, sizeof(*bbr));
    bbr->state = NFS_BBR_STARTUP;
    bbr->mss = mss > 0 ? mss : 1460;
    bbr->cwnd = NFS_BBR_MIN_CWND * bbr->mss;
    bbr->rt_prop = UINT64_MAX;
    bw_filter_init(&bbr->bw_filter);
}

void nfs_bbr_on_ack(struct nfs_bbr *bbr, uint64_t delivered, uint64_t rtt_us, uint64_t now_us,
                    uint64_t delivery_rate) {
    if (!bbr)
        return;

    bbr->delivered += delivered;

    /* Check if RTprop window has expired (before updating rt_prop,
     * so the stale timestamp can trigger PROBE_RTT). */
    int rtprop_expired = (now_us > bbr->rt_prop_stamp + NFS_BBR_RTPROP_WINDOW_US);

    /* Update RTprop (minimum RTT) */
    if (rtt_us > 0 && (rtt_us < bbr->rt_prop || rtprop_expired)) {
        bbr->rt_prop = rtt_us;
        bbr->rt_prop_stamp = now_us;
    }

    /* Update bandwidth filter */
    if (delivery_rate > 0) {
        bw_filter_add(&bbr->bw_filter, delivery_rate, now_us);
        bbr->btl_bw = bw_filter_max(&bbr->bw_filter);
    }

    /* State transitions */
    bbr_check_startup_done(bbr);
    bbr_check_drain_done(bbr, bbr->cwnd); /* approximate inflight with cwnd */

    /* Use pre-computed expiry flag for PROBE_RTT check */
    if (rtprop_expired)
        bbr->probe_rtt_pending = 1;
    bbr_check_probe_rtt(bbr, now_us);

    /* Exit PROBE_RTT after timeout */
    if (bbr->state == NFS_BBR_PROBE_RTT && now_us >= bbr->probe_rtt_done_stamp) {
        bbr->state = NFS_BBR_PROBE_BW;
        bbr->probe_bw_phase = 0;
    }

    /* Advance PROBE_BW phase (simplified: advance each ACK) */
    if (bbr->state == NFS_BBR_PROBE_BW) {
        bbr->probe_bw_phase = (bbr->probe_bw_phase + 1) % NFS_BBR_PROBE_BW_PHASES;
    }

    /* Update pacing rate and cwnd */
    bbr->pacing_rate = nfs_bbr_compute_pacing_rate(bbr);
    bbr->cwnd = nfs_bbr_compute_cwnd(bbr);
}

uint32_t nfs_bbr_pacing_gain(const struct nfs_bbr *bbr) {
    if (!bbr)
        return 1000;
    switch (bbr->state) {
    case NFS_BBR_STARTUP:
        return NFS_BBR_GAIN_STARTUP;
    case NFS_BBR_DRAIN:
        return NFS_BBR_GAIN_DRAIN;
    case NFS_BBR_PROBE_BW:
        return nfs_bbr_probe_bw_gain(bbr->probe_bw_phase);
    case NFS_BBR_PROBE_RTT:
        return NFS_BBR_GAIN_PROBE_RTT;
    default:
        return 1000;
    }
}

uint32_t nfs_bbr_cwnd_gain(const struct nfs_bbr *bbr) {
    if (!bbr)
        return 1000;
    switch (bbr->state) {
    case NFS_BBR_STARTUP:
        return 2885; /* same as pacing in startup */
    case NFS_BBR_DRAIN:
        return 2885; /* keep cwnd high to avoid drops */
    case NFS_BBR_PROBE_BW:
        return 2000; /* 2x BDP */
    case NFS_BBR_PROBE_RTT:
        return 1000;
    default:
        return 1000;
    }
}

uint64_t nfs_bbr_bdp(const struct nfs_bbr *bbr) {
    if (!bbr || bbr->rt_prop == UINT64_MAX || bbr->btl_bw == 0)
        return 0;
    /* BDP = BtlBw (bytes/sec) * RTprop (us) / 1,000,000 */
    return bbr->btl_bw * bbr->rt_prop / 1000000ULL;
}

uint64_t nfs_bbr_compute_pacing_rate(const struct nfs_bbr *bbr) {
    if (!bbr || bbr->btl_bw == 0)
        return 0;
    uint32_t gain = nfs_bbr_pacing_gain(bbr);
    return bbr->btl_bw * gain / 1000;
}

uint32_t nfs_bbr_compute_cwnd(const struct nfs_bbr *bbr) {
    if (!bbr)
        return 0;

    uint64_t bdp = nfs_bbr_bdp(bbr);
    uint32_t gain = nfs_bbr_cwnd_gain(bbr);

    uint64_t target = bdp * gain / 1000;

    /* Minimum cwnd */
    uint32_t min_cwnd = NFS_BBR_MIN_CWND * (uint32_t)bbr->mss;
    if (target < min_cwnd)
        target = min_cwnd;

    /* Cap at uint32_t max */
    if (target > UINT32_MAX)
        target = UINT32_MAX;

    return (uint32_t)target;
}

const char *nfs_bbr_state_name(nfs_bbr_state_t state) {
    switch (state) {
    case NFS_BBR_STARTUP:
        return "STARTUP";
    case NFS_BBR_DRAIN:
        return "DRAIN";
    case NFS_BBR_PROBE_BW:
        return "PROBE_BW";
    case NFS_BBR_PROBE_RTT:
        return "PROBE_RTT";
    default:
        return "UNKNOWN";
    }
}
