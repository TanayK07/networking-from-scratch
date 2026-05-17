/*
 * main.c -- TCP BBR demo driver
 */
#include "bbr.h"
#include <stdio.h>

int main(void) {
    struct nfs_bbr bbr;
    nfs_bbr_init(&bbr, 1460);

    printf("=== TCP BBR Congestion Control ===\n\n");
    printf("Initial state: %s\n", nfs_bbr_state_name(bbr.state));
    printf("MSS: %u bytes\n\n", bbr.mss);

    /* Simulate ACKs with increasing delivery rate (STARTUP phase) */
    printf("--- Simulating STARTUP phase ---\n");
    uint64_t now = 0;
    uint64_t rate = 100000; /* 100 KB/s initial */

    for (int i = 0; i < 20; i++) {
        now += 50000;        /* 50ms between ACKs */
        rate = rate * 3 / 2; /* exponential growth */

        nfs_bbr_on_ack(&bbr, 1460, 20000, now, rate);

        if (i % 5 == 0 || bbr.state != NFS_BBR_STARTUP) {
            printf("  ACK %2d: state=%-10s bw=%10llu B/s "
                   "rtt=%llu us  pacing=%llu B/s  cwnd=%u\n",
                   i, nfs_bbr_state_name(bbr.state), (unsigned long long)bbr.btl_bw,
                   (unsigned long long)bbr.rt_prop, (unsigned long long)bbr.pacing_rate, bbr.cwnd);
            if (bbr.state != NFS_BBR_STARTUP)
                break;
        }
    }

    /* Simulate steady state with constant rate */
    printf("\n--- Simulating PROBE_BW phase ---\n");
    for (int i = 0; i < 8; i++) {
        now += 50000;
        nfs_bbr_on_ack(&bbr, 1460, 20000, now, rate);
        printf("  Phase %d: gain=%u/1000  pacing=%llu B/s  cwnd=%u\n", bbr.probe_bw_phase,
               nfs_bbr_pacing_gain(&bbr), (unsigned long long)bbr.pacing_rate, bbr.cwnd);
    }

    printf("\nBDP: %llu bytes\n", (unsigned long long)nfs_bbr_bdp(&bbr));
    printf("BtlBw: %llu B/s\n", (unsigned long long)bbr.btl_bw);
    printf("RTprop: %llu us\n", (unsigned long long)bbr.rt_prop);

    return 0;
}
