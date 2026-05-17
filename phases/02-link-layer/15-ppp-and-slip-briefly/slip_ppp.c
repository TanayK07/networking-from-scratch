/*
 * slip_ppp.c -- SLIP and PPP framing implementation
 */
#include "slip_ppp.h"
#include <arpa/inet.h>
#include <string.h>

/* ---------------------------------------------------------------
 * SLIP encoding / decoding (RFC 1055)
 * --------------------------------------------------------------- */

int nfs_slip_encode(const uint8_t *data, size_t len, uint8_t *out, size_t out_sz) {
    if (!out || (!data && len > 0))
        return -1;

    size_t pos = 0;

    /* Leading END byte to flush any line noise */
    if (pos >= out_sz)
        return -1;
    out[pos++] = NFS_SLIP_END;

    for (size_t i = 0; i < len; i++) {
        switch (data[i]) {
        case NFS_SLIP_END:
            if (pos + 2 > out_sz)
                return -1;
            out[pos++] = NFS_SLIP_ESC;
            out[pos++] = NFS_SLIP_ESC_END;
            break;
        case NFS_SLIP_ESC:
            if (pos + 2 > out_sz)
                return -1;
            out[pos++] = NFS_SLIP_ESC;
            out[pos++] = NFS_SLIP_ESC_ESC;
            break;
        default:
            if (pos >= out_sz)
                return -1;
            out[pos++] = data[i];
            break;
        }
    }

    /* Trailing END */
    if (pos >= out_sz)
        return -1;
    out[pos++] = NFS_SLIP_END;

    return (int)pos;
}

int nfs_slip_decode(const uint8_t *frame, size_t frame_len, uint8_t *out, size_t out_sz) {
    if (!frame || !out || frame_len < 2)
        return -1;

    /* Frame must start and end with END */
    if (frame[0] != NFS_SLIP_END || frame[frame_len - 1] != NFS_SLIP_END)
        return -1;

    size_t pos = 0;
    size_t i = 1; /* skip leading END */

    while (i < frame_len - 1) { /* stop before trailing END */
        if (frame[i] == NFS_SLIP_ESC) {
            i++;
            if (i >= frame_len - 1)
                return -1; /* truncated escape */
            switch (frame[i]) {
            case NFS_SLIP_ESC_END:
                if (pos >= out_sz)
                    return -1;
                out[pos++] = NFS_SLIP_END;
                break;
            case NFS_SLIP_ESC_ESC:
                if (pos >= out_sz)
                    return -1;
                out[pos++] = NFS_SLIP_ESC;
                break;
            default:
                return -1; /* invalid escape sequence */
            }
        } else if (frame[i] == NFS_SLIP_END) {
            /* Unexpected END in the middle -- treat as end of frame */
            break;
        } else {
            if (pos >= out_sz)
                return -1;
            out[pos++] = frame[i];
        }
        i++;
    }

    return (int)pos;
}

/* ---------------------------------------------------------------
 * PPP CRC-16/HDLC (FCS-16) - ITU-T V.41 / RFC 1662
 *
 * Polynomial: x^16 + x^12 + x^5 + 1 = 0x8408 (reflected)
 * Init: 0xFFFF, final XOR: 0xFFFF
 * --------------------------------------------------------------- */

