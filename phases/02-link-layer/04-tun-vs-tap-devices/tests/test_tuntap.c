#include "tuntap.h"

#include <arpa/inet.h>
#include <linux/if_tun.h>
#include <stdio.h>
#include <string.h>

/* ---- Minimal test framework ---- */

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

/* ---- Test: prepare_ifr TUN mode ---- */

static void test_prepare_ifr_tun(void) {
    struct ifreq ifr;
    int rc = nfs_tuntap_prepare_ifr(&ifr, "tun0", NFS_TUN_MODE, 0);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(ifr.ifr_flags & IFF_TUN, IFF_TUN);
    ASSERT_TRUE(strcmp(ifr.ifr_name, "tun0") == 0);
}

/* ---- Test: prepare_ifr TAP mode ---- */

static void test_prepare_ifr_tap(void) {
    struct ifreq ifr;
    int rc = nfs_tuntap_prepare_ifr(&ifr, "tap0", NFS_TAP_MODE, 0);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(ifr.ifr_flags & IFF_TAP, IFF_TAP);
    ASSERT_TRUE(strcmp(ifr.ifr_name, "tap0") == 0);
}

/* ---- Test: prepare_ifr NO_PI ---- */

static void test_prepare_ifr_no_pi(void) {
    struct ifreq ifr;
    int rc = nfs_tuntap_prepare_ifr(&ifr, "tun1", NFS_TUN_MODE, 1);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(ifr.ifr_flags & IFF_NO_PI, IFF_NO_PI);
    ASSERT_EQ(ifr.ifr_flags & IFF_TUN, IFF_TUN);
}

/* ---- Test: prepare_ifr name too long ---- */

static void test_prepare_ifr_name_too_long(void) {
    struct ifreq ifr;
    /* 16 characters -- exceeds IFNAMSIZ-1 = 15 */
    int rc = nfs_tuntap_prepare_ifr(&ifr, "abcdefghijklmnop", NFS_TUN_MODE, 0);
    ASSERT_EQ(rc, -1);
}

/* ---- Test: prepare_ifr null name ---- */

static void test_prepare_ifr_null_name(void) {
    struct ifreq ifr;
    int rc = nfs_tuntap_prepare_ifr(&ifr, NULL, NFS_TUN_MODE, 0);
    ASSERT_EQ(rc, -1);
}

/* ---- Test: prepare_ifr empty name ---- */

static void test_prepare_ifr_empty_name(void) {
    struct ifreq ifr;
    /* Empty string is valid -- kernel auto-assigns. */
    int rc = nfs_tuntap_prepare_ifr(&ifr, "", NFS_TUN_MODE, 0);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(ifr.ifr_name[0], '\0');
}

/* ---- Test: PI header build/parse roundtrip ---- */

static void test_pi_roundtrip(void) {
    uint8_t buf[NFS_PI_HDR_SIZE];
    int written = nfs_pi_build(buf, sizeof(buf), 0, 0x0800);
    ASSERT_EQ(written, NFS_PI_HDR_SIZE);

    uint16_t flags, proto;
    int rc = nfs_pi_parse(buf, sizeof(buf), &flags, &proto);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(flags, 0);
    ASSERT_EQ(proto, 0x0800);
}

/* ---- Test: PI header parse IPv6 ---- */

static void test_pi_parse_ipv6(void) {
    uint8_t buf[NFS_PI_HDR_SIZE];
    nfs_pi_build(buf, sizeof(buf), 0, 0x86DD);

    uint16_t flags, proto;
    int rc = nfs_pi_parse(buf, sizeof(buf), &flags, &proto);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(proto, 0x86DD);
}

/* ---- Test: PI header parse too short ---- */

static void test_pi_parse_too_short(void) {
    uint8_t buf[2] = {0x00, 0x00};
    uint16_t flags, proto;
    int rc = nfs_pi_parse(buf, sizeof(buf), &flags, &proto);
    ASSERT_EQ(rc, -1);
}

/* ---- Test: PI header network byte order ---- */

static void test_pi_network_byte_order(void) {
    uint8_t buf[NFS_PI_HDR_SIZE];
    nfs_pi_build(buf, sizeof(buf), 0, 0x0800);

    /* flags=0 -> 0x00 0x00 */
    ASSERT_EQ(buf[0], 0x00);
    ASSERT_EQ(buf[1], 0x00);
    /* proto=0x0800 in network byte order -> 0x08 0x00 */
    ASSERT_EQ(buf[2], 0x08);
    ASSERT_EQ(buf[3], 0x00);
}

/* ---- Test: TUN wrap IPv4 ---- */

static void test_tun_wrap_ipv4(void) {
    uint8_t ip_pkt[] = {0x45, 0x00, 0x00, 0x1c}; /* minimal IP header start */
    uint8_t out[64];
    int total = nfs_tun_wrap_ip(ip_pkt, sizeof(ip_pkt), out, sizeof(out));
    ASSERT_EQ(total, NFS_PI_HDR_SIZE + (int)sizeof(ip_pkt));

    /* Verify PI header: flags=0, proto=0x0800. */
    uint16_t flags, proto;
    nfs_pi_parse(out, NFS_PI_HDR_SIZE, &flags, &proto);
    ASSERT_EQ(flags, 0);
    ASSERT_EQ(proto, 0x0800);

    /* Verify IP packet follows. */
    ASSERT_EQ(out[NFS_PI_HDR_SIZE], 0x45);
}

/* ---- Test: TUN unwrap ---- */

