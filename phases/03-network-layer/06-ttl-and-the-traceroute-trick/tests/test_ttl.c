/*
 * test_ttl.c -- Tests for TTL manipulation and traceroute simulation
 */

#include "ttl.h"

#include <stdio.h>
#include <string.h>

/* ---- test macros ---------------------------------------------------- */

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

/* ---- helpers -------------------------------------------------------- */

static uint32_t ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | (uint32_t)d;
}

static struct nfs_ipv4_hdr make_hdr(uint8_t ttl) {
    struct nfs_ipv4_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.ver_ihl = 0x45; /* IPv4, IHL=5 (20 bytes) */
    hdr.total_len = 20;
    hdr.ttl = ttl;
    hdr.protocol = 17; /* UDP */
    return hdr;
}

/* ---- tests ---------------------------------------------------------- */

static void test_struct_size(void) {
    ASSERT_EQ(sizeof(struct nfs_ipv4_hdr), 20);
}

static void test_ttl_decrement_normal(void) {
    struct nfs_ipv4_hdr hdr = make_hdr(64);

    int result = nfs_ttl_decrement(&hdr);
    ASSERT_EQ(result, 63);
    ASSERT_EQ(hdr.ttl, 63);

    result = nfs_ttl_decrement(&hdr);
    ASSERT_EQ(result, 62);
    ASSERT_EQ(hdr.ttl, 62);
}

static void test_ttl_decrement_to_zero(void) {
    struct nfs_ipv4_hdr hdr = make_hdr(1);

    int result = nfs_ttl_decrement(&hdr);
    ASSERT_EQ(result, 0); /* Time Exceeded */
    ASSERT_EQ(hdr.ttl, 0);
}

static void test_ttl_decrement_already_zero(void) {
    struct nfs_ipv4_hdr hdr = make_hdr(0);

    int result = nfs_ttl_decrement(&hdr);
    ASSERT_EQ(result, -1); /* Cannot decrement */
    ASSERT_EQ(hdr.ttl, 0); /* Still 0 */
}

static void test_ttl_decrement_from_255(void) {
    struct nfs_ipv4_hdr hdr = make_hdr(255);

    int result = nfs_ttl_decrement(&hdr);
    ASSERT_EQ(result, 254);
    ASSERT_EQ(hdr.ttl, 254);
}

static void test_ttl_decrement_from_2(void) {
    struct nfs_ipv4_hdr hdr = make_hdr(2);

    int result = nfs_ttl_decrement(&hdr);
    ASSERT_EQ(result, 1);
    ASSERT_EQ(hdr.ttl, 1);

    result = nfs_ttl_decrement(&hdr);
    ASSERT_EQ(result, 0); /* Time Exceeded */
    ASSERT_EQ(hdr.ttl, 0);

    result = nfs_ttl_decrement(&hdr);
    ASSERT_EQ(result, -1); /* Already 0 */
}

static void test_ttl_set_get(void) {
    struct nfs_ipv4_hdr hdr = make_hdr(0);

    nfs_ttl_set(&hdr, 128);
    ASSERT_EQ(nfs_ttl_get(&hdr), 128);

    nfs_ttl_set(&hdr, 0);
    ASSERT_EQ(nfs_ttl_get(&hdr), 0);

    nfs_ttl_set(&hdr, 255);
    ASSERT_EQ(nfs_ttl_get(&hdr), 255);

    nfs_ttl_set(&hdr, 1);
    ASSERT_EQ(nfs_ttl_get(&hdr), 1);
}

static void test_traceroute_3_hops(void) {
    /*
     * Path: src -> R1 -> R2 -> R3 -> dst
     * With 3 intermediate routers, traceroute should produce 4 entries:
     *   hop 1: R1 (TTL=1, expires at R1)
     *   hop 2: R2 (TTL=2, expires at R2)
     *   hop 3: R3 (TTL=3, expires at R3)
     *   hop 4: dst (TTL=4, reaches destination)
     */
    uint32_t src = ip(192, 168, 1, 100);
    uint32_t dst = ip(8, 8, 8, 8);
    uint32_t hops[] = {
        ip(192, 168, 1, 1),
        ip(10, 0, 0, 1),
        ip(72, 14, 233, 1),
    };

    struct nfs_traceroute_hop results[16];
    int count = nfs_traceroute_sim(src, dst, hops, 3, results, 16);

    ASSERT_EQ(count, 4);

    /* Hop 1: first router */
    ASSERT_EQ(results[0].hop_num, 1);
    ASSERT_EQ(results[0].responder_ip, ip(192, 168, 1, 1));
    ASSERT_EQ(results[0].reached_dest, 0);

    /* Hop 2: second router */
    ASSERT_EQ(results[1].hop_num, 2);
    ASSERT_EQ(results[1].responder_ip, ip(10, 0, 0, 1));
    ASSERT_EQ(results[1].reached_dest, 0);

    /* Hop 3: third router */
    ASSERT_EQ(results[2].hop_num, 3);
    ASSERT_EQ(results[2].responder_ip, ip(72, 14, 233, 1));
    ASSERT_EQ(results[2].reached_dest, 0);

    /* Hop 4: destination */
    ASSERT_EQ(results[3].hop_num, 4);
    ASSERT_EQ(results[3].responder_ip, ip(8, 8, 8, 8));
    ASSERT_EQ(results[3].reached_dest, 1);
}

