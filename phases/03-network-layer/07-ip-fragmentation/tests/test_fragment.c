/* Tests for IP fragmentation and reassembly (RFC 791).
 *
 * Test families:
 *   1. No fragmentation needed — packet fits in MTU
 *   2. Fragment a 4000-byte payload — 3 fragments at MTU=1500
 *   3. Verify MF flags and offsets
 *   4. Reassemble back to original
 *   5. DF flag rejects fragmentation
 *   6. Fragment offsets are multiples of 8
 *   7. Small packet (single byte payload)
 *   8. Out-of-order reassembly
 */

#include "fragment.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Minimal test harness                                               */
/* ------------------------------------------------------------------ */

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ(expr, expected)                                                                  \
    do {                                                                                           \
        tests_run++;                                                                               \
        long long _got = (long long)(expr);                                                        \
        long long _exp = (long long)(expected);                                                    \
        if (_got != _exp) {                                                                        \
            fprintf(stderr, "  FAIL %s:%d: %s == 0x%llx, want 0x%llx\n", __FILE__, __LINE__,       \
                    #expr, _got, _exp);                                                            \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

#define ASSERT_TRUE(expr)                                                                          \
    do {                                                                                           \
        tests_run++;                                                                               \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "  FAIL %s:%d: %s is false\n", __FILE__, __LINE__, #expr);             \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

#define RUN(fn)                                                                                    \
    do {                                                                                           \
        printf("  %-50s ", #fn);                                                                   \
        fn();                                                                                      \
        printf("OK\n");                                                                            \
    } while (0)

/* ------------------------------------------------------------------ */
/*  Helper: compute checksum for building test packets                 */
/* ------------------------------------------------------------------ */

static uint16_t test_checksum(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0;
    for (size_t i = 0; i + 1 < len; i += 2)
        sum += (uint32_t)((uint16_t)p[i] << 8 | p[i + 1]);
    if (len & 1)
        sum += (uint32_t)p[len - 1] << 8;
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return htons((uint16_t)~sum);
}

/* Build a test IP packet with the given payload size. */
static size_t build_test_packet(uint8_t *buf, size_t buf_sz, uint16_t payload_len, uint16_t id,
                                uint16_t flags_frag_host) {
    if (buf_sz < (size_t)(20 + payload_len))
        return 0;

    memset(buf, 0, 20 + payload_len);
    struct nfs_ipv4_hdr *h = (struct nfs_ipv4_hdr *)buf;
    h->ver_ihl = 0x45;
    h->tos = 0;
    h->total_len = htons((uint16_t)(20 + payload_len));
    h->id = htons(id);
    h->flags_frag = htons(flags_frag_host);
    h->ttl = 64;
    h->protocol = 17;                /* UDP */
    h->src_addr = htonl(0xC0A80001); /* 192.168.0.1 */
    h->dst_addr = htonl(0xC0A80002); /* 192.168.0.2 */
    h->checksum = 0;
    h->checksum = test_checksum(h, 20);

    /* Fill payload with pattern. */
    for (uint16_t i = 0; i < payload_len; i++)
        buf[20 + i] = (uint8_t)(i & 0xFF);

    return 20 + payload_len;
}

/* ------------------------------------------------------------------ */
/*  Tests                                                              */
/* ------------------------------------------------------------------ */

/* 1. Packet fits MTU — no fragmentation needed. */
static void test_no_fragmentation(void) {
    uint8_t pkt[1500];
    size_t len = build_test_packet(pkt, sizeof(pkt), 1000, 0x1234, 0);

    struct nfs_ip_fragment frags[4];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 4);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(frags[0].payload_len, 1000);
    ASSERT_TRUE(memcmp(frags[0].payload, pkt + 20, 1000) == 0);
}

/* 2. Fragment a 4000-byte payload at MTU=1500 — expect 3 fragments. */
static void test_fragment_4000(void) {
    uint8_t pkt[4020];
    size_t len = build_test_packet(pkt, sizeof(pkt), 4000, 0xABCD, 0);

    struct nfs_ip_fragment frags[10];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 10);
    ASSERT_EQ(n, 3);

    /* MTU=1500, header=20 => max_payload = (1480) & ~7 = 1480. */
    ASSERT_EQ(frags[0].payload_len, 1480);
    ASSERT_EQ(frags[1].payload_len, 1480);
    ASSERT_EQ(frags[2].payload_len, 1040); /* 4000 - 1480 - 1480 */
}

/* 3. Verify MF flags on the 3 fragments. */
static void test_fragment_mf_flags(void) {
    uint8_t pkt[4020];
    size_t len = build_test_packet(pkt, sizeof(pkt), 4000, 0x1111, 0);

    struct nfs_ip_fragment frags[10];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 10);
    ASSERT_EQ(n, 3);

    /* First two fragments: MF=1, last: MF=0. */
    uint16_t ff0 = ntohs(((struct nfs_ipv4_hdr *)frags[0].header)->flags_frag);
    uint16_t ff1 = ntohs(((struct nfs_ipv4_hdr *)frags[1].header)->flags_frag);
    uint16_t ff2 = ntohs(((struct nfs_ipv4_hdr *)frags[2].header)->flags_frag);

    ASSERT_TRUE((ff0 & NFS_IP_FLAG_MF) != 0);
    ASSERT_TRUE((ff1 & NFS_IP_FLAG_MF) != 0);
    ASSERT_TRUE((ff2 & NFS_IP_FLAG_MF) == 0);
}

/* 4. Verify fragment offsets. */
static void test_fragment_offsets(void) {
    uint8_t pkt[4020];
    size_t len = build_test_packet(pkt, sizeof(pkt), 4000, 0x2222, 0);

    struct nfs_ip_fragment frags[10];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 10);
    ASSERT_EQ(n, 3);

    uint16_t ff0 = ntohs(((struct nfs_ipv4_hdr *)frags[0].header)->flags_frag);
    uint16_t ff1 = ntohs(((struct nfs_ipv4_hdr *)frags[1].header)->flags_frag);
    uint16_t ff2 = ntohs(((struct nfs_ipv4_hdr *)frags[2].header)->flags_frag);

    /* Offsets in 8-byte units. */
    ASSERT_EQ(nfs_ip_get_frag_offset(ff0), 0);
    ASSERT_EQ(nfs_ip_get_frag_offset(ff1), 185); /* 1480/8 = 185 */
    ASSERT_EQ(nfs_ip_get_frag_offset(ff2), 370); /* 2960/8 = 370 */
}

/* 5. Fragment offsets are multiples of 8 (except last fragment size). */
static void test_fragment_alignment(void) {
    uint8_t pkt[4020];
    size_t len = build_test_packet(pkt, sizeof(pkt), 4000, 0x3333, 0);

    struct nfs_ip_fragment frags[10];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 10);
    ASSERT_EQ(n, 3);

    /* All non-last fragment payloads must be multiples of 8. */
    ASSERT_EQ(frags[0].payload_len % 8, 0);
    ASSERT_EQ(frags[1].payload_len % 8, 0);
    /* Last fragment need not be a multiple of 8 (but 1040 happens to be). */
}