static void test_tun_unwrap(void) {
    /* Build a wrapped TUN packet. */
    uint8_t ip_pkt[] = {0x45, 0x00, 0xAA, 0xBB};
    uint8_t wrapped[64];
    int total = nfs_tun_wrap_ip(ip_pkt, sizeof(ip_pkt), wrapped, sizeof(wrapped));
    ASSERT_TRUE(total > 0);

    uint16_t proto;
    const uint8_t *payload;
    size_t payload_len;
    int rc = nfs_tun_unwrap(wrapped, (size_t)total, &proto, &payload, &payload_len);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(proto, 0x0800);
    ASSERT_EQ(payload_len, sizeof(ip_pkt));
    ASSERT_EQ(payload[0], 0x45);
    ASSERT_EQ(payload[3], 0xBB);
}

/* ---- Test: TAP wrap ---- */

static void test_tap_wrap(void) {
    /* Fake Ethernet frame: dst(6) + src(6) + ethertype(2) + payload(4). */
    uint8_t frame[18];
    memset(frame, 0xAA, sizeof(frame));
    frame[12] = 0x08;
    frame[13] = 0x00; /* EtherType = IPv4 */

    uint8_t out[64];
    int total = nfs_tap_wrap_frame(frame, sizeof(frame), out, sizeof(out));
    ASSERT_EQ(total, NFS_PI_HDR_SIZE + (int)sizeof(frame));

    /* PI header present. */
    uint16_t flags, proto;
    nfs_pi_parse(out, NFS_PI_HDR_SIZE, &flags, &proto);
    ASSERT_EQ(proto, 0x0800);

    /* Frame data follows PI header. */
    ASSERT_EQ(out[NFS_PI_HDR_SIZE], 0xAA);
}

/* ---- Test: TAP unwrap ---- */

static void test_tap_unwrap(void) {
    uint8_t frame[18];
    memset(frame, 0xBB, sizeof(frame));

    uint8_t wrapped[64];
    int total = nfs_tap_wrap_frame(frame, sizeof(frame), wrapped, sizeof(wrapped));
    ASSERT_TRUE(total > 0);

    uint16_t proto;
    const uint8_t *payload;
    size_t payload_len;
    int rc = nfs_tap_unwrap(wrapped, (size_t)total, &proto, &payload, &payload_len);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(payload_len, sizeof(frame));
    ASSERT_EQ(payload[0], 0xBB);
}

/* ---- Test: valid names ---- */

static void test_valid_names(void) {
    ASSERT_EQ(nfs_tuntap_valid_name("tun0"), 1);
    ASSERT_EQ(nfs_tuntap_valid_name("tap1"), 1);
    ASSERT_EQ(nfs_tuntap_valid_name("my-dev"), 1);
    ASSERT_EQ(nfs_tuntap_valid_name("a"), 1);
    ASSERT_EQ(nfs_tuntap_valid_name("under_score"), 1);
}

/* ---- Test: invalid names ---- */

static void test_invalid_names(void) {
    ASSERT_EQ(nfs_tuntap_valid_name(NULL), 0);
    /* 16 characters -- too long. */
    ASSERT_EQ(nfs_tuntap_valid_name("abcdefghijklmnop"), 0);
    /* Contains space. */
    ASSERT_EQ(nfs_tuntap_valid_name("bad name"), 0);
    /* Empty string. */
    ASSERT_EQ(nfs_tuntap_valid_name(""), 0);
    /* Contains dot. */
    ASSERT_EQ(nfs_tuntap_valid_name("dev.0"), 0);
}

/* ---- Test: MTU constants ---- */

static void test_mtu_constants(void) {
    ASSERT_EQ(NFS_TUN_DEFAULT_MTU, 1500);
    ASSERT_EQ(NFS_TAP_DEFAULT_MTU, 1500);
    ASSERT_EQ(NFS_TUN_OVERHEAD, 0);
    ASSERT_EQ(NFS_TAP_OVERHEAD, 14);
}

/* ---- Test: buffer too small for wrap ---- */

static void test_buffer_too_small(void) {
    uint8_t ip_pkt[] = {0x45, 0x00, 0x00, 0x1c};
    uint8_t out[4]; /* too small: need NFS_PI_HDR_SIZE(4) + 4 = 8 */
    int rc = nfs_tun_wrap_ip(ip_pkt, sizeof(ip_pkt), out, sizeof(out));
    ASSERT_EQ(rc, -1);

    uint8_t frame[18];
    uint8_t out2[4];
    rc = nfs_tap_wrap_frame(frame, sizeof(frame), out2, sizeof(out2));
    ASSERT_EQ(rc, -1);
}

/* ---- Test: IFF_TUN value ---- */

static void test_iff_tun_value(void) {
    ASSERT_EQ(IFF_TUN, 0x0001);
}

/* ---- Test: IFF_TAP value ---- */

static void test_iff_tap_value(void) {
    ASSERT_EQ(IFF_TAP, 0x0002);
}

/* ---- Test: IFF_NO_PI value ---- */

static void test_iff_no_pi_value(void) {
    ASSERT_EQ(IFF_NO_PI, 0x1000);
}

/* ---- Runner ---- */

int main(void) {
    printf("Running TUN/TAP tests...\n\n");

    test_prepare_ifr_tun();
    test_prepare_ifr_tap();
    test_prepare_ifr_no_pi();
    test_prepare_ifr_name_too_long();
    test_prepare_ifr_null_name();
    test_prepare_ifr_empty_name();
    test_pi_roundtrip();
    test_pi_parse_ipv6();
    test_pi_parse_too_short();
    test_pi_network_byte_order();
    test_tun_wrap_ipv4();
    test_tun_unwrap();
    test_tap_wrap();
    test_tap_unwrap();
    test_valid_names();
    test_invalid_names();
    test_mtu_constants();
    test_buffer_too_small();
    test_iff_tun_value();
    test_iff_tap_value();
    test_iff_no_pi_value();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
