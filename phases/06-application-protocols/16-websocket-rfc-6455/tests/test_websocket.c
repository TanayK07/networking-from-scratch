/* Tests for WebSocket frame parser/builder (RFC 6455). */

#include "../websocket.h"

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

/* ------------------------------------------------------------------ */

static void test_parse_small_unmasked(void) {
    printf("  parse small unmasked frame...");
    /* FIN=1, opcode=text, MASK=0, len=5 */
    uint8_t data[] = {0x81, 0x05, 'H', 'e', 'l', 'l', 'o'};
    struct nfs_ws_frame f;
    size_t hdr_len;

    ASSERT_EQ(nfs_ws_parse_frame(data, sizeof(data), &f, &hdr_len), 0);
    ASSERT_EQ(f.fin, 1);
    ASSERT_EQ(f.opcode, NFS_WS_TEXT);
    ASSERT_EQ(f.masked, 0);
    ASSERT_EQ(f.payload_len, 5);
    ASSERT_EQ(hdr_len, 2);
    printf(" OK\n");
}

static void test_parse_masked(void) {
    printf("  parse masked frame...");
    /* FIN=1, opcode=text, MASK=1, len=5, mask_key=0x37,0xfa,0x21,0x3d */
    uint8_t data[] = {0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58};
    struct nfs_ws_frame f;
    size_t hdr_len;

    ASSERT_EQ(nfs_ws_parse_frame(data, sizeof(data), &f, &hdr_len), 0);
    ASSERT_EQ(f.fin, 1);
    ASSERT_EQ(f.masked, 1);
    ASSERT_EQ(f.payload_len, 5);
    ASSERT_EQ(hdr_len, 6);
    ASSERT_EQ(f.mask_key[0], 0x37);
    ASSERT_EQ(f.mask_key[3], 0x3d);
    printf(" OK\n");
}

static void test_parse_16bit_len(void) {
    printf("  parse 16-bit length...");
    /* FIN=1, opcode=binary, MASK=0, len=256 (extended 16-bit) */
    uint8_t data[4] = {0x82, 0x7E, 0x01, 0x00};
    struct nfs_ws_frame f;
    size_t hdr_len;

    ASSERT_EQ(nfs_ws_parse_frame(data, sizeof(data), &f, &hdr_len), 0);
    ASSERT_EQ(f.payload_len, 256);
    ASSERT_EQ(hdr_len, 4);
    printf(" OK\n");
}

static void test_parse_64bit_len(void) {
    printf("  parse 64-bit length...");
    /* FIN=1, opcode=binary, MASK=0, len=70000 (extended 64-bit) */
    uint8_t data[10] = {0x82, 0x7F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x11, 0x70};
    struct nfs_ws_frame f;
    size_t hdr_len;

    ASSERT_EQ(nfs_ws_parse_frame(data, sizeof(data), &f, &hdr_len), 0);
    ASSERT_EQ(f.payload_len, 70000);
    ASSERT_EQ(hdr_len, 10);
    printf(" OK\n");
}

static void test_parse_need_more(void) {
    printf("  parse need more data...");
    uint8_t data[1] = {0x81};
    struct nfs_ws_frame f;
    size_t hdr_len;

    ASSERT_EQ(nfs_ws_parse_frame(data, 1, &f, &hdr_len), 1);
    printf(" OK\n");
}

static void test_parse_rejects_null(void) {
    printf("  parse rejects NULL...");
    struct nfs_ws_frame f;
    size_t hdr_len;
    ASSERT_EQ(nfs_ws_parse_frame(NULL, 10, &f, &hdr_len), -1);
    printf(" OK\n");
}

static void test_parse_control_close(void) {
    printf("  parse close frame...");
    /* FIN=1, opcode=close, MASK=0, len=0 */
    uint8_t data[] = {0x88, 0x00};
    struct nfs_ws_frame f;
    size_t hdr_len;

    ASSERT_EQ(nfs_ws_parse_frame(data, sizeof(data), &f, &hdr_len), 0);
    ASSERT_EQ(f.opcode, NFS_WS_CLOSE);
    ASSERT_EQ(f.fin, 1);
    ASSERT_EQ(f.payload_len, 0);
    printf(" OK\n");
}

