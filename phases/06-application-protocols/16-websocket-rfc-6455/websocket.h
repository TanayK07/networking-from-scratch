#ifndef NFS_WEBSOCKET_H
#define NFS_WEBSOCKET_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * WebSocket frame parser and builder (RFC 6455).
 *
 * Handles frame header parsing (including 7-bit, 16-bit, and
 * 64-bit payload length encoding) and masking/unmasking.
 * --------------------------------------------------------------- */

/* Opcodes */
#define NFS_WS_CONTINUATION  0x0
#define NFS_WS_TEXT          0x1
#define NFS_WS_BINARY        0x2
#define NFS_WS_CLOSE         0x8
#define NFS_WS_PING          0x9
#define NFS_WS_PONG          0xA

/* Parsed WebSocket frame header. */
struct nfs_ws_frame {
    int      fin;           /* FIN bit */
    int      rsv;           /* RSV1-3 (3 bits) */
    uint8_t  opcode;        /* 4-bit opcode */
    int      masked;        /* MASK bit */
    uint64_t payload_len;   /* payload length */
    uint8_t  mask_key[4];   /* masking key (if masked) */
};

/* Parse a WebSocket frame header from `data` (len bytes available).
 * Populates `frame` and sets `*header_len` to the total header size.
 * Returns:
 *   0  on success
 *  -1  on error (invalid frame)
 *   1  need more data */
int nfs_ws_parse_frame(const uint8_t *data, size_t len,
                       struct nfs_ws_frame *frame, size_t *header_len);

/* Build a WebSocket frame.  If `masked` is set, `mask_key` must be
 * non-NULL and the payload will be XOR-masked in the output.
 * Returns the total frame length written to `out`, or -1 on error. */
int nfs_ws_build_frame(uint8_t opcode, int fin, int masked,
                       const uint8_t *mask_key,
                       const uint8_t *payload, size_t payload_len,
                       uint8_t *out, size_t out_sz);

/* XOR mask (or unmask) payload data in place. */
void nfs_ws_mask_payload(uint8_t *data, size_t len,
                         const uint8_t mask_key[4]);

/* Returns non-zero if opcode is a control frame (0x8-0xF). */
int nfs_ws_is_control(uint8_t opcode);

/* Returns a string description of the opcode. */
const char *nfs_ws_opcode_str(uint8_t opcode);

#endif /* NFS_WEBSOCKET_H */
