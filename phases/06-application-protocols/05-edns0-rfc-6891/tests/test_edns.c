/* Unit tests for EDNS(0) (RFC 6891). */

#include "../edns.h"
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

/* ---- Option encode/decode tests ---- */

static void test_encode_option_basic(void) {
    uint8_t data[] = {0xAA, 0xBB};
    uint8_t out[32];
    int n = nfs_edns_encode_option(3, data, 2, out, sizeof(out));
    ASSERT_EQ(n, 6); /* 4 header + 2 data */
    /* Check code = 3 in network byte order */
    uint16_t code;
    memcpy(&code, out, 2);
    ASSERT_EQ(ntohs(code), 3);
    /* Check length = 2 */
    uint16_t len;
    memcpy(&len, out + 2, 2);
    ASSERT_EQ(ntohs(len), 2);
    ASSERT_EQ(out[4], 0xAA);
    ASSERT_EQ(out[5], 0xBB);
}

static void test_encode_option_empty(void) {
    uint8_t out[32];
    int n = nfs_edns_encode_option(12, NULL, 0, out, sizeof(out));
    ASSERT_EQ(n, 4); /* header only */
}

static void test_encode_option_null_out(void) {
    ASSERT_EQ(nfs_edns_encode_option(1, NULL, 0, NULL, 32), -1);
}

static void test_decode_option_basic(void) {
    /* Build wire: code=3, length=4, data=01020304 */
    uint8_t wire[8];
    uint16_t tmp;
    tmp = htons(3);
    memcpy(wire, &tmp, 2);
    tmp = htons(4);
    memcpy(wire + 2, &tmp, 2);
    wire[4] = 1;
    wire[5] = 2;
    wire[6] = 3;
    wire[7] = 4;

    struct nfs_edns_option opt;
    int consumed = nfs_edns_decode_option(wire, sizeof(wire), &opt);
    ASSERT_EQ(consumed, 8);
    ASSERT_EQ(opt.code, 3);
    ASSERT_EQ(opt.length, 4);
    ASSERT_EQ(opt.data[0], 1);
    ASSERT_EQ(opt.data[3], 4);
}

static void test_decode_option_too_short(void) {
    uint8_t wire[2] = {0, 0};
    struct nfs_edns_option opt;
    ASSERT_EQ(nfs_edns_decode_option(wire, sizeof(wire), &opt), -1);
}

static void test_decode_option_truncated_data(void) {
    /* Says length=10 but only 2 bytes of data available */
    uint8_t wire[6];
    uint16_t tmp;
    tmp = htons(1);
    memcpy(wire, &tmp, 2);
    tmp = htons(10);
    memcpy(wire + 2, &tmp, 2);
    wire[4] = 0;
    wire[5] = 0;

    struct nfs_edns_option opt;
    ASSERT_EQ(nfs_edns_decode_option(wire, sizeof(wire), &opt), -1);
}

/* ---- OPT RR build/parse roundtrip tests ---- */

