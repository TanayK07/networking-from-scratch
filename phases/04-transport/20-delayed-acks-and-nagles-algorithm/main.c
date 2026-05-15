#include "nagle.h"
#include <stdio.h>

/* ---------------------------------------------------------------
 * Demo: Nagle's algorithm and delayed ACK interaction.
 * --------------------------------------------------------------- */

int main(void)
{
    printf("=== Nagle's Algorithm Demo ===\n\n");

    struct nfs_nagle nagle;
    nfs_nagle_init(&nagle, 1460);

    /* No unacked data: small segment goes out */
    printf("No unacked data, 10 bytes: can_send=%d (expected 1)\n",
           nfs_nagle_can_send(&nagle, 10));

    /* Send 10 bytes */
    nfs_nagle_sent(&nagle, 10);
    printf("After sending 10 bytes: snd_una=0, snd_nxt=10\n");

    /* Now there's unacked data: small segment blocked */
    printf("With unacked data, 5 bytes: can_send=%d (expected 0)\n",
           nfs_nagle_can_send(&nagle, 5));

    /* Full MSS always goes out */
    printf("With unacked data, 1460 bytes (MSS): can_send=%d (expected 1)\n",
           nfs_nagle_can_send(&nagle, 1460));

    /* ACK arrives */
    nfs_nagle_acked(&nagle, 10);
    printf("After ACK 10: snd_una=10, snd_nxt=10\n");
    printf("No unacked data, 5 bytes: can_send=%d (expected 1)\n",
           nfs_nagle_can_send(&nagle, 5));

    /* Disable Nagle (TCP_NODELAY) */
    nfs_nagle_disable(&nagle);
    nfs_nagle_sent(&nagle, 5);
    printf("\nNagle disabled, unacked data, 3 bytes: can_send=%d (expected 1)\n",
           nfs_nagle_can_send(&nagle, 3));

    printf("\n=== Delayed ACK Demo ===\n\n");

    struct nfs_delayed_ack dack;
    nfs_delayed_ack_init(&dack, 0.200);

    /* First segment: delayed */
    double now = 1.000;
    int ack_now = nfs_delayed_ack_receive(&dack, now);
    printf("[t=%.3f] Segment 1 received: ack_now=%d (expected 0)\n",
           now, ack_now);

    /* Check before deadline */
    now = 1.100;
    printf("[t=%.3f] Timer check: should_ack=%d (expected 0)\n",
           now, nfs_delayed_ack_check(&dack, now));

    /* Second segment: immediate ACK */
    now = 1.150;
    ack_now = nfs_delayed_ack_receive(&dack, now);
    printf("[t=%.3f] Segment 2 received: ack_now=%d (expected 1)\n",
           now, ack_now);

    nfs_delayed_ack_sent(&dack);
    printf("ACK sent, state reset.\n");

    /* Single segment, wait for timeout */
    now = 2.000;
    nfs_delayed_ack_receive(&dack, now);
    printf("\n[t=%.3f] Segment received, delayed\n", now);
    now = 2.200;
    printf("[t=%.3f] Timer check: should_ack=%d (expected 1)\n",
           now, nfs_delayed_ack_check(&dack, now));

    printf("\nDone.\n");
    return 0;
}
