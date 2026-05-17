/* Unit tests for TFTP-like protocol (RFC 1350). */

#include "../tftp.h"
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

/* ---- RRQ/WRQ tests ---- */

static void test_build_parse_rrq(void) {
    uint8_t buf[256];
    int n = nfs_tftp_build_rrq(buf, sizeof(buf), NFS_TFTP_OP_RRQ, "test.txt", "octet");
    ASSERT_TRUE(n > 0);
    /* opcode(2) + "test.txt"(8) + NUL(1) + "octet"(5) + NUL(1) = 17 */
    ASSERT_EQ(n, 17);

    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(buf, (size_t)n, &pkt), 0);
    ASSERT_EQ(pkt.opcode, NFS_TFTP_OP_RRQ);
    ASSERT_TRUE(strcmp(pkt.u.rq.filename, "test.txt") == 0);
    ASSERT_TRUE(strcmp(pkt.u.rq.mode, "octet") == 0);
}

static void test_build_parse_wrq(void) {
    uint8_t buf[256];
    int n = nfs_tftp_build_rrq(buf, sizeof(buf), NFS_TFTP_OP_WRQ, "output.bin", "netascii");
    ASSERT_TRUE(n > 0);

    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(buf, (size_t)n, &pkt), 0);
    ASSERT_EQ(pkt.opcode, NFS_TFTP_OP_WRQ);
    ASSERT_TRUE(strcmp(pkt.u.rq.filename, "output.bin") == 0);
    ASSERT_TRUE(strcmp(pkt.u.rq.mode, "netascii") == 0);
}

static void test_rrq_network_byte_order(void) {
    uint8_t buf[256];
    nfs_tftp_build_rrq(buf, sizeof(buf), NFS_TFTP_OP_RRQ, "f", "m");
    /* First two bytes should be opcode 1 in network byte order */
    uint16_t op;
    memcpy(&op, buf, 2);
    ASSERT_EQ(ntohs(op), NFS_TFTP_OP_RRQ);
}

static void test_rrq_invalid_opcode(void) {
    uint8_t buf[256];
    ASSERT_EQ(nfs_tftp_build_rrq(buf, sizeof(buf), NFS_TFTP_OP_DATA, "f", "m"), -1);
}

static void test_rrq_null_filename(void) {
    uint8_t buf[256];
    ASSERT_EQ(nfs_tftp_build_rrq(buf, sizeof(buf), NFS_TFTP_OP_RRQ, NULL, "octet"), -1);
}

static void test_rrq_empty_filename(void) {
    uint8_t buf[256];
    ASSERT_EQ(nfs_tftp_build_rrq(buf, sizeof(buf), NFS_TFTP_OP_RRQ, "", "octet"), -1);
}

/* ---- DATA tests ---- */

static void test_build_parse_data(void) {
    uint8_t buf[600];
    const uint8_t payload[] = "Hello, TFTP!";
    int n = nfs_tftp_build_data(buf, sizeof(buf), 1, payload, 12);
    ASSERT_TRUE(n > 0);
    ASSERT_EQ(n, 16); /* 2 + 2 + 12 */

    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(buf, (size_t)n, &pkt), 0);
    ASSERT_EQ(pkt.opcode, NFS_TFTP_OP_DATA);
    ASSERT_EQ(pkt.u.data.block, 1);
    ASSERT_EQ(pkt.u.data.data_len, 12);
    ASSERT_TRUE(memcmp(pkt.u.data.data, "Hello, TFTP!", 12) == 0);
}

static void test_data_max_block(void) {
    uint8_t buf[520];
    uint8_t payload[NFS_TFTP_DATA_MAX];
    memset(payload, 0xAA, NFS_TFTP_DATA_MAX);

    int n = nfs_tftp_build_data(buf, sizeof(buf), 0xFFFF, payload, NFS_TFTP_DATA_MAX);
    ASSERT_EQ(n, 4 + NFS_TFTP_DATA_MAX);

    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(buf, (size_t)n, &pkt), 0);
    ASSERT_EQ(pkt.u.data.block, 0xFFFF);
    ASSERT_EQ(pkt.u.data.data_len, NFS_TFTP_DATA_MAX);
}

static void test_data_empty_payload(void) {
    uint8_t buf[16];
    int n = nfs_tftp_build_data(buf, sizeof(buf), 5, NULL, 0);
    ASSERT_EQ(n, 4);

    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(buf, (size_t)n, &pkt), 0);
    ASSERT_EQ(pkt.u.data.data_len, 0);
    ASSERT_EQ(pkt.u.data.block, 5);
}

static void test_data_too_large(void) {
    uint8_t buf[600];
    uint8_t payload[513];
    memset(payload, 0, sizeof(payload));
    ASSERT_EQ(nfs_tftp_build_data(buf, sizeof(buf), 1, payload, 513), -1);
}

static void test_data_network_byte_order(void) {
    uint8_t buf[16];
    nfs_tftp_build_data(buf, sizeof(buf), 0x0102, (const uint8_t *)"ab", 2);
    /* opcode at offset 0 */
    ASSERT_EQ(buf[0], 0x00);
    ASSERT_EQ(buf[1], 0x03); /* DATA = 3 */
    /* block at offset 2 */
    ASSERT_EQ(buf[2], 0x01);
    ASSERT_EQ(buf[3], 0x02);
}

