/* Unit tests for Ethernet II frame parsing and building. */

#include "../ethernet.h"

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
/*  Test: struct size                                                   */
/* ------------------------------------------------------------------ */
static void test_struct_size(void) {
    ASSERT_EQ(sizeof(struct nfs_eth_hdr), 14);
}

/* ------------------------------------------------------------------ */
/*  Test: parse a known frame                                          */
/* ------------------------------------------------------------------ */
static void test_parse_known_frame(void) {
    /* 60-byte frame: broadcast dst, known src, IPv4 ethertype */
    uint8_t frame[60];
    memset(frame, 0, sizeof(frame));
    memset(frame, 0xFF, 6); /* dst = broadcast */
    uint8_t src[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    memcpy(frame + 6, src, 6);
    frame[12] = 0x08;
    frame[13] = 0x00; /* IPv4 */

    struct nfs_eth_hdr hdr;
    const uint8_t *payload;
    size_t payload_len;

    ASSERT_EQ(nfs_eth_parse(frame, sizeof(frame), &hdr, &payload, &payload_len), 0);
    ASSERT_EQ(hdr.dst[0], 0xFF);
    ASSERT_EQ(hdr.dst[5], 0xFF);
    ASSERT_EQ(hdr.src[0], 0x00);
    ASSERT_EQ(hdr.src[1], 0x11);
    ASSERT_EQ(hdr.src[5], 0x55);
    ASSERT_EQ(hdr.ethertype, 0x0800);
    ASSERT_EQ(payload_len, 46);
    ASSERT_TRUE(payload == frame + 14);
}

/* ------------------------------------------------------------------ */
/*  Test: parse ARP ethertype                                          */
/* ------------------------------------------------------------------ */
static void test_parse_arp(void) {
    uint8_t frame[60];
    memset(frame, 0, sizeof(frame));
    frame[12] = 0x08;
    frame[13] = 0x06; /* ARP */

    struct nfs_eth_hdr hdr;
    const uint8_t *payload;
    size_t payload_len;

    ASSERT_EQ(nfs_eth_parse(frame, sizeof(frame), &hdr, &payload, &payload_len), 0);
    ASSERT_EQ(hdr.ethertype, 0x0806);
}

/* ------------------------------------------------------------------ */
/*  Test: short frame rejection                                        */
/* ------------------------------------------------------------------ */
static void test_short_frame_rejected(void) {
    uint8_t frame[13];
    memset(frame, 0, sizeof(frame));

    struct nfs_eth_hdr hdr;
    const uint8_t *payload;
    size_t payload_len;

    /* 13 bytes — too short for header */
    ASSERT_EQ(nfs_eth_parse(frame, 13, &hdr, &payload, &payload_len), -1);
    /* 0 bytes */
    ASSERT_EQ(nfs_eth_parse(frame, 0, &hdr, &payload, &payload_len), -1);
    /* NULL frame */
    ASSERT_EQ(nfs_eth_parse(NULL, 60, &hdr, &payload, &payload_len), -1);
}

/* ------------------------------------------------------------------ */
/*  Test: minimum 14-byte frame accepted                               */
/* ------------------------------------------------------------------ */
static void test_min_frame_accepted(void) {
    uint8_t frame[14];
    memset(frame, 0xAA, sizeof(frame));
    frame[12] = 0x86;
    frame[13] = 0xDD; /* IPv6 */

    struct nfs_eth_hdr hdr;
    const uint8_t *payload;
    size_t payload_len;

    ASSERT_EQ(nfs_eth_parse(frame, 14, &hdr, &payload, &payload_len), 0);
    ASSERT_EQ(payload_len, 0);
    ASSERT_EQ(hdr.ethertype, 0x86DD);
}

/* ------------------------------------------------------------------ */
/*  Test: build + parse roundtrip                                      */
/* ------------------------------------------------------------------ */
static void test_build_parse_roundtrip(void) {
    uint8_t dst[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    uint8_t src[6] = {0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x02};
    uint8_t data[] = {0x45, 0x00, 0x00, 0x3C}; /* fake IP header start */

    uint8_t out[1518];
    int len = nfs_eth_build(dst, src, NFS_ETHERTYPE_IPV4, data, sizeof(data), out, sizeof(out));

    /* Minimum frame: 14 + 46 = 60 */
    ASSERT_EQ(len, 60);

    struct nfs_eth_hdr hdr;
    const uint8_t *payload;
    size_t payload_len;
    ASSERT_EQ(nfs_eth_parse(out, (size_t)len, &hdr, &payload, &payload_len), 0);
    ASSERT_EQ(hdr.ethertype, NFS_ETHERTYPE_IPV4);
    ASSERT_TRUE(memcmp(hdr.dst, dst, 6) == 0);
    ASSERT_TRUE(memcmp(hdr.src, src, 6) == 0);
    ASSERT_EQ(payload_len, 46);
    /* First 4 bytes of payload match our data */
    ASSERT_TRUE(memcmp(payload, data, sizeof(data)) == 0);
    /* Padding bytes should be zero */
    ASSERT_EQ(payload[sizeof(data)], 0x00);
}

/* ------------------------------------------------------------------ */
/*  Test: build with large payload (no padding needed)                 */
/* ------------------------------------------------------------------ */
static void test_build_large_payload(void) {
    uint8_t dst[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t src[6] = {0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    uint8_t data[100];
    memset(data, 0xBB, sizeof(data));

    uint8_t out[1518];
    int len = nfs_eth_build(dst, src, NFS_ETHERTYPE_ARP, data, sizeof(data), out, sizeof(out));

    ASSERT_EQ(len, 14 + 100);

    struct nfs_eth_hdr hdr;
    const uint8_t *payload;
    size_t payload_len;
    ASSERT_EQ(nfs_eth_parse(out, (size_t)len, &hdr, &payload, &payload_len), 0);
    ASSERT_EQ(payload_len, 100);
    ASSERT_EQ(hdr.ethertype, NFS_ETHERTYPE_ARP);
}

/* ------------------------------------------------------------------ */
/*  Test: build buffer too small                                       */
/* ------------------------------------------------------------------ */
static void test_build_buffer_too_small(void) {
    uint8_t dst[6] = {0};
    uint8_t src[6] = {0};
    uint8_t data[] = {0x01};

    uint8_t out[20]; /* too small for 60-byte minimum frame */
    int len = nfs_eth_build(dst, src, NFS_ETHERTYPE_IPV4, data, sizeof(data), out, sizeof(out));
    ASSERT_EQ(len, -1);
}

/* ------------------------------------------------------------------ */
/*  Test: ethertype names                                              */
/* ------------------------------------------------------------------ */
static void test_ethertype_names(void) {
    ASSERT_TRUE(strcmp(nfs_ethertype_name(0x0800), "IPv4") == 0);
    ASSERT_TRUE(strcmp(nfs_ethertype_name(0x0806), "ARP") == 0);
    ASSERT_TRUE(strcmp(nfs_ethertype_name(0x86DD), "IPv6") == 0);
    ASSERT_TRUE(strcmp(nfs_ethertype_name(0x8100), "802.1Q") == 0);
    ASSERT_TRUE(strcmp(nfs_ethertype_name(0x9999), "Unknown") == 0);
    ASSERT_TRUE(strcmp(nfs_ethertype_name(0x0000), "Unknown") == 0);
}

/* ------------------------------------------------------------------ */
/*  Test: MAC format                                                   */
/* ------------------------------------------------------------------ */
static void test_mac_format(void) {
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    char buf[32];
    ASSERT_EQ(nfs_mac_format(mac, buf, sizeof(buf)), 0);
    ASSERT_TRUE(strcmp(buf, "aa:bb:cc:dd:ee:ff") == 0);

    /* All zeros */
    uint8_t zero[6] = {0};
    ASSERT_EQ(nfs_mac_format(zero, buf, sizeof(buf)), 0);
    ASSERT_TRUE(strcmp(buf, "00:00:00:00:00:00") == 0);

    /* Buffer too small */
    ASSERT_EQ(nfs_mac_format(mac, buf, 5), -1);
}

/* ------------------------------------------------------------------ */
/*  Test: MAC parse                                                    */
/* ------------------------------------------------------------------ */
static void test_mac_parse(void) {
    uint8_t mac[6];
    ASSERT_EQ(nfs_mac_parse("aa:bb:cc:dd:ee:ff", mac), 0);
    ASSERT_EQ(mac[0], 0xAA);
    ASSERT_EQ(mac[1], 0xBB);
    ASSERT_EQ(mac[5], 0xFF);

    ASSERT_EQ(nfs_mac_parse("00:00:00:00:00:00", mac), 0);
    ASSERT_EQ(mac[0], 0x00);
    ASSERT_EQ(mac[5], 0x00);

    /* Upper case */
    ASSERT_EQ(nfs_mac_parse("FF:EE:DD:CC:BB:AA", mac), 0);
    ASSERT_EQ(mac[0], 0xFF);
    ASSERT_EQ(mac[5], 0xAA);

    /* Bad input */
    ASSERT_EQ(nfs_mac_parse(NULL, mac), -1);
    ASSERT_EQ(nfs_mac_parse("not-a-mac", mac), -1);
}

/* ------------------------------------------------------------------ */
/*  Test: MAC format+parse roundtrip                                   */
/* ------------------------------------------------------------------ */
static void test_mac_roundtrip(void) {
    uint8_t original[6] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    char buf[32];
    ASSERT_EQ(nfs_mac_format(original, buf, sizeof(buf)), 0);

    uint8_t parsed[6];
    ASSERT_EQ(nfs_mac_parse(buf, parsed), 0);
    ASSERT_TRUE(memcmp(original, parsed, 6) == 0);
}

/* ------------------------------------------------------------------ */
/*  Test: eth_format                                                   */
/* ------------------------------------------------------------------ */
static void test_eth_format(void) {
    struct nfs_eth_hdr hdr;
    uint8_t src[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t dst[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    memcpy(hdr.src, src, 6);
    memcpy(hdr.dst, dst, 6);
    hdr.ethertype = 0x0800;

    char buf[128];
    nfs_eth_format(&hdr, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "00:11:22:33:44:55") != NULL);
    ASSERT_TRUE(strstr(buf, "ff:ff:ff:ff:ff:ff") != NULL);
    ASSERT_TRUE(strstr(buf, "IPv4") != NULL);
}

/* ------------------------------------------------------------------ */
/*  Test: build with zero-length payload still pads                    */
/* ------------------------------------------------------------------ */
static void test_build_zero_payload(void) {
    uint8_t dst[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t src[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    uint8_t out[128];

    int len = nfs_eth_build(dst, src, NFS_ETHERTYPE_ARP, NULL, 0, out, sizeof(out));
    ASSERT_EQ(len, 60); /* 14 header + 46 padding */
}

/* ------------------------------------------------------------------ */
/*  Test: build with exactly 46-byte payload (no extra padding)        */
/* ------------------------------------------------------------------ */
static void test_build_exact_min_payload(void) {
    uint8_t dst[6] = {0};
    uint8_t src[6] = {0};
    uint8_t data[46];
    memset(data, 0xCC, sizeof(data));

    uint8_t out[128];
    int len = nfs_eth_build(dst, src, NFS_ETHERTYPE_IPV6, data, sizeof(data), out, sizeof(out));
    ASSERT_EQ(len, 60);

    struct nfs_eth_hdr hdr;
    const uint8_t *payload;
    size_t payload_len;
    ASSERT_EQ(nfs_eth_parse(out, (size_t)len, &hdr, &payload, &payload_len), 0);
    ASSERT_EQ(payload_len, 46);
    ASSERT_EQ(payload[0], 0xCC);
    ASSERT_EQ(payload[45], 0xCC);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_struct_size();
    test_parse_known_frame();
    test_parse_arp();
    test_short_frame_rejected();
    test_min_frame_accepted();
    test_build_parse_roundtrip();
    test_build_large_payload();
    test_build_buffer_too_small();
    test_ethertype_names();
    test_mac_format();
    test_mac_parse();
    test_mac_roundtrip();
    test_eth_format();
    test_build_zero_payload();
    test_build_exact_min_payload();

    printf("\n  %d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        fprintf(stderr, "  SOME TESTS FAILED\n");
        return 1;
    }
    printf("  All tests passed.\n");
    return 0;
}
