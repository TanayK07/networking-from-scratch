/* Unit tests for DNS over UDP then TCP. */

#include "../dns_transport.h"
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

/* Helper: create a minimal DNS message (just a header) */
static void make_dns_msg(uint8_t *buf, uint16_t id, uint16_t flags) {
    memset(buf, 0, NFS_DNS_HDR_SIZE);
    struct nfs_dns_transport_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.id = htons(id);
    hdr.flags = htons(flags);
    hdr.qdcount = htons(1);
    memcpy(buf, &hdr, NFS_DNS_HDR_SIZE);
}

/* ---- TCP framing tests ---- */

static void test_tcp_frame_basic(void) {
    uint8_t msg[NFS_DNS_HDR_SIZE];
    make_dns_msg(msg, 0x1234, 0x0100);

    uint8_t framed[64];
    int flen = nfs_dns_tcp_frame(msg, NFS_DNS_HDR_SIZE, framed, sizeof(framed));
    ASSERT_EQ(flen, NFS_DNS_HDR_SIZE + 2);

    /* Check length prefix */
    uint16_t net_len;
    memcpy(&net_len, framed, 2);
    ASSERT_EQ(ntohs(net_len), NFS_DNS_HDR_SIZE);

    /* Check message content preserved */
    ASSERT_TRUE(memcmp(framed + 2, msg, NFS_DNS_HDR_SIZE) == 0);
}

static void test_tcp_frame_null(void) {
    uint8_t out[64];
    ASSERT_EQ(nfs_dns_tcp_frame(NULL, 12, out, sizeof(out)), -1);
}

static void test_tcp_frame_zero_len(void) {
    uint8_t msg[1] = {0};
    uint8_t out[64];
    ASSERT_EQ(nfs_dns_tcp_frame(msg, 0, out, sizeof(out)), -1);
}

static void test_tcp_frame_buffer_too_small(void) {
    uint8_t msg[NFS_DNS_HDR_SIZE];
    make_dns_msg(msg, 1, 0);
    uint8_t out[4]; /* too small */
    ASSERT_EQ(nfs_dns_tcp_frame(msg, NFS_DNS_HDR_SIZE, out, sizeof(out)), -1);
}

/* ---- TCP unframing tests ---- */

static void test_tcp_unframe_basic(void) {
    uint8_t msg[NFS_DNS_HDR_SIZE];
    make_dns_msg(msg, 0x5678, 0x8180);

    uint8_t framed[64];
    int flen = nfs_dns_tcp_frame(msg, NFS_DNS_HDR_SIZE, framed, sizeof(framed));
    ASSERT_TRUE(flen > 0);

    const uint8_t *extracted;
    uint16_t elen;
    int consumed = nfs_dns_tcp_unframe(framed, (size_t)flen, &extracted, &elen);
    ASSERT_EQ(consumed, flen);
    ASSERT_EQ(elen, NFS_DNS_HDR_SIZE);
    ASSERT_TRUE(memcmp(extracted, msg, NFS_DNS_HDR_SIZE) == 0);
}

static void test_tcp_unframe_roundtrip(void) {
    /* Larger message */
    uint8_t msg[100];
    make_dns_msg(msg, 0xAAAA, 0x0100);
    /* Fill rest with pattern */
    for (int i = NFS_DNS_HDR_SIZE; i < 100; i++)
        msg[i] = (uint8_t)(i & 0xFF);

    uint8_t framed[256];
    int flen = nfs_dns_tcp_frame(msg, 100, framed, sizeof(framed));
    ASSERT_EQ(flen, 102);

    const uint8_t *extracted;
    uint16_t elen;
    int consumed = nfs_dns_tcp_unframe(framed, (size_t)flen, &extracted, &elen);
    ASSERT_EQ(consumed, 102);
    ASSERT_EQ(elen, 100);
    ASSERT_TRUE(memcmp(extracted, msg, 100) == 0);
}

static void test_tcp_unframe_incomplete(void) {
    /* Only 1 byte of prefix */
    uint8_t data[1] = {0x00};
    const uint8_t *msg;
    uint16_t len;
    ASSERT_EQ(nfs_dns_tcp_unframe(data, 1, &msg, &len), -1);
}

