/*
 * main.c -- Demo driver for Connection Tracking
 */

#include "conntrack.h"
#include <stdio.h>

int main(void) {
    struct nfs_conntrack ct;
    nfs_conntrack_init(&ct, 60);

    /* Simulate a TCP three-way handshake */
    time_t t = 1000;

    /* SYN: client -> server */
    struct nfs_ct_packet syn = {.src_ip = 0x0A000001,
                                .dst_ip = 0x0A000002,
                                .src_port = 12345,
                                .dst_port = 80,
                                .protocol = 6,
                                .tcp_flags = NFS_CT_TCP_SYN,
                                .length = 60};
    nfs_ct_state_t s = nfs_conntrack_process(&ct, &syn, t);
    printf("SYN:     %s (active=%d)\n", nfs_ct_state_name(s), nfs_conntrack_count(&ct));

    /* SYN+ACK: server -> client */
    struct nfs_ct_packet synack = {.src_ip = 0x0A000002,
                                   .dst_ip = 0x0A000001,
                                   .src_port = 80,
                                   .dst_port = 12345,
                                   .protocol = 6,
                                   .tcp_flags = NFS_CT_TCP_SYN | NFS_CT_TCP_ACK,
                                   .length = 60};
    s = nfs_conntrack_process(&ct, &synack, t + 1);
    printf("SYN+ACK: %s (active=%d)\n", nfs_ct_state_name(s), nfs_conntrack_count(&ct));

    /* ACK: client -> server */
    struct nfs_ct_packet ack = {.src_ip = 0x0A000001,
                                .dst_ip = 0x0A000002,
                                .src_port = 12345,
                                .dst_port = 80,
                                .protocol = 6,
                                .tcp_flags = NFS_CT_TCP_ACK,
                                .length = 52};
    s = nfs_conntrack_process(&ct, &ack, t + 2);
    printf("ACK:     %s (active=%d)\n", nfs_ct_state_name(s), nfs_conntrack_count(&ct));

    /* Expire after timeout */
    int n = nfs_conntrack_expire(&ct, t + 120);
    printf("\nExpired %d entries (active=%d)\n", n, nfs_conntrack_count(&ct));

    return 0;
}
