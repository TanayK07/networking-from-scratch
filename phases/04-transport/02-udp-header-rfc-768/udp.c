/* UDP header parser, builder, and checksum (RFC 768).
 *
 * The struct nfs_udp_hdr stores every field in host byte order after
 * parsing.  nfs_udp_build() converts back to network order for the wire.
 *
 * The checksum uses the shared internet_checksum() from common/c and
 * prepends a 12-byte IPv4 pseudo-header per RFC 768.
 */

#include "udp.h"
#include "checksum.h"

#include <stdio.h>
#include <string.h>

/* IPv4 protocol number for UDP. */
#define IPPROTO_UDP_NUM 17

/* ------------------------------------------------------------------ */
/*  Parse                                                              */
/* ------------------------------------------------------------------ */

int nfs_udp_parse(const uint8_t *buf, size_t len, struct nfs_udp_hdr *out) {
    if (!buf || !out || len < NFS_UDP_HDR_LEN)
        return -1;

    /* Copy the raw bytes, then convert multi-byte fields to host
     * order so callers don't have to sprinkle ntohs(). */
    memcpy(out, buf, sizeof(*out));

    /* Convert from network (big-endian) to host order. */
    out->src_port = (uint16_t)((buf[0] << 8) | buf[1]);
    out->dst_port = (uint16_t)((buf[2] << 8) | buf[3]);
    out->length = (uint16_t)((buf[4] << 8) | buf[5]);
    out->checksum = (uint16_t)((buf[6] << 8) | buf[7]);

    /* RFC 768: Length must be >= 8. */
    if (out->length < NFS_UDP_HDR_LEN)
        return -1;

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Build                                                              */
/* ------------------------------------------------------------------ */

size_t nfs_udp_build(const struct nfs_udp_hdr *hdr, const uint8_t *payload, size_t payload_len,
                     uint8_t *buf, size_t buf_len) {
    if (!hdr || !buf)
        return 0;

    size_t total = NFS_UDP_HDR_LEN + payload_len;
    if (buf_len < total)
        return 0;

    /* Write header fields in network byte order (big-endian). */
    buf[0] = (uint8_t)(hdr->src_port >> 8);
    buf[1] = (uint8_t)(hdr->src_port & 0xFF);
    buf[2] = (uint8_t)(hdr->dst_port >> 8);
    buf[3] = (uint8_t)(hdr->dst_port & 0xFF);
    buf[4] = (uint8_t)(hdr->length >> 8);
    buf[5] = (uint8_t)(hdr->length & 0xFF);
    buf[6] = (uint8_t)(hdr->checksum >> 8);
    buf[7] = (uint8_t)(hdr->checksum & 0xFF);

    /* Append payload. */
    if (payload && payload_len > 0)
        memcpy(buf + NFS_UDP_HDR_LEN, payload, payload_len);

    return total;
}

/* ------------------------------------------------------------------ */
/*  Checksum (IPv4 pseudo-header + UDP datagram)                       */
/* ------------------------------------------------------------------ */

uint16_t nfs_udp_checksum_ipv4(const uint8_t *src_ip, const uint8_t *dst_ip, const uint8_t *udp_buf,
                               size_t udp_len) {
    /*
     * IPv4 pseudo-header (12 bytes):
     *   +--------+--------+--------+--------+
     *   |           source address           |
     *   +--------+--------+--------+--------+
     *   |         destination address        |
     *   +--------+--------+--------+--------+
     *   |  zero  | proto  |   UDP length     |
     *   +--------+--------+--------+--------+
     *
     * proto = 17 (IPPROTO_UDP).
     */
    uint8_t pseudo[12];
    memcpy(pseudo + 0, src_ip, 4);
    memcpy(pseudo + 4, dst_ip, 4);
    pseudo[8] = 0;
    pseudo[9] = IPPROTO_UDP_NUM;
    pseudo[10] = (uint8_t)(udp_len >> 8);
    pseudo[11] = (uint8_t)(udp_len & 0xFF);

    /* Accumulate: pseudo-header, then the UDP datagram. */
    uint32_t sum = 0;
    sum = internet_checksum_partial(pseudo, sizeof(pseudo), sum);
    sum = internet_checksum_partial(udp_buf, udp_len, sum);
    uint16_t cs = internet_checksum_fold(sum);

    /* RFC 768: if the computed checksum is zero, transmit as 0xFFFF.
     * A checksum value of 0x0000 on the wire means "not computed". */
    if (cs == 0x0000)
        return 0xFFFF;

    return cs;
}

/* ------------------------------------------------------------------ */
/*  Validate                                                           */
/* ------------------------------------------------------------------ */

int nfs_udp_validate(const uint8_t *src_ip, const uint8_t *dst_ip, const uint8_t *udp_buf,
                     size_t udp_len) {
    if (!src_ip || !dst_ip || !udp_buf || udp_len < NFS_UDP_HDR_LEN)
        return -1;

    /* Extract the on-wire checksum (bytes 6-7, network order). */
    uint16_t wire_cs = (uint16_t)((udp_buf[6] << 8) | udp_buf[7]);

    /* RFC 768: checksum of 0x0000 means "not computed" — accept. */
    if (wire_cs == 0x0000)
        return 0;

    /* Compute the checksum over pseudo-header + entire UDP datagram
     * (which already includes the checksum field). If the datagram is
     * intact, the result folds to zero. */
    uint8_t pseudo[12];
    memcpy(pseudo + 0, src_ip, 4);
    memcpy(pseudo + 4, dst_ip, 4);
    pseudo[8] = 0;
    pseudo[9] = IPPROTO_UDP_NUM;
    pseudo[10] = (uint8_t)(udp_len >> 8);
    pseudo[11] = (uint8_t)(udp_len & 0xFF);

    uint32_t sum = 0;
    sum = internet_checksum_partial(pseudo, sizeof(pseudo), sum);
    sum = internet_checksum_partial(udp_buf, udp_len, sum);
    uint16_t verify = internet_checksum_fold(sum);

    return (verify == 0x0000) ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/*  Format                                                             */
/* ------------------------------------------------------------------ */

void nfs_udp_format(const struct nfs_udp_hdr *hdr, char *buf, size_t buf_len) {
    if (!hdr || !buf || buf_len == 0)
        return;

    snprintf(buf, buf_len, "UDP: src_port=%u dst_port=%u length=%u checksum=0x%04x", hdr->src_port,
             hdr->dst_port, hdr->length, hdr->checksum);
}