static void test_tcp_unframe_truncated_message(void) {
    /* Length prefix says 100 bytes but only 50 available */
    uint8_t data[52];
    uint16_t net_len = htons(100);
    memcpy(data, &net_len, 2);
    memset(data + 2, 0, 50);

    const uint8_t *msg;
    uint16_t len;
    ASSERT_EQ(nfs_dns_tcp_unframe(data, sizeof(data), &msg, &len), -1);
}

static void test_tcp_unframe_null(void) {
    const uint8_t *msg;
    uint16_t len;
    ASSERT_EQ(nfs_dns_tcp_unframe(NULL, 100, &msg, &len), -1);
}

/* ---- Truncation tests ---- */

static void test_truncation_not_set(void) {
    uint8_t msg[NFS_DNS_HDR_SIZE];
    make_dns_msg(msg, 1, 0x0100); /* RD=1, TC=0 */
    ASSERT_EQ(nfs_dns_is_truncated(msg, NFS_DNS_HDR_SIZE), 0);
}

static void test_truncation_set(void) {
    uint8_t msg[NFS_DNS_HDR_SIZE];
    make_dns_msg(msg, 1, 0x0300); /* RD=1, TC=1 */
    ASSERT_EQ(nfs_dns_is_truncated(msg, NFS_DNS_HDR_SIZE), 1);
}

static void test_set_truncated(void) {
    uint8_t msg[NFS_DNS_HDR_SIZE];
    make_dns_msg(msg, 1, 0x0100);
    ASSERT_EQ(nfs_dns_is_truncated(msg, NFS_DNS_HDR_SIZE), 0);

    ASSERT_EQ(nfs_dns_set_truncated(msg, NFS_DNS_HDR_SIZE), 0);
    ASSERT_EQ(nfs_dns_is_truncated(msg, NFS_DNS_HDR_SIZE), 1);
}

static void test_clear_truncated(void) {
    uint8_t msg[NFS_DNS_HDR_SIZE];
    make_dns_msg(msg, 1, 0x0300); /* TC=1 */
    ASSERT_EQ(nfs_dns_is_truncated(msg, NFS_DNS_HDR_SIZE), 1);

    ASSERT_EQ(nfs_dns_clear_truncated(msg, NFS_DNS_HDR_SIZE), 0);
    ASSERT_EQ(nfs_dns_is_truncated(msg, NFS_DNS_HDR_SIZE), 0);
}

static void test_set_truncated_too_short(void) {
    uint8_t msg[4] = {0};
    ASSERT_EQ(nfs_dns_set_truncated(msg, sizeof(msg)), -1);
}

static void test_truncated_null_msg(void) {
    ASSERT_EQ(nfs_dns_is_truncated(NULL, 12), 0);
}

/* ---- Size validation tests ---- */

static void test_fits_udp(void) {
    ASSERT_EQ(nfs_dns_fits_udp(100), 1);
    ASSERT_EQ(nfs_dns_fits_udp(512), 1);
    ASSERT_EQ(nfs_dns_fits_udp(513), 0);
    ASSERT_EQ(nfs_dns_fits_udp(0), 1);
}

static void test_fits_tcp(void) {
    ASSERT_EQ(nfs_dns_fits_tcp(100), 1);
    ASSERT_EQ(nfs_dns_fits_tcp(65535), 1);
    ASSERT_EQ(nfs_dns_fits_tcp(65536), 0);
}

/* ---- Truncation response tests ---- */

static void test_truncate_response_already_fits(void) {
    uint8_t msg[NFS_DNS_HDR_SIZE];
    make_dns_msg(msg, 1, 0x8100); /* response */

    uint8_t out[64];
    int tlen = nfs_dns_truncate_response(msg, NFS_DNS_HDR_SIZE, 512, out, sizeof(out));
    ASSERT_EQ(tlen, NFS_DNS_HDR_SIZE);
    ASSERT_TRUE(memcmp(out, msg, NFS_DNS_HDR_SIZE) == 0);
}

