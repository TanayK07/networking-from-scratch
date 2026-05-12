/* IPv4 header parser and builder (RFC 791).
 *
 * The struct nfs_ipv4_hdr stores every field in host byte order.
 * nfs_ipv4_parse() converts from the wire, nfs_ipv4_build() converts back.
 * The header checksum uses the shared internet_checksum() from common/c.
 */

#include "ipv4.h"
#include "checksum.h"

#include <stdio.h>
#include <string.h>

/* Minimum IPv4 header is 20 bytes (IHL = 5). */
#define IPV4_MIN_HDR 20

/* ------------------------------------------------------------------ */
/*  Parse                                                              */
/* ------------------------------------------------------------------ */

int nfs_ipv4_parse(const uint8_t *buf, size_t len, struct nfs_ipv4_hdr *out) {
    if (len < IPV4_MIN_HDR)
        return NFS_IPV4_ERR_SHORT;

    memset(out, 0, sizeof(*out));

    /* Byte 0: version (high nibble) | IHL (low nibble). */
    out->version = (buf[0] >> 4) & 0x0F;
    out->ihl = buf[0] & 0x0F;

    if (out->version != 4)
        return NFS_IPV4_ERR_VERSION;
    if (out->ihl < 5)
        return NFS_IPV4_ERR_IHL;

    size_t hdr_len = (size_t)out->ihl * 4;
    if (len < hdr_len)
        return NFS_IPV4_ERR_SHORT;

    /* Byte 1: DSCP (6 bits) | ECN (2 bits). */
    out->dscp = (buf[1] >> 2) & 0x3F;
    out->ecn = buf[1] & 0x03;

    /* Bytes 2-3: total length (network order). */
    out->total_length = (uint16_t)((buf[2] << 8) | buf[3]);

    /* Bytes 4-5: identification. */
    out->identification = (uint16_t)((buf[4] << 8) | buf[5]);

    /* Bytes 6-7: flags (3 bits) | fragment offset (13 bits). */
    uint16_t flags_frag = (uint16_t)((buf[6] << 8) | buf[7]);
    out->flags = (uint8_t)((flags_frag >> 13) & 0x07);
    out->frag_offset = flags_frag & 0x1FFF;

    /* Byte 8: TTL. */
    out->ttl = buf[8];

    /* Byte 9: protocol. */
    out->protocol = buf[9];

    /* Bytes 10-11: header checksum (as it appears on wire). */
    out->checksum = (uint16_t)((buf[10] << 8) | buf[11]);

    /* Bytes 12-15: source address. */
    out->src_addr = ((uint32_t)buf[12] << 24) | ((uint32_t)buf[13] << 16) |
                    ((uint32_t)buf[14] << 8) | ((uint32_t)buf[15]);

    /* Bytes 16-19: destination address. */
    out->dst_addr = ((uint32_t)buf[16] << 24) | ((uint32_t)buf[17] << 16) |
                    ((uint32_t)buf[18] << 8) | ((uint32_t)buf[19]);

    /* Verify checksum: the RFC 1071 checksum over the entire header
     * (with the checksum field included) should equal zero. */
    uint16_t cs = internet_checksum(buf, hdr_len);
    if (cs != 0x0000)
        return NFS_IPV4_ERR_CHECKSUM;

    return NFS_IPV4_OK;
}

/* ------------------------------------------------------------------ */
/*  Build                                                              */
/* ------------------------------------------------------------------ */

size_t nfs_ipv4_build(const struct nfs_ipv4_hdr *hdr, uint8_t *buf, size_t len) {
    size_t hdr_len = (size_t)hdr->ihl * 4;
    if (hdr_len < IPV4_MIN_HDR || len < hdr_len)
        return 0;

    memset(buf, 0, hdr_len);

    /* Byte 0: version | IHL. */
    buf[0] = (uint8_t)((hdr->version << 4) | (hdr->ihl & 0x0F));

    /* Byte 1: DSCP | ECN. */
    buf[1] = (uint8_t)((hdr->dscp << 2) | (hdr->ecn & 0x03));

    /* Bytes 2-3: total length. */
    buf[2] = (uint8_t)(hdr->total_length >> 8);
    buf[3] = (uint8_t)(hdr->total_length & 0xFF);

    /* Bytes 4-5: identification. */
    buf[4] = (uint8_t)(hdr->identification >> 8);
    buf[5] = (uint8_t)(hdr->identification & 0xFF);

    /* Bytes 6-7: flags | fragment offset. */
    uint16_t flags_frag = (uint16_t)(((uint16_t)hdr->flags << 13) | (hdr->frag_offset & 0x1FFF));
    buf[6] = (uint8_t)(flags_frag >> 8);
    buf[7] = (uint8_t)(flags_frag & 0xFF);

    /* Byte 8: TTL. */
    buf[8] = hdr->ttl;

    /* Byte 9: protocol. */
    buf[9] = hdr->protocol;

    /* Bytes 10-11: checksum — zero for computation. */
    buf[10] = 0;
    buf[11] = 0;

    /* Bytes 12-15: source address. */
    buf[12] = (uint8_t)(hdr->src_addr >> 24);
    buf[13] = (uint8_t)(hdr->src_addr >> 16);
    buf[14] = (uint8_t)(hdr->src_addr >> 8);
    buf[15] = (uint8_t)(hdr->src_addr);

    /* Bytes 16-19: destination address. */
    buf[16] = (uint8_t)(hdr->dst_addr >> 24);
    buf[17] = (uint8_t)(hdr->dst_addr >> 16);
    buf[18] = (uint8_t)(hdr->dst_addr >> 8);
    buf[19] = (uint8_t)(hdr->dst_addr);

    /* Compute checksum over the header with checksum field zeroed. */
    uint16_t cs = internet_checksum(buf, hdr_len);
    buf[10] = (uint8_t)(cs >> 8);
    buf[11] = (uint8_t)(cs & 0xFF);

    return hdr_len;
}

/* ------------------------------------------------------------------ */
/*  Checksum helper                                                    */
/* ------------------------------------------------------------------ */

uint16_t nfs_ipv4_checksum(const uint8_t *buf, size_t hdr_len) {
    return internet_checksum(buf, hdr_len);
}

/* ------------------------------------------------------------------ */
/*  Formatting helpers                                                 */
/* ------------------------------------------------------------------ */

char *nfs_ipv4_format_addr(uint32_t addr, char *buf) {
    snprintf(buf, 16, "%u.%u.%u.%u", (addr >> 24) & 0xFF, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF,
             addr & 0xFF);
    return buf;
}

const char *nfs_ipv4_protocol_name(uint8_t proto) {
    switch (proto) {
    case NFS_IPPROTO_ICMP:
        return "ICMP";
    case NFS_IPPROTO_TCP:
        return "TCP";
    case NFS_IPPROTO_UDP:
        return "UDP";
    case NFS_IPPROTO_IPV6:
        return "IPv6-in-IPv4";
    case 2:
        return "IGMP";
    case 47:
        return "GRE";
    case 50:
        return "ESP";
    case 51:
        return "AH";
    case 89:
        return "OSPF";
    default:
        return "unknown";
    }
}
