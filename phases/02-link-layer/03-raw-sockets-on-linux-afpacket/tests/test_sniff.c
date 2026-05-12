/* test_sniff.c -- unit tests for the AF_PACKET sniff helpers.
 *
 * These tests verify struct layout, parsing logic, and string formatting
 * WITHOUT needing root or a real network interface.
 *
 * Compile + run:
 *     make
 *     ./test_sniff
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

#include "../sniff.h"

/* ---- Minimal test framework ---- */

static int tests_run    = 0;
static int tests_passed = 0;

#define ASSERT_EQ(a, b, msg) do {                                       \
    tests_run++;                                                        \
    if ((a) == (b)) {                                                   \
        tests_passed++;                                                 \
    } else {                                                            \
        printf("FAIL: %s (line %d): got %lld, expected %lld\n",        \
               (msg), __LINE__,                                         \
               (long long)(a), (long long)(b));                         \
    }                                                                   \
} while (0)

#define ASSERT_STR_EQ(a, b, msg) do {                                   \
    tests_run++;                                                        \
    if (strcmp((a), (b)) == 0) {                                        \
        tests_passed++;                                                 \
    } else {                                                            \
        printf("FAIL: %s (line %d): got \"%s\", expected \"%s\"\n",    \
               (msg), __LINE__, (a), (b));                              \
    }                                                                   \
} while (0)

#define ASSERT_NOT_NULL(ptr, msg) do {                                  \
    tests_run++;                                                        \
    if ((ptr) != NULL) {                                                \
        tests_passed++;                                                 \
    } else {                                                            \
        printf("FAIL: %s (line %d): pointer is NULL\n",                \
               (msg), __LINE__);                                        \
    }                                                                   \
} while (0)

#define ASSERT_NULL(ptr, msg) do {                                      \
    tests_run++;                                                        \
    if ((ptr) == NULL) {                                                \
        tests_passed++;                                                 \
    } else {                                                            \
        printf("FAIL: %s (line %d): pointer is not NULL\n",            \
               (msg), __LINE__);                                        \
    }                                                                   \
} while (0)

/* ================================================================== */
/*  Pinned tests: struct sizes and field offsets                        */
/* ================================================================== */

static void test_eth_header_size(void)
{
    ASSERT_EQ(sizeof(struct nfs_eth_header), 14, "eth_header size == 14");
}

static void test_eth_header_offsets(void)
{
    ASSERT_EQ(offsetof(struct nfs_eth_header, dst),       0, "dst offset");
    ASSERT_EQ(offsetof(struct nfs_eth_header, src),       6, "src offset");
    ASSERT_EQ(offsetof(struct nfs_eth_header, ethertype), 12, "ethertype offset");
}

static void test_parsed_frame_size(void)
{
    /* nfs_parsed_frame is NOT a wire format, so no pinned size.
     * But it must be large enough to hold all fields. */
    ASSERT_EQ(sizeof(((struct nfs_parsed_frame *)0)->dst), 6,
              "parsed_frame dst is 6 bytes");
    ASSERT_EQ(sizeof(((struct nfs_parsed_frame *)0)->src), 6,
              "parsed_frame src is 6 bytes");
}

/* ================================================================== */
/*  Pinned test: known frame bytes                                     */
/* ================================================================== */

static void test_parse_known_frame(void)
{
    /* Construct a minimal valid Ethernet frame:
     *   dst: ff:ff:ff:ff:ff:ff  (broadcast)
     *   src: 02:00:00:00:00:01
     *   ethertype: 0x0800 (IPv4), network byte order = 08 00
     *   payload: "HELLO" (5 bytes)
     */
    const uint8_t frame[] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   /* dst */
        0x02, 0x00, 0x00, 0x00, 0x00, 0x01,   /* src */
        0x08, 0x00,                             /* ethertype: IPv4 */
        'H', 'E', 'L', 'L', 'O'               /* payload */
    };

    struct nfs_parsed_frame pf;
    int rc = nfs_parse_eth_frame(frame, sizeof(frame), &pf);

    ASSERT_EQ(rc, 0, "parse returns 0");
    ASSERT_EQ(pf.dst[0], 0xff, "dst[0] == 0xff");
    ASSERT_EQ(pf.dst[5], 0xff, "dst[5] == 0xff");
    ASSERT_EQ(pf.src[0], 0x02, "src[0] == 0x02");
    ASSERT_EQ(pf.src[5], 0x01, "src[5] == 0x01");
    ASSERT_EQ(pf.ethertype, 0x0800, "ethertype == 0x0800 (host order)");
    ASSERT_EQ(pf.payload_len, 5, "payload_len == 5");
    ASSERT_EQ(memcmp(pf.payload, "HELLO", 5), 0, "payload == 'HELLO'");
}

static void test_parse_arp_frame(void)
{
    /* An ARP-typed frame (EtherType 0x0806). */
    const uint8_t frame[] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,   /* dst: broadcast */
        0xaa, 0xbb, 0xcc, 0x11, 0x22, 0x33,   /* src */
        0x08, 0x06,                             /* ethertype: ARP */
        0x00, 0x01,                             /* partial ARP payload */
    };

    struct nfs_parsed_frame pf;
    int rc = nfs_parse_eth_frame(frame, sizeof(frame), &pf);

    ASSERT_EQ(rc, 0, "parse ARP frame returns 0");
    ASSERT_EQ(pf.ethertype, 0x0806, "ethertype == 0x0806 (ARP)");
    ASSERT_EQ(pf.payload_len, 2, "ARP payload_len == 2");
}

/* ================================================================== */
/*  Edge cases                                                         */
/* ================================================================== */

