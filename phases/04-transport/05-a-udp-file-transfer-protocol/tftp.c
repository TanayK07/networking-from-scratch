/*
 * tftp.c -- TFTP-like protocol (based on RFC 1350)
 *
 * Build and parse TFTP packets: RRQ, WRQ, DATA, ACK, ERROR.
 * All multi-byte fields use network byte order.
 */

#include "tftp.h"
#include <string.h>
#include <arpa/inet.h>

/* ---- Builders ------------------------------------------------- */

int nfs_tftp_build_rrq(uint8_t *buf, size_t buf_max,
                       uint16_t opcode,
                       const char *filename, const char *mode)
{
    if (!buf || !filename || !mode) return -1;
    if (opcode != NFS_TFTP_OP_RRQ && opcode != NFS_TFTP_OP_WRQ) return -1;

    size_t fn_len = strlen(filename);
    size_t mode_len = strlen(mode);
    if (fn_len == 0 || fn_len >= NFS_TFTP_FILENAME_MAX) return -1;
    if (mode_len == 0 || mode_len >= NFS_TFTP_MODE_MAX) return -1;

    /* opcode(2) + filename + NUL + mode + NUL */
    size_t total = 2 + fn_len + 1 + mode_len + 1;
    if (total > buf_max) return -1;

    uint16_t op_net = htons(opcode);
    memcpy(buf, &op_net, 2);
    memcpy(buf + 2, filename, fn_len);
    buf[2 + fn_len] = 0;
    memcpy(buf + 2 + fn_len + 1, mode, mode_len);
    buf[2 + fn_len + 1 + mode_len] = 0;

    return (int)total;
}

int nfs_tftp_build_data(uint8_t *buf, size_t buf_max,
                        uint16_t block,
                        const uint8_t *data, uint16_t data_len)
{
    if (!buf) return -1;
    if (data_len > NFS_TFTP_DATA_MAX) return -1;
    if (data_len > 0 && !data) return -1;

    size_t total = 2 + 2 + data_len;
    if (total > buf_max) return -1;

    uint16_t op_net = htons(NFS_TFTP_OP_DATA);
    uint16_t blk_net = htons(block);
    memcpy(buf, &op_net, 2);
    memcpy(buf + 2, &blk_net, 2);
    if (data_len > 0) {
        memcpy(buf + 4, data, data_len);
    }

    return (int)total;
}

int nfs_tftp_build_ack(uint8_t *buf, size_t buf_max, uint16_t block)
{
    if (!buf) return -1;
    if (buf_max < 4) return -1;

    uint16_t op_net = htons(NFS_TFTP_OP_ACK);
    uint16_t blk_net = htons(block);
    memcpy(buf, &op_net, 2);
    memcpy(buf + 2, &blk_net, 2);

    return 4;
}

int nfs_tftp_build_error(uint8_t *buf, size_t buf_max,
                         uint16_t code, const char *msg)
{
    if (!buf || !msg) return -1;

    size_t msg_len = strlen(msg);
    if (msg_len >= NFS_TFTP_ERRMSG_MAX) return -1;

    /* opcode(2) + errcode(2) + errmsg + NUL */
    size_t total = 2 + 2 + msg_len + 1;
    if (total > buf_max) return -1;

    uint16_t op_net = htons(NFS_TFTP_OP_ERROR);
    uint16_t code_net = htons(code);
    memcpy(buf, &op_net, 2);
    memcpy(buf + 2, &code_net, 2);
    memcpy(buf + 4, msg, msg_len);
    buf[4 + msg_len] = 0;

    return (int)total;
}

/* ---- Parser --------------------------------------------------- */

/* Find a NUL terminator within buf[start..len-1]. Returns offset of NUL or -1. */
static int find_nul(const uint8_t *buf, size_t start, size_t len)
{
    for (size_t i = start; i < len; i++) {
        if (buf[i] == 0) return (int)i;
    }
    return -1;
}

int nfs_tftp_parse(const uint8_t *buf, size_t len,
                   struct nfs_tftp_packet *out)
{
    if (!buf || !out || len < 2) return -1;