static void test_traceroute_1_hop(void) {
    /*
     * Path: src -> R1 -> dst
     * One intermediate router.
     */
    uint32_t src = ip(192, 168, 1, 100);
    uint32_t dst = ip(10, 0, 0, 5);
    uint32_t hops[] = {ip(192, 168, 1, 1)};

    struct nfs_traceroute_hop results[16];
    int count = nfs_traceroute_sim(src, dst, hops, 1, results, 16);

    ASSERT_EQ(count, 2);

    ASSERT_EQ(results[0].hop_num, 1);
    ASSERT_EQ(results[0].responder_ip, ip(192, 168, 1, 1));
    ASSERT_EQ(results[0].reached_dest, 0);

    ASSERT_EQ(results[1].hop_num, 2);
    ASSERT_EQ(results[1].responder_ip, ip(10, 0, 0, 5));
    ASSERT_EQ(results[1].reached_dest, 1);
}

static void test_traceroute_direct(void) {
    /*
     * Path: src -> dst (no intermediate hops)
     * Destination is directly connected.
     * TTL=1 probe: no routers to decrement, reaches dst immediately.
     */
    uint32_t src = ip(192, 168, 1, 100);
    uint32_t dst = ip(192, 168, 1, 1);

    struct nfs_traceroute_hop results[16];
    int count = nfs_traceroute_sim(src, dst, NULL, 0, results, 16);

    ASSERT_EQ(count, 1);
    ASSERT_EQ(results[0].hop_num, 1);
    ASSERT_EQ(results[0].responder_ip, ip(192, 168, 1, 1));
    ASSERT_EQ(results[0].reached_dest, 1);
}

static void test_traceroute_max_results_limit(void) {
    /*
     * With 5 hops but max_results=3, we should only get 3 entries.
     */
    uint32_t src = ip(10, 0, 0, 1);
    uint32_t dst = ip(10, 0, 0, 100);
    uint32_t hops[] = {
        ip(10, 0, 0, 2), ip(10, 0, 0, 3), ip(10, 0, 0, 4), ip(10, 0, 0, 5), ip(10, 0, 0, 6),
    };

    struct nfs_traceroute_hop results[3];
    int count = nfs_traceroute_sim(src, dst, hops, 5, results, 3);

    ASSERT_EQ(count, 3);
    /* All 3 should be intermediate (Time Exceeded) */
    ASSERT_EQ(results[0].reached_dest, 0);
    ASSERT_EQ(results[1].reached_dest, 0);
    ASSERT_EQ(results[2].reached_dest, 0);
}

static void test_ttl_decrement_chain(void) {
    /* Walk TTL all the way from 5 to 0 and beyond */
    struct nfs_ipv4_hdr hdr = make_hdr(5);

    ASSERT_EQ(nfs_ttl_decrement(&hdr), 4);
    ASSERT_EQ(nfs_ttl_decrement(&hdr), 3);
    ASSERT_EQ(nfs_ttl_decrement(&hdr), 2);
    ASSERT_EQ(nfs_ttl_decrement(&hdr), 1);
    ASSERT_EQ(nfs_ttl_decrement(&hdr), 0);  /* Time Exceeded */
    ASSERT_EQ(nfs_ttl_decrement(&hdr), -1); /* Already 0 */
    ASSERT_EQ(nfs_ttl_decrement(&hdr), -1); /* Still 0 */
}

static void test_ipv4_hdr_fields(void) {
    /* Verify the header struct can hold typical values correctly */
    struct nfs_ipv4_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));

    hdr.ver_ihl = 0x45;
    hdr.tos = 0x00;
    hdr.total_len = 60;
    hdr.id = 0x1234;
    hdr.flags_frag = 0x4000; /* Don't Fragment */
    hdr.ttl = 64;
    hdr.protocol = 6; /* TCP */
    hdr.checksum = 0xABCD;
    hdr.src_addr = ip(192, 168, 1, 100);
    hdr.dst_addr = ip(10, 0, 0, 1);

    ASSERT_EQ(hdr.ver_ihl, 0x45);
    ASSERT_EQ(hdr.tos, 0x00);
    ASSERT_EQ(hdr.total_len, 60);
    ASSERT_EQ(hdr.id, 0x1234);
    ASSERT_EQ(hdr.flags_frag, 0x4000);
    ASSERT_EQ(hdr.ttl, 64);
    ASSERT_EQ(hdr.protocol, 6);
    ASSERT_EQ(hdr.checksum, 0xABCD);
    ASSERT_EQ(hdr.src_addr, ip(192, 168, 1, 100));
    ASSERT_EQ(hdr.dst_addr, ip(10, 0, 0, 1));
}

static void test_traceroute_hop_numbers(void) {
    /* Verify hop numbers are sequential 1-based */
    uint32_t src = ip(10, 0, 0, 1);
    uint32_t dst = ip(10, 0, 0, 10);
    uint32_t hops[] = {
        ip(10, 0, 0, 2),
        ip(10, 0, 0, 3),
    };

    struct nfs_traceroute_hop results[16];
    int count = nfs_traceroute_sim(src, dst, hops, 2, results, 16);

    ASSERT_EQ(count, 3);
    ASSERT_EQ(results[0].hop_num, 1);
    ASSERT_EQ(results[1].hop_num, 2);
    ASSERT_EQ(results[2].hop_num, 3);
}

/* ---- main ----------------------------------------------------------- */

int main(void) {
    printf("Running TTL and traceroute tests...\n");

    test_struct_size();
    test_ttl_decrement_normal();
    test_ttl_decrement_to_zero();
    test_ttl_decrement_already_zero();
    test_ttl_decrement_from_255();
    test_ttl_decrement_from_2();
    test_ttl_set_get();
    test_traceroute_3_hops();
    test_traceroute_1_hop();
    test_traceroute_direct();
    test_traceroute_max_results_limit();
    test_ttl_decrement_chain();
    test_ipv4_hdr_fields();
    test_traceroute_hop_numbers();

    printf("%d / %d tests passed\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
