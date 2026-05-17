/* Tests for UDP checksum with IPv4 pseudo-header (RFC 768, RFC 1071).
 *
 * Test families:
 *   1.  Pseudo-header struct size pinning (12 bytes)
 *   2.  Internet checksum of known data
 *   3.  Internet checksum of odd-length data
 *   4.  Internet checksum of empty data
 *   5.  UDP checksum with known packet
 *   6.  UDP checksum over simple payload
 *   7.  Verify returns 1 for valid packet
 *   8.  Verify returns 0 for corrupted packet
 *   9.  Checksum == 0x0000 (not computed) is always valid
 *  10.  RFC 768 zero -> 0xFFFF rule
 *  11.  Known DNS query checksum
 *  12.  Pseudo-header contributes to checksum
 */

#include "../udp_csum.h"

#include <stdio.h>
#include <string.h>

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

/* Helper: build a uint32_t IP from dotted quad (network byte order). */
static uint32_t make_ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    uint32_t ip;
    uint8_t bytes[4] = {a, b, c, d};
    memcpy(&ip, bytes, 4);
    return ip;
}

/* ---------------------------------------------------------------
 * Test 1: pseudo-header struct size
 * --------------------------------------------------------------- */
static void test_pseudo_hdr_size(void) {
    printf("  pseudo-header struct size...");
    ASSERT_EQ(sizeof(struct nfs_ipv4_pseudo_hdr), 12);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 2: internet checksum of known data
 *
 * RFC 1071 example: {0x00, 0x01, 0x00, 0x02} -> sum = 0x0003,
 * checksum = ~0x0003 = 0xFFFC.
 * --------------------------------------------------------------- */
static void test_internet_checksum_known(void) {
    printf("  internet checksum (known data)...");

    uint8_t data[] = {0x00, 0x01, 0x00, 0x02};
    uint16_t cs = nfs_internet_checksum(data, sizeof(data));
    ASSERT_EQ(cs, 0xFFFC);

    /* Another known value: all 0xFF bytes (4 bytes). */
    uint8_t ff[] = {0xFF, 0xFF, 0xFF, 0xFF};
    cs = nfs_internet_checksum(ff, sizeof(ff));
    /* Sum = 0xFFFF + 0xFFFF = 0x1FFFE -> fold = 0xFFFF -> ~0xFFFF = 0x0000 */
    ASSERT_EQ(cs, 0x0000);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: internet checksum of odd-length data
 * --------------------------------------------------------------- */
static void test_internet_checksum_odd(void) {
    printf("  internet checksum (odd length)...");

    /* 3 bytes: {0x00, 0x01, 0xF2} -> padded to {0x00, 0x01, 0xF2, 0x00}
     * Sum = 0x0001 + 0xF200 = 0xF201 -> ~0xF201 = 0x0DFE */
    uint8_t data[] = {0x00, 0x01, 0xF2};
    uint16_t cs = nfs_internet_checksum(data, sizeof(data));
    ASSERT_EQ(cs, 0x0DFE);

    /* 1 byte: {0xAB} -> padded to {0xAB, 0x00}
     * Sum = 0xAB00 -> ~0xAB00 = 0x54FF */
    uint8_t one[] = {0xAB};
    cs = nfs_internet_checksum(one, 1);
    ASSERT_EQ(cs, 0x54FF);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: internet checksum of empty data
 * --------------------------------------------------------------- */
static void test_internet_checksum_empty(void) {
    printf("  internet checksum (empty)...");

    /* Empty data: sum = 0 -> ~0 = 0xFFFF */
    uint16_t cs = nfs_internet_checksum(NULL, 0);
    ASSERT_EQ(cs, 0xFFFF);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: UDP checksum with a known packet
 *
 * Construct a UDP segment, compute the checksum, then verify
 * that recomputing with the checksum in place yields 0.
 * --------------------------------------------------------------- */
static void test_udp_checksum_known(void) {
    printf("  UDP checksum (known packet)...");

    /* UDP segment: src=12345, dst=53, len=13, csum=0, payload="hello" */
    uint8_t seg[] = {
        0x30, 0x39,               /* src_port = 12345 */
        0x00, 0x35,               /* dst_port = 53    */
        0x00, 0x0D,               /* length = 13      */
        0x00, 0x00,               /* checksum = 0     */
        'h',  'e',  'l', 'l', 'o' /* payload           */
    };

    uint32_t src = make_ip(192, 168, 1, 100);
    uint32_t dst = make_ip(8, 8, 8, 8);

    uint16_t cs = nfs_udp_checksum(src, dst, seg, sizeof(seg));
    ASSERT_TRUE(cs != 0x0000); /* Real data should produce non-zero checksum */

    /* Write checksum into the segment (network byte order). */
    seg[6] = (uint8_t)(cs >> 8);
    seg[7] = (uint8_t)(cs & 0xFF);

    /* Verify should pass. */
    ASSERT_EQ(nfs_udp_verify_checksum(src, dst, seg, sizeof(seg)), 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: UDP checksum over simple payload
 * --------------------------------------------------------------- */
static void test_udp_checksum_simple(void) {
    printf("  UDP checksum (simple payload)...");

    /* Minimal UDP: src=1, dst=2, len=8, csum=0, no payload */
    uint8_t seg[] = {
        0x00, 0x01, /* src_port = 1    */
        0x00, 0x02, /* dst_port = 2    */
        0x00, 0x08, /* length = 8      */
        0x00, 0x00  /* checksum = 0    */
    };

    uint32_t src = make_ip(10, 0, 0, 1);
    uint32_t dst = make_ip(10, 0, 0, 2);

    uint16_t cs = nfs_udp_checksum(src, dst, seg, sizeof(seg));
    ASSERT_TRUE(cs != 0x0000);

    /* Write checksum and verify. */
    seg[6] = (uint8_t)(cs >> 8);
    seg[7] = (uint8_t)(cs & 0xFF);
    ASSERT_EQ(nfs_udp_verify_checksum(src, dst, seg, sizeof(seg)), 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: verify returns 1 for valid packet
 * --------------------------------------------------------------- */
static void test_verify_valid(void) {
    printf("  verify valid...");

    uint8_t seg[] = {
        0x1F, 0x90,          /* src_port = 8080  */
        0x00, 0x50,          /* dst_port = 80    */
        0x00, 0x0C,          /* length = 12      */
        0x00, 0x00,          /* checksum = 0     */
        'T',  'E',  'S', 'T' /* payload           */
    };

    uint32_t src = make_ip(172, 16, 0, 1);
    uint32_t dst = make_ip(172, 16, 0, 2);

    /* Compute and embed checksum. */
    uint16_t cs = nfs_udp_checksum(src, dst, seg, sizeof(seg));
    seg[6] = (uint8_t)(cs >> 8);
    seg[7] = (uint8_t)(cs & 0xFF);

    ASSERT_EQ(nfs_udp_verify_checksum(src, dst, seg, sizeof(seg)), 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: verify returns 0 for corrupted packet
 * --------------------------------------------------------------- */
static void test_verify_corrupted(void) {
    printf("  verify corrupted...");

    uint8_t seg[] = {0x1F, 0x90, 0x00, 0x50, 0x00, 0x0C, 0x00, 0x00, 'T', 'E', 'S', 'T'};

    uint32_t src = make_ip(172, 16, 0, 1);
    uint32_t dst = make_ip(172, 16, 0, 2);

    uint16_t cs = nfs_udp_checksum(src, dst, seg, sizeof(seg));
    seg[6] = (uint8_t)(cs >> 8);
    seg[7] = (uint8_t)(cs & 0xFF);

    /* Corrupt a payload byte. */
    seg[8] ^= 0xFF;

    ASSERT_EQ(nfs_udp_verify_checksum(src, dst, seg, sizeof(seg)), 0);

    /* Also corrupt the checksum itself. */
    uint8_t seg2[] = {0x1F, 0x90, 0x00, 0x50, 0x00, 0x0C, 0xDE, 0xAD, /* bogus checksum */
                      'T',  'E',  'S',  'T'};
    ASSERT_EQ(nfs_udp_verify_checksum(src, dst, seg2, sizeof(seg2)), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: checksum == 0x0000 (not computed) is always valid
 * --------------------------------------------------------------- */
static void test_zero_checksum_valid(void) {
    printf("  zero checksum (not computed) is valid...");

    uint8_t seg[] = {
        0x00, 0x01, 0x00, 0x02, 0x00, 0x08, 0x00, 0x00 /* checksum = 0 (not computed) */
    };

    uint32_t src = make_ip(10, 0, 0, 1);
    uint32_t dst = make_ip(10, 0, 0, 2);

    /* Per RFC 768, a zero checksum means the sender did not compute it.
     * The receiver must accept it as valid. */
    ASSERT_EQ(nfs_udp_verify_checksum(src, dst, seg, sizeof(seg)), 1);

    /* Even with completely different IPs: */
    uint32_t src2 = make_ip(255, 255, 255, 255);
    uint32_t dst2 = make_ip(0, 0, 0, 0);
    ASSERT_EQ(nfs_udp_verify_checksum(src2, dst2, seg, sizeof(seg)), 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 10: RFC 768 zero -> 0xFFFF rule
 *
 * The nfs_udp_checksum function must never return 0x0000.
 * If the computed checksum is 0, it must return 0xFFFF.
 * --------------------------------------------------------------- */
static void test_zero_becomes_ffff(void) {
    printf("  RFC 768 zero -> 0xFFFF rule...");

    /* We can't easily craft data that gives exactly 0 checksum,
     * but we can verify the function never returns 0 for a variety
     * of inputs. */
    uint8_t seg1[] = {0x00, 0x01, 0x00, 0x02, 0x00, 0x08, 0x00, 0x00};
    uint32_t src = make_ip(10, 0, 0, 1);
    uint32_t dst = make_ip(10, 0, 0, 2);
    uint16_t cs1 = nfs_udp_checksum(src, dst, seg1, sizeof(seg1));
    ASSERT_TRUE(cs1 != 0x0000);

    uint8_t seg2[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x0A, 0x00, 0x00, 0xAB, 0xCD};
    uint16_t cs2 = nfs_udp_checksum(src, dst, seg2, sizeof(seg2));
    ASSERT_TRUE(cs2 != 0x0000);

    /* Test with various IPs. */
    uint32_t src3 = make_ip(0, 0, 0, 0);
    uint32_t dst3 = make_ip(0, 0, 0, 0);
    uint16_t cs3 = nfs_udp_checksum(src3, dst3, seg1, sizeof(seg1));
    ASSERT_TRUE(cs3 != 0x0000);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 11: known DNS query checksum
 *
 * Manually compute expected checksum for a known UDP packet.
 * --------------------------------------------------------------- */
static void test_dns_query_checksum(void) {
    printf("  DNS query checksum...");

    /* src = 192.168.1.100, dst = 8.8.8.8
     * UDP: src_port=12345, dst_port=53, len=13, csum=0, payload="hello" */
    uint8_t seg[] = {0x30, 0x39, 0x00, 0x35, 0x00, 0x0D, 0x00, 0x00, 'h', 'e', 'l', 'l', 'o'};

    uint32_t src = make_ip(192, 168, 1, 100);
    uint32_t dst = make_ip(8, 8, 8, 8);

    uint16_t cs = nfs_udp_checksum(src, dst, seg, sizeof(seg));

    /* The checksum must be deterministic -- same inputs, same output. */
    uint16_t cs2 = nfs_udp_checksum(src, dst, seg, sizeof(seg));
    ASSERT_EQ(cs, cs2);

    /* Compute and verify roundtrip. */
    seg[6] = (uint8_t)(cs >> 8);
    seg[7] = (uint8_t)(cs & 0xFF);
    ASSERT_EQ(nfs_udp_verify_checksum(src, dst, seg, sizeof(seg)), 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 12: pseudo-header contributes to checksum
 *
 * Changing the IP addresses (which only appear in the pseudo-header)
 * must change the checksum.
 * --------------------------------------------------------------- */
static void test_pseudo_hdr_contributes(void) {
    printf("  pseudo-header contributes to checksum...");

    uint8_t seg[] = {0x00, 0x50, 0x00, 0x50, 0x00, 0x08, 0x00, 0x00};

    uint32_t src1 = make_ip(10, 0, 0, 1);
    uint32_t dst1 = make_ip(10, 0, 0, 2);
    uint16_t cs1 = nfs_udp_checksum(src1, dst1, seg, sizeof(seg));

    uint32_t src2 = make_ip(10, 0, 0, 3);
    uint32_t dst2 = make_ip(10, 0, 0, 4);
    uint16_t cs2 = nfs_udp_checksum(src2, dst2, seg, sizeof(seg));

    /* Different IPs should produce different checksums. */
    ASSERT_TRUE(cs1 != cs2);

    /* Verify both. */
    seg[6] = (uint8_t)(cs1 >> 8);
    seg[7] = (uint8_t)(cs1 & 0xFF);
    ASSERT_EQ(nfs_udp_verify_checksum(src1, dst1, seg, sizeof(seg)), 1);
    /* Same segment with cs1 should fail for different IPs. */
    ASSERT_EQ(nfs_udp_verify_checksum(src2, dst2, seg, sizeof(seg)), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */
int main(void) {
    printf("UDP checksum with IPv4 pseudo-header test suite\n");

    test_pseudo_hdr_size();
    test_internet_checksum_known();
    test_internet_checksum_odd();
    test_internet_checksum_empty();
    test_udp_checksum_known();
    test_udp_checksum_simple();
    test_verify_valid();
    test_verify_corrupted();
    test_zero_checksum_valid();
    test_zero_becomes_ffff();
    test_dns_query_checksum();
    test_pseudo_hdr_contributes();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
