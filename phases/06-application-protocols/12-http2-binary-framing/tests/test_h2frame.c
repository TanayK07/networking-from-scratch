/* Unit tests for HTTP/2 binary framing. */

#include "../h2frame.h"
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

/* ---- Frame build/parse roundtrip ---- */

static void test_data_frame_roundtrip(void) {
    uint8_t buf[128];
    const char *data = "Hello";
    int n = nfs_h2_frame_build(buf, sizeof(buf), NFS_H2_DATA, NFS_H2_FLAG_END_STREAM, 1,
                               (const uint8_t *)data, 5);
    ASSERT_EQ(n, NFS_H2_FRAME_HEADER_SIZE + 5);

    struct nfs_h2_frame f;
    ASSERT_EQ(nfs_h2_frame_parse(buf, (size_t)n, &f), 0);
    ASSERT_EQ(f.length, 5);
    ASSERT_EQ(f.type, NFS_H2_DATA);
    ASSERT_EQ(f.flags, NFS_H2_FLAG_END_STREAM);
    ASSERT_EQ(f.stream_id, 1);
    ASSERT_TRUE(memcmp(f.payload, "Hello", 5) == 0);
}

static void test_headers_frame(void) {
    uint8_t buf[128];
    uint8_t hblock[] = {0x82, 0x86, 0x84}; /* mock HPACK */
    int n = nfs_h2_frame_build(buf, sizeof(buf), NFS_H2_HEADERS,
                               NFS_H2_FLAG_END_HEADERS | NFS_H2_FLAG_END_STREAM, 3, hblock, 3);
    ASSERT_TRUE(n > 0);

    struct nfs_h2_frame f;
    ASSERT_EQ(nfs_h2_frame_parse(buf, (size_t)n, &f), 0);
    ASSERT_EQ(f.type, NFS_H2_HEADERS);
    ASSERT_EQ(f.flags, NFS_H2_FLAG_END_HEADERS | NFS_H2_FLAG_END_STREAM);
    ASSERT_EQ(f.stream_id, 3);
    ASSERT_EQ(f.length, 3);
}

static void test_settings_frame_roundtrip(void) {
    struct nfs_h2_setting settings[] = {
        {NFS_H2_SETTINGS_HEADER_TABLE_SIZE, 4096},
        {NFS_H2_SETTINGS_INITIAL_WINDOW_SIZE, 65535},
        {NFS_H2_SETTINGS_MAX_FRAME_SIZE, 16384},
    };
    uint8_t spayload[64];
    int slen = nfs_h2_settings_build(spayload, sizeof(spayload), settings, 3);
    ASSERT_EQ(slen, 18); /* 3 * 6 = 18 */

    uint8_t buf[64];
    int total =
        nfs_h2_frame_build(buf, sizeof(buf), NFS_H2_SETTINGS, 0, 0, spayload, (uint32_t)slen);
    ASSERT_EQ(total, NFS_H2_FRAME_HEADER_SIZE + 18);

    struct nfs_h2_frame f;
    ASSERT_EQ(nfs_h2_frame_parse(buf, (size_t)total, &f), 0);
    ASSERT_EQ(f.type, NFS_H2_SETTINGS);
    ASSERT_EQ(f.stream_id, 0);
    ASSERT_EQ(f.length, 18);

    struct nfs_h2_setting parsed[8];
    int count = nfs_h2_settings_parse(f.payload, f.length, parsed, 8);
    ASSERT_EQ(count, 3);
    ASSERT_EQ(parsed[0].id, NFS_H2_SETTINGS_HEADER_TABLE_SIZE);
    ASSERT_EQ(parsed[0].value, 4096);
    ASSERT_EQ(parsed[1].id, NFS_H2_SETTINGS_INITIAL_WINDOW_SIZE);
    ASSERT_EQ(parsed[1].value, 65535);
    ASSERT_EQ(parsed[2].id, NFS_H2_SETTINGS_MAX_FRAME_SIZE);
    ASSERT_EQ(parsed[2].value, 16384);
}

