/* WebSocket frame parser and builder (RFC 6455). */

#include "websocket.h"
#include <string.h>

/* Network byte order helpers for 16/64-bit reads. */
static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] << 8 | (uint16_t)p[1]);
}

static uint64_t read_be64(const uint8_t *p)
{
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) {
        v = (v << 8) | p[i];
    }
    return v;
}

static void write_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

static void write_be64(uint8_t *p, uint64_t v)
{
    for (int i = 7; i >= 0; i--) {
        p[i] = (uint8_t)(v & 0xFF);
        v >>= 8;
    }
}

int nfs_ws_parse_frame(const uint8_t *data, size_t len,
                       struct nfs_ws_frame *frame, size_t *header_len)
{
    if (!data || !frame || !header_len) return -1;
    if (len < 2) return 1; /* need more data */

    memset(frame, 0, sizeof(*frame));

    /* Byte 0: FIN(1) RSV(3) OPCODE(4) */
    frame->fin = (data[0] >> 7) & 1;
    frame->rsv = (data[0] >> 4) & 0x07;
    frame->opcode = data[0] & 0x0F;

    /* Byte 1: MASK(1) PAYLOAD_LEN(7) */
    frame->masked = (data[1] >> 7) & 1;
    uint8_t len7 = data[1] & 0x7F;

    size_t hdr_sz = 2;
    uint64_t payload_len;

    if (len7 <= 125) {
        payload_len = len7;
    } else if (len7 == 126) {
        hdr_sz += 2;
        if (len < hdr_sz) return 1;
        payload_len = read_be16(data + 2);
    } else { /* 127 */
        hdr_sz += 8;
        if (len < hdr_sz) return 1;
        payload_len = read_be64(data + 2);
        /* MSB must be 0 per RFC 6455 */
        if (payload_len >> 63) return -1;
    }

    frame->payload_len = payload_len;

    if (frame->masked) {
        if (len < hdr_sz + 4) return 1;
        memcpy(frame->mask_key, data + hdr_sz, 4);
        hdr_sz += 4;
    }

    *header_len = hdr_sz;
    return 0;
}

int nfs_ws_build_frame(uint8_t opcode, int fin, int masked,
                       const uint8_t *mask_key,
                       const uint8_t *payload, size_t payload_len,
                       uint8_t *out, size_t out_sz)
{
    if (!out) return -1;
    if (masked && !mask_key) return -1;

    /* Calculate header size */
    size_t hdr_sz = 2;
    if (payload_len > 125 && payload_len <= 65535) {
        hdr_sz += 2;
    } else if (payload_len > 65535) {
        hdr_sz += 8;
    }
    if (masked) hdr_sz += 4;

    size_t total = hdr_sz + payload_len;
    if (total > out_sz) return -1;

    size_t pos = 0;

    /* Byte 0: FIN + opcode */
    out[pos++] = (uint8_t)((fin ? 0x80 : 0x00) | (opcode & 0x0F));

    /* Byte 1: MASK + payload length */
    uint8_t mask_bit = masked ? 0x80 : 0x00;
    if (payload_len <= 125) {
        out[pos++] = mask_bit | (uint8_t)payload_len;
    } else if (payload_len <= 65535) {
        out[pos++] = mask_bit | 126;
        write_be16(out + pos, (uint16_t)payload_len);
        pos += 2;
    } else {
        out[pos++] = mask_bit | 127;
        write_be64(out + pos, (uint64_t)payload_len);
        pos += 8;
    }

    /* Mask key */
    if (masked) {
        memcpy(out + pos, mask_key, 4);
        pos += 4;
    }

    /* Payload */
    if (payload && payload_len > 0) {
        memcpy(out + pos, payload, payload_len);
        if (masked) {
            nfs_ws_mask_payload(out + pos, payload_len, mask_key);
        }
    }

    return (int)total;
}

void nfs_ws_mask_payload(uint8_t *data, size_t len,
                         const uint8_t mask_key[4])
{
    if (!data || !mask_key) return;
    for (size_t i = 0; i < len; i++) {
        data[i] ^= mask_key[i % 4];
    }
}

int nfs_ws_is_control(uint8_t opcode)
{
    return (opcode & 0x08) != 0;
}

const char *nfs_ws_opcode_str(uint8_t opcode)
{
    switch (opcode) {
    case NFS_WS_CONTINUATION: return "continuation";
    case NFS_WS_TEXT:         return "text";
    case NFS_WS_BINARY:       return "binary";
    case NFS_WS_CLOSE:        return "close";
    case NFS_WS_PING:         return "ping";
    case NFS_WS_PONG:         return "pong";
    default:                  return "unknown";
    }
}
