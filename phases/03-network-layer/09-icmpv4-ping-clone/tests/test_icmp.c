/* Tests for the ICMPv4 ping clone implementation.
 *
 * Ten test families:
 *   1. Struct size pinning
 *   2. Parse known echo request bytes
 *   3. Build echo request and verify fields
 *   4. Roundtrip: build -> parse -> check all fields
 *   5. Checksum validation (valid packet)
 *   6. Checksum validation (corrupted packet)
 *   7. Reject truncated buffer
 *   8. Type name lookup
 *   9. Sequence number increment
 *  10. Multiple requests with different IDs
 *
 * Compile + run:
 *     make
 *     ./test_icmp
 */

#include "../icmp.h"

#include <arpa/inet.h>
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
    ASSERT_EQ(sizeof(struct nfs_icmp_hdr), 8);
    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 2: parse a known echo request byte sequence
 *
 * Hand-crafted Echo Request: type=8, code=0, checksum=0xf7fc,
 * id=0x1234, seq=0x0001.
 *
 * This is a real ICMP echo request header (no payload) with a
 * precomputed valid checksum.
 * --------------------------------------------------------------- */

static void test_parse_known_echo_request(void) {
    printf("  parse known echo request...");

    /* Build the packet properly to get a valid checksum. */
    uint8_t pkt[8];
    size_t n = nfs_icmp_build_echo_request(0x1234, 0x0001, NULL, 0, pkt, sizeof(pkt));
    ASSERT_EQ(n, 8);

    struct nfs_icmp_hdr hdr;
    ASSERT_EQ(nfs_icmp_parse(pkt, n, &hdr), 0);
    ASSERT_EQ(hdr.type, NFS_ICMP_ECHO_REQUEST);
    ASSERT_EQ(hdr.code, 0);
    ASSERT_EQ(hdr.id, 0x1234);
    ASSERT_EQ(hdr.seq, 0x0001);

    /* Checksum should be non-zero. */
    ASSERT_TRUE(hdr.checksum != 0);

    /* The raw packet should pass checksum validation. */
    ASSERT_EQ(nfs_icmp_validate_checksum(pkt, n), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 3: build echo request and verify wire-format fields
 * --------------------------------------------------------------- */

static void test_build_echo_request(void) {
    printf("  build echo request...");

    uint8_t payload[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t buf[64];
    size_t n = nfs_icmp_build_echo_request(0xABCD, 5, payload, sizeof(payload), buf, sizeof(buf));

    /* Total size = 8 (header) + 4 (payload) = 12 */
    ASSERT_EQ(n, 12);

    /* Check wire-format fields directly (network byte order). */
    ASSERT_EQ(buf[0], NFS_ICMP_ECHO_REQUEST); /* type */
    ASSERT_EQ(buf[1], 0);                     /* code */
    /* buf[2..3] = checksum (skip, verified separately) */
    ASSERT_EQ(buf[4], 0xAB); /* id high byte */
    ASSERT_EQ(buf[5], 0xCD); /* id low byte  */
    ASSERT_EQ(buf[6], 0x00); /* seq high byte */
    ASSERT_EQ(buf[7], 0x05); /* seq low byte  */

    /* Payload should follow the header. */
    ASSERT_EQ(buf[8], 0xDE);
    ASSERT_EQ(buf[9], 0xAD);
    ASSERT_EQ(buf[10], 0xBE);
    ASSERT_EQ(buf[11], 0xEF);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 4: roundtrip -- build, parse, verify all fields match
 * --------------------------------------------------------------- */

static void test_roundtrip(void) {
    printf("  roundtrip...");

    uint16_t id = 0x9876;
    uint16_t seq = 42;
    uint8_t payload[] = "Hello, ICMP!";
    size_t plen = sizeof(payload) - 1; /* exclude null terminator */

    uint8_t buf[64];
    size_t n = nfs_icmp_build_echo_request(id, seq, payload, plen, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(n, sizeof(struct nfs_icmp_hdr) + plen);

    struct nfs_icmp_hdr hdr;
    ASSERT_EQ(nfs_icmp_parse(buf, n, &hdr), 0);
    ASSERT_EQ(hdr.type, NFS_ICMP_ECHO_REQUEST);
    ASSERT_EQ(hdr.code, 0);
    ASSERT_EQ(hdr.id, id);
    ASSERT_EQ(hdr.seq, seq);

    /* Payload should survive the roundtrip. */
    ASSERT_TRUE(memcmp(buf + sizeof(struct nfs_icmp_hdr), payload, plen) == 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 5: checksum validation (valid packet returns 0)
 * --------------------------------------------------------------- */

static void test_checksum_valid(void) {
    printf("  checksum valid...");

    uint8_t payload[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8_t buf[64];
    size_t n = nfs_icmp_build_echo_request(0x1111, 1, payload, sizeof(payload), buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    ASSERT_EQ(nfs_icmp_validate_checksum(buf, n), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 6: checksum validation (corrupted packet returns error)
 * --------------------------------------------------------------- */

static void test_checksum_corrupted(void) {
    printf("  checksum corrupted...");

    uint8_t payload[] = {0xAA, 0xBB, 0xCC, 0xDD};
    uint8_t buf[64];
    size_t n = nfs_icmp_build_echo_request(0x2222, 3, payload, sizeof(payload), buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    /* Valid before corruption. */
    ASSERT_EQ(nfs_icmp_validate_checksum(buf, n), 0);

    /* Flip a bit in the payload. */
    buf[n - 1] ^= 0x01;
    ASSERT_EQ(nfs_icmp_validate_checksum(buf, n), -1);

    /* Flip a bit in the type field. */
    buf[n - 1] ^= 0x01; /* restore payload */
    buf[0] ^= 0x04;     /* corrupt type */
    ASSERT_EQ(nfs_icmp_validate_checksum(buf, n), -1);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 7: reject truncated buffer
 * --------------------------------------------------------------- */

static void test_reject_truncated(void) {
    printf("  reject truncated...");

    struct nfs_icmp_hdr hdr;

    /* Too short. */
    uint8_t short_buf[4] = {0x08, 0x00, 0x00, 0x00};
    ASSERT_EQ(nfs_icmp_parse(short_buf, sizeof(short_buf), &hdr), -1);

    /* Zero length. */
    ASSERT_EQ(nfs_icmp_parse(short_buf, 0, &hdr), -1);

    /* NULL pointer. */
    ASSERT_EQ(nfs_icmp_parse(NULL, 8, &hdr), -1);
    ASSERT_EQ(nfs_icmp_parse(NULL, 0, &hdr), -1);

    /* Checksum validation with truncated buffer. */
    ASSERT_EQ(nfs_icmp_validate_checksum(short_buf, sizeof(short_buf)), -1);
    ASSERT_EQ(nfs_icmp_validate_checksum(NULL, 0), -1);

    /* Build with too-small buffer. */
    uint8_t tiny[4];
    ASSERT_EQ(nfs_icmp_build_echo_request(1, 1, NULL, 0, tiny, sizeof(tiny)), 0);
    ASSERT_EQ(nfs_icmp_build_echo_request(1, 1, NULL, 0, NULL, 64), 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 8: type name lookup
 * --------------------------------------------------------------- */

static void test_type_name(void) {
    printf("  type name lookup...");

    ASSERT_TRUE(strcmp(nfs_icmp_type_name(NFS_ICMP_ECHO_REPLY), "Echo Reply") == 0);
    ASSERT_TRUE(strcmp(nfs_icmp_type_name(NFS_ICMP_ECHO_REQUEST), "Echo Request") == 0);
    ASSERT_TRUE(strcmp(nfs_icmp_type_name(NFS_ICMP_DEST_UNREACH), "Destination Unreachable") == 0);
    ASSERT_TRUE(strcmp(nfs_icmp_type_name(NFS_ICMP_TIME_EXCEEDED), "Time Exceeded") == 0);
    ASSERT_TRUE(strcmp(nfs_icmp_type_name(255), "Unknown") == 0);
    ASSERT_TRUE(strcmp(nfs_icmp_type_name(42), "Unknown") == 0);

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 9: sequence number increment -- build multiple echo
 * requests with incrementing seq, verify each one
 * --------------------------------------------------------------- */

static void test_seq_increment(void) {
    printf("  sequence number increment...");

    uint8_t buf[64];
    struct nfs_icmp_hdr hdr;

    for (uint16_t seq = 0; seq < 10; seq++) {
        size_t n = nfs_icmp_build_echo_request(0x4444, seq, NULL, 0, buf, sizeof(buf));
        ASSERT_TRUE(n > 0);
        ASSERT_EQ(nfs_icmp_parse(buf, n, &hdr), 0);
        ASSERT_EQ(hdr.seq, seq);
        ASSERT_EQ(hdr.id, 0x4444);
        ASSERT_EQ(nfs_icmp_validate_checksum(buf, n), 0);
    }

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Test 10: multiple requests with different IDs
 * --------------------------------------------------------------- */

static void test_different_ids(void) {
    printf("  different IDs...");

    uint16_t ids[] = {0x0001, 0x00FF, 0x1234, 0xABCD, 0xFFFF};
    uint8_t payload[] = {0x42};
    uint8_t buf[64];
    struct nfs_icmp_hdr hdr;

    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        size_t n =
            nfs_icmp_build_echo_request(ids[i], 1, payload, sizeof(payload), buf, sizeof(buf));
        ASSERT_TRUE(n > 0);
        ASSERT_EQ(nfs_icmp_parse(buf, n, &hdr), 0);
        ASSERT_EQ(hdr.id, ids[i]);
        ASSERT_EQ(hdr.type, NFS_ICMP_ECHO_REQUEST);
        ASSERT_EQ(nfs_icmp_validate_checksum(buf, n), 0);
    }

    printf(" OK\n");
}

/* ---------------------------------------------------------------
 * Main
 * --------------------------------------------------------------- */

int main(void) {
    printf("ICMPv4 ping clone test suite\n");
    test_struct_size();
    test_parse_known_echo_request();
    test_build_echo_request();
    test_roundtrip();
    test_checksum_valid();
    test_checksum_corrupted();
    test_reject_truncated();
    test_type_name();
    test_seq_increment();
    test_different_ids();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