/* 6. Reassemble fragments back to the original packet. */
static void test_reassemble_4000(void) {
    uint8_t pkt[4020];
    size_t len = build_test_packet(pkt, sizeof(pkt), 4000, 0x4444, 0);

    struct nfs_ip_fragment frags[10];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 10);
    ASSERT_EQ(n, 3);

    uint8_t reassembled[8192];
    int total = nfs_ip_reassemble(frags, (size_t)n, reassembled, sizeof(reassembled));
    ASSERT_EQ(total, 4020);

    /* Verify payload matches. */
    ASSERT_TRUE(memcmp(pkt + 20, reassembled + 20, 4000) == 0);

    /* Verify header fields. */
    struct nfs_ipv4_hdr *rh = (struct nfs_ipv4_hdr *)reassembled;
    ASSERT_EQ(ntohs(rh->total_len), 4020);
    ASSERT_EQ(ntohs(rh->id), 0x4444);
    ASSERT_EQ(ntohs(rh->flags_frag), 0); /* no flags, offset=0 */
}

/* 7. DF flag prevents fragmentation. */
static void test_df_rejects(void) {
    uint8_t pkt[4020];
    size_t len = build_test_packet(pkt, sizeof(pkt), 4000, 0x5555, NFS_IP_FLAG_DF);

    struct nfs_ip_fragment frags[10];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 10);
    ASSERT_EQ(n, -1);
}

/* 8. max_frags too small returns -2. */
static void test_max_frags_exceeded(void) {
    uint8_t pkt[4020];
    size_t len = build_test_packet(pkt, sizeof(pkt), 4000, 0x6666, 0);

    struct nfs_ip_fragment frags[2]; /* need 3, only have 2 */
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 2);
    ASSERT_EQ(n, -2);
}