static void test_parse_ping(void) {
    printf("  parse ping frame...");
    uint8_t data[] = {0x89, 0x04, 0xDE, 0xAD, 0xBE, 0xEF};
    struct nfs_ws_frame f;
    size_t hdr_len;

    ASSERT_EQ(nfs_ws_parse_frame(data, sizeof(data), &f, &hdr_len), 0);
    ASSERT_EQ(f.opcode, NFS_WS_PING);
    ASSERT_EQ(f.payload_len, 4);
    printf(" OK\n");
}

static void test_build_small_unmasked(void) {
    printf("  build small unmasked...");
    uint8_t payload[] = "Hello";
    uint8_t out[64];

    int n = nfs_ws_build_frame(NFS_WS_TEXT, 1, 0, NULL, payload, 5, out, sizeof(out));
    ASSERT_EQ(n, 7);
    ASSERT_EQ(out[0], 0x81);
    ASSERT_EQ(out[1], 0x05);
    ASSERT_TRUE(memcmp(out + 2, "Hello", 5) == 0);
    printf(" OK\n");
}

static void test_build_masked(void) {
    printf("  build masked...");
    uint8_t payload[] = "Hi";
    uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78};
    uint8_t out[64];

    int n = nfs_ws_build_frame(NFS_WS_TEXT, 1, 1, mask, payload, 2, out, sizeof(out));
    ASSERT_EQ(n, 8);
    ASSERT_EQ(out[0], 0x81);
    ASSERT_EQ(out[1], 0x82);
    ASSERT_TRUE(memcmp(out + 2, mask, 4) == 0);
    printf(" OK\n");
}

static void test_build_16bit_len(void) {
    printf("  build 16-bit length...");
    uint8_t out[512];
    uint8_t payload[200];
    memset(payload, 'A', 200);

    int n = nfs_ws_build_frame(NFS_WS_BINARY, 1, 0, NULL, payload, 200, out, sizeof(out));
    ASSERT_EQ(n, 204);
    ASSERT_EQ(out[1] & 0x7F, 126);
    printf(" OK\n");
}

static void test_build_rejects_small_buf(void) {
    printf("  build rejects small buf...");
    uint8_t out[2];
    ASSERT_EQ(nfs_ws_build_frame(NFS_WS_TEXT, 1, 0, NULL, (uint8_t *)"Hi", 2, out, 2), -1);
    printf(" OK\n");
}

static void test_build_rejects_masked_no_key(void) {
    printf("  build rejects masked without key...");
    uint8_t out[64];
    ASSERT_EQ(nfs_ws_build_frame(NFS_WS_TEXT, 1, 1, NULL, (uint8_t *)"Hi", 2, out, sizeof(out)),
              -1);
    printf(" OK\n");
}

static void test_mask_roundtrip(void) {
    printf("  mask/unmask roundtrip...");
    uint8_t data[] = "Hello, WebSocket!";
    size_t len = strlen((char *)data);
    uint8_t original[32];
    memcpy(original, data, len);

    uint8_t mask[4] = {0xAB, 0xCD, 0xEF, 0x01};
    nfs_ws_mask_payload(data, len, mask);
    ASSERT_TRUE(memcmp(data, original, len) != 0);

    nfs_ws_mask_payload(data, len, mask);
    ASSERT_TRUE(memcmp(data, original, len) == 0);
    printf(" OK\n");
}

