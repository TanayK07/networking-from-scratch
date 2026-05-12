/* Tests for the TCP three-way handshake implementation.
 *
 * Seven test families:
 *   1. Struct size pinning
 *   2. Parse a known SYN
 *   3. Full handshake in memory
 *   4. SYN has correct flags
 *   5. SYN-ACK acknowledgement number
 *   6. Final ACK acknowledgement number
 *   7. ISN randomness (property test)
 *
 * Compile + run:
 *     make
 *     ./test_tcp
 */

#include "../tcp.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int tests_run    = 0;
static int tests_passed = 0;

#define ASSERT_EQ(expr, expected) do {                                    \
    tests_run++;                                                          \
    long long _got = (long long)(expr);                                   \
    long long _exp = (long long)(expected);                               \
    if (_got != _exp) {                                                   \
        fprintf(stderr, "  FAIL %s:%d: %s == %lld, want %lld\n",         \
                __FILE__, __LINE__, #expr, _got, _exp);                   \
        return;                                                           \
    }                                                                     \
    tests_passed++;                                                       \
} while (0)

#define ASSERT_TRUE(expr) do {                                            \
    tests_run++;                                                          \
    if (!(expr)) {                                                        \
        fprintf(stderr, "  FAIL %s:%d: %s is false\n",                    \
                __FILE__, __LINE__, #expr);                               \
        return;                                                           \
    }                                                                     \
    tests_passed++;                                                       \
} while (0)

/* ---------------------------------------------------------------
 * Test 1: struct size must be exactly 20 bytes
 * --------------------------------------------------------------- */

