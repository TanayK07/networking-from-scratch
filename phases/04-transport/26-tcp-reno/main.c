/* main.c — demonstrate TCP Reno congestion control phases. */

#include "tcp_reno.h"
#include <stdio.h>

int main(void) {
    struct nfs_reno r;
    uint32_t mss = 1460;

    nfs_reno_init(&r, mss);

    printf("=== TCP Reno Congestion Controller ===\n\n");

    /* --- Slow start phase --- */
    printf("--- Slow Start ---\n");
    for (int i = 0; i < 10; i++) {
        nfs_reno_on_ack(&r, mss);
        printf("  ACK %2d: cwnd = %6u  phase = %s\n", i + 1, nfs_reno_cwnd(&r), nfs_reno_phase(&r));
    }

    /* Force into congestion avoidance by setting ssthresh low. */
    printf("\n--- Congestion Avoidance ---\n");
    /* Keep going until we hit ssthresh (65535), then watch linear growth. */
    while (r.cwnd < r.ssthresh) {
        nfs_reno_on_ack(&r, mss);
    }
    printf("  Entered avoidance at cwnd = %u\n", nfs_reno_cwnd(&r));
    uint32_t start = nfs_reno_cwnd(&r);
    for (int i = 0; i < 5; i++) {
        nfs_reno_on_ack(&r, mss);
        printf("  ACK: cwnd = %6u (+%u)  phase = %s\n", nfs_reno_cwnd(&r),
               nfs_reno_cwnd(&r) - start, nfs_reno_phase(&r));
    }

    /* --- Triple dup ACK → fast retransmit/recovery --- */
    printf("\n--- Fast Retransmit / Recovery ---\n");
    for (int i = 1; i <= 5; i++) {
        int retx = nfs_reno_on_dup_ack(&r);
        printf("  Dup ACK %d: cwnd = %6u  phase = %-22s %s\n", i, nfs_reno_cwnd(&r),
               nfs_reno_phase(&r), retx ? "<-- RETRANSMIT" : "");
    }

    /* New ACK exits recovery. */
    printf("\n--- Exit Recovery ---\n");
    nfs_reno_on_ack(&r, mss);
    printf("  New ACK: cwnd = %6u  phase = %s\n", nfs_reno_cwnd(&r), nfs_reno_phase(&r));

    /* --- Timeout --- */
    printf("\n--- Timeout ---\n");
    nfs_reno_on_timeout(&r);
    printf("  After timeout: cwnd = %6u  ssthresh = %u  phase = %s\n", nfs_reno_cwnd(&r),
           r.ssthresh, nfs_reno_phase(&r));

    return 0;
}
