/* Unit tests for nfs_dhcp functions. */

#include "../dhcp.h"
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

/* ---- Struct size ---- */

static void test_dhcp_msg_size(void) {
    ASSERT_EQ(sizeof(struct nfs_dhcp_msg), 548);
}

/* ---- Build DISCOVER: correct op/htype/hlen ---- */

static void test_build_discover_fields(void) {
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t buf[1024];
    int len = nfs_dhcp_build_discover(mac, 0x42, buf, sizeof(buf));
    ASSERT_TRUE(len > 0);

    struct nfs_dhcp_msg *msg = (struct nfs_dhcp_msg *)buf;
    ASSERT_EQ(msg->op, NFS_DHCP_BOOTREQUEST);
    ASSERT_EQ(msg->htype, NFS_DHCP_HTYPE_ETH);
    ASSERT_EQ(msg->hlen, NFS_DHCP_HLEN_ETH);
    ASSERT_EQ(msg->xid, 0x42);
    ASSERT_EQ(msg->chaddr[0], 0xAA);
    ASSERT_EQ(msg->chaddr[1], 0xBB);
    ASSERT_EQ(msg->chaddr[2], 0xCC);
    ASSERT_EQ(msg->chaddr[3], 0xDD);
    ASSERT_EQ(msg->chaddr[4], 0xEE);
    ASSERT_EQ(msg->chaddr[5], 0xFF);
}

/* ---- Build DISCOVER: magic cookie present ---- */

