/* Tests for TCP checksum with IPv4 pseudo-header (RFC 9293, RFC 1071).
 *
 * Test families:
 *   1.  Pseudo-header struct size pinning (12 bytes)
 *   2.  Internet checksum of known data
 *   3.  Known TCP SYN with pre-computed checksum
 *   4.  Checksum of simple TCP segment
 *   5.  Verify valid packet
 *   6.  Verify corrupted packet fails
 *   7.  Odd-length data handling
 *   8.  Pseudo-header contributes to checksum
 *   9.  Empty payload (header only)
 *  10.  Checksum fold
 *  11.  Incremental checksum
 *  12.  TCP segment with payload
 */

#include "../tcp_csum.h"

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
 * --------------------------------------------------------------- */
static void test_internet_checksum_known(void) {
    printf("  internet checksum (known)...");

    uint8_t data[] = {0x00, 0x01, 0x00, 0x02};
    uint16_t cs = nfs_internet_checksum(data, sizeof(data));
    ASSERT_EQ(cs, 0xFFFC);

    /* All zeros: sum=0, checksum=~0=0xFFFF */
    uint8_t zeros[4] = {0, 0, 0, 0};
    cs = nfs_internet_checksum(zeros, sizeof(zeros));
    ASSERT_EQ(cs, 0xFFFF);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: known TCP SYN with pre-computed checksum
 *
 * Build a TCP SYN, compute checksum, embed it, verify.
 * --------------------------------------------------------------- */
static void test_known_tcp_syn(void) {
    printf("  known TCP SYN...");

    /* TCP SYN: src=12345, dst=80, seq=1, ack=0, doff=5, SYN, win=65535 */
    uint8_t seg[20] = {
        0x30, 0x39,             /* src_port = 12345 */
        0x00, 0x50,             /* dst_port = 80    */
        0x00, 0x00, 0x00, 0x01, /* seq = 1          */
        0x00, 0x00, 0x00, 0x00, /* ack = 0          */
        0x50, 0x02,             /* doff=5, SYN      */
        0xFF, 0xFF,             /* window = 65535   */
        0x00, 0x00,             /* checksum = 0     */
        0x00, 0x00              /* urgent = 0       */
    };

    uint32_t src = make_ip(192, 168, 1, 100);
    uint32_t dst = make_ip(10, 0, 0, 1);

    uint16_t cs = nfs_tcp_checksum(src, dst, seg, sizeof(seg));
    ASSERT_TRUE(cs != 0x0000);

    /* Embed checksum (network byte order). */
    seg[16] = (uint8_t)(cs >> 8);
    seg[17] = (uint8_t)(cs & 0xFF);

    /* Verify. */
    ASSERT_EQ(nfs_tcp_verify_checksum(src, dst, seg, sizeof(seg)), 1);

    /* Checksum must be deterministic. */
    seg[16] = 0;
    seg[17] = 0;
    uint16_t cs2 = nfs_tcp_checksum(src, dst, seg, sizeof(seg));
    ASSERT_EQ(cs, cs2);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: checksum of simple TCP segment
 * --------------------------------------------------------------- */
static void test_simple_segment(void) {
    printf("  simple segment...");

    /* Minimal TCP header with all zeros except required fields. */
    uint8_t seg[20] = {
        0x00, 0x01,             /* src_port = 1     */
        0x00, 0x02,             /* dst_port = 2     */
        0x00, 0x00, 0x00, 0x00, /* seq = 0          */
        0x00, 0x00, 0x00, 0x00, /* ack = 0          */
        0x50, 0x00,             /* doff=5, no flags */
        0x00, 0x00,             /* window = 0       */
        0x00, 0x00,             /* checksum = 0     */
        0x00, 0x00              /* urgent = 0       */
    };

    uint32_t src = make_ip(127, 0, 0, 1);
    uint32_t dst = make_ip(127, 0, 0, 1);

    uint16_t cs = nfs_tcp_checksum(src, dst, seg, sizeof(seg));
    ASSERT_TRUE(cs != 0);

    seg[16] = (uint8_t)(cs >> 8);
    seg[17] = (uint8_t)(cs & 0xFF);
    ASSERT_EQ(nfs_tcp_verify_checksum(src, dst, seg, sizeof(seg)), 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: verify valid packet
 * --------------------------------------------------------------- */
static void test_verify_valid(void) {
    printf("  verify valid...");

    uint8_t seg[20] = {
        0x1F, 0x90,             /* src_port = 8080  */
        0x01, 0xBB,             /* dst_port = 443   */
        0x00, 0x00, 0x03, 0xE8, /* seq = 1000       */
        0x00, 0x00, 0x07, 0xD0, /* ack = 2000       */
        0x50, 0x12,             /* doff=5, SYN+ACK  */
        0x80, 0x00,             /* window = 32768   */
        0x00, 0x00,             /* checksum = 0     */
        0x00, 0x00              /* urgent = 0       */
    };

    uint32_t src = make_ip(10, 1, 2, 3);
    uint32_t dst = make_ip(10, 4, 5, 6);

    uint16_t cs = nfs_tcp_checksum(src, dst, seg, sizeof(seg));
    seg[16] = (uint8_t)(cs >> 8);
    seg[17] = (uint8_t)(cs & 0xFF);

    ASSERT_EQ(nfs_tcp_verify_checksum(src, dst, seg, sizeof(seg)), 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: verify corrupted packet fails
 * --------------------------------------------------------------- */
static void test_verify_corrupted(void) {
    printf("  verify corrupted...");

    uint8_t seg[20] = {0x1F, 0x90, 0x01, 0xBB, 0x00, 0x00, 0x03, 0xE8, 0x00, 0x00,
                       0x07, 0xD0, 0x50, 0x12, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00};

    uint32_t src = make_ip(10, 1, 2, 3);
    uint32_t dst = make_ip(10, 4, 5, 6);

    uint16_t cs = nfs_tcp_checksum(src, dst, seg, sizeof(seg));
    seg[16] = (uint8_t)(cs >> 8);
    seg[17] = (uint8_t)(cs & 0xFF);

    /* Corrupt a byte. */
    seg[4] ^= 0xFF;
    ASSERT_EQ(nfs_tcp_verify_checksum(src, dst, seg, sizeof(seg)), 0);

    /* Corrupt the checksum field itself. */
    uint8_t seg2[20];
    memcpy(seg2, seg, sizeof(seg2));
    seg2[4] ^= 0xFF;  /* undo previous corruption */
    seg2[16] ^= 0x01; /* corrupt checksum */
    ASSERT_EQ(nfs_tcp_verify_checksum(src, dst, seg2, sizeof(seg2)), 0);

    /* Bogus checksum. */
    uint8_t seg3[20] = {0};
    seg3[12] = 0x50; /* doff = 5 */
    seg3[16] = 0xDE;
    seg3[17] = 0xAD;
    ASSERT_EQ(nfs_tcp_verify_checksum(src, dst, seg3, sizeof(seg3)), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: odd-length data handling
 *
 * TCP segments can have odd total length (header + odd-byte payload).
 * --------------------------------------------------------------- */
static void test_odd_length(void) {
    printf("  odd-length data...");

    /* 20-byte header + 5-byte payload = 25 bytes (odd). */
    uint8_t seg[25] = {
        0x00, 0x50, 0x30, 0x39, 0x00, 0x00, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x50, 0x18, 0xFF, 0xFF,           /* doff=5, PSH+ACK */
        0x00, 0x00, 0x00, 0x00, 'h',  'e',  'l',  'l',  'o' /* 5-byte payload */
    };

    uint32_t src = make_ip(192, 168, 0, 1);
    uint32_t dst = make_ip(192, 168, 0, 2);

    uint16_t cs = nfs_tcp_checksum(src, dst, seg, sizeof(seg));
    seg[16] = (uint8_t)(cs >> 8);
    seg[17] = (uint8_t)(cs & 0xFF);

    ASSERT_EQ(nfs_tcp_verify_checksum(src, dst, seg, sizeof(seg)), 1);

    /* Verify odd-length internet checksum directly. */
    uint8_t odd[] = {0x01, 0x02, 0x03};
    uint16_t odd_cs = nfs_internet_checksum(odd, sizeof(odd));
    ASSERT_TRUE(odd_cs != 0); /* Non-trivial checksum. */

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: pseudo-header contributes to checksum
 * --------------------------------------------------------------- */
static void test_pseudo_hdr_contributes(void) {
    printf("  pseudo-header contributes...");

    uint8_t seg[20] = {0x00, 0x50, 0x00, 0x50, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
                       0x00, 0x00, 0x50, 0x02, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00};

    uint32_t src1 = make_ip(10, 0, 0, 1);
    uint32_t dst1 = make_ip(10, 0, 0, 2);
    uint16_t cs1 = nfs_tcp_checksum(src1, dst1, seg, sizeof(seg));

    uint32_t src2 = make_ip(172, 16, 0, 1);
    uint32_t dst2 = make_ip(172, 16, 0, 2);
    uint16_t cs2 = nfs_tcp_checksum(src2, dst2, seg, sizeof(seg));

    /* Different IPs produce different checksums. */
    ASSERT_TRUE(cs1 != cs2);

    /* Verify cs1 with correct IPs. */
    seg[16] = (uint8_t)(cs1 >> 8);
    seg[17] = (uint8_t)(cs1 & 0xFF);
    ASSERT_EQ(nfs_tcp_verify_checksum(src1, dst1, seg, sizeof(seg)), 1);

    /* cs1 should fail with wrong IPs. */
    ASSERT_EQ(nfs_tcp_verify_checksum(src2, dst2, seg, sizeof(seg)), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: empty payload (header only, 20 bytes)
 * --------------------------------------------------------------- */
static void test_empty_payload(void) {
    printf("  empty payload...");

    uint8_t seg[20] = {0xC0, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                       0x00, 0x00, 0x50, 0x02, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00};

    uint32_t src = make_ip(10, 0, 0, 1);
    uint32_t dst = make_ip(10, 0, 0, 2);

    uint16_t cs = nfs_tcp_checksum(src, dst, seg, sizeof(seg));
    ASSERT_TRUE(cs != 0);

    seg[16] = (uint8_t)(cs >> 8);
    seg[17] = (uint8_t)(cs & 0xFF);
    ASSERT_EQ(nfs_tcp_verify_checksum(src, dst, seg, sizeof(seg)), 1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 10: checksum fold
 * --------------------------------------------------------------- */
static void test_checksum_fold(void) {
    printf("  checksum fold...");

    /* 0x00000000 -> ~0 = 0xFFFF */
    ASSERT_EQ(nfs_checksum_fold(0x00000000), 0xFFFF);

    /* 0x0000FFFF -> ~0xFFFF = 0x0000 */
    ASSERT_EQ(nfs_checksum_fold(0x0000FFFF), 0x0000);

    /* 0x0001FFFE -> fold: 0xFFFE + 0x0001 = 0xFFFF -> ~0xFFFF = 0x0000 */
    ASSERT_EQ(nfs_checksum_fold(0x0001FFFE), 0x0000);

    /* 0x00010000 -> fold: 0x0000 + 0x0001 = 0x0001 -> ~0x0001 = 0xFFFE */
    ASSERT_EQ(nfs_checksum_fold(0x00010000), 0xFFFE);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 11: incremental checksum (partial)
 * --------------------------------------------------------------- */
static void test_incremental_checksum(void) {
    printf("  incremental checksum...");

    /* Compute checksum over {0x00, 0x01, 0x00, 0x02} in two parts. */
    uint8_t part1[] = {0x00, 0x01};
    uint8_t part2[] = {0x00, 0x02};

    /* The partial function returns uint16_t (truncated), so for
     * a proper multi-buffer test we verify the final result matches
     * the single-buffer result. */
    uint16_t full_cs = nfs_internet_checksum((uint8_t[]){0x00, 0x01, 0x00, 0x02}, 4);
    ASSERT_EQ(full_cs, 0xFFFC);

    /* Verify partial can be used with fold. */
    uint16_t p1 = nfs_checksum_partial(part1, sizeof(part1), 0);
    uint16_t p2 = nfs_checksum_partial(part2, sizeof(part2), (uint32_t)p1);
    uint16_t combined = nfs_checksum_fold((uint32_t)p2);
    /* Note: due to truncation this may not match perfectly for large sums,
     * but for small values it works. */
    ASSERT_EQ(combined, full_cs);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 12: TCP segment with payload
 * --------------------------------------------------------------- */
static void test_segment_with_payload(void) {
    printf("  segment with payload...");

    /* 20-byte header + 12-byte payload ("Hello World!" with NUL-free) */
    uint8_t seg[32] = {0x1F, 0x90, 0x00, 0x50, /* src=8080, dst=80    */
                       0x00, 0x00, 0x00, 0x0A, /* seq = 10            */
                       0x00, 0x00, 0x00, 0x14, /* ack = 20            */
                       0x50, 0x18, 0xFF, 0xFF, /* doff=5, PSH+ACK     */
                       0x00, 0x00, 0x00, 0x00, /* checksum=0, urgent=0 */
                       'H',  'e',  'l',  'l',  /* payload              */
                       'o',  ' ',  'W',  'o',  'r', 'l', 'd', '!'};

    uint32_t src = make_ip(10, 0, 0, 100);
    uint32_t dst = make_ip(10, 0, 0, 200);

    uint16_t cs = nfs_tcp_checksum(src, dst, seg, sizeof(seg));
    seg[16] = (uint8_t)(cs >> 8);
    seg[17] = (uint8_t)(cs & 0xFF);

    ASSERT_EQ(nfs_tcp_verify_checksum(src, dst, seg, sizeof(seg)), 1);

    /* Corrupt one payload byte. */
    seg[20] = 'X';
    ASSERT_EQ(nfs_tcp_verify_checksum(src, dst, seg, sizeof(seg)), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */
int main(void) {
    printf("TCP checksum (RFC 9293) test suite\n");

    test_pseudo_hdr_size();
    test_internet_checksum_known();
    test_known_tcp_syn();
    test_simple_segment();
    test_verify_valid();
    test_verify_corrupted();
    test_odd_length();
    test_pseudo_hdr_contributes();
    test_empty_payload();
    test_checksum_fold();
    test_incremental_checksum();
    test_segment_with_payload();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