/* 9. Single-byte payload — no fragmentation needed. */
static void test_single_byte(void) {
    uint8_t pkt[21];
    size_t len = build_test_packet(pkt, sizeof(pkt), 1, 0x7777, 0);

    struct nfs_ip_fragment frags[4];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 4);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(frags[0].payload_len, 1);
    ASSERT_EQ(frags[0].payload[0], 0);
}

/* 10. Reassemble out-of-order fragments. */
static void test_reassemble_out_of_order(void) {
    uint8_t pkt[4020];
    size_t len = build_test_packet(pkt, sizeof(pkt), 4000, 0x8888, 0);

    struct nfs_ip_fragment frags[10];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 10);
    ASSERT_EQ(n, 3);

    /* Shuffle: put them in reverse order. */
    struct nfs_ip_fragment shuffled[3];
    shuffled[0] = frags[2];
    shuffled[1] = frags[0];
    shuffled[2] = frags[1];

    uint8_t reassembled[8192];
    int total = nfs_ip_reassemble(shuffled, 3, reassembled, sizeof(reassembled));
    ASSERT_EQ(total, 4020);
    ASSERT_TRUE(memcmp(pkt + 20, reassembled + 20, 4000) == 0);
}

/* 11. nfs_ip_get_flags extracts the flag bits correctly. */
static void test_get_flags(void) {
    ASSERT_EQ(nfs_ip_get_flags(NFS_IP_FLAG_DF), NFS_IP_FLAG_DF);
    ASSERT_EQ(nfs_ip_get_flags(NFS_IP_FLAG_MF), NFS_IP_FLAG_MF);
    ASSERT_EQ(nfs_ip_get_flags(NFS_IP_FLAG_MF | 100), NFS_IP_FLAG_MF);
    ASSERT_EQ(nfs_ip_get_flags(0x0000), 0);
    ASSERT_EQ(nfs_ip_get_flags(NFS_IP_FLAG_DF | NFS_IP_FLAG_MF), NFS_IP_FLAG_DF | NFS_IP_FLAG_MF);
}

/* 12. nfs_ip_get_frag_offset extracts the offset correctly. */
static void test_get_frag_offset(void) {
    ASSERT_EQ(nfs_ip_get_frag_offset(0x0000), 0);
    ASSERT_EQ(nfs_ip_get_frag_offset(0x00FF), 0xFF);
    ASSERT_EQ(nfs_ip_get_frag_offset(NFS_IP_FLAG_MF | 185), 185);
    ASSERT_EQ(nfs_ip_get_frag_offset(NFS_IP_FRAG_OFFSET_MASK), NFS_IP_FRAG_OFFSET_MASK);
}

/* 13. nfs_ip_is_fragment detects fragments correctly. */
static void test_is_fragment(void) {
    ASSERT_TRUE(!nfs_ip_is_fragment(0));
    ASSERT_TRUE(!nfs_ip_is_fragment(NFS_IP_FLAG_DF));
    ASSERT_TRUE(nfs_ip_is_fragment(NFS_IP_FLAG_MF));
    ASSERT_TRUE(nfs_ip_is_fragment(100)); /* offset != 0 */
    ASSERT_TRUE(nfs_ip_is_fragment(NFS_IP_FLAG_MF | 100));
}

/* 14. Each fragment preserves the original ID. */
static void test_fragment_preserves_id(void) {
    uint8_t pkt[4020];
    size_t len = build_test_packet(pkt, sizeof(pkt), 4000, 0xBEEF, 0);

    struct nfs_ip_fragment frags[10];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 10);
    ASSERT_EQ(n, 3);

    for (int i = 0; i < n; i++) {
        struct nfs_ipv4_hdr *fh = (struct nfs_ipv4_hdr *)frags[i].header;
        ASSERT_EQ(ntohs(fh->id), 0xBEEF);
    }
}

/* 15. Each fragment preserves TTL and protocol. */
static void test_fragment_preserves_ttl_proto(void) {
    uint8_t pkt[4020];
    size_t len = build_test_packet(pkt, sizeof(pkt), 4000, 0xCAFE, 0);

    struct nfs_ip_fragment frags[10];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 10);
    ASSERT_EQ(n, 3);

    for (int i = 0; i < n; i++) {
        struct nfs_ipv4_hdr *fh = (struct nfs_ipv4_hdr *)frags[i].header;
        ASSERT_EQ(fh->ttl, 64);
        ASSERT_EQ(fh->protocol, 17);
    }
}

