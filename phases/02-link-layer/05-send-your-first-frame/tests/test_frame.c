/* Unit tests for Ethernet frame construction, parsing, and utilities. */

#include "../frame.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
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

/* ------------------------------------------------------------------ */
/*  1. Build minimal frame: 1-byte payload -> padded to 60 bytes       */
/* ------------------------------------------------------------------ */
static void test_build_minimal(void) {
    uint8_t dst[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t src[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t payload[1] = {0x42};

    uint8_t out[1518];
    int len = nfs_frame_build(dst, src, NFS_ETHERTYPE_IP, payload, 1, out, sizeof(out));
    ASSERT_EQ(len, 60); /* 14 header + 46 padded payload */
}

/* ------------------------------------------------------------------ */
/*  2. Build max frame: 1500-byte payload -> 1514 bytes total          */
/* ------------------------------------------------------------------ */
static void test_build_max(void) {
    uint8_t dst[6] = {0};
    uint8_t src[6] = {0};
    uint8_t payload[1500];
    memset(payload, 0xAB, sizeof(payload));

    uint8_t out[1518];
    int len = nfs_frame_build(dst, src, NFS_ETHERTYPE_IP, payload, 1500, out, sizeof(out));
    ASSERT_EQ(len, 1514);
}

/* ------------------------------------------------------------------ */
/*  3. Build rejects oversized: 1501 bytes -> -1                       */
/* ------------------------------------------------------------------ */
static void test_build_oversized(void) {
    uint8_t dst[6] = {0};
    uint8_t src[6] = {0};
    uint8_t payload[1501];
    memset(payload, 0xCC, sizeof(payload));

    uint8_t out[2048];
    int len = nfs_frame_build(dst, src, NFS_ETHERTYPE_IP, payload, 1501, out, sizeof(out));
    ASSERT_EQ(len, -1);
}

/* ------------------------------------------------------------------ */
/*  4. Build rejects buffer too small                                  */
/* ------------------------------------------------------------------ */
static void test_build_buffer_too_small(void) {
    uint8_t dst[6] = {0};
    uint8_t src[6] = {0};
    uint8_t payload[1] = {0x01};

    uint8_t out[20]; /* too small for 60-byte minimum */
    int len = nfs_frame_build(dst, src, NFS_ETHERTYPE_IP, payload, 1, out, sizeof(out));
    ASSERT_EQ(len, -1);
}

/* ------------------------------------------------------------------ */
/*  5. Parse roundtrip: build then parse, verify all fields match      */
/* ------------------------------------------------------------------ */
static void test_parse_roundtrip(void) {
    uint8_t dst[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    uint8_t src[6] = {0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x02};
    uint8_t payload[] = {0x45, 0x00, 0x00, 0x3C}; /* fake IP header */

    uint8_t out[1518];
    int len =
        nfs_frame_build(dst, src, NFS_ETHERTYPE_IP, payload, sizeof(payload), out, sizeof(out));
    ASSERT_EQ(len, 60);

    struct nfs_eth_frame frame;
    ASSERT_EQ(nfs_frame_parse(out, (size_t)len, &frame), 0);
    ASSERT_TRUE(memcmp(frame.dst, dst, 6) == 0);
    ASSERT_TRUE(memcmp(frame.src, src, 6) == 0);
    ASSERT_EQ(frame.ethertype, NFS_ETHERTYPE_IP);
    ASSERT_EQ(frame.payload_len, 46);
    /* First 4 bytes of payload should match our data. */
    ASSERT_TRUE(memcmp(frame.payload, payload, sizeof(payload)) == 0);
}

/* ------------------------------------------------------------------ */
/*  6. Parse extracts ethertype correctly                              */
/* ------------------------------------------------------------------ */
static void test_parse_ethertype(void) {
    uint8_t dst[6] = {0};
    uint8_t src[6] = {0};
    uint8_t payload[46];
    memset(payload, 0, sizeof(payload));

    uint8_t out[1518];
    int len =
        nfs_frame_build(dst, src, NFS_ETHERTYPE_IP, payload, sizeof(payload), out, sizeof(out));
    ASSERT_TRUE(len > 0);

    struct nfs_eth_frame frame;
    ASSERT_EQ(nfs_frame_parse(out, (size_t)len, &frame), 0);
    ASSERT_EQ(frame.ethertype, 0x0800);
}

/* ------------------------------------------------------------------ */
/*  7. Parse rejects too short: < 14 bytes -> -1                       */
/* ------------------------------------------------------------------ */
static void test_parse_too_short(void) {
    uint8_t data[13];
    memset(data, 0, sizeof(data));

    struct nfs_eth_frame frame;
    ASSERT_EQ(nfs_frame_parse(data, 13, &frame), -1);
    ASSERT_EQ(nfs_frame_parse(data, 0, &frame), -1);
}

/* ------------------------------------------------------------------ */
/*  8. Padding verification: 5-byte payload -> bytes 19-59 are zero    */
/* ------------------------------------------------------------------ */
static void test_padding_verification(void) {
    uint8_t dst[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t src[6] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    uint8_t payload[5] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

    uint8_t out[1518];
    int len = nfs_frame_build(dst, src, NFS_ETHERTYPE_IP, payload, 5, out, sizeof(out));
    ASSERT_EQ(len, 60);

    /* Bytes 14..18 are our payload. */
    ASSERT_EQ(out[14], 0xAA);
    ASSERT_EQ(out[18], 0xEE);

    /* Bytes 19..59 should be zero (padding). */
    for (int i = 19; i < 60; i++) {
        ASSERT_EQ(out[i], 0x00);
    }
}

/* ------------------------------------------------------------------ */
/*  9. Network byte order: ethertype 0x0800 -> bytes 12=0x08, 13=0x00 */
/* ------------------------------------------------------------------ */
static void test_network_byte_order(void) {
    uint8_t dst[6] = {0};
    uint8_t src[6] = {0};
    uint8_t payload[1] = {0x00};

    uint8_t out[1518];
    int len = nfs_frame_build(dst, src, 0x0800, payload, 1, out, sizeof(out));
    ASSERT_TRUE(len > 0);

    /* Check raw bytes on the wire. */
    ASSERT_EQ(out[12], 0x08);
    ASSERT_EQ(out[13], 0x00);
}

/* ------------------------------------------------------------------ */
/*  10. MAC parse valid: "AA:BB:CC:DD:EE:FF"                           */
/* ------------------------------------------------------------------ */
static void test_mac_parse_valid(void) {
    uint8_t mac[6];
    ASSERT_EQ(nfs_mac_parse("AA:BB:CC:DD:EE:FF", mac), 0);
    ASSERT_EQ(mac[0], 0xAA);
    ASSERT_EQ(mac[1], 0xBB);
    ASSERT_EQ(mac[2], 0xCC);
    ASSERT_EQ(mac[3], 0xDD);
    ASSERT_EQ(mac[4], 0xEE);
    ASSERT_EQ(mac[5], 0xFF);
}

/* ------------------------------------------------------------------ */
/*  11. MAC parse lowercase: "aa:bb:cc:dd:ee:ff"                       */
/* ------------------------------------------------------------------ */
static void test_mac_parse_lowercase(void) {
    uint8_t mac[6];
    ASSERT_EQ(nfs_mac_parse("aa:bb:cc:dd:ee:ff", mac), 0);
    ASSERT_EQ(mac[0], 0xAA);
    ASSERT_EQ(mac[1], 0xBB);
    ASSERT_EQ(mac[2], 0xCC);
    ASSERT_EQ(mac[3], 0xDD);
    ASSERT_EQ(mac[4], 0xEE);
    ASSERT_EQ(mac[5], 0xFF);
}

/* ------------------------------------------------------------------ */
/*  12. MAC parse invalid: "not-a-mac" -> -1                           */
/* ------------------------------------------------------------------ */
static void test_mac_parse_invalid(void) {
    uint8_t mac[6];
    ASSERT_EQ(nfs_mac_parse("not-a-mac", mac), -1);
}

/* ------------------------------------------------------------------ */
/*  13. MAC format: {0xDE,...} -> "de:ad:be:ef:00:01"                  */
/* ------------------------------------------------------------------ */
static void test_mac_format(void) {
    uint8_t mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    char buf[32];
    char *ret = nfs_mac_format(mac, buf, sizeof(buf));
    ASSERT_TRUE(ret != NULL);
    ASSERT_TRUE(ret == buf);
    ASSERT_TRUE(strcmp(buf, "de:ad:be:ef:00:01") == 0);
}

/* ------------------------------------------------------------------ */
/*  14. MAC broadcast: ff:ff:ff:ff:ff:ff -> is_broadcast=1             */
/* ------------------------------------------------------------------ */
static void test_mac_broadcast(void) {
    uint8_t mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ASSERT_EQ(nfs_mac_is_broadcast(mac), 1);
}

/* ------------------------------------------------------------------ */
/*  15. MAC multicast: 01:00:5e:00:00:01 -> multicast=1, broadcast=0  */
/* ------------------------------------------------------------------ */
static void test_mac_multicast(void) {
    uint8_t mac[6] = {0x01, 0x00, 0x5E, 0x00, 0x00, 0x01};
    ASSERT_EQ(nfs_mac_is_multicast(mac), 1);
    ASSERT_EQ(nfs_mac_is_broadcast(mac), 0);
}

/* ------------------------------------------------------------------ */
/*  16. MAC unicast: 00:11:22:33:44:55 -> is_multicast=0               */
/* ------------------------------------------------------------------ */
static void test_mac_unicast(void) {
    uint8_t mac[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    ASSERT_EQ(nfs_mac_is_multicast(mac), 0);
}

/* ------------------------------------------------------------------ */
/*  17. Frame valid: 60-byte frame with ethertype 0x0800 -> valid      */
/* ------------------------------------------------------------------ */
static void test_frame_valid(void) {
    uint8_t wire[60];
    memset(wire, 0, sizeof(wire));
    /* Set ethertype = 0x0800 at bytes 12-13 (network byte order). */
    wire[12] = 0x08;
    wire[13] = 0x00;

    ASSERT_EQ(nfs_frame_valid(wire, 60), 1);
}

/* ------------------------------------------------------------------ */
/*  18. Frame too short: 59 bytes -> invalid                           */
/* ------------------------------------------------------------------ */
static void test_frame_too_short(void) {
    uint8_t wire[59];
    memset(wire, 0, sizeof(wire));
    wire[12] = 0x08;
    wire[13] = 0x00;

    ASSERT_EQ(nfs_frame_valid(wire, 59), 0);
}

/* ------------------------------------------------------------------ */
/*  19. Frame too long: 1515 bytes -> invalid                          */
/* ------------------------------------------------------------------ */
static void test_frame_too_long(void) {
    uint8_t wire[1515];
    memset(wire, 0, sizeof(wire));
    wire[12] = 0x08;
    wire[13] = 0x00;

    ASSERT_EQ(nfs_frame_valid(wire, 1515), 0);
}

/* ------------------------------------------------------------------ */
/*  20. Frame 802.3 length: ethertype < 0x0600 -> invalid              */
/* ------------------------------------------------------------------ */
static void test_frame_802_3_length(void) {
    uint8_t wire[60];
    memset(wire, 0, sizeof(wire));
    /* Length field = 0x0500 — this is 802.3, not Ethernet II. */
    wire[12] = 0x05;
    wire[13] = 0x00;

    ASSERT_EQ(nfs_frame_valid(wire, 60), 0);
}

/* ------------------------------------------------------------------ */
/*  21. EtherType constants                                            */
/* ------------------------------------------------------------------ */
static void test_ethertype_constants(void) {
    ASSERT_EQ(NFS_ETHERTYPE_IP, 0x0800);
    ASSERT_EQ(NFS_ETHERTYPE_ARP, 0x0806);
    ASSERT_EQ(NFS_ETHERTYPE_IPV6, 0x86DD);
    ASSERT_EQ(NFS_ETHERTYPE_VLAN, 0x8100);
}

/* ------------------------------------------------------------------ */
/*  22. Raw addr init: verify fields stored correctly                  */
/* ------------------------------------------------------------------ */
static void test_raw_addr_init(void) {
    uint8_t dst[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    struct nfs_raw_addr addr;
    memset(&addr, 0, sizeof(addr));

    ASSERT_EQ(nfs_raw_addr_init(&addr, 7, dst, NFS_ETHERTYPE_ARP), 0);
    ASSERT_EQ(addr.ifindex, 7);
    ASSERT_TRUE(memcmp(addr.dst, dst, 6) == 0);
    ASSERT_EQ(addr.ethertype, NFS_ETHERTYPE_ARP);
}

/* ------------------------------------------------------------------ */
/*  23. NULL pointer handling: build/parse with NULL -> -1              */
/* ------------------------------------------------------------------ */
static void test_null_pointers(void) {
    uint8_t dst[6] = {0};
    uint8_t src[6] = {0};
    uint8_t payload[1] = {0};
    uint8_t out[128];
    struct nfs_eth_frame frame;

    /* Build with NULL dst. */
    ASSERT_EQ(nfs_frame_build(NULL, src, NFS_ETHERTYPE_IP, payload, 1, out, sizeof(out)), -1);
    /* Build with NULL src. */
    ASSERT_EQ(nfs_frame_build(dst, NULL, NFS_ETHERTYPE_IP, payload, 1, out, sizeof(out)), -1);
    /* Build with NULL out. */
    ASSERT_EQ(nfs_frame_build(dst, src, NFS_ETHERTYPE_IP, payload, 1, NULL, 128), -1);
    /* Parse with NULL data. */
    ASSERT_EQ(nfs_frame_parse(NULL, 60, &frame), -1);
    /* Parse with NULL frame. */
    uint8_t data[60];
    memset(data, 0, sizeof(data));
    ASSERT_EQ(nfs_frame_parse(data, 60, NULL), -1);
    /* Raw addr with NULL addr. */
    ASSERT_EQ(nfs_raw_addr_init(NULL, 1, dst, NFS_ETHERTYPE_IP), -1);
    /* Raw addr with NULL dst. */
    struct nfs_raw_addr addr;
    ASSERT_EQ(nfs_raw_addr_init(&addr, 1, NULL, NFS_ETHERTYPE_IP), -1);
}

/* ------------------------------------------------------------------ */
/*  24. Broadcast destination: build to ff:..., parse, verify          */
/* ------------------------------------------------------------------ */
static void test_broadcast_destination(void) {
    uint8_t dst[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t src[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t payload[10];
    memset(payload, 0x77, sizeof(payload));

    uint8_t out[1518];
    int len =
        nfs_frame_build(dst, src, NFS_ETHERTYPE_ARP, payload, sizeof(payload), out, sizeof(out));
    ASSERT_TRUE(len > 0);

    struct nfs_eth_frame frame;
    ASSERT_EQ(nfs_frame_parse(out, (size_t)len, &frame), 0);
    ASSERT_EQ(nfs_mac_is_broadcast(frame.dst), 1);
    ASSERT_EQ(nfs_mac_is_multicast(frame.dst), 1); /* broadcast is also multicast */
    ASSERT_EQ(frame.ethertype, NFS_ETHERTYPE_ARP);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_build_minimal();
    test_build_max();
    test_build_oversized();
    test_build_buffer_too_small();
    test_parse_roundtrip();
    test_parse_ethertype();
    test_parse_too_short();
    test_padding_verification();
    test_network_byte_order();
    test_mac_parse_valid();
    test_mac_parse_lowercase();
    test_mac_parse_invalid();
    test_mac_format();
    test_mac_broadcast();
    test_mac_multicast();
    test_mac_unicast();
    test_frame_valid();
    test_frame_too_short();
    test_frame_too_long();
    test_frame_802_3_length();
    test_ethertype_constants();
    test_raw_addr_init();
    test_null_pointers();
    test_broadcast_destination();

    printf("\n  %d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        fprintf(stderr, "  SOME TESTS FAILED\n");
        return 1;
    }
    printf("  All tests passed.\n");
    return 0;
}
