#ifndef NFS_TFTP_H
#define NFS_TFTP_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * TFTP-like protocol (based on RFC 1350)
 *
 * Packet formats:
 *   RRQ/WRQ:  opcode(2) + filename(NUL) + mode(NUL)
 *   DATA:     opcode(2) + block#(2) + data(0..512)
 *   ACK:      opcode(2) + block#(2)
 *   ERROR:    opcode(2) + errcode(2) + errmsg(NUL)
 *
 * All multi-byte fields are in network byte order (big-endian).
 * --------------------------------------------------------------- */

/* Opcodes */
#define NFS_TFTP_OP_RRQ    1
#define NFS_TFTP_OP_WRQ    2
#define NFS_TFTP_OP_DATA   3
#define NFS_TFTP_OP_ACK    4
#define NFS_TFTP_OP_ERROR  5

/* Error codes */
#define NFS_TFTP_ERR_UNDEFINED    0
#define NFS_TFTP_ERR_NOT_FOUND    1
#define NFS_TFTP_ERR_ACCESS       2
#define NFS_TFTP_ERR_DISK_FULL    3
#define NFS_TFTP_ERR_ILLEGAL_OP   4
#define NFS_TFTP_ERR_UNKNOWN_TID  5
#define NFS_TFTP_ERR_EXISTS       6
#define NFS_TFTP_ERR_NO_USER      7

#define NFS_TFTP_DATA_MAX     512   /* max data bytes per DATA packet */
#define NFS_TFTP_FILENAME_MAX 256
#define NFS_TFTP_MODE_MAX     16
#define NFS_TFTP_ERRMSG_MAX   128

/* Parsed TFTP packet. */
struct nfs_tftp_packet {
    uint16_t opcode;
    union {
        struct {
            char     filename[NFS_TFTP_FILENAME_MAX];
            char     mode[NFS_TFTP_MODE_MAX];
        } rq;       /* for RRQ / WRQ */
        struct {
            uint16_t block;
            uint8_t  data[NFS_TFTP_DATA_MAX];
            uint16_t data_len;
        } data;     /* for DATA */
        struct {
            uint16_t block;
        } ack;      /* for ACK */
        struct {
            uint16_t code;
            char     msg[NFS_TFTP_ERRMSG_MAX];
        } error;    /* for ERROR */
    } u;
};

/* Build a RRQ or WRQ packet.
 * opcode: NFS_TFTP_OP_RRQ or NFS_TFTP_OP_WRQ
 * Returns bytes written to buf, or -1 on error. */
int nfs_tftp_build_rrq(uint8_t *buf, size_t buf_max,
                       uint16_t opcode,
                       const char *filename, const char *mode);

/* Build a DATA packet.
 * Returns bytes written to buf, or -1 on error. */
int nfs_tftp_build_data(uint8_t *buf, size_t buf_max,
                        uint16_t block,
                        const uint8_t *data, uint16_t data_len);

/* Build an ACK packet.
 * Returns bytes written to buf, or -1 on error. */
int nfs_tftp_build_ack(uint8_t *buf, size_t buf_max, uint16_t block);

/* Build an ERROR packet.
 * Returns bytes written to buf, or -1 on error. */
int nfs_tftp_build_error(uint8_t *buf, size_t buf_max,
                         uint16_t code, const char *msg);

/* Parse a raw TFTP packet into a structured form.
 * Returns 0 on success, -1 on error. */
int nfs_tftp_parse(const uint8_t *buf, size_t len,
                   struct nfs_tftp_packet *out);

/* Return a human-readable name for an opcode, or "UNKNOWN". */
const char *nfs_tftp_opcode_name(uint16_t opcode);

/* Return a human-readable name for an error code, or "UNKNOWN". */
const char *nfs_tftp_error_name(uint16_t code);

#endif /* NFS_TFTP_H */