static const uint16_t fcs16_table[256] = {
    0x0000, 0x1189, 0x2312, 0x329B, 0x4624, 0x57AD, 0x6536, 0x74BF, 0x8C48, 0x9DC1, 0xAF5A, 0xBED3,
    0xCA6C, 0xDBE5, 0xE97E, 0xF8F7, 0x1081, 0x0108, 0x3393, 0x221A, 0x56A5, 0x472C, 0x75B7, 0x643E,
    0x9CC9, 0x8D40, 0xBFDB, 0xAE52, 0xDAED, 0xCB64, 0xF9FF, 0xE876, 0x2102, 0x308B, 0x0210, 0x1399,
    0x6726, 0x76AF, 0x4434, 0x55BD, 0xAD4A, 0xBCC3, 0x8E58, 0x9FD1, 0xEB6E, 0xFAE7, 0xC87C, 0xD9F5,
    0x3183, 0x200A, 0x1291, 0x0318, 0x77A7, 0x662E, 0x54B5, 0x453C, 0xBDCB, 0xAC42, 0x9ED9, 0x8F50,
    0xFBEF, 0xEA66, 0xD8FD, 0xC974, 0x4204, 0x538D, 0x6116, 0x709F, 0x0420, 0x15A9, 0x2732, 0x36BB,
    0xCE4C, 0xDFC5, 0xED5E, 0xFCD7, 0x8868, 0x99E1, 0xAB7A, 0xBAF3, 0x5285, 0x430C, 0x7197, 0x601E,
    0x14A1, 0x0528, 0x37B3, 0x263A, 0xDECD, 0xCF44, 0xFDDF, 0xEC56, 0x98E9, 0x8960, 0xBBFB, 0xAA72,
    0x6306, 0x728F, 0x4014, 0x519D, 0x2522, 0x34AB, 0x0630, 0x17B9, 0xEF4E, 0xFEC7, 0xCC5C, 0xDDD5,
    0xA96A, 0xB8E3, 0x8A78, 0x9BF1, 0x7387, 0x620E, 0x5095, 0x411C, 0x35A3, 0x242A, 0x16B1, 0x0738,
    0xFFCF, 0xEE46, 0xDCDD, 0xCD54, 0xB9EB, 0xA862, 0x9AF9, 0x8B70, 0x8408, 0x9581, 0xA71A, 0xB693,
    0xC22C, 0xD3A5, 0xE13E, 0xF0B7, 0x0840, 0x19C9, 0x2B52, 0x3ADB, 0x4E64, 0x5FED, 0x6D76, 0x7CFF,
    0x9489, 0x8500, 0xB79B, 0xA612, 0xD2AD, 0xC324, 0xF1BF, 0xE036, 0x18C1, 0x0948, 0x3BD3, 0x2A5A,
    0x5EE5, 0x4F6C, 0x7DF7, 0x6C7E, 0xA50A, 0xB483, 0x8618, 0x9791, 0xE32E, 0xF2A7, 0xC03C, 0xD1B5,
    0x2942, 0x38CB, 0x0A50, 0x1BD9, 0x6F66, 0x7EEF, 0x4C74, 0x5DFD, 0xB58B, 0xA402, 0x9699, 0x8710,
    0xF3AF, 0xE226, 0xD0BD, 0xC134, 0x39C3, 0x284A, 0x1AD1, 0x0B58, 0x7FE7, 0x6E6E, 0x5CF5, 0x4D7C,
    0xC60C, 0xD785, 0xE51E, 0xF497, 0x8028, 0x91A1, 0xA33A, 0xB2B3, 0x4A44, 0x5BCD, 0x6956, 0x78DF,
    0x0C60, 0x1DE9, 0x2F72, 0x3EFB, 0xD68D, 0xC704, 0xF59F, 0xE416, 0x90A9, 0x8120, 0xB3BB, 0xA232,
    0x5AC5, 0x4B4C, 0x79D7, 0x685E, 0x1CE1, 0x0D68, 0x3FF3, 0x2E7A, 0xE70E, 0xF687, 0xC41C, 0xD595,
    0xA12A, 0xB0A3, 0x8238, 0x93B1, 0x6B46, 0x7ACF, 0x4854, 0x59DD, 0x2D62, 0x3CEB, 0x0E70, 0x1FF9,
    0xF78F, 0xE606, 0xD49D, 0xC514, 0xB1AB, 0xA022, 0x92B9, 0x8330, 0x7BC7, 0x6A4E, 0x58D5, 0x495C,
    0x3DE3, 0x2C6A, 0x1EF1, 0x0F78,
};

