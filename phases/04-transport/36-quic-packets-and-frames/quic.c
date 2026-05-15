/*
 * quic.c -- QUIC packets and frames implementation
 */
#include "quic.h"
#include <string.h>
#include <arpa/inet.h>

/* ---- Variable-length integer encoding (RFC 9000 Section 16) ---- */

int nfs_quic_varint_size(uint64_t value)
{
    if (value <= 63)                  return 1;
    if (value <= 16383)               return 2;
    if (value <= 1073741823ULL)       return 4;
    if (value <= 4611686018427387903ULL) return 8;
    return -1; /* too large */
}

int nfs_quic_varint_encode(uint64_t value, uint8_t *out, size_t out_sz)
{
    if (!out) return -1;

    int size = nfs_quic_varint_size(value);
    if (size < 0 || (size_t)size > out_sz) return -1;

    switch (size) {
    case 1:
        out[0] = (uint8_t)value; /* 2MSB = 00 */
        break;
    case 2: {
        uint16_t v = (uint16_t)(0x4000 | value);
        out[0] = (uint8_t)(v >> 8);
        out[1] = (uint8_t)(v & 0xFF);
        break;
    }
    case 4: {
        uint32_t v = (uint32_t)(0x80000000UL | value);
        out[0] = (uint8_t)(v >> 24);
        out[1] = (uint8_t)(v >> 16);
        out[2] = (uint8_t)(v >> 8);
        out[3] = (uint8_t)(v & 0xFF);
        break;
    }
    case 8: {
        uint64_t v = 0xC000000000000000ULL | value;
        out[0] = (uint8_t)(v >> 56);
        out[1] = (uint8_t)(v >> 48);
        out[2] = (uint8_t)(v >> 40);
        out[3] = (uint8_t)(v >> 32);
        out[4] = (uint8_t)(v >> 24);
        out[5] = (uint8_t)(v >> 16);
        out[6] = (uint8_t)(v >> 8);
        out[7] = (uint8_t)(v & 0xFF);
        break;
    }
    default:
        return -1;
    }

    return size;
}

int nfs_quic_varint_decode(const uint8_t *data, size_t len, uint64_t *value)
{
    if (!data || !value || len == 0) return -1;

    uint8_t prefix = data[0] >> 6;
    size_t size = (size_t)(1 << prefix); /* 1, 2, 4, or 8 */

    if (len < size) return -1;

    switch (size) {
    case 1:
        *value = data[0] & 0x3F;
        break;
    case 2:
        *value = ((uint64_t)(data[0] & 0x3F) << 8) | data[1];
        break;
    case 4:
        *value = ((uint64_t)(data[0] & 0x3F) << 24) |
                 ((uint64_t)data[1] << 16) |
                 ((uint64_t)data[2] << 8) |
                 data[3];
        break;
    case 8:
        *value = ((uint64_t)(data[0] & 0x3F) << 56) |
                 ((uint64_t)data[1] << 48) |
                 ((uint64_t)data[2] << 40) |
                 ((uint64_t)data[3] << 32) |
                 ((uint64_t)data[4] << 24) |
                 ((uint64_t)data[5] << 16) |
                 ((uint64_t)data[6] << 8) |
                 data[7];
        break;
    default:
        return -1;
    }

    return (int)size;
}

/* ---- Long header ---- */

int nfs_quic_long_hdr_build(const struct nfs_quic_long_hdr *hdr,
                            uint8_t *out, size_t out_sz)
{
    if (!hdr || !out) return -1;
    if (hdr->dcid_len > NFS_QUIC_MAX_CID_LEN ||
        hdr->scid_len > NFS_QUIC_MAX_CID_LEN) return -1;

    size_t total = 1 + 4 + 1 + hdr->dcid_len + 1 + hdr->scid_len;
    if (total > out_sz) return -1;

    size_t pos = 0;

    /* First byte: must have form bit set */
    out[pos++] = hdr->first_byte | NFS_QUIC_HEADER_FORM_BIT;

    /* Version (4 bytes, network byte order) */
    uint32_t ver = htonl(hdr->version);
    memcpy(out + pos, &ver, 4);
    pos += 4;

    /* DCID length + DCID */
    out[pos++] = hdr->dcid_len;
    if (hdr->dcid_len > 0) {
        memcpy(out + pos, hdr->dcid, hdr->dcid_len);
        pos += hdr->dcid_len;
    }

    /* SCID length + SCID */
    out[pos++] = hdr->scid_len;
    if (hdr->scid_len > 0) {
        memcpy(out + pos, hdr->scid, hdr->scid_len);
        pos += hdr->scid_len;
    }

    return (int)pos;
}

int nfs_quic_long_hdr_parse(const uint8_t *data, size_t len,
                            struct nfs_quic_long_hdr *hdr)
{
    if (!data || !hdr) return -1;
    if (len < 7) return -1; /* minimum: first + version(4) + dcid_len(1) + scid_len(1) */

    /* Check header form bit */
    if (!(data[0] & NFS_QUIC_HEADER_FORM_BIT)) return -1;

