/* UDP checksum with IPv4 pseudo-header (RFC 768, RFC 1071).
 *
 * The checksum covers a 12-byte pseudo-header prepended to the UDP segment.
 * The pseudo-header includes source/dest IPs, protocol (17), and UDP length.
 *
 * The algorithm is the RFC 1071 Internet checksum: 16-bit one's complement
 * of the one's complement sum of all 16-bit words in the data.
 */

#include "udp_csum.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/*  RFC 1071 Internet Checksum helpers                                 */
/* ------------------------------------------------------------------ */

/* Accumulate partial checksum over a buffer.  Returns the running
 * 32-bit sum (carries not yet folded). */
static uint32_t checksum_partial(const void *data, size_t len, uint32_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = seed;

    /* Sum 16 bits at a time, big-endian (wire order). */
    while (len >= 2) {
        sum += ((uint32_t)p[0] << 8) | (uint32_t)p[1];
        p += 2;
        len -= 2;
    }

    /* Odd trailing byte: pad with zero on the right. */
    if (len == 1) {
        sum += (uint32_t)p[0] << 8;
    }

    return sum;
}

/* Fold a 32-bit partial sum into the final 16-bit checksum
 * with the bitwise NOT applied (RFC 1071, section 1). */
static uint16_t checksum_fold(uint32_t partial) {
    while (partial >> 16) {
        partial = (partial & 0xFFFF) + (partial >> 16);
    }
    return (uint16_t)~partial;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

uint16_t nfs_internet_checksum(const void *data, size_t len) {
    return checksum_fold(checksum_partial(data, len, 0));
}

uint16_t nfs_udp_checksum(uint32_t src_ip, uint32_t dst_ip, const uint8_t *udp_segment,
                          size_t udp_len) {
    /* Build the pseudo-header.
     * IPs are already in network byte order; udp_length must be in
     * network byte order as well. */
    struct nfs_ipv4_pseudo_hdr pseudo;
    pseudo.src_addr = src_ip;
    pseudo.dst_addr = dst_ip;
    pseudo.zero = 0;
    pseudo.protocol = 17; /* IPPROTO_UDP */
    pseudo.udp_length = (uint16_t)(((udp_len & 0xFF) << 8) | ((udp_len >> 8) & 0xFF));

    /* Accumulate: pseudo-header, then the entire UDP segment. */
    uint32_t sum = 0;
    sum = checksum_partial(&pseudo, sizeof(pseudo), sum);
    sum = checksum_partial(udp_segment, udp_len, sum);
    uint16_t cs = checksum_fold(sum);

    /* RFC 768: if the computed checksum is zero, transmit as 0xFFFF.
     * A checksum of 0x0000 on the wire means "not computed". */
    if (cs == 0x0000)
        return 0xFFFF;

    return cs;
}

int nfs_udp_verify_checksum(uint32_t src_ip, uint32_t dst_ip, const uint8_t *udp_segment,
                            size_t udp_len) {
    if (!udp_segment || udp_len < 8)
        return 0;

    /* Extract on-wire checksum (bytes 6-7, network byte order). */
    uint16_t wire_cs = (uint16_t)((udp_segment[6] << 8) | udp_segment[7]);

    /* RFC 768: checksum of 0x0000 means "not computed" -- always valid. */
    if (wire_cs == 0x0000)
        return 1;

    /* Compute checksum over pseudo-header + entire UDP segment
     * (which already includes the checksum field).  If the segment
     * is intact, the result folds to 0x0000. */
    struct nfs_ipv4_pseudo_hdr pseudo;
    pseudo.src_addr = src_ip;
    pseudo.dst_addr = dst_ip;
    pseudo.zero = 0;
    pseudo.protocol = 17;
    pseudo.udp_length = (uint16_t)(((udp_len & 0xFF) << 8) | ((udp_len >> 8) & 0xFF));

    uint32_t sum = 0;
    sum = checksum_partial(&pseudo, sizeof(pseudo), sum);
    sum = checksum_partial(udp_segment, udp_len, sum);
    uint16_t verify = checksum_fold(sum);

    /* If the datagram is intact, the checksum over the whole thing
     * (including the checksum field) should fold to 0. */
    return (verify == 0x0000) ? 1 : 0;
}
