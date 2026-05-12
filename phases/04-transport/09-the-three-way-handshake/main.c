/* Simulate a complete TCP three-way handshake in memory.
 *
 * No sockets, no kernel — just byte buffers passed between a "client"
 * and a "server" context.
 *
 *   $ ./handshake
 *   === TCP Three-Way Handshake Simulation ===
 *   [client] SYN  → seq=3291845102, ack=0, flags=SYN
 *   [server] SYN-ACK → seq=981273644, ack=3291845103, flags=SYN ACK
 *   [client] ACK  → seq=3291845103, ack=981273645, flags=ACK
 *   Connection ESTABLISHED.
 */

#include "tcp.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Pretty-print TCP flags. */
static void print_flags(uint8_t flags) {
    const char *names[] = {"FIN", "SYN", "RST", "PSH", "ACK", "URG", "ECE", "CWR"};
    int first = 1;
    for (int i = 0; i < 8; i++) {
        if (flags & (1 << i)) {
            printf("%s%s", first ? "" : " ", names[i]);
            first = 0;
        }
    }
}

static void print_step(const char *role, const char *label, const struct nfs_tcp_hdr *h) {
    printf("[%s] %-7s seq=%" PRIu32 ", ack=%" PRIu32 ", flags=", role, label, h->seq_num,
           h->ack_num);
    print_flags(h->flags);
    printf("\n");
}

int main(void) {
    srand((unsigned)time(NULL));

    printf("=== TCP Three-Way Handshake Simulation ===\n\n");

    /* --- Set up client and server contexts --- */

    struct nfs_tcp_handshake client, server;
    nfs_tcp_handshake_init(&client, 49152, 80);
    nfs_tcp_handshake_init(&server, 80, 49152);

    uint8_t wire[64];
    struct nfs_tcp_hdr parsed;

    /* --- Step 1: Client → SYN --- */

    size_t n = nfs_tcp_build_syn(&client, wire, sizeof(wire));
    if (n == 0) {
        fprintf(stderr, "build_syn failed\n");
        return 1;
    }

    if (nfs_tcp_parse(wire, n, &parsed) < 0) {
        fprintf(stderr, "parse SYN failed\n");
        return 1;
    }
    print_step("client", "SYN ->", &parsed);

    /* --- Step 2: Server receives SYN, sends SYN-ACK --- */

    struct nfs_tcp_hdr syn_parsed = parsed;
    n = nfs_tcp_build_synack(&server, &syn_parsed, wire, sizeof(wire));
    if (n == 0) {
        fprintf(stderr, "build_synack failed\n");
        return 1;
    }

    if (nfs_tcp_parse(wire, n, &parsed) < 0) {
        fprintf(stderr, "parse SYN-ACK failed\n");
        return 1;
    }
    print_step("server", "SYN-ACK", &parsed);

    /* --- Step 3: Client receives SYN-ACK, sends ACK --- */

    struct nfs_tcp_hdr synack_parsed = parsed;
    n = nfs_tcp_build_ack(&client, &synack_parsed, wire, sizeof(wire));
    if (n == 0) {
        fprintf(stderr, "build_ack failed\n");
        return 1;
    }

    if (nfs_tcp_parse(wire, n, &parsed) < 0) {
        fprintf(stderr, "parse ACK failed\n");
        return 1;
    }
    print_step("client", "ACK ->", &parsed);

    /* --- Verify --- */

    printf("\n");
    if (client.state == NFS_TCP_ESTABLISHED) {
        printf("Client: ESTABLISHED\n");
    } else {
        printf("Client: NOT established (state=%d)\n", client.state);
    }
    if (server.state == NFS_TCP_SYN_RECEIVED) {
        /* Server moves to ESTABLISHED only after receiving the final ACK.
         * In this simulation we don't feed the ACK back to the server,
         * so it stays in SYN_RECEIVED. In a real stack, the server's
         * receive path would advance it. */
        printf("Server: SYN_RECEIVED (awaiting ACK delivery)\n");
    }

    /* --- Demonstrate checksum --- */

    printf("\n--- Checksum demo ---\n");
    uint8_t src_ip[4] = {192, 168, 1, 100};
    uint8_t dst_ip[4] = {93, 184, 216, 34};

    /* Rebuild the SYN for checksum demo. */
    nfs_tcp_handshake_init(&client, 49152, 80);
    n = nfs_tcp_build_syn(&client, wire, sizeof(wire));
    uint16_t cs = nfs_tcp_checksum_pseudo(src_ip, dst_ip, wire, n);
    printf("TCP checksum over SYN segment: 0x%04x\n", cs);

    return 0;
}