static void test_parse_too_short(void)
{
    uint8_t tiny[] = {0x00, 0x01, 0x02};
    struct nfs_parsed_frame pf;

    ASSERT_EQ(nfs_parse_eth_frame(tiny, sizeof(tiny), &pf), -1,
              "reject < 14 bytes");
}

static void test_parse_exact_header(void)
{
    /* Exactly 14 bytes: valid header, zero-length payload. */
    const uint8_t frame[14] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x86, 0xdd   /* IPv6 */
    };

    struct nfs_parsed_frame pf;
    int rc = nfs_parse_eth_frame(frame, sizeof(frame), &pf);

    ASSERT_EQ(rc, 0, "exact header is valid");
    ASSERT_EQ(pf.ethertype, 0x86dd, "ethertype == IPv6");
    ASSERT_EQ(pf.payload_len, 0, "zero payload");
}

static void test_parse_null_buf(void)
{
    struct nfs_parsed_frame pf;
    ASSERT_EQ(nfs_parse_eth_frame(NULL, 100, &pf), -1,
              "reject NULL buf");
}

static void test_parse_null_out(void)
{
    uint8_t frame[14] = {0};
    ASSERT_EQ(nfs_parse_eth_frame(frame, sizeof(frame), NULL), -1,
              "reject NULL out");
}

/* ================================================================== */
/*  format_mac tests                                                   */
/* ================================================================== */

static void test_format_mac_basic(void)
{
    const uint8_t mac[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    char buf[18];
    char *ret = nfs_format_mac(mac, buf, sizeof(buf));

    ASSERT_NOT_NULL(ret, "format_mac returns non-NULL");
    ASSERT_STR_EQ(buf, "aa:bb:cc:dd:ee:ff", "format_mac output");
}

static void test_format_mac_zeros(void)
{
    const uint8_t mac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    char buf[18];
    nfs_format_mac(mac, buf, sizeof(buf));

    ASSERT_STR_EQ(buf, "00:00:00:00:00:00", "all-zero MAC");
}

static void test_format_mac_broadcast(void)
{
    const uint8_t mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    char buf[18];
    nfs_format_mac(mac, buf, sizeof(buf));

    ASSERT_STR_EQ(buf, "ff:ff:ff:ff:ff:ff", "broadcast MAC");
}

static void test_format_mac_buf_too_small(void)
{
    const uint8_t mac[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    char buf[10];
    char *ret = nfs_format_mac(mac, buf, sizeof(buf));

    ASSERT_NULL(ret, "format_mac rejects small buffer");
}

static void test_format_mac_null_mac(void)
{
    char buf[18];
    char *ret = nfs_format_mac(NULL, buf, sizeof(buf));
    ASSERT_NULL(ret, "format_mac rejects NULL mac");
}

/* ================================================================== */
/*  ethertype_str tests                                                */
/* ================================================================== */

static void test_ethertype_str(void)
{
    ASSERT_STR_EQ(nfs_ethertype_str(0x0800), "IPv4",       "0x0800 -> IPv4");
    ASSERT_STR_EQ(nfs_ethertype_str(0x0806), "ARP",        "0x0806 -> ARP");
    ASSERT_STR_EQ(nfs_ethertype_str(0x86dd), "IPv6",       "0x86dd -> IPv6");
    ASSERT_STR_EQ(nfs_ethertype_str(0x8100), "802.1Q VLAN","0x8100 -> VLAN");
    ASSERT_STR_EQ(nfs_ethertype_str(0x1234), "unknown",    "0x1234 -> unknown");
}

/* ================================================================== */
/*  Property test: any 14+ byte buffer round-trips                     */
/* ================================================================== */

static void test_parse_preserves_bytes(void)
{
    /* Fill a frame with a recognisable pattern and verify that the
     * parser faithfully reproduces the MAC addresses and EtherType. */
    uint8_t frame[64];
    for (size_t i = 0; i < sizeof(frame); i++) {
        frame[i] = (uint8_t)(i & 0xff);
    }
    /* Byte 0..5 = dst, 6..11 = src, 12-13 = ethertype */

    struct nfs_parsed_frame pf;
    int rc = nfs_parse_eth_frame(frame, sizeof(frame), &pf);

    ASSERT_EQ(rc, 0, "pattern frame parses");
    ASSERT_EQ(memcmp(pf.dst, frame, 6), 0, "dst matches raw bytes");
    ASSERT_EQ(memcmp(pf.src, frame + 6, 6), 0, "src matches raw bytes");

    /* EtherType in frame is {0x0c, 0x0d} -> host order 0x0c0d */
    ASSERT_EQ(pf.ethertype, 0x0c0d, "ethertype from pattern");
    ASSERT_EQ(pf.payload_len, 50, "payload = 64 - 14 = 50");
}

/* ================================================================== */
/*  Runner                                                             */
/* ================================================================== */

int main(void)
{
    /* struct layout */
    test_eth_header_size();
    test_eth_header_offsets();
    test_parsed_frame_size();

    /* parsing known frames */
    test_parse_known_frame();
    test_parse_arp_frame();

    /* edge cases */
    test_parse_too_short();
    test_parse_exact_header();
    test_parse_null_buf();
    test_parse_null_out();

    /* format_mac */
    test_format_mac_basic();
    test_format_mac_zeros();
    test_format_mac_broadcast();
    test_format_mac_buf_too_small();
    test_format_mac_null_mac();

    /* ethertype_str */
    test_ethertype_str();

    /* property */
    test_parse_preserves_bytes();

    printf("\n%d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("all sniff tests passed\n");
        return 0;
    }
    return 1;
}
