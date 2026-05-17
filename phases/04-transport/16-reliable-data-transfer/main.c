#include "rdt.h"
#include <stdio.h>
#include <string.h>

/* Demonstrate Stop-and-Wait and Go-Back-N reliable data transfer. */

static void demo_stop_and_wait(void) {
    printf("--- Stop-and-Wait (window=1) ---\n");
    struct nfs_rdt_sender sender;
    struct nfs_rdt_receiver receiver;
    nfs_rdt_sender_init(&sender, 1);
    nfs_rdt_receiver_init(&receiver);

    const char *msgs[] = {"Hello", "World", "RDT"};
    for (int i = 0; i < 3; i++) {
        const uint8_t *data = (const uint8_t *)msgs[i];
        int seq = nfs_rdt_send(&sender, data, strlen(msgs[i]));
        printf("  Sent seq=%d \"%s\"\n", seq, msgs[i]);

        /* Deliver to receiver */
        uint32_t ack;
        int rc = nfs_rdt_receive(&receiver, &sender.segments[sender.count - 1], &ack);
        printf("  Received: rc=%d, ACK=%u\n", rc, ack);

        /* Process ACK */
        int acked = nfs_rdt_ack(&sender, ack);
        printf("  ACKed %d segments, in_flight=%d\n\n", acked, nfs_rdt_in_flight(&sender));
    }

    printf("  Receiver got %zu bytes total\n\n", receiver.recv_len);
    nfs_rdt_sender_free(&sender);
}

static void demo_go_back_n(void) {
    printf("--- Go-Back-N (window=4) ---\n");
    struct nfs_rdt_sender sender;
    struct nfs_rdt_receiver receiver;
    nfs_rdt_sender_init(&sender, 4);
    nfs_rdt_receiver_init(&receiver);

    /* Send 4 segments */
    const char *msgs[] = {"seg0", "seg1", "seg2", "seg3"};
    for (int i = 0; i < 4; i++) {
        int seq = nfs_rdt_send(&sender, (const uint8_t *)msgs[i], strlen(msgs[i]));
        printf("  Sent seq=%d \"%s\"\n", seq, msgs[i]);
    }
    printf("  In flight: %d\n", nfs_rdt_in_flight(&sender));

    /* Simulate: only first 2 arrive */
    for (int i = 0; i < 2; i++) {
        uint32_t ack;
        nfs_rdt_receive(&receiver, &sender.segments[i], &ack);
        nfs_rdt_ack(&sender, ack);
    }
    printf("  After 2 ACKs, in_flight=%d\n", nfs_rdt_in_flight(&sender));

    /* Timeout: retransmit remaining */
    int retx = nfs_rdt_timeout(&sender);
    printf("  Timeout: retransmitted %d segments\n", retx);

    /* Now deliver remaining */
    for (uint32_t i = sender.base; i < sender.count; i++) {
        uint32_t ack;
        nfs_rdt_receive(&receiver, &sender.segments[i], &ack);
        nfs_rdt_ack(&sender, ack);
    }
    printf("  After retransmit + ACK, in_flight=%d\n", nfs_rdt_in_flight(&sender));
    printf("  Receiver got %zu bytes total\n\n", receiver.recv_len);

    nfs_rdt_sender_free(&sender);
}

int main(void) {
    printf("=== Reliable Data Transfer Demo ===\n\n");
    demo_stop_and_wait();
    demo_go_back_n();
    return 0;
}