/* 16. Fragment total_len fields are correct. */
static void test_fragment_total_len(void) {
    uint8_t pkt[4020];
    size_t len = build_test_packet(pkt, sizeof(pkt), 4000, 0xDEAD, 0);

    struct nfs_ip_fragment frags[10];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 10);
    ASSERT_EQ(n, 3);

    struct nfs_ipv4_hdr *f0 = (struct nfs_ipv4_hdr *)frags[0].header;
    struct nfs_ipv4_hdr *f1 = (struct nfs_ipv4_hdr *)frags[1].header;
    struct nfs_ipv4_hdr *f2 = (struct nfs_ipv4_hdr *)frags[2].header;

    ASSERT_EQ(ntohs(f0->total_len), 1500); /* 20 + 1480 */
    ASSERT_EQ(ntohs(f1->total_len), 1500); /* 20 + 1480 */
    ASSERT_EQ(ntohs(f2->total_len), 1060); /* 20 + 1040 */
}

/* 17. Packet exactly at MTU — no fragmentation. */
static void test_exact_mtu(void) {
    uint8_t pkt[1500];
    size_t len = build_test_packet(pkt, sizeof(pkt), 1480, 0x0001, 0);

    struct nfs_ip_fragment frags[4];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 4);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(frags[0].payload_len, 1480);
}

/* 18. Packet one byte over MTU — must fragment. */
static void test_one_byte_over_mtu(void) {
    uint8_t pkt[1501];
    size_t len = build_test_packet(pkt, sizeof(pkt), 1481, 0x0002, 0);

    struct nfs_ip_fragment frags[4];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 4);
    ASSERT_EQ(n, 2);

    /* First: 1480 bytes, second: 1 byte. */
    ASSERT_EQ(frags[0].payload_len, 1480);
    ASSERT_EQ(frags[1].payload_len, 1);
}

/* 19. Reassembly with insufficient output buffer fails. */
static void test_reassemble_small_buffer(void) {
    uint8_t pkt[4020];
    size_t len = build_test_packet(pkt, sizeof(pkt), 4000, 0x9999, 0);

    struct nfs_ip_fragment frags[10];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 10);
    ASSERT_EQ(n, 3);

    uint8_t small[100]; /* Way too small for 4020 bytes. */
    int total = nfs_ip_reassemble(frags, (size_t)n, small, sizeof(small));
    ASSERT_EQ(total, -1);
}

/* 20. Fragment payload data integrity. */
static void test_fragment_payload_data(void) {
    uint8_t pkt[4020];
    size_t len = build_test_packet(pkt, sizeof(pkt), 4000, 0xAAAA, 0);

    struct nfs_ip_fragment frags[10];
    int n = nfs_ip_fragment_packet(pkt, len, 1500, frags, 10);
    ASSERT_EQ(n, 3);

    /* First fragment contains bytes 0..1479 of original payload. */
    ASSERT_TRUE(memcmp(frags[0].payload, pkt + 20, 1480) == 0);
    /* Second fragment: bytes 1480..2959. */
    ASSERT_TRUE(memcmp(frags[1].payload, pkt + 20 + 1480, 1480) == 0);
    /* Third fragment: bytes 2960..3999. */
    ASSERT_TRUE(memcmp(frags[2].payload, pkt + 20 + 2960, 1040) == 0);
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    printf("IP fragmentation test suite\n");

    RUN(test_no_fragmentation);
    RUN(test_fragment_4000);
    RUN(test_fragment_mf_flags);
    RUN(test_fragment_offsets);
    RUN(test_fragment_alignment);
    RUN(test_reassemble_4000);
    RUN(test_df_rejects);
    RUN(test_max_frags_exceeded);
    RUN(test_single_byte);
    RUN(test_reassemble_out_of_order);
    RUN(test_get_flags);
    RUN(test_get_frag_offset);
    RUN(test_is_fragment);
    RUN(test_fragment_preserves_id);
    RUN(test_fragment_preserves_ttl_proto);
    RUN(test_fragment_total_len);
    RUN(test_exact_mtu);
    RUN(test_one_byte_over_mtu);
    RUN(test_reassemble_small_buffer);
    RUN(test_fragment_payload_data);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        printf("FAIL\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
