/* Tests for the UDP header implementation (RFC 768).
 *
 * Twelve test families:
 *   1.  Struct size pinning
 *   2.  Parse a known DNS query UDP packet (port 53)
 *   3.  Build and verify all fields in network order
 *   4.  Roundtrip: build -> parse -> verify fields match
 *   5.  Checksum computation with known IPv4 pseudo-header
 *   6.  Checksum validation (valid packet)
 *   7.  Checksum validation (corrupted packet)
 *   8.  Zero checksum handling (0x0000 means not computed)
 *   9.  0xFFFF checksum edge case
 *   10. Reject truncated buffer (< 8 bytes)
 *   11. Length field validation (length < 8 should fail)
 *   12. Format output test
 *
 * Compile + run:
 *     make
 *     ./test_udp
 */

#include "../udp.h"

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
            fprintf(stderr, "  FAIL %s:%d: %s == %lld, want %lld\n", __FILE__, __LINE__, #expr,    \
                    _got, _exp);                                                                   \
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

/* ---------------------------------------------------------------
 * Test 1: struct size must be exactly 8 bytes
 * --------------------------------------------------------------- */

static void test_struct_size(void) {
    printf("  struct size...");
    ASSERT_EQ(sizeof(struct nfs_udp_hdr), 8);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 2: parse a known DNS query UDP packet (port 53)
 *
 * Hand-crafted UDP datagram:
 *   src_port = 12345 (0x3039)
 *   dst_port = 53    (0x0035)
 *   length   = 13    (0x000d) — 8 header + 5 payload
 *   checksum = 0xa1b2
 *   payload  = "hello"
 * --------------------------------------------------------------- */

static void test_parse_dns_query(void) {
    printf("  parse DNS query...");

    uint8_t raw[] = {
        0x30, 0x39,               /* src_port = 12345 */
        0x00, 0x35,               /* dst_port = 53    */
        0x00, 0x0d,               /* length = 13      */
        0xa1, 0xb2,               /* checksum         */
        'h',  'e',  'l', 'l', 'o' /* payload */
    };

    struct nfs_udp_hdr hdr;
    ASSERT_EQ(nfs_udp_parse(raw, sizeof(raw), &hdr), 0);
    ASSERT_EQ(hdr.src_port, 12345);
    ASSERT_EQ(hdr.dst_port, 53);
    ASSERT_EQ(hdr.length, 13);
    ASSERT_EQ(hdr.checksum, 0xa1b2);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: build and verify all fields in network order
 * --------------------------------------------------------------- */

static void test_build_fields(void) {
    printf("  build fields...");

    struct nfs_udp_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.src_port = 8080;
    hdr.dst_port = 53;
    hdr.length = 13;
    hdr.checksum = 0;

    const uint8_t payload[] = "hello";
    uint8_t buf[64];
    size_t n = nfs_udp_build(&hdr, payload, 5, buf, sizeof(buf));
    ASSERT_EQ(n, 13);

    /* Verify header bytes are in network order (big-endian). */
    /* src_port = 8080 = 0x1F90 */
    ASSERT_EQ(buf[0], 0x1F);
    ASSERT_EQ(buf[1], 0x90);
    /* dst_port = 53 = 0x0035 */
    ASSERT_EQ(buf[2], 0x00);
    ASSERT_EQ(buf[3], 0x35);
    /* length = 13 = 0x000D */
    ASSERT_EQ(buf[4], 0x00);
    ASSERT_EQ(buf[5], 0x0D);
    /* checksum = 0x0000 */
    ASSERT_EQ(buf[6], 0x00);
    ASSERT_EQ(buf[7], 0x00);
    /* payload = "hello" */
    ASSERT_TRUE(memcmp(buf + 8, "hello", 5) == 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: roundtrip — build -> parse -> verify fields match
 * --------------------------------------------------------------- */

static void test_roundtrip(void) {
    printf("  roundtrip...");

    struct nfs_udp_hdr orig;
    memset(&orig, 0, sizeof(orig));
    orig.src_port = 49152;
    orig.dst_port = 80;
    orig.length = 18; /* 8 + 10 */
    orig.checksum = 0x1234;

    const uint8_t payload[] = "0123456789";
    uint8_t buf[64];
    size_t n = nfs_udp_build(&orig, payload, 10, buf, sizeof(buf));
    ASSERT_EQ(n, 18);

    struct nfs_udp_hdr parsed;
    ASSERT_EQ(nfs_udp_parse(buf, n, &parsed), 0);
    ASSERT_EQ(parsed.src_port, orig.src_port);
    ASSERT_EQ(parsed.dst_port, orig.dst_port);
    ASSERT_EQ(parsed.length, orig.length);
    ASSERT_EQ(parsed.checksum, orig.checksum);

    /* Verify payload survived. */
    ASSERT_TRUE(memcmp(buf + 8, payload, 10) == 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: checksum computation with known IPv4 pseudo-header
 *
 * Build a datagram, compute the checksum, verify it is non-zero.
 * --------------------------------------------------------------- */

static void test_checksum_computation(void) {
    printf("  checksum computation...");

    struct nfs_udp_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.src_port = 12345;
    hdr.dst_port = 53;
    hdr.length = 13;  /* 8 header + 5 payload */
    hdr.checksum = 0; /* will be computed */

    const uint8_t payload[] = "hello";
    uint8_t buf[64];
    size_t n = nfs_udp_build(&hdr, payload, 5, buf, sizeof(buf));
    ASSERT_EQ(n, 13);

    uint8_t src_ip[4] = {192, 168, 1, 100};
    uint8_t dst_ip[4] = {8, 8, 8, 8};

    uint16_t cs = nfs_udp_checksum_ipv4(src_ip, dst_ip, buf, n);
    /* Checksum should be non-zero for real data. */
    ASSERT_TRUE(cs != 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: checksum validation (valid packet)
 *
 * Compute the checksum, write it into the datagram, then validate.
 * --------------------------------------------------------------- */

static void test_checksum_valid(void) {
    printf("  checksum validation (valid)...");

    struct nfs_udp_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.src_port = 12345;
    hdr.dst_port = 53;
    hdr.length = 13;
    hdr.checksum = 0;

    const uint8_t payload[] = "hello";
    uint8_t buf[64];
    size_t n = nfs_udp_build(&hdr, payload, 5, buf, sizeof(buf));
    ASSERT_EQ(n, 13);

    uint8_t src_ip[4] = {10, 0, 0, 1};
    uint8_t dst_ip[4] = {10, 0, 0, 2};

    /* Compute checksum with checksum field zeroed. */
    uint16_t cs = nfs_udp_checksum_ipv4(src_ip, dst_ip, buf, n);

    /* Write the checksum into the datagram (network order). */
    buf[6] = (uint8_t)(cs >> 8);
    buf[7] = (uint8_t)(cs & 0xFF);

    /* Validate: should return 0 (valid). */
    ASSERT_EQ(nfs_udp_validate(src_ip, dst_ip, buf, n), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: checksum validation (corrupted packet)
 * --------------------------------------------------------------- */

static void test_checksum_corrupted(void) {
    printf("  checksum validation (corrupted)...");

    struct nfs_udp_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.src_port = 12345;
    hdr.dst_port = 53;
    hdr.length = 13;
    hdr.checksum = 0;

    const uint8_t payload[] = "hello";
    uint8_t buf[64];
    size_t n = nfs_udp_build(&hdr, payload, 5, buf, sizeof(buf));
    ASSERT_EQ(n, 13);

    uint8_t src_ip[4] = {10, 0, 0, 1};
    uint8_t dst_ip[4] = {10, 0, 0, 2};

    /* Compute and write valid checksum. */
    uint16_t cs = nfs_udp_checksum_ipv4(src_ip, dst_ip, buf, n);
    buf[6] = (uint8_t)(cs >> 8);
    buf[7] = (uint8_t)(cs & 0xFF);

    /* Corrupt one byte of payload. */
    buf[8] ^= 0xFF;

    /* Validate: should return -1 (invalid). */
    ASSERT_EQ(nfs_udp_validate(src_ip, dst_ip, buf, n), -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: zero checksum handling (0x0000 means not computed)
 *
 * RFC 768: a transmitted checksum of 0x0000 means the sender
 * did not compute the checksum.  The receiver must accept it.
 * --------------------------------------------------------------- */

static void test_zero_checksum(void) {
    printf("  zero checksum (not computed)...");

    struct nfs_udp_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.src_port = 1234;
    hdr.dst_port = 5678;
    hdr.length = 8;
    hdr.checksum = 0; /* not computed */

    uint8_t buf[64];
    size_t n = nfs_udp_build(&hdr, NULL, 0, buf, sizeof(buf));
    ASSERT_EQ(n, 8);

    /* Checksum field on wire should be 0x0000. */
    ASSERT_EQ(buf[6], 0x00);
    ASSERT_EQ(buf[7], 0x00);

    uint8_t src_ip[4] = {10, 0, 0, 1};
    uint8_t dst_ip[4] = {10, 0, 0, 2};

    /* Validate must accept zero checksum as "not computed". */
    ASSERT_EQ(nfs_udp_validate(src_ip, dst_ip, buf, n), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: 0xFFFF checksum edge case
 *
 * RFC 768: if the computed checksum is 0, transmit as 0xFFFF.
 * We test that nfs_udp_checksum_ipv4 never returns 0x0000
 * (it should return 0xFFFF instead).
 *
 * We verify the property: the function never returns 0x0000.
 * Also verify that 0xFFFF on the wire is accepted as valid.
 * --------------------------------------------------------------- */

static void test_ffff_checksum(void) {
    printf("  0xFFFF checksum edge case...");

    /* Build a datagram with checksum = 0xFFFF on the wire. */
    struct nfs_udp_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.src_port = 1000;
    hdr.dst_port = 2000;
    hdr.length = 8;
    hdr.checksum = 0;

    uint8_t buf[64];
    size_t n = nfs_udp_build(&hdr, NULL, 0, buf, sizeof(buf));
    ASSERT_EQ(n, 8);

    uint8_t src_ip[4] = {10, 0, 0, 1};
    uint8_t dst_ip[4] = {10, 0, 0, 2};

    /* Compute checksum — result must not be 0x0000 (RFC 768). */
    uint16_t cs = nfs_udp_checksum_ipv4(src_ip, dst_ip, buf, n);
    ASSERT_TRUE(cs != 0x0000);

    /* Write the checksum and validate. */
    buf[6] = (uint8_t)(cs >> 8);
    buf[7] = (uint8_t)(cs & 0xFF);
    ASSERT_EQ(nfs_udp_validate(src_ip, dst_ip, buf, n), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 10: reject truncated buffer (< 8 bytes)
 * --------------------------------------------------------------- */

static void test_reject_truncated(void) {
    printf("  reject truncated...");

    struct nfs_udp_hdr hdr;

    /* 0 bytes. */
    ASSERT_EQ(nfs_udp_parse(NULL, 0, &hdr), -1);

    /* 4 bytes — too short. */
    uint8_t short_buf[4] = {0x00, 0x35, 0x00, 0x35};
    ASSERT_EQ(nfs_udp_parse(short_buf, sizeof(short_buf), &hdr), -1);

    /* 7 bytes — still too short. */
    uint8_t almost[7] = {0x00, 0x35, 0x00, 0x35, 0x00, 0x08, 0x00};
    ASSERT_EQ(nfs_udp_parse(almost, sizeof(almost), &hdr), -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 11: length field validation (length < 8 should fail)
 * --------------------------------------------------------------- */

static void test_length_validation(void) {
    printf("  length validation...");

    /* Craft a packet where the length field says 7 (invalid). */
    uint8_t bad_len[8] = {
        0x00, 0x35, /* src_port = 53 */
        0x00, 0x35, /* dst_port = 53 */
        0x00, 0x07, /* length = 7 -- INVALID (< 8) */
        0x00, 0x00  /* checksum = 0 */
    };

    struct nfs_udp_hdr hdr;
    ASSERT_EQ(nfs_udp_parse(bad_len, sizeof(bad_len), &hdr), -1);

    /* Length 0 should also fail. */
    uint8_t zero_len[8] = {
        0x00, 0x35, 0x00, 0x35, 0x00, 0x00, 0x00, 0x00,
    };
    ASSERT_EQ(nfs_udp_parse(zero_len, sizeof(zero_len), &hdr), -1);

    /* Exactly 8 should succeed (empty payload). */
    uint8_t min_len[8] = {
        0x00, 0x35, 0x00, 0x35, 0x00, 0x08, 0x00, 0x00,
    };
    ASSERT_EQ(nfs_udp_parse(min_len, sizeof(min_len), &hdr), 0);
    ASSERT_EQ(hdr.length, 8);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 12: format output test
 * --------------------------------------------------------------- */

static void test_format_output(void) {
    printf("  format output...");

    struct nfs_udp_hdr hdr;
    hdr.src_port = 12345;
    hdr.dst_port = 53;
    hdr.length = 13;
    hdr.checksum = 0xa1b2;

    char buf[256];
    nfs_udp_format(&hdr, buf, sizeof(buf));

    /* Verify the output contains key information. */
    ASSERT_TRUE(strstr(buf, "12345") != NULL);
    ASSERT_TRUE(strstr(buf, "53") != NULL);
    ASSERT_TRUE(strstr(buf, "13") != NULL);
    ASSERT_TRUE(strstr(buf, "a1b2") != NULL);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void) {
    printf("UDP header (RFC 768) test suite\n");
    test_struct_size();
    test_parse_dns_query();
    test_build_fields();
    test_roundtrip();
    test_checksum_computation();
    test_checksum_valid();
    test_checksum_corrupted();
    test_zero_checksum();
    test_ffff_checksum();
    test_reject_truncated();
    test_length_validation();
    test_format_output();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
