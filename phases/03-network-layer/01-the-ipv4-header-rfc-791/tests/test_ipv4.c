/* Tests for IPv4 header parse/build (RFC 791).
 *
 * Three families:
 *   1. Pinned cases — known-answer inputs from real captures.
 *   2. Roundtrip — build then parse and compare all fields.
 *   3. Error paths — bad version, short buffer, bad checksum.
 */

#include "../ipv4.h"
#include "../../../../common/c/checksum.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Minimal test harness                                               */
/* ------------------------------------------------------------------ */

static int tests_run = 0;
static int tests_failed = 0;

#define ASSERT_EQ(got, want, fmt)                                       \
    do {                                                                \
        if ((got) != (want)) {                                          \
            fprintf(stderr, "  FAIL %s:%d: " #got " = " fmt            \
                    ", want " fmt "\n", __func__, __LINE__,             \
                    (got), (want));                                     \
            tests_failed++;                                             \
            return;                                                     \
        }                                                               \
    } while (0)

#define ASSERT_STR_EQ(got, want)                                        \
    do {                                                                \
        if (strcmp((got), (want)) != 0) {                               \
            fprintf(stderr, "  FAIL %s:%d: \"%s\" != \"%s\"\n",        \
                    __func__, __LINE__, (got), (want));                 \
            tests_failed++;                                             \
            return;                                                     \
        }                                                               \
    } while (0)

