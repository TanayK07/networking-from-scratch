#include "h2frame.h"
#include <string.h>
#include <arpa/inet.h>

/* ---------------------------------------------------------------
 * Parse a 9-byte HTTP/2 frame header + validate payload presence.
 * --------------------------------------------------------------- */

int nfs_h2_frame_parse(const uint8_t *buf, size_t len,
                       struct nfs_h2_frame *out)
{
    if (!buf || !out || len < NFS_H2_FRAME_HEADER_SIZE)
        return -1;

    /* Length: 3 bytes big-endian */
    out->length = ((uint32_t)buf[0] << 16) |
                  ((uint32_t)buf[1] << 8)  |
                  ((uint32_t)buf[2]);

    out->type  = buf[3];
    out->flags = buf[4];

    /* Stream ID: 4 bytes big-endian, top bit reserved (must mask) */
    uint32_t raw_id;
    memcpy(&raw_id, &buf[5], 4);
    out->stream_id = ntohl(raw_id) & 0x7FFFFFFFU;

    /* Check that full payload is present in buffer */
    if (len < NFS_H2_FRAME_HEADER_SIZE + out->length)
        return -1;

    out->payload = (out->length > 0) ? &buf[NFS_H2_FRAME_HEADER_SIZE] : NULL;
    return 0;
}

/* ---------------------------------------------------------------
 * Build a frame into buf.
 * --------------------------------------------------------------- */

int nfs_h2_frame_build(uint8_t *buf, size_t out_sz,
                       uint8_t type, uint8_t flags, uint32_t stream_id,
                       const uint8_t *payload, uint32_t payload_len)
{
    if (!buf)
        return -1;

    /* Payload length must fit in 24 bits */
    if (payload_len > 0x00FFFFFF)
        return -1;

    size_t total = NFS_H2_FRAME_HEADER_SIZE + payload_len;
    if (total > out_sz)
        return -1;

    /* Length: 3 bytes big-endian */
    buf[0] = (uint8_t)((payload_len >> 16) & 0xFF);
    buf[1] = (uint8_t)((payload_len >>  8) & 0xFF);
    buf[2] = (uint8_t)((payload_len      ) & 0xFF);

    buf[3] = type;
    buf[4] = flags;

    /* Stream ID: mask reserved bit, big-endian */
    uint32_t net_id = htonl(stream_id & 0x7FFFFFFFU);
    memcpy(&buf[5], &net_id, 4);

    if (payload && payload_len > 0)
        memcpy(&buf[NFS_H2_FRAME_HEADER_SIZE], payload, payload_len);

    return (int)total;
}

/* ---------------------------------------------------------------
 * Frame type name lookup.
 * --------------------------------------------------------------- */

const char *nfs_h2_frame_type_name(uint8_t type)
{
    static const char *names[] = {
        "DATA",          /* 0 */
        "HEADERS",       /* 1 */
        "PRIORITY",      /* 2 */
        "RST_STREAM",    /* 3 */
        "SETTINGS",      /* 4 */
        "PUSH_PROMISE",  /* 5 */
        "PING",          /* 6 */
        "GOAWAY",        /* 7 */
        "WINDOW_UPDATE", /* 8 */
        "CONTINUATION",  /* 9 */
    };

    if (type <= 9)
        return names[type];
    return "UNKNOWN";
}

/* ---------------------------------------------------------------
 * SETTINGS payload: sequence of 6-byte entries (id:16 + value:32).
 * --------------------------------------------------------------- */

int nfs_h2_settings_parse(const uint8_t *payload, size_t len,
                          struct nfs_h2_setting *out, size_t max)
{
    if (!payload && len > 0)
        return -1;
    if (!out && max > 0)
        return -1;

    /* Must be a multiple of 6 */
    if (len % 6 != 0)
        return -1;

    size_t count = len / 6;
    if (count > max)
        return -1;

    for (size_t i = 0; i < count; i++) {
        const uint8_t *p = payload + i * 6;
        uint16_t raw_id;
        uint32_t raw_val;
        memcpy(&raw_id,  p,     2);
        memcpy(&raw_val, p + 2, 4);
        out[i].id    = ntohs(raw_id);
        out[i].value = ntohl(raw_val);
    }

    return (int)count;
}

int nfs_h2_settings_build(uint8_t *buf, size_t out_sz,
                          const struct nfs_h2_setting *settings, size_t count)
{
    if (!buf || (!settings && count > 0))
        return -1;

    size_t needed = count * 6;
    if (needed > out_sz)
        return -1;

    for (size_t i = 0; i < count; i++) {
        uint8_t *p = buf + i * 6;
        uint16_t net_id  = htons(settings[i].id);
        uint32_t net_val = htonl(settings[i].value);
        memcpy(p,     &net_id,  2);
        memcpy(p + 2, &net_val, 4);
    }

    return (int)needed;
}