static void test_ping_frame(void) {
    uint8_t opaque[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t buf[32];
    int n = nfs_h2_frame_build(buf, sizeof(buf), NFS_H2_PING, NFS_H2_FLAG_ACK, 0, opaque, 8);
    ASSERT_EQ(n, NFS_H2_FRAME_HEADER_SIZE + 8);

    struct nfs_h2_frame f;
    ASSERT_EQ(nfs_h2_frame_parse(buf, (size_t)n, &f), 0);
    ASSERT_EQ(f.type, NFS_H2_PING);
    ASSERT_EQ(f.flags, NFS_H2_FLAG_ACK);
    ASSERT_EQ(f.length, 8);
    ASSERT_TRUE(memcmp(f.payload, opaque, 8) == 0);
}

static void test_window_update_frame(void) {
    /* WINDOW_UPDATE payload: 4 bytes, top bit reserved */
    uint8_t payload[4];
    uint32_t increment = 65535;
    payload[0] = (uint8_t)((increment >> 24) & 0x7F);
    payload[1] = (uint8_t)((increment >> 16) & 0xFF);
    payload[2] = (uint8_t)((increment >> 8) & 0xFF);
    payload[3] = (uint8_t)((increment) & 0xFF);

    uint8_t buf[32];
    int n = nfs_h2_frame_build(buf, sizeof(buf), NFS_H2_WINDOW_UPDATE, 0, 5, payload, 4);
    ASSERT_TRUE(n > 0);

    struct nfs_h2_frame f;
    ASSERT_EQ(nfs_h2_frame_parse(buf, (size_t)n, &f), 0);
    ASSERT_EQ(f.type, NFS_H2_WINDOW_UPDATE);
    ASSERT_EQ(f.stream_id, 5);
}

static void test_goaway_frame(void) {
    uint8_t payload[8] = {0, 0, 0, 7, 0, 0, 0, 0}; /* last_stream=7, error=0 */
    uint8_t buf[32];
    int n = nfs_h2_frame_build(buf, sizeof(buf), NFS_H2_GOAWAY, 0, 0, payload, 8);
    ASSERT_TRUE(n > 0);

    struct nfs_h2_frame f;
    ASSERT_EQ(nfs_h2_frame_parse(buf, (size_t)n, &f), 0);
    ASSERT_EQ(f.type, NFS_H2_GOAWAY);
    ASSERT_EQ(f.stream_id, 0);
    ASSERT_EQ(f.length, 8);
}

/* ---- Frame type names ---- */

static void test_frame_type_names(void) {
    ASSERT_TRUE(strcmp(nfs_h2_frame_type_name(NFS_H2_DATA), "DATA") == 0);
    ASSERT_TRUE(strcmp(nfs_h2_frame_type_name(NFS_H2_HEADERS), "HEADERS") == 0);
    ASSERT_TRUE(strcmp(nfs_h2_frame_type_name(NFS_H2_PRIORITY), "PRIORITY") == 0);
    ASSERT_TRUE(strcmp(nfs_h2_frame_type_name(NFS_H2_RST_STREAM), "RST_STREAM") == 0);
    ASSERT_TRUE(strcmp(nfs_h2_frame_type_name(NFS_H2_SETTINGS), "SETTINGS") == 0);
    ASSERT_TRUE(strcmp(nfs_h2_frame_type_name(NFS_H2_PUSH_PROMISE), "PUSH_PROMISE") == 0);
    ASSERT_TRUE(strcmp(nfs_h2_frame_type_name(NFS_H2_PING), "PING") == 0);
    ASSERT_TRUE(strcmp(nfs_h2_frame_type_name(NFS_H2_GOAWAY), "GOAWAY") == 0);
    ASSERT_TRUE(strcmp(nfs_h2_frame_type_name(NFS_H2_WINDOW_UPDATE), "WINDOW_UPDATE") == 0);
    ASSERT_TRUE(strcmp(nfs_h2_frame_type_name(NFS_H2_CONTINUATION), "CONTINUATION") == 0);
    ASSERT_TRUE(strcmp(nfs_h2_frame_type_name(99), "UNKNOWN") == 0);
}

/* ---- Error cases ---- */

static void test_parse_truncated(void) {
    uint8_t buf[4] = {0};
    struct nfs_h2_frame f;
    ASSERT_EQ(nfs_h2_frame_parse(buf, 4, &f), -1);
}

static void test_parse_payload_truncated(void) {
    /* Build a valid header claiming 100 bytes of payload, but buffer is only 9 */
    uint8_t buf[NFS_H2_FRAME_HEADER_SIZE];
    buf[0] = 0;
    buf[1] = 0;
    buf[2] = 100; /* length = 100 */
    buf[3] = 0;
    buf[4] = 0;
    buf[5] = buf[6] = buf[7] = buf[8] = 0;

    struct nfs_h2_frame f;
    ASSERT_EQ(nfs_h2_frame_parse(buf, NFS_H2_FRAME_HEADER_SIZE, &f), -1);
}

static void test_parse_null_buf(void) {
    struct nfs_h2_frame f;
    ASSERT_EQ(nfs_h2_frame_parse(NULL, 100, &f), -1);
}

static void test_build_buffer_too_small(void) {
    uint8_t buf[4];
    ASSERT_EQ(nfs_h2_frame_build(buf, sizeof(buf), NFS_H2_DATA, 0, 1, NULL, 0), -1);
}

static void test_empty_payload_frame(void) {
    uint8_t buf[32];
    int n = nfs_h2_frame_build(buf, sizeof(buf), NFS_H2_SETTINGS, NFS_H2_FLAG_ACK, 0, NULL, 0);
    ASSERT_EQ(n, NFS_H2_FRAME_HEADER_SIZE);

    struct nfs_h2_frame f;
    ASSERT_EQ(nfs_h2_frame_parse(buf, (size_t)n, &f), 0);
    ASSERT_EQ(f.length, 0);
    ASSERT_EQ(f.type, NFS_H2_SETTINGS);
    ASSERT_EQ(f.flags, NFS_H2_FLAG_ACK);
    ASSERT_TRUE(f.payload == NULL);
}

static void test_stream_id_reserved_bit_masked(void) {
    uint8_t buf[32];
    /* Stream ID with reserved bit set (0x80000001) should be masked to 1 */
    int n = nfs_h2_frame_build(buf, sizeof(buf), NFS_H2_DATA, 0, 0x80000001U, NULL, 0);
    ASSERT_TRUE(n > 0);

    struct nfs_h2_frame f;
    ASSERT_EQ(nfs_h2_frame_parse(buf, (size_t)n, &f), 0);
    ASSERT_EQ(f.stream_id, 1);
}

static void test_settings_invalid_length(void) {
    uint8_t payload[7] = {0}; /* not a multiple of 6 */
    struct nfs_h2_setting out[4];
    ASSERT_EQ(nfs_h2_settings_parse(payload, 7, out, 4), -1);
}

static void test_settings_empty(void) {
    struct nfs_h2_setting out[4];
    int n = nfs_h2_settings_parse(NULL, 0, out, 4);
    ASSERT_EQ(n, 0);
}

static void test_rst_stream_frame(void) {
    /* RST_STREAM payload: 4 bytes error code */
    uint8_t payload[4] = {0, 0, 0, 2}; /* INTERNAL_ERROR = 2 */
    uint8_t buf[32];
    int n = nfs_h2_frame_build(buf, sizeof(buf), NFS_H2_RST_STREAM, 0, 3, payload, 4);
    ASSERT_TRUE(n > 0);

    struct nfs_h2_frame f;
    ASSERT_EQ(nfs_h2_frame_parse(buf, (size_t)n, &f), 0);
    ASSERT_EQ(f.type, NFS_H2_RST_STREAM);
    ASSERT_EQ(f.stream_id, 3);
    ASSERT_EQ(f.length, 4);
}

static void test_wire_format_bytes(void) {
    /* Manually verify wire format for a known frame */
    uint8_t buf[32];
    int n = nfs_h2_frame_build(buf, sizeof(buf), NFS_H2_PING, 0, 0,
                               (const uint8_t *)"\x01\x02\x03\x04\x05\x06\x07\x08", 8);
    ASSERT_EQ(n, 17);
    /* Length: 0x000008 */
    ASSERT_EQ(buf[0], 0x00);
    ASSERT_EQ(buf[1], 0x00);
    ASSERT_EQ(buf[2], 0x08);
    /* Type: PING = 6 */
    ASSERT_EQ(buf[3], 0x06);
    /* Flags: 0 */
    ASSERT_EQ(buf[4], 0x00);
    /* Stream ID: 0 */
    ASSERT_EQ(buf[5], 0x00);
    ASSERT_EQ(buf[6], 0x00);
    ASSERT_EQ(buf[7], 0x00);
    ASSERT_EQ(buf[8], 0x00);
}

int main(void) {
    printf("HTTP/2 Binary Framing Tests\n");

    test_data_frame_roundtrip();
    test_headers_frame();
    test_settings_frame_roundtrip();
    test_ping_frame();
    test_window_update_frame();
    test_goaway_frame();
    test_frame_type_names();
    test_parse_truncated();
    test_parse_payload_truncated();
    test_parse_null_buf();
    test_build_buffer_too_small();
    test_empty_payload_frame();
    test_stream_id_reserved_bit_masked();
    test_settings_invalid_length();
    test_settings_empty();
    test_rst_stream_frame();
    test_wire_format_bytes();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
