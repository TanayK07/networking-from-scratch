#include "congestion.h"
#include <stdio.h>

/* Demonstrate TCP Reno-style congestion control:
 * slow start, congestion avoidance, loss, fast retransmit. */

int main(void) {
    struct nfs_cong_state c;
    uint32_t mss = 1460;

    nfs_cong_init(&c, mss);

    printf("=== TCP Congestion Control Demo (MSS=%u) ===\n\n", mss);

    /* Slow start phase: simulate ACKs until we reach ssthresh */
    printf("--- Slow Start ---\n");
    int rtt = 0;
    while (c.phase == NFS_CONG_SLOW_START && rtt < 20) {
        uint32_t segs = c.cwnd / c.mss;
        printf("  RTT %2d: cwnd=%6u (%u segs)  phase=%s\n", rtt, c.cwnd, segs,
               nfs_cong_phase_str(c.phase));
        /* Simulate one RTT: ACK for each segment in window */
        for (uint32_t i = 0; i < segs; i++) {
            nfs_cong_on_ack(&c);
            if (c.phase != NFS_CONG_SLOW_START)
                break;
        }
        rtt++;
    }

    /* Congestion avoidance phase */
    printf("\n--- Congestion Avoidance ---\n");
    for (int i = 0; i < 10; i++) {
        uint32_t segs = c.cwnd / c.mss;
        printf("  RTT %2d: cwnd=%6u (%u segs)  phase=%s\n", rtt, c.cwnd, segs,
               nfs_cong_phase_str(c.phase));
        for (uint32_t j = 0; j < segs; j++)
            nfs_cong_on_ack(&c);
        rtt++;
    }

    /* Simulate loss (timeout) */
    printf("\n--- Timeout Loss ---\n");
    printf("  Before: cwnd=%u ssthresh=%u\n", c.cwnd, c.ssthresh);
    nfs_cong_on_loss(&c);
    printf("  After:  cwnd=%u ssthresh=%u phase=%s\n", c.cwnd, c.ssthresh,
           nfs_cong_phase_str(c.phase));

    /* Recover through slow start */
    printf("\n--- Recovery (Slow Start) ---\n");
    for (int i = 0; i < 5; i++) {
        uint32_t segs = c.cwnd / c.mss;
        if (segs == 0)
            segs = 1;
        printf("  RTT %2d: cwnd=%6u (%u segs)  phase=%s\n", rtt, c.cwnd, segs,
               nfs_cong_phase_str(c.phase));
        for (uint32_t j = 0; j < segs; j++)
            nfs_cong_on_ack(&c);
        rtt++;
    }

    /* Triple duplicate ACK */
    printf("\n--- Triple Duplicate ACK ---\n");
    printf("  Before: cwnd=%u ssthresh=%u\n", c.cwnd, c.ssthresh);
    nfs_cong_on_triple_dup_ack(&c);
    printf("  After:  cwnd=%u ssthresh=%u\n", c.cwnd, c.ssthresh);

    return 0;
}