static void test_build_parse_roundtrip(void) {
    printf("  build+parse roundtrip...");
    uint8_t payload[] = "test data";
    uint8_t buf[64];

    int n = nfs_ws_build_frame(NFS_WS_TEXT, 1, 0, NULL, payload, 9, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    struct nfs_ws_frame f;
    size_t hdr_len;
    ASSERT_EQ(nfs_ws_parse_frame(buf, (size_t)n, &f, &hdr_len), 0);
    ASSERT_EQ(f.fin, 1);
    ASSERT_EQ(f.opcode, NFS_WS_TEXT);
    ASSERT_EQ(f.payload_len, 9);
    ASSERT_TRUE(memcmp(buf + hdr_len, "test data", 9) == 0);
    printf(" OK\n");
}

static void test_is_control(void) {
    printf("  is_control...");
    ASSERT_EQ(nfs_ws_is_control(NFS_WS_CONTINUATION), 0);
    ASSERT_EQ(nfs_ws_is_control(NFS_WS_TEXT), 0);
    ASSERT_EQ(nfs_ws_is_control(NFS_WS_BINARY), 0);
    ASSERT_EQ(nfs_ws_is_control(NFS_WS_CLOSE), 1);
    ASSERT_EQ(nfs_ws_is_control(NFS_WS_PING), 1);
    ASSERT_EQ(nfs_ws_is_control(NFS_WS_PONG), 1);
    printf(" OK\n");
}

static void test_opcode_str(void) {
    printf("  opcode_str...");
    ASSERT_EQ(strcmp(nfs_ws_opcode_str(NFS_WS_TEXT), "text"), 0);
    ASSERT_EQ(strcmp(nfs_ws_opcode_str(NFS_WS_BINARY), "binary"), 0);
    ASSERT_EQ(strcmp(nfs_ws_opcode_str(NFS_WS_CLOSE), "close"), 0);
    ASSERT_EQ(strcmp(nfs_ws_opcode_str(NFS_WS_PING), "ping"), 0);
    ASSERT_EQ(strcmp(nfs_ws_opcode_str(NFS_WS_PONG), "pong"), 0);
    ASSERT_EQ(strcmp(nfs_ws_opcode_str(0x0), "continuation"), 0);
    ASSERT_EQ(strcmp(nfs_ws_opcode_str(0x7), "unknown"), 0);
    printf(" OK\n");
}

static void test_rsv_bits(void) {
    printf("  RSV bits...");
    /* FIN=1, RSV1=1, RSV2=0, RSV3=0, opcode=text => 0xC1 */
    uint8_t data[] = {0xC1, 0x00};
    struct nfs_ws_frame f;
    size_t hdr_len;

    ASSERT_EQ(nfs_ws_parse_frame(data, sizeof(data), &f, &hdr_len), 0);
    ASSERT_EQ(f.rsv, 0x04);
    ASSERT_EQ(f.fin, 1);
    ASSERT_EQ(f.opcode, NFS_WS_TEXT);
    printf(" OK\n");
}

static void test_continuation_frame(void) {
    printf("  continuation frame...");
    /* FIN=0, opcode=text (first fragment) */
    uint8_t frag1[] = {0x01, 0x03, 'H', 'e', 'l'};
    struct nfs_ws_frame f;
    size_t hdr_len;

    ASSERT_EQ(nfs_ws_parse_frame(frag1, sizeof(frag1), &f, &hdr_len), 0);
    ASSERT_EQ(f.fin, 0);
    ASSERT_EQ(f.opcode, NFS_WS_TEXT);

    /* FIN=1, opcode=continuation (final fragment) */
    uint8_t frag2[] = {0x80, 0x02, 'l', 'o'};
    ASSERT_EQ(nfs_ws_parse_frame(frag2, sizeof(frag2), &f, &hdr_len), 0);
    ASSERT_EQ(f.fin, 1);
    ASSERT_EQ(f.opcode, NFS_WS_CONTINUATION);
    printf(" OK\n");
}

/* ------------------------------------------------------------------ */

int main(void) {
    printf("WebSocket (RFC 6455) test suite\n");
    test_parse_small_unmasked();
    test_parse_masked();
    test_parse_16bit_len();
    test_parse_64bit_len();
    test_parse_need_more();
    test_parse_rejects_null();
    test_parse_control_close();
    test_parse_ping();
    test_build_small_unmasked();
    test_build_masked();
    test_build_16bit_len();
    test_build_rejects_small_buf();
    test_build_rejects_masked_no_key();
    test_mask_roundtrip();
    test_build_parse_roundtrip();
    test_is_control();
    test_opcode_str();
    test_rsv_bits();
    test_continuation_frame();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