/* ---- ACK tests ---- */

static void test_build_parse_ack(void) {
    uint8_t buf[16];
    int n = nfs_tftp_build_ack(buf, sizeof(buf), 42);
    ASSERT_EQ(n, 4);

    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(buf, (size_t)n, &pkt), 0);
    ASSERT_EQ(pkt.opcode, NFS_TFTP_OP_ACK);
    ASSERT_EQ(pkt.u.ack.block, 42);
}

static void test_ack_block_zero(void) {
    uint8_t buf[16];
    nfs_tftp_build_ack(buf, sizeof(buf), 0);

    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(buf, 4, &pkt), 0);
    ASSERT_EQ(pkt.u.ack.block, 0);
}

static void test_ack_buffer_too_small(void) {
    uint8_t buf[3];
    ASSERT_EQ(nfs_tftp_build_ack(buf, sizeof(buf), 1), -1);
}

/* ---- ERROR tests ---- */

static void test_build_parse_error(void) {
    uint8_t buf[256];
    int n = nfs_tftp_build_error(buf, sizeof(buf), NFS_TFTP_ERR_NOT_FOUND, "File not found");
    ASSERT_TRUE(n > 0);

    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(buf, (size_t)n, &pkt), 0);
    ASSERT_EQ(pkt.opcode, NFS_TFTP_OP_ERROR);
    ASSERT_EQ(pkt.u.error.code, NFS_TFTP_ERR_NOT_FOUND);
    ASSERT_TRUE(strcmp(pkt.u.error.msg, "File not found") == 0);
}

static void test_error_empty_msg(void) {
    uint8_t buf[64];
    int n = nfs_tftp_build_error(buf, sizeof(buf), 0, "");
    ASSERT_EQ(n, 5); /* opcode(2) + errcode(2) + NUL */

    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(buf, (size_t)n, &pkt), 0);
    ASSERT_TRUE(strcmp(pkt.u.error.msg, "") == 0);
}

/* ---- Parse error handling ---- */

static void test_parse_too_short(void) {
    uint8_t buf[] = {0x00};
    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(buf, 1, &pkt), -1);
}

static void test_parse_unknown_opcode(void) {
    uint8_t buf[] = {0x00, 0x09, 0x00, 0x00};
    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(buf, 4, &pkt), -1);
}

static void test_parse_data_truncated(void) {
    /* DATA packet with only opcode, no block number */
    uint8_t buf[] = {0x00, 0x03, 0x00};
    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(buf, 3, &pkt), -1);
}

static void test_parse_ack_truncated(void) {
    uint8_t buf[] = {0x00, 0x04, 0x00};
    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(buf, 3, &pkt), -1);
}

static void test_parse_null(void) {
    struct nfs_tftp_packet pkt;
    ASSERT_EQ(nfs_tftp_parse(NULL, 4, &pkt), -1);
    uint8_t buf[4] = {0};
    ASSERT_EQ(nfs_tftp_parse(buf, 4, NULL), -1);
}

/* ---- Name lookup tests ---- */

static void test_opcode_names(void) {
    ASSERT_TRUE(strcmp(nfs_tftp_opcode_name(NFS_TFTP_OP_RRQ), "RRQ") == 0);
    ASSERT_TRUE(strcmp(nfs_tftp_opcode_name(NFS_TFTP_OP_WRQ), "WRQ") == 0);
    ASSERT_TRUE(strcmp(nfs_tftp_opcode_name(NFS_TFTP_OP_DATA), "DATA") == 0);
    ASSERT_TRUE(strcmp(nfs_tftp_opcode_name(NFS_TFTP_OP_ACK), "ACK") == 0);
    ASSERT_TRUE(strcmp(nfs_tftp_opcode_name(NFS_TFTP_OP_ERROR), "ERROR") == 0);
    ASSERT_TRUE(strcmp(nfs_tftp_opcode_name(99), "UNKNOWN") == 0);
}

static void test_error_names(void) {
    ASSERT_TRUE(strcmp(nfs_tftp_error_name(NFS_TFTP_ERR_NOT_FOUND), "File not found") == 0);
    ASSERT_TRUE(strcmp(nfs_tftp_error_name(NFS_TFTP_ERR_DISK_FULL), "Disk full") == 0);
    ASSERT_TRUE(strcmp(nfs_tftp_error_name(99), "UNKNOWN") == 0);
}

int main(void) {
    printf("TFTP Protocol Tests\n");

    test_build_parse_rrq();
    test_build_parse_wrq();
    test_rrq_network_byte_order();
    test_rrq_invalid_opcode();
    test_rrq_null_filename();
    test_rrq_empty_filename();
    test_build_parse_data();
    test_data_max_block();
    test_data_empty_payload();
    test_data_too_large();
    test_data_network_byte_order();
    test_build_parse_ack();
    test_ack_block_zero();
    test_ack_buffer_too_small();
    test_build_parse_error();
    test_error_empty_msg();
    test_parse_too_short();
    test_parse_unknown_opcode();
    test_parse_data_truncated();
    test_parse_ack_truncated();
    test_parse_null();
    test_opcode_names();
    test_error_names();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