uint16_t nfs_ppp_fcs16(const uint8_t *data, size_t len) {
    uint16_t fcs = 0xFFFF;
    for (size_t i = 0; i < len; i++)
        fcs = (fcs >> 8) ^ fcs16_table[(fcs ^ data[i]) & 0xFF];
    return fcs ^ 0xFFFF;
}

/* ---------------------------------------------------------------
 * PPP frame build / parse
 * --------------------------------------------------------------- */

int nfs_ppp_frame_build(uint16_t protocol, const uint8_t *payload, size_t payload_len, uint8_t *out,
                        size_t out_sz) {
    if (!out || (!payload && payload_len > 0))
        return -1;
    if (payload_len > NFS_PPP_MAX_PAYLOAD)
        return -1;

    /* Total: flag(1) + addr(1) + ctrl(1) + proto(2) + payload + fcs(2) + flag(1) */
    size_t total = 1 + 1 + 1 + 2 + payload_len + 2 + 1;
    if (total > out_sz)
        return -1;

    size_t pos = 0;

    /* Opening flag */
    out[pos++] = NFS_PPP_FLAG;

    /* Address and control */
    out[pos++] = NFS_PPP_ADDR;
    out[pos++] = NFS_PPP_CTRL;

    /* Protocol in network byte order */
    out[pos++] = (uint8_t)(protocol >> 8);
    out[pos++] = (uint8_t)(protocol & 0xFF);

    /* Payload */
    if (payload && payload_len > 0)
        memcpy(out + pos, payload, payload_len);
    pos += payload_len;

    /* FCS over addr + ctrl + protocol + payload */
    uint16_t fcs = nfs_ppp_fcs16(out + 1, pos - 1);
    /* FCS is transmitted LSB first (little-endian on wire) */
    out[pos++] = (uint8_t)(fcs & 0xFF);
    out[pos++] = (uint8_t)(fcs >> 8);

    /* Closing flag */
    out[pos++] = NFS_PPP_FLAG;

    return (int)pos;
}

int nfs_ppp_frame_parse(const uint8_t *data, size_t len, struct nfs_ppp_frame *frame) {
    if (!data || !frame)
        return -1;

    /* Minimum frame: flag + addr + ctrl + proto(2) + fcs(2) + flag = 8 */
    if (len < 8)
        return -1;

    /* Check flags */
    if (data[0] != NFS_PPP_FLAG || data[len - 1] != NFS_PPP_FLAG)
        return -1;

    /* Check address and control */
    if (data[1] != NFS_PPP_ADDR || data[2] != NFS_PPP_CTRL)
        return -1;

    frame->address = data[1];
    frame->control = data[2];

    /* Protocol (network byte order) */
    frame->protocol = (uint16_t)((data[3] << 8) | data[4]);

    /* FCS is last 2 bytes before closing flag (little-endian) */
    size_t fcs_offset = len - 3; /* -1 for closing flag, -2 for FCS */
    frame->fcs = (uint16_t)(data[fcs_offset] | (data[fcs_offset + 1] << 8));

    /* Payload is between protocol and FCS */
    size_t payload_start = 5; /* flag + addr + ctrl + proto(2) */
    size_t payload_len = fcs_offset - payload_start;

    if (payload_len > NFS_PPP_MAX_PAYLOAD)
        return -1;

    memcpy(frame->payload, data + payload_start, payload_len);
    frame->payload_len = (uint16_t)payload_len;

    /* Verify FCS: compute over addr+ctrl+proto+payload */
    uint16_t computed = nfs_ppp_fcs16(data + 1, fcs_offset - 1);
    if (computed != frame->fcs)
        return -1;

    return 0;
}

const char *nfs_ppp_protocol_name(uint16_t protocol) {
    switch (protocol) {
    case NFS_PPP_PROTO_IPV4:
        return "IPv4";
    case NFS_PPP_PROTO_IPV6:
        return "IPv6";
    case NFS_PPP_PROTO_LCP:
        return "LCP";
    case NFS_PPP_PROTO_IPCP:
        return "IPCP";
    default:
        return "Unknown";
    }
}