#define RUN(fn)                                                         \
    do {                                                                \
        tests_run++;                                                    \
        printf("  %-45s ", #fn);                                        \
        fn();                                                           \
        if (tests_failed == 0 || tests_failed == prev_fail)             \
            printf("OK\n");                                             \
        prev_fail = tests_failed;                                       \
    } while (0)

/* ------------------------------------------------------------------ */
/*  A real ICMP echo request header captured with tcpdump.             */
/*  src=10.0.0.1, dst=10.0.0.2, TTL=64, proto=ICMP, total_len=84     */
/*  Checksum field = 0x26a7 (verified by Wireshark).                   */
/* ------------------------------------------------------------------ */

static const uint8_t REAL_PKT[] = {
    0x45, 0x00, 0x00, 0x54,    /* ver=4, IHL=5, DSCP=0, ECN=0, len=84 */
    0x00, 0x00, 0x40, 0x00,    /* id=0, flags=0x2 (DF), frag_off=0    */
    0x40, 0x01,                 /* TTL=64, proto=ICMP                   */
    0x26, 0xa7,                 /* checksum                             */
    0x0a, 0x00, 0x00, 0x01,    /* src 10.0.0.1                         */
    0x0a, 0x00, 0x00, 0x02     /* dst 10.0.0.2                         */
};

/* ------------------------------------------------------------------ */
/*  Tests                                                              */
/* ------------------------------------------------------------------ */

/* 1. The parsed struct contains the correct field values for a known packet. */
static void test_parse_real_packet(void) {
    struct nfs_ipv4_hdr h;
    int rc = nfs_ipv4_parse(REAL_PKT, sizeof(REAL_PKT), &h);
    ASSERT_EQ(rc, NFS_IPV4_OK, "%d");

    ASSERT_EQ(h.version,        4,      "%u");
    ASSERT_EQ(h.ihl,            5,      "%u");
    ASSERT_EQ(h.dscp,           0,      "%u");
    ASSERT_EQ(h.ecn,            0,      "%u");
    ASSERT_EQ(h.total_length,   84,     "%u");
    ASSERT_EQ(h.identification, 0,      "%u");
    ASSERT_EQ(h.flags,          0x2,    "%u");   /* DF */
    ASSERT_EQ(h.frag_offset,    0,      "%u");
    ASSERT_EQ(h.ttl,            64,     "%u");
    ASSERT_EQ(h.protocol,       1,      "%u");   /* ICMP */
    ASSERT_EQ(h.checksum,       0x26a7, "0x%04x");
    ASSERT_EQ(h.src_addr, 0x0a000001u, "0x%08x"); /* 10.0.0.1 */
    ASSERT_EQ(h.dst_addr, 0x0a000002u, "0x%08x"); /* 10.0.0.2 */
}

/* 2. Build a header, then parse it back — all fields must survive. */
static void test_build_roundtrip(void) {
    struct nfs_ipv4_hdr orig = {
        .version        = 4,
        .ihl            = 5,
        .dscp           = 46,     /* EF PHB */
        .ecn            = 1,      /* ECT(1) */
        .total_length   = 1500,
        .identification = 0xABCD,
        .flags          = NFS_IPV4_FLAG_DF,
        .frag_offset    = 0,
        .ttl            = 128,
        .protocol       = NFS_IPPROTO_TCP,
        .checksum       = 0,      /* build will compute */
        .src_addr       = 0xC0A80001u,  /* 192.168.0.1 */
        .dst_addr       = 0xC0A800FEu,  /* 192.168.0.254 */
    };

    uint8_t wire[20];
    size_t n = nfs_ipv4_build(&orig, wire, sizeof(wire));
    ASSERT_EQ(n, (size_t)20, "%zu");

    struct nfs_ipv4_hdr parsed;
    int rc = nfs_ipv4_parse(wire, n, &parsed);
    ASSERT_EQ(rc, NFS_IPV4_OK, "%d");

    ASSERT_EQ(parsed.version,        orig.version,        "%u");
    ASSERT_EQ(parsed.ihl,            orig.ihl,            "%u");
    ASSERT_EQ(parsed.dscp,           orig.dscp,           "%u");
    ASSERT_EQ(parsed.ecn,            orig.ecn,            "%u");
    ASSERT_EQ(parsed.total_length,   orig.total_length,   "%u");
    ASSERT_EQ(parsed.identification, orig.identification,  "%u");
    ASSERT_EQ(parsed.flags,          orig.flags,          "%u");
    ASSERT_EQ(parsed.frag_offset,    orig.frag_offset,    "%u");
    ASSERT_EQ(parsed.ttl,            orig.ttl,            "%u");
    ASSERT_EQ(parsed.protocol,       orig.protocol,       "%u");
    ASSERT_EQ(parsed.src_addr,       orig.src_addr,       "0x%08x");
    ASSERT_EQ(parsed.dst_addr,       orig.dst_addr,       "0x%08x");
}

/* 3. The checksum over the real packet (including checksum field) is valid. */
static void test_checksum_valid(void) {
    uint16_t cs = nfs_ipv4_checksum(REAL_PKT, sizeof(REAL_PKT));
    /* RFC 1071: checksum of data that includes a correct checksum field
     * must be zero.  internet_checksum returns ~sum, so 0x0000. */
    ASSERT_EQ(cs, (uint16_t)0x0000, "0x%04x");
}

/* 4. Verify that the one's complement sum of the header (with checksum
 *    field included) equals 0xFFFF before the final NOT. We test this
 *    indirectly: the folded partial sum should be 0xFFFF. */
static void test_checksum_zero_after_include(void) {
    uint32_t partial = internet_checksum_partial(REAL_PKT, sizeof(REAL_PKT), 0);
    /* Fold without the NOT — this is the raw one's complement sum. */
    while (partial >> 16)
        partial = (partial & 0xFFFF) + (partial >> 16);
    ASSERT_EQ((uint16_t)partial, (uint16_t)0xFFFF, "0x%04x");
}

/* 5. Version != 4 must be rejected. */
static void test_bad_version(void) {
    uint8_t pkt[20];
    memcpy(pkt, REAL_PKT, sizeof(pkt));
    /* Set version to 6, keep IHL=5. */
    pkt[0] = 0x65;
    /* Recompute checksum so only the version check triggers. */
    pkt[10] = 0;
    pkt[11] = 0;
    uint16_t cs = internet_checksum(pkt, 20);
    pkt[10] = (uint8_t)(cs >> 8);
    pkt[11] = (uint8_t)(cs & 0xFF);

    struct nfs_ipv4_hdr h;
    int rc = nfs_ipv4_parse(pkt, sizeof(pkt), &h);
    ASSERT_EQ(rc, NFS_IPV4_ERR_VERSION, "%d");
}

/* 6. Buffer shorter than 20 bytes must be rejected. */
static void test_short_packet(void) {
    uint8_t pkt[19] = {0};
    struct nfs_ipv4_hdr h;

    int rc = nfs_ipv4_parse(pkt, sizeof(pkt), &h);
    ASSERT_EQ(rc, NFS_IPV4_ERR_SHORT, "%d");

    /* Also test zero-length. */
    rc = nfs_ipv4_parse(pkt, 0, &h);
    ASSERT_EQ(rc, NFS_IPV4_ERR_SHORT, "%d");
}

/* 7. A corrupted checksum must be detected. */
static void test_bad_checksum(void) {
    uint8_t pkt[20];
    memcpy(pkt, REAL_PKT, sizeof(pkt));
    /* Flip a bit in the checksum. */
    pkt[10] ^= 0x01;

    struct nfs_ipv4_hdr h;
    int rc = nfs_ipv4_parse(pkt, sizeof(pkt), &h);
    ASSERT_EQ(rc, NFS_IPV4_ERR_CHECKSUM, "%d");
}

/* 8. Format address produces correct dotted decimal. */
static void test_format_addr(void) {
    char buf[16];
    nfs_ipv4_format_addr(0x0a000001u, buf);
    ASSERT_STR_EQ(buf, "10.0.0.1");

    nfs_ipv4_format_addr(0xC0A80001u, buf);
    ASSERT_STR_EQ(buf, "192.168.0.1");

    nfs_ipv4_format_addr(0xFFFFFFFFu, buf);
    ASSERT_STR_EQ(buf, "255.255.255.255");

    nfs_ipv4_format_addr(0x00000000u, buf);
    ASSERT_STR_EQ(buf, "0.0.0.0");
}

/* 9. Protocol name helper. */
static void test_protocol_name(void) {
    ASSERT_STR_EQ(nfs_ipv4_protocol_name(1),  "ICMP");
    ASSERT_STR_EQ(nfs_ipv4_protocol_name(6),  "TCP");
    ASSERT_STR_EQ(nfs_ipv4_protocol_name(17), "UDP");
    ASSERT_STR_EQ(nfs_ipv4_protocol_name(41), "IPv6-in-IPv4");
    ASSERT_STR_EQ(nfs_ipv4_protocol_name(255), "unknown");
}

/* 10. Build with buffer too small returns 0. */
static void test_build_too_small(void) {
    struct nfs_ipv4_hdr h = { .version = 4, .ihl = 5 };
    uint8_t buf[19];
    size_t n = nfs_ipv4_build(&h, buf, sizeof(buf));
    ASSERT_EQ(n, (size_t)0, "%zu");
}

/* 11. Roundtrip with fragment fields. */
static void test_fragment_fields(void) {
    struct nfs_ipv4_hdr orig = {
        .version        = 4,
        .ihl            = 5,
        .dscp           = 0,
        .ecn            = 0,
        .total_length   = 576,
        .identification = 0x1234,
        .flags          = NFS_IPV4_FLAG_MF,  /* More Fragments */
        .frag_offset    = 185,               /* 185 * 8 = 1480 bytes */
        .ttl            = 30,
        .protocol       = NFS_IPPROTO_UDP,
        .checksum       = 0,
        .src_addr       = 0x08080808u,  /* 8.8.8.8 */
        .dst_addr       = 0x01010101u,  /* 1.1.1.1 */
    };

    uint8_t wire[20];
    size_t n = nfs_ipv4_build(&orig, wire, sizeof(wire));
    ASSERT_EQ(n, (size_t)20, "%zu");

    struct nfs_ipv4_hdr parsed;
    int rc = nfs_ipv4_parse(wire, n, &parsed);
    ASSERT_EQ(rc, NFS_IPV4_OK, "%d");

    ASSERT_EQ(parsed.flags,       NFS_IPV4_FLAG_MF, "%u");
    ASSERT_EQ(parsed.frag_offset, (uint16_t)185,    "%u");
    ASSERT_EQ(parsed.identification, (uint16_t)0x1234, "0x%04x");
}

/* ------------------------------------------------------------------ */
/*  Main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    int prev_fail = 0;

    printf("IPv4 header test suite\n");

    RUN(test_parse_real_packet);
    RUN(test_build_roundtrip);
    RUN(test_checksum_valid);
    RUN(test_checksum_zero_after_include);
    RUN(test_bad_version);
    RUN(test_short_packet);
    RUN(test_bad_checksum);
    RUN(test_format_addr);
    RUN(test_protocol_name);
    RUN(test_build_too_small);
    RUN(test_fragment_fields);

    printf("\n%d tests, %d failed\n", tests_run, tests_failed);
    if (tests_failed > 0) {
        printf("FAIL\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
