/* Unit tests for QUIC packets and frames. */

#include "../quic.h"
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

/* ---- Varint encode tests ---- */

static void test_varint_encode_1byte(void) {
    uint8_t buf[8];
    ASSERT_EQ(nfs_quic_varint_encode(0, buf, sizeof(buf)), 1);
    ASSERT_EQ(buf[0], 0x00);

    ASSERT_EQ(nfs_quic_varint_encode(37, buf, sizeof(buf)), 1);
    ASSERT_EQ(buf[0], 0x25);

    ASSERT_EQ(nfs_quic_varint_encode(63, buf, sizeof(buf)), 1);
    ASSERT_EQ(buf[0], 0x3F);
}

static void test_varint_encode_2byte(void) {
    uint8_t buf[8];
    ASSERT_EQ(nfs_quic_varint_encode(15293, buf, sizeof(buf)), 2);
    /* 15293 = 0x3BBD, with prefix 01: 0x7BBD */
    ASSERT_EQ(buf[0], 0x7B);
    ASSERT_EQ(buf[1], 0xBD);
}

static void test_varint_encode_4byte(void) {
    uint8_t buf[8];
    ASSERT_EQ(nfs_quic_varint_encode(494878333, buf, sizeof(buf)), 4);
    /* 494878333 = 0x1D7F3E7D, with prefix 10: 0x9D7F3E7D */
    ASSERT_EQ(buf[0], 0x9D);
    ASSERT_EQ(buf[1], 0x7F);
    ASSERT_EQ(buf[2], 0x3E);
    ASSERT_EQ(buf[3], 0x7D);
}

static void test_varint_encode_8byte(void) {
    uint8_t buf[8];
    ASSERT_EQ(nfs_quic_varint_encode(151288809941952652ULL, buf, sizeof(buf)), 8);
    ASSERT_EQ(buf[0], 0xC2);
    ASSERT_EQ(buf[1], 0x19);
    ASSERT_EQ(buf[2], 0x7C);
    ASSERT_EQ(buf[3], 0x5E);
    ASSERT_EQ(buf[4], 0xFF);
    ASSERT_EQ(buf[5], 0x14);
    ASSERT_EQ(buf[6], 0xE8);
    ASSERT_EQ(buf[7], 0x8C);
}

static void test_varint_encode_buffer_too_small(void) {
    uint8_t buf[1];
    ASSERT_EQ(nfs_quic_varint_encode(15293, buf, sizeof(buf)), -1);
}

static void test_varint_encode_null(void) {
    ASSERT_EQ(nfs_quic_varint_encode(0, NULL, 8), -1);
}

/* ---- Varint decode tests ---- */

static void test_varint_decode_1byte(void) {
    uint8_t data[] = {0x25}; /* 37 */
    uint64_t val;
    ASSERT_EQ(nfs_quic_varint_decode(data, sizeof(data), &val), 1);
    ASSERT_EQ(val, 37u);
}

static void test_varint_decode_2byte(void) {
    uint8_t data[] = {0x7B, 0xBD};
    uint64_t val;
    ASSERT_EQ(nfs_quic_varint_decode(data, sizeof(data), &val), 2);
    ASSERT_EQ(val, 15293u);
}

static void test_varint_decode_4byte(void) {
    uint8_t data[] = {0x9D, 0x7F, 0x3E, 0x7D};
    uint64_t val;
    ASSERT_EQ(nfs_quic_varint_decode(data, sizeof(data), &val), 4);
    ASSERT_EQ(val, 494878333u);
}

static void test_varint_decode_8byte(void) {
    uint8_t data[] = {0xC2, 0x19, 0x7C, 0x5E, 0xFF, 0x14, 0xE8, 0x8C};
    uint64_t val;
    ASSERT_EQ(nfs_quic_varint_decode(data, sizeof(data), &val), 8);
    ASSERT_TRUE(val == 151288809941952652ULL);
}

static void test_varint_roundtrip(void) {
    uint64_t values[] = {0, 1, 63, 64, 16383, 16384, 1073741823, 1073741824};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        uint8_t buf[8];
        int enc = nfs_quic_varint_encode(values[i], buf, sizeof(buf));
        ASSERT_TRUE(enc > 0);
        uint64_t decoded;
        int dec = nfs_quic_varint_decode(buf, (size_t)enc, &decoded);
        ASSERT_EQ(dec, enc);
        ASSERT_TRUE(decoded == values[i]);
    }
}

static void test_varint_decode_too_short(void) {
    uint8_t data[] = {0x7B}; /* needs 2 bytes */
    uint64_t val;
    ASSERT_EQ(nfs_quic_varint_decode(data, 1, &val), -1);
}

static void test_varint_decode_null(void) {
    uint64_t val;
    ASSERT_EQ(nfs_quic_varint_decode(NULL, 8, &val), -1);
    uint8_t data[] = {0x00};
    ASSERT_EQ(nfs_quic_varint_decode(data, 1, NULL), -1);
}

