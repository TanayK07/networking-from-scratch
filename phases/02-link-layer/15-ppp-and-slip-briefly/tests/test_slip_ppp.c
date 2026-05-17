/* Unit tests for SLIP and PPP framing. */

#include "../slip_ppp.h"
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

/* ---- SLIP tests ---- */

static void test_slip_encode_simple(void) {
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint8_t out[32];
    int n = nfs_slip_encode(data, 3, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    /* Frame: END + data + END = 5 bytes */
    ASSERT_EQ(n, 5);
    ASSERT_EQ(out[0], NFS_SLIP_END);
    ASSERT_EQ(out[1], 0x01);
    ASSERT_EQ(out[2], 0x02);
    ASSERT_EQ(out[3], 0x03);
    ASSERT_EQ(out[4], NFS_SLIP_END);
}

static void test_slip_encode_escape_end(void) {
    /* Data contains END byte (0xC0) */
    uint8_t data[] = {0xC0};
    uint8_t out[32];
    int n = nfs_slip_encode(data, 1, out, sizeof(out));
    ASSERT_EQ(n, 4); /* END + ESC + ESC_END + END */
    ASSERT_EQ(out[0], NFS_SLIP_END);
    ASSERT_EQ(out[1], NFS_SLIP_ESC);
    ASSERT_EQ(out[2], NFS_SLIP_ESC_END);
    ASSERT_EQ(out[3], NFS_SLIP_END);
}

static void test_slip_encode_escape_esc(void) {
    /* Data contains ESC byte (0xDB) */
    uint8_t data[] = {0xDB};
    uint8_t out[32];
    int n = nfs_slip_encode(data, 1, out, sizeof(out));
    ASSERT_EQ(n, 4); /* END + ESC + ESC_ESC + END */
    ASSERT_EQ(out[1], NFS_SLIP_ESC);
    ASSERT_EQ(out[2], NFS_SLIP_ESC_ESC);
}

static void test_slip_encode_both_special(void) {
    uint8_t data[] = {0xC0, 0xDB};
    uint8_t out[32];
    int n = nfs_slip_encode(data, 2, out, sizeof(out));
    /* END + ESC+ESC_END + ESC+ESC_ESC + END = 6 */
    ASSERT_EQ(n, 6);
}

static void test_slip_encode_empty(void) {
    uint8_t out[32];
    int n = nfs_slip_encode(NULL, 0, out, sizeof(out));
    ASSERT_EQ(n, 2); /* Just END + END */
    ASSERT_EQ(out[0], NFS_SLIP_END);
    ASSERT_EQ(out[1], NFS_SLIP_END);
}

static void test_slip_encode_buffer_too_small(void) {
    uint8_t data[] = {0x01, 0x02, 0x03};
    uint8_t out[3]; /* Too small for END + 3 bytes + END */
    int n = nfs_slip_encode(data, 3, out, sizeof(out));
    ASSERT_EQ(n, -1);
}

static void test_slip_decode_simple(void) {
    uint8_t frame[] = {NFS_SLIP_END, 0x01, 0x02, 0x03, NFS_SLIP_END};
    uint8_t out[32];
    int n = nfs_slip_decode(frame, sizeof(frame), out, sizeof(out));
    ASSERT_EQ(n, 3);
    ASSERT_EQ(out[0], 0x01);
    ASSERT_EQ(out[1], 0x02);
    ASSERT_EQ(out[2], 0x03);
}

static void test_slip_decode_escape_end(void) {
    uint8_t frame[] = {NFS_SLIP_END, NFS_SLIP_ESC, NFS_SLIP_ESC_END, NFS_SLIP_END};
    uint8_t out[32];
    int n = nfs_slip_decode(frame, sizeof(frame), out, sizeof(out));
    ASSERT_EQ(n, 1);
    ASSERT_EQ(out[0], NFS_SLIP_END);
}

static void test_slip_decode_escape_esc(void) {
    uint8_t frame[] = {NFS_SLIP_END, NFS_SLIP_ESC, NFS_SLIP_ESC_ESC, NFS_SLIP_END};
    uint8_t out[32];
    int n = nfs_slip_decode(frame, sizeof(frame), out, sizeof(out));
    ASSERT_EQ(n, 1);
    ASSERT_EQ(out[0], NFS_SLIP_ESC);
}

static void test_slip_roundtrip(void) {
    uint8_t data[] = {0x45, 0x00, 0xC0, 0xDB, 0xFF, 0x01};
    uint8_t encoded[64], decoded[64];
    int enc_len = nfs_slip_encode(data, sizeof(data), encoded, sizeof(encoded));
    ASSERT_TRUE(enc_len > 0);
    int dec_len = nfs_slip_decode(encoded, (size_t)enc_len, decoded, sizeof(decoded));
    ASSERT_EQ(dec_len, (int)sizeof(data));
    ASSERT_TRUE(memcmp(data, decoded, sizeof(data)) == 0);
}

static void test_slip_decode_invalid_no_end(void) {
    uint8_t frame[] = {0x01, 0x02, 0x03};
    uint8_t out[32];
    int n = nfs_slip_decode(frame, sizeof(frame), out, sizeof(out));
    ASSERT_EQ(n, -1);
}

static void test_slip_decode_truncated_escape(void) {
    /* ESC at end of payload (before closing END) */
    uint8_t frame[] = {NFS_SLIP_END, NFS_SLIP_ESC, NFS_SLIP_END};
    uint8_t out[32];
    int n = nfs_slip_decode(frame, sizeof(frame), out, sizeof(out));
    ASSERT_EQ(n, -1);
}

static void test_slip_decode_invalid_escape_seq(void) {
    uint8_t frame[] = {NFS_SLIP_END, NFS_SLIP_ESC, 0x42, NFS_SLIP_END};
    uint8_t out[32];
    int n = nfs_slip_decode(frame, sizeof(frame), out, sizeof(out));
    ASSERT_EQ(n, -1);
}

static void test_slip_decode_null_input(void) {
    uint8_t out[32];
    int n = nfs_slip_decode(NULL, 5, out, sizeof(out));
    ASSERT_EQ(n, -1);
}

/* ---- PPP FCS-16 tests ---- */

static void test_ppp_fcs16_known(void) {
    /* RFC 1662 test string "123456789" should give FCS = 0x906E */
    uint16_t fcs = nfs_ppp_fcs16((const uint8_t *)"123456789", 9);
    ASSERT_EQ(fcs, 0x906E);
}

static void test_ppp_fcs16_empty(void) {
    uint16_t fcs = nfs_ppp_fcs16(NULL, 0);
    /* FCS of empty data: init ^ final = 0xFFFF ^ 0xFFFF = 0x0000
     * Actually with no data, the loop doesn't execute, so fcs = 0xFFFF ^ 0xFFFF */
    ASSERT_EQ(fcs, 0x0000);
}

/* ---- PPP frame tests ---- */

static void test_ppp_build_basic(void) {
    const char *msg = "test";
    uint8_t buf[64];
    int n = nfs_ppp_frame_build(NFS_PPP_PROTO_IPV4, (const uint8_t *)msg, 4, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    /* flag + addr + ctrl + proto(2) + payload(4) + fcs(2) + flag = 12 */
    ASSERT_EQ(n, 12);
    ASSERT_EQ(buf[0], NFS_PPP_FLAG);
    ASSERT_EQ(buf[1], NFS_PPP_ADDR);
    ASSERT_EQ(buf[2], NFS_PPP_CTRL);
    ASSERT_EQ(buf[3], 0x00); /* protocol high byte */
    ASSERT_EQ(buf[4], 0x21); /* protocol low byte */
    ASSERT_EQ(buf[(size_t)n - 1], NFS_PPP_FLAG);
}

static void test_ppp_build_empty_payload(void) {
    uint8_t buf[64];
    int n = nfs_ppp_frame_build(NFS_PPP_PROTO_LCP, NULL, 0, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    /* flag + addr + ctrl + proto(2) + fcs(2) + flag = 8 */
    ASSERT_EQ(n, 8);
}

static void test_ppp_build_buffer_too_small(void) {
    uint8_t buf[4];
    int n = nfs_ppp_frame_build(NFS_PPP_PROTO_IPV4, (const uint8_t *)"x", 1, buf, sizeof(buf));
    ASSERT_EQ(n, -1);
}

static void test_ppp_parse_basic(void) {
    const char *msg = "hello";
    uint8_t buf[64];
    int n = nfs_ppp_frame_build(NFS_PPP_PROTO_IPV4, (const uint8_t *)msg, 5, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    struct nfs_ppp_frame frame;
    int rc = nfs_ppp_frame_parse(buf, (size_t)n, &frame);
    ASSERT_EQ(rc, 0);
    ASSERT_EQ(frame.address, NFS_PPP_ADDR);
    ASSERT_EQ(frame.control, NFS_PPP_CTRL);
    ASSERT_EQ(frame.protocol, NFS_PPP_PROTO_IPV4);
    ASSERT_EQ(frame.payload_len, 5);
    ASSERT_TRUE(memcmp(frame.payload, "hello", 5) == 0);
}

static void test_ppp_roundtrip_ipv6(void) {
    uint8_t data[] = {0x60, 0x00, 0x00, 0x00, 0x00, 0x10, 0x3A, 0xFF};
    uint8_t buf[64];
    int n = nfs_ppp_frame_build(NFS_PPP_PROTO_IPV6, data, sizeof(data), buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    struct nfs_ppp_frame frame;
    ASSERT_EQ(nfs_ppp_frame_parse(buf, (size_t)n, &frame), 0);
    ASSERT_EQ(frame.protocol, NFS_PPP_PROTO_IPV6);
    ASSERT_EQ(frame.payload_len, (int)sizeof(data));
    ASSERT_TRUE(memcmp(frame.payload, data, sizeof(data)) == 0);
}

static void test_ppp_parse_bad_flag(void) {
    uint8_t buf[] = {0x00, 0xFF, 0x03, 0x00, 0x21, 0x00, 0x00, 0x7E};
    struct nfs_ppp_frame frame;
    ASSERT_EQ(nfs_ppp_frame_parse(buf, sizeof(buf), &frame), -1);
}

static void test_ppp_parse_bad_address(void) {
    uint8_t buf[64];
    int n = nfs_ppp_frame_build(NFS_PPP_PROTO_IPV4, (const uint8_t *)"x", 1, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    buf[1] = 0x00; /* corrupt address */
    struct nfs_ppp_frame frame;
    ASSERT_EQ(nfs_ppp_frame_parse(buf, (size_t)n, &frame), -1);
}

static void test_ppp_parse_bad_fcs(void) {
    uint8_t buf[64];
    int n = nfs_ppp_frame_build(NFS_PPP_PROTO_IPV4, (const uint8_t *)"test", 4, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    /* Corrupt a payload byte */
    buf[5] ^= 0xFF;
    struct nfs_ppp_frame frame;
    ASSERT_EQ(nfs_ppp_frame_parse(buf, (size_t)n, &frame), -1);
}

static void test_ppp_parse_too_short(void) {
    uint8_t buf[] = {0x7E, 0xFF, 0x03, 0x7E};
    struct nfs_ppp_frame frame;
    ASSERT_EQ(nfs_ppp_frame_parse(buf, sizeof(buf), &frame), -1);
}

static void test_ppp_parse_null(void) {
    struct nfs_ppp_frame frame;
    ASSERT_EQ(nfs_ppp_frame_parse(NULL, 10, &frame), -1);
    ASSERT_EQ(nfs_ppp_frame_parse((const uint8_t *)"x", 1, NULL), -1);
}

static void test_ppp_protocol_name(void) {
    ASSERT_TRUE(strcmp(nfs_ppp_protocol_name(NFS_PPP_PROTO_IPV4), "IPv4") == 0);
    ASSERT_TRUE(strcmp(nfs_ppp_protocol_name(NFS_PPP_PROTO_IPV6), "IPv6") == 0);
    ASSERT_TRUE(strcmp(nfs_ppp_protocol_name(NFS_PPP_PROTO_LCP), "LCP") == 0);
    ASSERT_TRUE(strcmp(nfs_ppp_protocol_name(0x9999), "Unknown") == 0);
}

int main(void) {
    printf("SLIP and PPP Tests\n");

    /* SLIP tests */
    test_slip_encode_simple();
    test_slip_encode_escape_end();
    test_slip_encode_escape_esc();
    test_slip_encode_both_special();
    test_slip_encode_empty();
    test_slip_encode_buffer_too_small();
    test_slip_decode_simple();
    test_slip_decode_escape_end();
    test_slip_decode_escape_esc();
    test_slip_roundtrip();
    test_slip_decode_invalid_no_end();
    test_slip_decode_truncated_escape();
    test_slip_decode_invalid_escape_seq();
    test_slip_decode_null_input();

    /* PPP tests */
    test_ppp_fcs16_known();
    test_ppp_fcs16_empty();
    test_ppp_build_basic();
    test_ppp_build_empty_payload();
    test_ppp_build_buffer_too_small();
    test_ppp_parse_basic();
    test_ppp_roundtrip_ipv6();
    test_ppp_parse_bad_flag();
    test_ppp_parse_bad_address();
    test_ppp_parse_bad_fcs();
    test_ppp_parse_too_short();
    test_ppp_parse_null();
    test_ppp_protocol_name();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
