/*
 * main.c -- Demo driver for TFTP-like protocol
 */

#include "tftp.h"
#include <stdio.h>
#include <string.h>

static void hexdump(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
}

int main(void) {
    uint8_t buf[600];
    int n;

    /* Build and parse RRQ */
    n = nfs_tftp_build_rrq(buf, sizeof(buf), NFS_TFTP_OP_RRQ, "test.txt", "octet");
    printf("RRQ (%d bytes): ", n);
    hexdump(buf, (size_t)n);

    struct nfs_tftp_packet pkt;
    if (nfs_tftp_parse(buf, (size_t)n, &pkt) == 0) {
        printf("  opcode=%s file='%s' mode='%s'\n", nfs_tftp_opcode_name(pkt.opcode),
               pkt.u.rq.filename, pkt.u.rq.mode);
    }

    /* Build and parse DATA */
    const uint8_t data[] = "Hello from TFTP!";
    n = nfs_tftp_build_data(buf, sizeof(buf), 1, data, 16);
    printf("\nDATA (%d bytes): ", n);
    hexdump(buf, (size_t)n);

    if (nfs_tftp_parse(buf, (size_t)n, &pkt) == 0) {
        printf("  opcode=%s block=%u data_len=%u\n", nfs_tftp_opcode_name(pkt.opcode),
               pkt.u.data.block, pkt.u.data.data_len);
    }

    /* Build and parse ACK */
    n = nfs_tftp_build_ack(buf, sizeof(buf), 1);
    printf("\nACK (%d bytes): ", n);
    hexdump(buf, (size_t)n);

    if (nfs_tftp_parse(buf, (size_t)n, &pkt) == 0) {
        printf("  opcode=%s block=%u\n", nfs_tftp_opcode_name(pkt.opcode), pkt.u.ack.block);
    }

    /* Build and parse ERROR */
    n = nfs_tftp_build_error(buf, sizeof(buf), NFS_TFTP_ERR_NOT_FOUND, "File not found");
    printf("\nERROR (%d bytes): ", n);
    hexdump(buf, (size_t)n);

    if (nfs_tftp_parse(buf, (size_t)n, &pkt) == 0) {
        printf("  opcode=%s errcode=%u (%s) msg='%s'\n", nfs_tftp_opcode_name(pkt.opcode),
               pkt.u.error.code, nfs_tftp_error_name(pkt.u.error.code), pkt.u.error.msg);
    }

    return 0;
}