static void test_build_opt_no_options(void) {
    uint8_t out[64];
    int n = nfs_edns_build_opt(4096, 0, 0, 0, NULL, 0, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    /* Minimum OPT RR: name(1) + type(2) + class(2) + ttl(4) + rdlen(2) = 11 */
    ASSERT_EQ(n, 11);

    /* Parse it back */
    struct nfs_edns_opt parsed;
    int consumed = nfs_edns_parse_opt(out, (size_t)n, &parsed);
    ASSERT_EQ(consumed, n);
    ASSERT_EQ(parsed.udp_payload_size, 4096);
    ASSERT_EQ(parsed.flags.ext_rcode, 0);
    ASSERT_EQ(parsed.flags.version, 0);
    ASSERT_EQ(parsed.flags.do_bit, 0);
    ASSERT_EQ(parsed.option_count, 0);
}

static void test_build_opt_with_do_bit(void) {
    uint8_t out[64];
    int n = nfs_edns_build_opt(1232, 0, 0, 1, NULL, 0, out, sizeof(out));
    ASSERT_TRUE(n > 0);

    struct nfs_edns_opt parsed;
    nfs_edns_parse_opt(out, (size_t)n, &parsed);
    ASSERT_EQ(parsed.udp_payload_size, 1232);
    ASSERT_EQ(parsed.flags.do_bit, 1);
}

static void test_build_opt_ext_rcode(void) {
    uint8_t out[64];
    int n = nfs_edns_build_opt(512, 5, 0, 0, NULL, 0, out, sizeof(out));
    ASSERT_TRUE(n > 0);

    struct nfs_edns_opt parsed;
    nfs_edns_parse_opt(out, (size_t)n, &parsed);
    ASSERT_EQ(parsed.flags.ext_rcode, 5);
    ASSERT_EQ(parsed.flags.version, 0);
}

static void test_build_opt_with_nsid(void) {
    struct nfs_edns_option opts[1];
    nfs_edns_build_nsid("test-server", &opts[0]);

    uint8_t out[128];
    int n = nfs_edns_build_opt(4096, 0, 0, 1, opts, 1, out, sizeof(out));
    ASSERT_TRUE(n > 0);

    struct nfs_edns_opt parsed;
    int consumed = nfs_edns_parse_opt(out, (size_t)n, &parsed);
    ASSERT_EQ(consumed, n);
    ASSERT_EQ(parsed.option_count, 1);
    ASSERT_EQ(parsed.options[0].code, NFS_EDNS_OPT_NSID);
    ASSERT_EQ(parsed.options[0].length, 11);
    ASSERT_TRUE(memcmp(parsed.options[0].data, "test-server", 11) == 0);
}

static void test_build_opt_with_cookie(void) {
    uint8_t client[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t server[8] = {0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11};

    struct nfs_edns_option opts[1];
    nfs_edns_build_cookie(client, server, 8, &opts[0]);

    uint8_t out[128];
    int n = nfs_edns_build_opt(4096, 0, 0, 0, opts, 1, out, sizeof(out));
    ASSERT_TRUE(n > 0);

    struct nfs_edns_opt parsed;
    nfs_edns_parse_opt(out, (size_t)n, &parsed);
    ASSERT_EQ(parsed.option_count, 1);
    ASSERT_EQ(parsed.options[0].code, NFS_EDNS_OPT_COOKIE);
    ASSERT_EQ(parsed.options[0].length, 16); /* 8 client + 8 server */
    ASSERT_TRUE(memcmp(parsed.options[0].data, client, 8) == 0);
    ASSERT_TRUE(memcmp(parsed.options[0].data + 8, server, 8) == 0);
}

static void test_build_opt_with_padding(void) {
    struct nfs_edns_option opts[1];
    nfs_edns_build_padding(32, &opts[0]);

    uint8_t out[128];
    int n = nfs_edns_build_opt(4096, 0, 0, 0, opts, 1, out, sizeof(out));
    ASSERT_TRUE(n > 0);

    struct nfs_edns_opt parsed;
    nfs_edns_parse_opt(out, (size_t)n, &parsed);
    ASSERT_EQ(parsed.option_count, 1);
    ASSERT_EQ(parsed.options[0].code, NFS_EDNS_OPT_PADDING);
    ASSERT_EQ(parsed.options[0].length, 32);
    /* Padding should be all zeros */
    for (int i = 0; i < 32; i++) {
        ASSERT_EQ(parsed.options[0].data[i], 0);
    }
}

static void test_build_opt_multiple_options(void) {
    struct nfs_edns_option opts[2];
    nfs_edns_build_nsid("ns1", &opts[0]);
    nfs_edns_build_padding(8, &opts[1]);

    uint8_t out[128];
    int n = nfs_edns_build_opt(4096, 0, 0, 1, opts, 2, out, sizeof(out));
    ASSERT_TRUE(n > 0);

    struct nfs_edns_opt parsed;
    nfs_edns_parse_opt(out, (size_t)n, &parsed);
    ASSERT_EQ(parsed.option_count, 2);
    ASSERT_EQ(parsed.options[0].code, NFS_EDNS_OPT_NSID);
    ASSERT_EQ(parsed.options[1].code, NFS_EDNS_OPT_PADDING);
}

/* ---- Parse error tests ---- */

static void test_parse_opt_null(void) {
    struct nfs_edns_opt opt;
    ASSERT_EQ(nfs_edns_parse_opt(NULL, 64, &opt), -1);
}

static void test_parse_opt_wrong_name(void) {
    /* Name byte is 0x01 instead of 0x00 */
    uint8_t wire[11] = {0x01, 0x00, 0x29, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    struct nfs_edns_opt opt;
    ASSERT_EQ(nfs_edns_parse_opt(wire, sizeof(wire), &opt), -1);
}

static void test_parse_opt_wrong_type(void) {
    /* Type is 1 (A) instead of 41 (OPT) */
    uint8_t wire[11] = {0x00, 0x00, 0x01, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    struct nfs_edns_opt opt;
    ASSERT_EQ(nfs_edns_parse_opt(wire, sizeof(wire), &opt), -1);
}

/* ---- Convenience builder tests ---- */

static void test_nsid_null(void) {
    struct nfs_edns_option opt;
    ASSERT_EQ(nfs_edns_build_nsid(NULL, &opt), -1);
}

static void test_cookie_client_only(void) {
    uint8_t client[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    struct nfs_edns_option opt;
    ASSERT_EQ(nfs_edns_build_cookie(client, NULL, 0, &opt), 0);
    ASSERT_EQ(opt.length, 8);
}

static void test_cookie_invalid_server_len(void) {
    uint8_t client[8] = {0};
    uint8_t server[4] = {0};
    struct nfs_edns_option opt;
    ASSERT_EQ(nfs_edns_build_cookie(client, server, 4, &opt), -1); /* too short */
}

static void test_padding_null(void) {
    ASSERT_EQ(nfs_edns_build_padding(8, NULL), -1);
}

/* ---- Utility tests ---- */

static void test_option_str(void) {
    ASSERT_TRUE(strcmp(nfs_edns_option_str(NFS_EDNS_OPT_NSID), "NSID") == 0);
    ASSERT_TRUE(strcmp(nfs_edns_option_str(NFS_EDNS_OPT_COOKIE), "Cookie") == 0);
    ASSERT_TRUE(strcmp(nfs_edns_option_str(NFS_EDNS_OPT_PADDING), "Padding") == 0);
    ASSERT_TRUE(strcmp(nfs_edns_option_str(9999), "UNKNOWN") == 0);
}

/* ---- Build null tests ---- */

static void test_build_opt_null(void) {
    ASSERT_EQ(nfs_edns_build_opt(4096, 0, 0, 0, NULL, 0, NULL, 64), -1);
}

int main(void) {
    printf("EDNS(0) Tests\n");

    test_encode_option_basic();
    test_encode_option_empty();
    test_encode_option_null_out();
    test_decode_option_basic();
    test_decode_option_too_short();
    test_decode_option_truncated_data();
    test_build_opt_no_options();
    test_build_opt_with_do_bit();
    test_build_opt_ext_rcode();
    test_build_opt_with_nsid();
    test_build_opt_with_cookie();
    test_build_opt_with_padding();
    test_build_opt_multiple_options();
    test_parse_opt_null();
    test_parse_opt_wrong_name();
    test_parse_opt_wrong_type();
    test_nsid_null();
    test_cookie_client_only();
    test_cookie_invalid_server_len();
    test_padding_null();
    test_option_str();
    test_build_opt_null();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
