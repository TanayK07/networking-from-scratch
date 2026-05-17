#include "retx_queue.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Demo: simulate a sender pushing segments, receiving ACKs, and
 * handling retransmission timeouts.
 * --------------------------------------------------------------- */

int main(void) {
    struct nfs_retx_queue q;
    nfs_retx_queue_init(&q, 16);

    printf("=== Retransmission Queue Demo ===\n\n");

    /* Simulate sending 4 segments */
    const char *msgs[] = {"SYN", "Hello", "World", "FIN"};
    uint32_t seqs[] = {1000, 1003, 1008, 1013};
    double times[] = {0.0, 0.1, 0.2, 0.3};

    for (int i = 0; i < 4; i++) {
        size_t len = strlen(msgs[i]);
        nfs_retx_queue_push(&q, seqs[i], (const uint8_t *)msgs[i], len, times[i]);
        printf("[t=%.1f] Sent seg seq=%u len=%zu \"%s\"\n", times[i], seqs[i], len, msgs[i]);
    }
    printf("\nQueue size: %zu\n\n", nfs_retx_queue_size(&q));

    /* Simulate cumulative ACK for first 2 segments */
    printf("Received ACK 1008 (cumulative)\n");
    int removed = nfs_retx_queue_ack(&q, 1008);
    printf("Removed %d segments, queue size: %zu\n\n", removed, nfs_retx_queue_size(&q));

    /* Simulate timeout on remaining segments */
    double now = 1.5;
    double rto = 1.0;
    printf("Checking timeouts at t=%.1f (RTO=%.1f)\n", now, rto);
    int retx = nfs_retx_queue_timeout(&q, now, rto);
    printf("Timed out segments: %d\n", retx);

    struct nfs_retx_seg *seg = nfs_retx_queue_find(&q, 1008);
    if (seg) {
        printf("Seg seq=%u: retx_count=%d, new send_time=%.1f\n", seg->seq, seg->retx_count,
               seg->send_time);
    }

    /* ACK everything */
    printf("\nReceived ACK 1016 (all data)\n");
    removed = nfs_retx_queue_ack(&q, 1016);
    printf("Removed %d segments, empty=%d\n", removed, nfs_retx_queue_empty(&q));

    nfs_retx_queue_free(&q);
    printf("\nDone.\n");
    return 0;
}