static void test_varint_size(void) {
    ASSERT_EQ(nfs_quic_varint_size(0), 1);
    ASSERT_EQ(nfs_quic_varint_size(63), 1);
    ASSERT_EQ(nfs_quic_varint_size(64), 2);
    ASSERT_EQ(nfs_quic_varint_size(16383), 2);
    ASSERT_EQ(nfs_quic_varint_size(16384), 4);
    ASSERT_EQ(nfs_quic_varint_size(1073741823), 4);
    ASSERT_EQ(nfs_quic_varint_size(1073741824), 8);
}

/* ---- Long header tests ---- */

static void test_long_hdr_build_initial(void) {
    struct nfs_quic_long_hdr hdr = {
        .first_byte =
            NFS_QUIC_HEADER_FORM_BIT | NFS_QUIC_FIXED_BIT | (NFS_QUIC_LONG_TYPE_INITIAL << 4),
        .version = NFS_QUIC_VERSION_1,
        .dcid_len = 4,
        .dcid = {0x01, 0x02, 0x03, 0x04},
        .scid_len = 4,
        .scid = {0x05, 0x06, 0x07, 0x08},
    };
    uint8_t buf[64];
    int n = nfs_quic_long_hdr_build(&hdr, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    /* 1 + 4 + 1 + 4 + 1 + 4 = 15 */
    ASSERT_EQ(n, 15);
    ASSERT_TRUE(buf[0] & NFS_QUIC_HEADER_FORM_BIT);
}

static void test_long_hdr_roundtrip(void) {
    struct nfs_quic_long_hdr hdr = {
        .first_byte =
            NFS_QUIC_HEADER_FORM_BIT | NFS_QUIC_FIXED_BIT | (NFS_QUIC_LONG_TYPE_HANDSHAKE << 4),
        .version = NFS_QUIC_VERSION_1,
        .dcid_len = 8,
        .dcid = {0x83, 0x94, 0xc8, 0xf0, 0x3e, 0x51, 0x57, 0x08},
        .scid_len = 0,
    };
    uint8_t buf[64];
    int n = nfs_quic_long_hdr_build(&hdr, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    struct nfs_quic_long_hdr parsed;
    int consumed = nfs_quic_long_hdr_parse(buf, (size_t)n, &parsed);
    ASSERT_EQ(consumed, n);
    ASSERT_EQ(parsed.version, NFS_QUIC_VERSION_1);
    ASSERT_EQ(parsed.dcid_len, 8);
    ASSERT_TRUE(memcmp(parsed.dcid, hdr.dcid, 8) == 0);
    ASSERT_EQ(parsed.scid_len, 0);
    ASSERT_EQ(nfs_quic_long_hdr_type(parsed.first_byte), NFS_QUIC_LONG_TYPE_HANDSHAKE);
}

static void test_long_hdr_parse_too_short(void) {
    uint8_t buf[] = {0xC0, 0x00, 0x00};
    struct nfs_quic_long_hdr hdr;
    ASSERT_EQ(nfs_quic_long_hdr_parse(buf, sizeof(buf), &hdr), -1);
}

static void test_long_hdr_parse_not_long(void) {
    /* Form bit = 0 -> short header */
    uint8_t buf[16] = {0x40, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00};
    struct nfs_quic_long_hdr hdr;
    ASSERT_EQ(nfs_quic_long_hdr_parse(buf, sizeof(buf), &hdr), -1);
}

static void test_long_hdr_null(void) {
    uint8_t buf[32];
    ASSERT_EQ(nfs_quic_long_hdr_build(NULL, buf, sizeof(buf)), -1);
    struct nfs_quic_long_hdr hdr;
    ASSERT_EQ(nfs_quic_long_hdr_parse(NULL, 32, &hdr), -1);
}

/* ---- Short header tests ---- */

static void test_short_hdr_build(void) {
    struct nfs_quic_short_hdr hdr = {
        .first_byte = NFS_QUIC_FIXED_BIT | NFS_QUIC_SHORT_SPIN_BIT,
        .dcid_len = 4,
        .dcid = {0x01, 0x02, 0x03, 0x04},
    };
    uint8_t buf[32];
    int n = nfs_quic_short_hdr_build(&hdr, buf, sizeof(buf));
    ASSERT_EQ(n, 5);                                   /* 1 + 4 */
    ASSERT_TRUE(!(buf[0] & NFS_QUIC_HEADER_FORM_BIT)); /* form = 0 */
    ASSERT_TRUE(buf[0] & NFS_QUIC_FIXED_BIT);
}

static void test_short_hdr_roundtrip(void) {
    struct nfs_quic_short_hdr hdr = {
        .first_byte = NFS_QUIC_FIXED_BIT | NFS_QUIC_SHORT_KEY_PHASE_BIT,
        .dcid_len = 8,
        .dcid = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44},
    };
    uint8_t buf[32];
    int n = nfs_quic_short_hdr_build(&hdr, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    struct nfs_quic_short_hdr parsed;
    int consumed = nfs_quic_short_hdr_parse(buf, (size_t)n, 8, &parsed);
    ASSERT_EQ(consumed, n);
    ASSERT_EQ(parsed.dcid_len, 8);
    ASSERT_TRUE(memcmp(parsed.dcid, hdr.dcid, 8) == 0);
    ASSERT_EQ(nfs_quic_short_hdr_key_phase(parsed.first_byte), 1);
}

static void test_short_hdr_spin_bit(void) {
    ASSERT_EQ(nfs_quic_short_hdr_spin(NFS_QUIC_FIXED_BIT | NFS_QUIC_SHORT_SPIN_BIT), 1);
    ASSERT_EQ(nfs_quic_short_hdr_spin(NFS_QUIC_FIXED_BIT), 0);
}

static void test_short_hdr_parse_long_rejected(void) {
    uint8_t buf[] = {0xC0, 0x01, 0x02, 0x03, 0x04}; /* form bit set */
    struct nfs_quic_short_hdr hdr;
    ASSERT_EQ(nfs_quic_short_hdr_parse(buf, sizeof(buf), 4, &hdr), -1);
}

/* ---- Frame type tests ---- */

static void test_frame_type_padding(void) {
    uint8_t data[] = {0x00};
    ASSERT_EQ(nfs_quic_frame_type(data, 1), NFS_QUIC_FRAME_PADDING);
}

static void test_frame_type_ping(void) {
    uint8_t data[] = {0x01};
    ASSERT_EQ(nfs_quic_frame_type(data, 1), NFS_QUIC_FRAME_PING);
}

static void test_frame_type_ack(void) {
    uint8_t data[] = {0x02};
    ASSERT_EQ(nfs_quic_frame_type(data, 1), NFS_QUIC_FRAME_ACK);
}

static void test_frame_type_empty(void) {
    ASSERT_EQ(nfs_quic_frame_type(NULL, 0), -1);
}

static void test_is_stream_frame(void) {
    for (uint8_t t = 0x08; t <= 0x0F; t++)
        ASSERT_EQ(nfs_quic_is_stream_frame(t), 1);
    ASSERT_EQ(nfs_quic_is_stream_frame(0x07), 0);
    ASSERT_EQ(nfs_quic_is_stream_frame(0x10), 0);
}

static void test_stream_bits(void) {
    /* 0x08 = base STREAM, no FIN/LEN/OFF */
    ASSERT_EQ(nfs_quic_stream_has_fin(0x08), 0);
    ASSERT_EQ(nfs_quic_stream_has_len(0x08), 0);
    ASSERT_EQ(nfs_quic_stream_has_off(0x08), 0);

    /* 0x0F = STREAM with FIN+LEN+OFF */
    ASSERT_EQ(nfs_quic_stream_has_fin(0x0F), 1);
    ASSERT_EQ(nfs_quic_stream_has_len(0x0F), 1);
    ASSERT_EQ(nfs_quic_stream_has_off(0x0F), 1);

    /* 0x0B = STREAM with FIN+LEN, no OFF */
    ASSERT_EQ(nfs_quic_stream_has_fin(0x0B), 1);
    ASSERT_EQ(nfs_quic_stream_has_len(0x0B), 1);
    ASSERT_EQ(nfs_quic_stream_has_off(0x0B), 0);
}

static void test_frame_names(void) {
    ASSERT_TRUE(strcmp(nfs_quic_frame_name(0x00), "PADDING") == 0);
    ASSERT_TRUE(strcmp(nfs_quic_frame_name(0x01), "PING") == 0);
    ASSERT_TRUE(strcmp(nfs_quic_frame_name(0x02), "ACK") == 0);
    ASSERT_TRUE(strcmp(nfs_quic_frame_name(0x08), "STREAM") == 0);
    ASSERT_TRUE(strcmp(nfs_quic_frame_name(0x0F), "STREAM") == 0);
}

static void test_long_type_names(void) {
    ASSERT_TRUE(strcmp(nfs_quic_long_type_name(0), "Initial") == 0);
    ASSERT_TRUE(strcmp(nfs_quic_long_type_name(1), "0-RTT") == 0);
    ASSERT_TRUE(strcmp(nfs_quic_long_type_name(2), "Handshake") == 0);
    ASSERT_TRUE(strcmp(nfs_quic_long_type_name(3), "Retry") == 0);
}

int main(void) {
    printf("QUIC Packets and Frames Tests\n");

    test_varint_encode_1byte();
    test_varint_encode_2byte();
    test_varint_encode_4byte();
    test_varint_encode_8byte();
    test_varint_encode_buffer_too_small();
    test_varint_encode_null();
    test_varint_decode_1byte();
    test_varint_decode_2byte();
    test_varint_decode_4byte();
    test_varint_decode_8byte();
    test_varint_roundtrip();
    test_varint_decode_too_short();
    test_varint_decode_null();
    test_varint_size();
    test_long_hdr_build_initial();
    test_long_hdr_roundtrip();
    test_long_hdr_parse_too_short();
    test_long_hdr_parse_not_long();
    test_long_hdr_null();
    test_short_hdr_build();
    test_short_hdr_roundtrip();
    test_short_hdr_spin_bit();
    test_short_hdr_parse_long_rejected();
    test_frame_type_padding();
    test_frame_type_ping();
    test_frame_type_ack();
    test_frame_type_empty();
    test_is_stream_frame();
    test_stream_bits();
    test_frame_names();
    test_long_type_names();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