static void test_truncate_response_sets_tc(void) {
    /* Build a "response" larger than 512 bytes */
    uint8_t msg[600];
    memset(msg, 0, sizeof(msg));
    make_dns_msg(msg, 0x1111, 0x8180);
    /* Pretend there's a question at bytes 12-20 */
    msg[12] = 3;
    msg[13] = 'w';
    msg[14] = 'w';
    msg[15] = 'w';
    msg[16] = 3;
    msg[17] = 'c';
    msg[18] = 'o';
    msg[19] = 'm';
    msg[20] = 0; /* end of name */
    /* QTYPE=A(1), QCLASS=IN(1) */
    msg[21] = 0;
    msg[22] = 1;
    msg[23] = 0;
    msg[24] = 1;

    uint8_t out[600];
    int tlen = nfs_dns_truncate_response(msg, 600, 512, out, sizeof(out));
    ASSERT_TRUE(tlen > 0);
    ASSERT_TRUE(tlen <= 512);
    ASSERT_EQ(nfs_dns_is_truncated(out, (size_t)tlen), 1);
}

static void test_truncate_response_null(void) {
    uint8_t out[64];
    ASSERT_EQ(nfs_dns_truncate_response(NULL, 100, 512, out, sizeof(out)), -1);
}

/* ---- TCP stream parsing tests ---- */

static void test_tcp_stream_single(void) {
    uint8_t msg[NFS_DNS_HDR_SIZE];
    make_dns_msg(msg, 1, 0);

    uint8_t stream[64];
    int flen = nfs_dns_tcp_frame(msg, NFS_DNS_HDR_SIZE, stream, sizeof(stream));
    ASSERT_TRUE(flen > 0);

    size_t offsets[4];
    uint16_t lengths[4];
    int count = nfs_dns_tcp_parse_stream(stream, (size_t)flen, offsets, lengths, 4);
    ASSERT_EQ(count, 1);
    ASSERT_EQ(lengths[0], NFS_DNS_HDR_SIZE);
    ASSERT_EQ(offsets[0], 2);
}

static void test_tcp_stream_multiple(void) {
    uint8_t stream[256];
    size_t pos = 0;

    /* Frame 3 messages */
    for (int i = 0; i < 3; i++) {
        uint8_t msg[NFS_DNS_HDR_SIZE];
        make_dns_msg(msg, (uint16_t)(i + 1), 0);
        int flen = nfs_dns_tcp_frame(msg, NFS_DNS_HDR_SIZE, stream + pos, sizeof(stream) - pos);
        ASSERT_TRUE(flen > 0);
        pos += (size_t)flen;
    }

    size_t offsets[4];
    uint16_t lengths[4];
    int count = nfs_dns_tcp_parse_stream(stream, pos, offsets, lengths, 4);
    ASSERT_EQ(count, 3);
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(lengths[i], NFS_DNS_HDR_SIZE);
    }
}

static void test_tcp_stream_empty(void) {
    size_t offsets[4];
    uint16_t lengths[4];
    uint8_t data[2] = {0, 0};
    int count = nfs_dns_tcp_parse_stream(data, 0, offsets, lengths, 4);
    ASSERT_EQ(count, 0);
}

int main(void) {
    printf("DNS Transport Tests\n");

    test_tcp_frame_basic();
    test_tcp_frame_null();
    test_tcp_frame_zero_len();
    test_tcp_frame_buffer_too_small();
    test_tcp_unframe_basic();
    test_tcp_unframe_roundtrip();
    test_tcp_unframe_incomplete();
    test_tcp_unframe_truncated_message();
    test_tcp_unframe_null();
    test_truncation_not_set();
    test_truncation_set();
    test_set_truncated();
    test_clear_truncated();
    test_set_truncated_too_short();
    test_truncated_null_msg();
    test_fits_udp();
    test_fits_tcp();
    test_truncate_response_already_fits();
    test_truncate_response_sets_tc();
    test_truncate_response_null();
    test_tcp_stream_single();
    test_tcp_stream_multiple();
    test_tcp_stream_empty();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
