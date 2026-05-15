#ifndef NFS_BBR_H
#define NFS_BBR_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP BBR (Bottleneck Bandwidth and Round-trip propagation time)
 *
 * BBR estimates two key parameters:
 *   - BtlBw (bottleneck bandwidth): max delivery rate over a window
 *   - RTprop (round-trip propagation time): min RTT over 10s window
 *
 * BDP = BtlBw * RTprop
 *
 * State machine:
 *   STARTUP  -> DRAIN -> PROBE_BW -> (occasionally) PROBE_RTT
 *
 * Pacing rate = BtlBw * pacing_gain
 * cwnd = BDP * cwnd_gain
 *
 * Pacing gains per state:
 *   STARTUP:   2.89 (= 2/ln(2))
 *   DRAIN:     1/2.89 (= ln(2)/2)
 *   PROBE_BW:  cycles through [1.25, 0.75, 1, 1, 1, 1, 1, 1]
 *   PROBE_RTT: 1.0
 * --------------------------------------------------------------- */

/* BBR states */
typedef enum {
    NFS_BBR_STARTUP   = 0,
    NFS_BBR_DRAIN     = 1,
    NFS_BBR_PROBE_BW  = 2,
    NFS_BBR_PROBE_RTT = 3,
} nfs_bbr_state_t;

/* Fixed-point pacing gains (x1000 for integer math) */
#define NFS_BBR_GAIN_STARTUP    2885   /* 2/ln(2) * 1000 ~= 2885 */
#define NFS_BBR_GAIN_DRAIN      347    /* ln(2)/2 * 1000 ~= 347 */
#define NFS_BBR_GAIN_PROBE_RTT  1000   /* 1.0 */

/* PROBE_BW cycle gains (x1000) -- 8 phases */
#define NFS_BBR_PROBE_BW_PHASES 8

/* Window filter size for bandwidth estimation */
#define NFS_BBR_BW_FILTER_LEN   10

/* RTprop window: 10 seconds (in microseconds) */
#define NFS_BBR_RTPROP_WINDOW_US  10000000ULL

/* Minimum cwnd in MSS units */
#define NFS_BBR_MIN_CWND  4

/* ---- Bandwidth filter (windowed max) ---- */

struct nfs_bbr_bw_filter {
    uint64_t samples[NFS_BBR_BW_FILTER_LEN]; /* delivery rate in bytes/sec */
    uint64_t timestamps[NFS_BBR_BW_FILTER_LEN]; /* time of sample (us) */
    size_t   count;
    size_t   head;
};

/* ---- BBR state ---- */

struct nfs_bbr {
    nfs_bbr_state_t state;

    /* Bandwidth estimation */
    struct nfs_bbr_bw_filter bw_filter;
    uint64_t btl_bw;              /* max bandwidth (bytes/sec) */

    /* RTT estimation */
    uint64_t rt_prop;             /* min RTT (microseconds) */
    uint64_t rt_prop_stamp;       /* timestamp of rt_prop measurement */

    /* Pacing */
    uint64_t pacing_rate;         /* bytes/sec */
    uint32_t cwnd;                /* congestion window (bytes) */
    uint16_t mss;                 /* maximum segment size */

    /* PROBE_BW cycle */
    uint8_t  probe_bw_phase;      /* 0-7 */

    /* Tracking */
    uint64_t delivered;           /* total bytes delivered */
    uint64_t next_send_time;      /* next allowed send time (us) */
    int      filled_pipe;         /* has STARTUP filled the pipe? */
    uint64_t full_bw;             /* bandwidth at which pipe was considered full */
    uint8_t  full_bw_count;       /* rounds without BW growth */

    /* PROBE_RTT tracking */
    int      probe_rtt_pending;   /* set when RTprop has expired */
    uint64_t probe_rtt_done_stamp; /* when to exit PROBE_RTT */
};

/* ---- API ---- */

/* Initialize BBR state. `mss` is max segment size in bytes. */
void nfs_bbr_init(struct nfs_bbr *bbr, uint16_t mss);

/* Process an ACK: update bandwidth and RTT estimates.
 * `delivered` = bytes newly acknowledged
 * `rtt_us` = measured RTT in microseconds
 * `now_us` = current timestamp in microseconds
 * `delivery_rate` = bytes/sec for this ACK's sample */
void nfs_bbr_on_ack(struct nfs_bbr *bbr,
                    uint64_t delivered,
                    uint64_t rtt_us,
                    uint64_t now_us,
                    uint64_t delivery_rate);

/* Get the current pacing gain (x1000). */
uint32_t nfs_bbr_pacing_gain(const struct nfs_bbr *bbr);

/* Get the current cwnd gain (x1000). */
uint32_t nfs_bbr_cwnd_gain(const struct nfs_bbr *bbr);

/* Compute BDP (bandwidth-delay product) in bytes. */
uint64_t nfs_bbr_bdp(const struct nfs_bbr *bbr);

/* Compute pacing rate in bytes/sec. */
uint64_t nfs_bbr_compute_pacing_rate(const struct nfs_bbr *bbr);

/* Compute cwnd in bytes. */
uint32_t nfs_bbr_compute_cwnd(const struct nfs_bbr *bbr);

/* Get the current BBR state as a string. */
const char *nfs_bbr_state_name(nfs_bbr_state_t state);

/* Get the PROBE_BW phase gain (x1000) for a given phase index. */
uint32_t nfs_bbr_probe_bw_gain(uint8_t phase);

#endif /* NFS_BBR_H */