    memset(hdr, 0, sizeof(*hdr));
    size_t pos = 0;

    hdr->first_byte = data[pos++];

    /* Version */
    uint32_t ver;
    memcpy(&ver, data + pos, 4);
    hdr->version = ntohl(ver);
    pos += 4;

    /* DCID */
    hdr->dcid_len = data[pos++];
    if (hdr->dcid_len > NFS_QUIC_MAX_CID_LEN) return -1;
    if (pos + hdr->dcid_len >= len) return -1;
    if (hdr->dcid_len > 0) {
        memcpy(hdr->dcid, data + pos, hdr->dcid_len);
        pos += hdr->dcid_len;
    }

    /* SCID */
    if (pos >= len) return -1;
    hdr->scid_len = data[pos++];
    if (hdr->scid_len > NFS_QUIC_MAX_CID_LEN) return -1;
    if (pos + hdr->scid_len > len) return -1;
    if (hdr->scid_len > 0) {
        memcpy(hdr->scid, data + pos, hdr->scid_len);
        pos += hdr->scid_len;
    }

    return (int)pos;
}

uint8_t nfs_quic_long_hdr_type(uint8_t first_byte)
{
    return (first_byte >> 4) & 0x03;
}

/* ---- Short header ---- */

int nfs_quic_short_hdr_build(const struct nfs_quic_short_hdr *hdr,
                             uint8_t *out, size_t out_sz)
{
    if (!hdr || !out) return -1;
    if (hdr->dcid_len > NFS_QUIC_MAX_CID_LEN) return -1;

    size_t total = 1 + hdr->dcid_len;
    if (total > out_sz) return -1;

    size_t pos = 0;

    /* First byte: form bit = 0, fixed bit = 1 */
    out[pos++] = (hdr->first_byte & ~NFS_QUIC_HEADER_FORM_BIT) | NFS_QUIC_FIXED_BIT;

    /* DCID */
    if (hdr->dcid_len > 0) {
        memcpy(out + pos, hdr->dcid, hdr->dcid_len);
        pos += hdr->dcid_len;
    }

    return (int)pos;
}

int nfs_quic_short_hdr_parse(const uint8_t *data, size_t len,
                             uint8_t dcid_len,
                             struct nfs_quic_short_hdr *hdr)
{
    if (!data || !hdr) return -1;
    if (dcid_len > NFS_QUIC_MAX_CID_LEN) return -1;
    if (len < 1 + (size_t)dcid_len) return -1;

    /* Check it's a short header (form bit = 0) */
    if (data[0] & NFS_QUIC_HEADER_FORM_BIT) return -1;

    memset(hdr, 0, sizeof(*hdr));
    size_t pos = 0;

    hdr->first_byte = data[pos++];
    hdr->dcid_len = dcid_len;

    if (dcid_len > 0) {
        memcpy(hdr->dcid, data + pos, dcid_len);
        pos += dcid_len;
    }

    return (int)pos;
}

int nfs_quic_short_hdr_spin(uint8_t first_byte)
{
    return (first_byte & NFS_QUIC_SHORT_SPIN_BIT) ? 1 : 0;
}

int nfs_quic_short_hdr_key_phase(uint8_t first_byte)
{
    return (first_byte & NFS_QUIC_SHORT_KEY_PHASE_BIT) ? 1 : 0;
}

/* ---- Frame identification ---- */

int nfs_quic_frame_type(const uint8_t *data, size_t len)
{
    if (!data || len == 0) return -1;
    return (int)data[0];
}

int nfs_quic_is_stream_frame(uint8_t frame_type)
{
    return (frame_type >= 0x08 && frame_type <= 0x0F) ? 1 : 0;
}

int nfs_quic_stream_has_fin(uint8_t frame_type)
{
    return (frame_type & 0x01) ? 1 : 0;
}

int nfs_quic_stream_has_len(uint8_t frame_type)
{
    return (frame_type & 0x02) ? 1 : 0;
}

int nfs_quic_stream_has_off(uint8_t frame_type)
{
    return (frame_type & 0x04) ? 1 : 0;
}

const char *nfs_quic_frame_name(uint8_t frame_type)
{
    switch (frame_type) {
    case NFS_QUIC_FRAME_PADDING:  return "PADDING";
    case NFS_QUIC_FRAME_PING:     return "PING";
    case NFS_QUIC_FRAME_ACK:      return "ACK";
    case NFS_QUIC_FRAME_ACK_ECN:  return "ACK_ECN";
    default:
        if (nfs_quic_is_stream_frame(frame_type))
            return "STREAM";
        return "UNKNOWN";
    }
}

const char *nfs_quic_long_type_name(uint8_t ptype)
{
    switch (ptype) {
    case NFS_QUIC_LONG_TYPE_INITIAL:   return "Initial";
    case NFS_QUIC_LONG_TYPE_0RTT:      return "0-RTT";
    case NFS_QUIC_LONG_TYPE_HANDSHAKE: return "Handshake";
    case NFS_QUIC_LONG_TYPE_RETRY:     return "Retry";
    default: return "Unknown";
    }
}