static void test_struct_size(void) {
    printf("  struct size...");
    ASSERT_EQ(sizeof(struct nfs_tcp_hdr), 20);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 2: parse a known SYN byte sequence
 *
 * Hand-crafted SYN: src=49152, dst=80, seq=0x12345678,
 * ack=0, data_off=5, flags=SYN, window=65535, checksum=0, urg=0.
 * --------------------------------------------------------------- */

static void test_parse_known_syn(void) {
    printf("  parse known SYN...");

    uint8_t raw[20] = {
        0xC0, 0x00,             /* src_port = 49152 */
        0x00, 0x50,             /* dst_port = 80    */
        0x12, 0x34, 0x56, 0x78, /* seq_num          */
        0x00, 0x00, 0x00, 0x00, /* ack_num          */
        0x50,                   /* data_off=5, rsv=0 */
        0x02,                   /* flags = SYN       */
        0xFF, 0xFF,             /* window = 65535    */
        0x00, 0x00,             /* checksum          */
        0x00, 0x00              /* urgent_ptr        */
    };

    struct nfs_tcp_hdr hdr;
    ASSERT_EQ(nfs_tcp_parse(raw, sizeof(raw), &hdr), 0);
    ASSERT_EQ(hdr.src_port, 49152);
    ASSERT_EQ(hdr.dst_port, 80);
    ASSERT_EQ(hdr.seq_num, 0x12345678);
    ASSERT_EQ(hdr.ack_num, 0);
    ASSERT_EQ(hdr.flags, NFS_TCP_SYN);
    ASSERT_EQ(hdr.window, 65535);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: full handshake — run client+server, verify all seq/ack
 * --------------------------------------------------------------- */

static void test_full_handshake(void) {
    printf("  full handshake...");

    struct nfs_tcp_handshake client, server;
    nfs_tcp_handshake_init(&client, 49152, 80);
    nfs_tcp_handshake_init(&server, 80, 49152);

    uint8_t wire[64];
    struct nfs_tcp_hdr parsed;

    /* Step 1: Client builds SYN */
    size_t n = nfs_tcp_build_syn(&client, wire, sizeof(wire));
    ASSERT_TRUE(n == 20);
    ASSERT_EQ(client.state, NFS_TCP_SYN_SENT);

    ASSERT_EQ(nfs_tcp_parse(wire, n, &parsed), 0);
    uint32_t client_isn = parsed.seq_num;

    /* Step 2: Server receives SYN, builds SYN-ACK */
    n = nfs_tcp_build_synack(&server, &parsed, wire, sizeof(wire));
    ASSERT_TRUE(n == 20);
    ASSERT_EQ(server.state, NFS_TCP_SYN_RECEIVED);

    ASSERT_EQ(nfs_tcp_parse(wire, n, &parsed), 0);
    uint32_t server_isn = parsed.seq_num;

    /* SYN-ACK must acknowledge client's SYN */
    ASSERT_EQ(parsed.ack_num, client_isn + 1);

    /* Step 3: Client receives SYN-ACK, builds ACK */
    n = nfs_tcp_build_ack(&client, &parsed, wire, sizeof(wire));
    ASSERT_TRUE(n == 20);
    ASSERT_EQ(client.state, NFS_TCP_ESTABLISHED);

    ASSERT_EQ(nfs_tcp_parse(wire, n, &parsed), 0);
    /* Final ACK must acknowledge server's SYN */
    ASSERT_EQ(parsed.ack_num, server_isn + 1);
    /* Seq in final ACK = client_isn + 1 */
    ASSERT_EQ(parsed.seq_num, client_isn + 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: SYN has SYN flag set, ACK flag clear
 * --------------------------------------------------------------- */

static void test_syn_has_correct_flags(void) {
    printf("  SYN flags...");

    struct nfs_tcp_handshake ctx;
    nfs_tcp_handshake_init(&ctx, 12345, 80);

    uint8_t wire[64];
    size_t n = nfs_tcp_build_syn(&ctx, wire, sizeof(wire));
    ASSERT_TRUE(n > 0);

    struct nfs_tcp_hdr hdr;
    ASSERT_EQ(nfs_tcp_parse(wire, n, &hdr), 0);
    ASSERT_TRUE((hdr.flags & NFS_TCP_SYN) != 0);
    ASSERT_TRUE((hdr.flags & NFS_TCP_ACK) == 0);
    ASSERT_TRUE((hdr.flags & NFS_TCP_FIN) == 0);
    ASSERT_TRUE((hdr.flags & NFS_TCP_RST) == 0);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: SYN-ACK's ack_num == SYN's seq_num + 1
 * --------------------------------------------------------------- */

static void test_synack_ack_numbers(void) {
    printf("  SYN-ACK ack numbers...");

    struct nfs_tcp_handshake client, server;
    nfs_tcp_handshake_init(&client, 55555, 443);
    nfs_tcp_handshake_init(&server, 443, 55555);

    uint8_t wire[64];
    struct nfs_tcp_hdr syn_hdr, synack_hdr;

    size_t n = nfs_tcp_build_syn(&client, wire, sizeof(wire));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(nfs_tcp_parse(wire, n, &syn_hdr), 0);

    n = nfs_tcp_build_synack(&server, &syn_hdr, wire, sizeof(wire));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(nfs_tcp_parse(wire, n, &synack_hdr), 0);

    /* The critical invariant: SYN consumes one sequence number. */
    ASSERT_EQ(synack_hdr.ack_num, syn_hdr.seq_num + 1);

    /* SYN-ACK must have both SYN and ACK flags. */
    ASSERT_TRUE((synack_hdr.flags & NFS_TCP_SYN) != 0);
    ASSERT_TRUE((synack_hdr.flags & NFS_TCP_ACK) != 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: final ACK's ack_num == SYN-ACK's seq_num + 1
 * --------------------------------------------------------------- */

static void test_final_ack_numbers(void) {
    printf("  final ACK numbers...");

    struct nfs_tcp_handshake client, server;
    nfs_tcp_handshake_init(&client, 60000, 22);
    nfs_tcp_handshake_init(&server, 22, 60000);

    uint8_t wire[64];
    struct nfs_tcp_hdr syn_hdr, synack_hdr, ack_hdr;

    /* SYN */
    size_t n = nfs_tcp_build_syn(&client, wire, sizeof(wire));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(nfs_tcp_parse(wire, n, &syn_hdr), 0);

    /* SYN-ACK */
    n = nfs_tcp_build_synack(&server, &syn_hdr, wire, sizeof(wire));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(nfs_tcp_parse(wire, n, &synack_hdr), 0);

    /* ACK */
    n = nfs_tcp_build_ack(&client, &synack_hdr, wire, sizeof(wire));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(nfs_tcp_parse(wire, n, &ack_hdr), 0);

    /* Final ACK acknowledges the server's SYN. */
    ASSERT_EQ(ack_hdr.ack_num, synack_hdr.seq_num + 1);

    /* Final ACK's seq should be client ISN + 1. */
    ASSERT_EQ(ack_hdr.seq_num, syn_hdr.seq_num + 1);

    /* Only ACK flag set. */
    ASSERT_TRUE((ack_hdr.flags & NFS_TCP_ACK) != 0);
    ASSERT_TRUE((ack_hdr.flags & NFS_TCP_SYN) == 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: ISN randomness — 100 ISNs, no duplicates
 * --------------------------------------------------------------- */

static void test_isn_randomness(void) {
    printf("  ISN randomness (100 values)...");

    uint32_t isns[100];
    for (int i = 0; i < 100; i++) {
        isns[i] = nfs_tcp_generate_isn();
    }

    /* O(n^2) but n=100, so it's fine. */
    for (int i = 0; i < 100; i++) {
        for (int j = i + 1; j < 100; j++) {
            ASSERT_TRUE(isns[i] != isns[j]);
        }
    }

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: reject truncated input
 * --------------------------------------------------------------- */

static void test_reject_truncated(void) {
    printf("  reject truncated...");

    struct nfs_tcp_hdr hdr;
    uint8_t short_buf[10] = {0};
    ASSERT_EQ(nfs_tcp_parse(short_buf, sizeof(short_buf), &hdr), -1);
    ASSERT_EQ(nfs_tcp_parse(NULL, 0, &hdr), -1);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: checksum over pseudo-header produces non-zero value
 * --------------------------------------------------------------- */

static void test_checksum_pseudo(void) {
    printf("  pseudo-header checksum...");

    struct nfs_tcp_handshake ctx;
    nfs_tcp_handshake_init(&ctx, 49152, 80);

    uint8_t wire[64];
    size_t n = nfs_tcp_build_syn(&ctx, wire, sizeof(wire));
    ASSERT_TRUE(n > 0);

    uint8_t src_ip[4] = {10, 0, 0, 1};
    uint8_t dst_ip[4] = {10, 0, 0, 2};

    uint16_t cs = nfs_tcp_checksum_pseudo(src_ip, dst_ip, wire, n);
    /* The checksum should be non-zero for a valid segment. */
    ASSERT_TRUE(cs != 0);

    /* Write the checksum into the segment and re-verify: the sum
     * over pseudo-header + segment (with checksum filled in) should
     * fold to zero (actually 0x0000 after complement). */
    wire[16] = (uint8_t)(cs >> 8);
    wire[17] = (uint8_t)(cs & 0xFF);
    uint16_t verify = nfs_tcp_checksum_pseudo(src_ip, dst_ip, wire, n);
    ASSERT_EQ(verify, 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void) {
    srand((unsigned)time(NULL));

    printf("TCP three-way handshake test suite\n");
    test_struct_size();
    test_parse_known_syn();
    test_full_handshake();
    test_syn_has_correct_flags();
    test_synack_ack_numbers();
    test_final_ack_numbers();
    test_isn_randomness();
    test_reject_truncated();
    test_checksum_pseudo();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