    memset(out, 0, sizeof(*out));

    uint16_t op_net;
    memcpy(&op_net, buf, 2);
    out->opcode = ntohs(op_net);

    switch (out->opcode) {
    case NFS_TFTP_OP_RRQ:
    case NFS_TFTP_OP_WRQ: {
        /* filename\0mode\0 */
        int fn_end = find_nul(buf, 2, len);
        if (fn_end < 0) return -1;

        size_t fn_len = (size_t)(fn_end - 2);
        if (fn_len == 0 || fn_len >= NFS_TFTP_FILENAME_MAX) return -1;

        int mode_end = find_nul(buf, (size_t)(fn_end + 1), len);
        if (mode_end < 0) return -1;

        size_t mode_len = (size_t)(mode_end - fn_end - 1);
        if (mode_len == 0 || mode_len >= NFS_TFTP_MODE_MAX) return -1;

        memcpy(out->u.rq.filename, buf + 2, fn_len);
        out->u.rq.filename[fn_len] = '\0';
        memcpy(out->u.rq.mode, buf + fn_end + 1, mode_len);
        out->u.rq.mode[mode_len] = '\0';
        return 0;
    }
    case NFS_TFTP_OP_DATA: {
        if (len < 4) return -1;
        uint16_t blk_net;
        memcpy(&blk_net, buf + 2, 2);
        out->u.data.block = ntohs(blk_net);

        size_t dlen = len - 4;
        if (dlen > NFS_TFTP_DATA_MAX) return -1;

        out->u.data.data_len = (uint16_t)dlen;
        if (dlen > 0) {
            memcpy(out->u.data.data, buf + 4, dlen);
        }
        return 0;
    }
    case NFS_TFTP_OP_ACK: {
        if (len < 4) return -1;
        uint16_t blk_net;
        memcpy(&blk_net, buf + 2, 2);
        out->u.ack.block = ntohs(blk_net);
        return 0;
    }
    case NFS_TFTP_OP_ERROR: {
        if (len < 5) return -1;  /* at least opcode + errcode + NUL */
        uint16_t code_net;
        memcpy(&code_net, buf + 2, 2);
        out->u.error.code = ntohs(code_net);

        int msg_end = find_nul(buf, 4, len);
        if (msg_end < 0) return -1;

        size_t msg_len = (size_t)(msg_end - 4);
        if (msg_len >= NFS_TFTP_ERRMSG_MAX) return -1;

        memcpy(out->u.error.msg, buf + 4, msg_len);
        out->u.error.msg[msg_len] = '\0';
        return 0;
    }
    default:
        return -1;
    }
}

/* ---- Name lookups --------------------------------------------- */

const char *nfs_tftp_opcode_name(uint16_t opcode)
{
    switch (opcode) {
    case NFS_TFTP_OP_RRQ:   return "RRQ";
    case NFS_TFTP_OP_WRQ:   return "WRQ";
    case NFS_TFTP_OP_DATA:  return "DATA";
    case NFS_TFTP_OP_ACK:   return "ACK";
    case NFS_TFTP_OP_ERROR: return "ERROR";
    default:                return "UNKNOWN";
    }
}

const char *nfs_tftp_error_name(uint16_t code)
{
    switch (code) {
    case NFS_TFTP_ERR_UNDEFINED:    return "Not defined";
    case NFS_TFTP_ERR_NOT_FOUND:    return "File not found";
    case NFS_TFTP_ERR_ACCESS:       return "Access violation";
    case NFS_TFTP_ERR_DISK_FULL:    return "Disk full";
    case NFS_TFTP_ERR_ILLEGAL_OP:   return "Illegal operation";
    case NFS_TFTP_ERR_UNKNOWN_TID:  return "Unknown transfer ID";
    case NFS_TFTP_ERR_EXISTS:       return "File already exists";
    case NFS_TFTP_ERR_NO_USER:      return "No such user";
    default:                        return "UNKNOWN";
    }
}