static void test_build_discover_magic_cookie(void) {
    uint8_t mac[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint8_t buf[1024];
    int len = nfs_dhcp_build_discover(mac, 0x01, buf, sizeof(buf));
    ASSERT_TRUE(len > 0);

    struct nfs_dhcp_msg *msg = (struct nfs_dhcp_msg *)buf;
    ASSERT_EQ(msg->options[0], 0x63);
    ASSERT_EQ(msg->options[1], 0x82);
    ASSERT_EQ(msg->options[2], 0x53);
    ASSERT_EQ(msg->options[3], 0x63);
}

/* ---- Parse options: simple with pad + end ---- */

static void test_parse_options_pad_end(void) {
    uint8_t opts[] = {
        0x63, 0x82, 0x53, 0x63, /* magic cookie */
        0x00,                   /* PAD */
        0x00,                   /* PAD */
        0xFF                    /* END */
    };
    struct nfs_dhcp_option out[8];
    size_t nfound = 0;
    int rc = nfs_dhcp_parse_options(opts, sizeof(opts), out, 8, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(nfound, 0); /* PADs and END are not returned as options */
}

/* ---- Parse options: msg type extraction ---- */

static void test_parse_msg_type(void) {
    uint8_t opts[] = {0x63,
                      0x82,
                      0x53,
                      0x63, /* magic cookie */
                      NFS_DHCP_OPT_MSG_TYPE,
                      1,
                      NFS_DHCP_DISCOVER,
                      0xFF};
    struct nfs_dhcp_option out[8];
    size_t nfound = 0;
    int rc = nfs_dhcp_parse_options(opts, sizeof(opts), out, 8, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(nfound, 1);
    ASSERT_EQ(out[0].code, NFS_DHCP_OPT_MSG_TYPE);
    ASSERT_EQ(out[0].length, 1);
    ASSERT_EQ(out[0].data[0], NFS_DHCP_DISCOVER);

    int mt = nfs_dhcp_get_msg_type(out, nfound);
    ASSERT_EQ(mt, NFS_DHCP_DISCOVER);
}

/* ---- Parse subnet + router + DNS options ---- */

static void test_parse_multiple_options(void) {
    uint8_t opts[] = {0x63,
                      0x82,
                      0x53,
                      0x63,
                      NFS_DHCP_OPT_MSG_TYPE,
                      1,
                      NFS_DHCP_OFFER,
                      NFS_DHCP_OPT_SUBNET,
                      4,
                      255,
                      255,
                      255,
                      0,
                      NFS_DHCP_OPT_ROUTER,
                      4,
                      192,
                      168,
                      1,
                      1,
                      NFS_DHCP_OPT_DNS,
                      4,
                      8,
                      8,
                      8,
                      8,
                      0xFF};
    struct nfs_dhcp_option out[16];
    size_t nfound = 0;
    int rc = nfs_dhcp_parse_options(opts, sizeof(opts), out, 16, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(nfound, 4);

    /* Message type = OFFER */
    ASSERT_EQ(out[0].code, NFS_DHCP_OPT_MSG_TYPE);
    ASSERT_EQ(out[0].data[0], NFS_DHCP_OFFER);

    /* Subnet mask = 255.255.255.0 */
    ASSERT_EQ(out[1].code, NFS_DHCP_OPT_SUBNET);
    ASSERT_EQ(out[1].length, 4);
    ASSERT_EQ(out[1].data[0], 255);
    ASSERT_EQ(out[1].data[1], 255);
    ASSERT_EQ(out[1].data[2], 255);
    ASSERT_EQ(out[1].data[3], 0);

    /* Router = 192.168.1.1 */
    ASSERT_EQ(out[2].code, NFS_DHCP_OPT_ROUTER);
    ASSERT_EQ(out[2].data[0], 192);
    ASSERT_EQ(out[2].data[1], 168);
    ASSERT_EQ(out[2].data[2], 1);
    ASSERT_EQ(out[2].data[3], 1);

    /* DNS = 8.8.8.8 */
    ASSERT_EQ(out[3].code, NFS_DHCP_OPT_DNS);
    ASSERT_EQ(out[3].data[0], 8);
    ASSERT_EQ(out[3].data[1], 8);
    ASSERT_EQ(out[3].data[2], 8);
    ASSERT_EQ(out[3].data[3], 8);
}

/* ---- Bad magic cookie ---- */

static void test_parse_bad_magic(void) {
    uint8_t opts[] = {0x00, 0x00, 0x00, 0x00, 0xFF};
    struct nfs_dhcp_option out[4];
    size_t nfound = 0;
    int rc = nfs_dhcp_parse_options(opts, sizeof(opts), out, 4, &nfound);
    ASSERT_EQ(rc, -1);
}

/* ---- Too short for magic cookie ---- */

static void test_parse_too_short(void) {
    uint8_t opts[] = {0x63, 0x82};
    struct nfs_dhcp_option out[4];
    size_t nfound = 0;
    int rc = nfs_dhcp_parse_options(opts, sizeof(opts), out, 4, &nfound);
    ASSERT_EQ(rc, -1);
}

/* ---- Build + parse roundtrip ---- */

static void test_build_parse_roundtrip(void) {
    uint8_t mac[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01};
    uint8_t buf[1024];
    int len = nfs_dhcp_build_discover(mac, 0xABCD1234, buf, sizeof(buf));
    ASSERT_TRUE(len > 0);

    struct nfs_dhcp_msg *msg = (struct nfs_dhcp_msg *)buf;

    /* Parse options from the built message */
    struct nfs_dhcp_option opts[16];
    size_t nfound = 0;
    int rc = nfs_dhcp_parse_options(msg->options, sizeof(msg->options), opts, 16, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(nfound >= 1);

    /* Should find message type = DISCOVER */
    int mt = nfs_dhcp_get_msg_type(opts, nfound);
    ASSERT_EQ(mt, NFS_DHCP_DISCOVER);
}

/* ---- Message type strings ---- */

static void test_msg_type_strings(void) {
    ASSERT_TRUE(strcmp(nfs_dhcp_msg_type_str(NFS_DHCP_DISCOVER), "DISCOVER") == 0);
    ASSERT_TRUE(strcmp(nfs_dhcp_msg_type_str(NFS_DHCP_OFFER), "OFFER") == 0);
    ASSERT_TRUE(strcmp(nfs_dhcp_msg_type_str(NFS_DHCP_REQUEST), "REQUEST") == 0);
    ASSERT_TRUE(strcmp(nfs_dhcp_msg_type_str(NFS_DHCP_ACK), "ACK") == 0);
    ASSERT_TRUE(strcmp(nfs_dhcp_msg_type_str(NFS_DHCP_NAK), "NAK") == 0);
    ASSERT_TRUE(strcmp(nfs_dhcp_msg_type_str(NFS_DHCP_RELEASE), "RELEASE") == 0);
    ASSERT_TRUE(strcmp(nfs_dhcp_msg_type_str(NFS_DHCP_DECLINE), "DECLINE") == 0);
    ASSERT_TRUE(strcmp(nfs_dhcp_msg_type_str(99), "UNKNOWN") == 0);
}

/* ---- get_msg_type with no option 53 ---- */

static void test_get_msg_type_missing(void) {
    struct nfs_dhcp_option opts[2];
    opts[0].code = NFS_DHCP_OPT_SUBNET;
    opts[0].length = 4;
    ASSERT_EQ(nfs_dhcp_get_msg_type(opts, 1), -1);
}

/* ---- Build DISCOVER: buffer too small ---- */

static void test_build_discover_too_small(void) {
    uint8_t mac[6] = {0};
    uint8_t buf[100]; /* too small for 548-byte message */
    int len = nfs_dhcp_build_discover(mac, 0x01, buf, sizeof(buf));
    ASSERT_EQ(len, -1);
}

/* ---- Build DISCOVER: returns correct length ---- */

static void test_build_discover_length(void) {
    uint8_t mac[6] = {0};
    uint8_t buf[1024];
    int len = nfs_dhcp_build_discover(mac, 0x01, buf, sizeof(buf));
    ASSERT_EQ(len, 548);
}

/* ---- Build DISCOVER: hops and secs are zero ---- */

static void test_build_discover_zero_fields(void) {
    uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t buf[1024];
    nfs_dhcp_build_discover(mac, 0xFF, buf, sizeof(buf));
    struct nfs_dhcp_msg *msg = (struct nfs_dhcp_msg *)buf;
    ASSERT_EQ(msg->hops, 0);
    ASSERT_EQ(msg->secs, 0);
    ASSERT_EQ(msg->ciaddr, 0);
    ASSERT_EQ(msg->yiaddr, 0);
}

/* ---- Parse options: hostname option ---- */

static void test_parse_hostname(void) {
    uint8_t opts[] = {0x63, 0x82, 0x53, 0x63, NFS_DHCP_OPT_HOSTNAME, 6, 'm', 'y', 'h',
                      'o',  's',  't',  0xFF};
    struct nfs_dhcp_option out[8];
    size_t nfound = 0;
    int rc = nfs_dhcp_parse_options(opts, sizeof(opts), out, 8, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(nfound, 1);
    ASSERT_EQ(out[0].code, NFS_DHCP_OPT_HOSTNAME);
    ASSERT_EQ(out[0].length, 6);
    ASSERT_TRUE(memcmp(out[0].data, "myhost", 6) == 0);
}

/* ---- Parse options: lease time ---- */

static void test_parse_lease_time(void) {
    uint8_t opts[] = {0x63, 0x82, 0x53, 0x63, NFS_DHCP_OPT_LEASE_TIME,
                      4,    0x00, 0x01, 0x51, 0x80, /* 86400 seconds */
                      0xFF};
    struct nfs_dhcp_option out[8];
    size_t nfound = 0;
    int rc = nfs_dhcp_parse_options(opts, sizeof(opts), out, 8, &nfound);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(nfound, 1);
    ASSERT_EQ(out[0].code, NFS_DHCP_OPT_LEASE_TIME);
    ASSERT_EQ(out[0].length, 4);
    /* 86400 = 0x00015180 */
    uint32_t lease = ((uint32_t)out[0].data[0] << 24) | ((uint32_t)out[0].data[1] << 16) |
                     ((uint32_t)out[0].data[2] << 8) | (uint32_t)out[0].data[3];
    ASSERT_EQ(lease, 86400);
}

int main(void) {
    printf("DHCPv4 Tests\n");

    test_dhcp_msg_size();
    test_build_discover_fields();
    test_build_discover_magic_cookie();
    test_parse_options_pad_end();
    test_parse_msg_type();
    test_parse_multiple_options();
    test_parse_bad_magic();
    test_parse_too_short();
    test_build_parse_roundtrip();
    test_msg_type_strings();
    test_get_msg_type_missing();
    test_build_discover_too_small();
    test_build_discover_length();
    test_build_discover_zero_fields();
    test_parse_hostname();
    test_parse_lease_time();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
