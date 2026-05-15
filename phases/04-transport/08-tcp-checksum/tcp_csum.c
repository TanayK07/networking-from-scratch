/* TCP checksum with IPv4 pseudo-header (RFC 9293, RFC 1071).
 *
 * TCP checksum is mandatory (unlike UDP where it is optional in IPv4).
 * The checksum covers a 12-byte pseudo-header prepended to the TCP segment.
 * The pseudo-header includes source/dest IPs, protocol (6), and TCP length.
 *
 * The algorithm is the same RFC 1071 Internet checksum used by UDP:
 * 16-bit one's complement of the one's complement sum of all 16-bit words.
 */

#include "tcp_csum.h"

#include <string.h>

/* IP protocol number for TCP. */
#define IPPROTO_TCP_NUM 6

/* ------------------------------------------------------------------ */
/*  RFC 1071 Internet Checksum helpers                                 */
/* ------------------------------------------------------------------ */

static uint32_t checksum_partial_internal(const void *data, size_t len, uint32_t seed) {
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

static uint16_t checksum_fold_internal(uint32_t partial) {
    while (partial >> 16) {
        partial = (partial & 0xFFFF) + (partial >> 16);
    }
    return (uint16_t)~partial;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

uint16_t nfs_checksum_partial(const void *data, size_t len, uint32_t seed) {
    /* Note: the return type is uint16_t per the header, but we are
     * actually returning a truncated 32-bit partial sum.  We cast
     * through the internal function that returns uint32_t.  For the
     * public API to be useful for multi-buffer checksums, callers
     * should use this with nfs_checksum_fold. */
    return (uint16_t)checksum_partial_internal(data, len, seed);
}

uint16_t nfs_checksum_fold(uint32_t sum) {
    return checksum_fold_internal(sum);
}

uint16_t nfs_internet_checksum(const void *data, size_t len) {
    return checksum_fold_internal(checksum_partial_internal(data, len, 0));
}

uint16_t nfs_tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                          const uint8_t *tcp_segment, size_t tcp_len) {
    /* Build the pseudo-header.
     * IPs are already in network byte order; tcp_length must be in
     * network byte order as well. */
    struct nfs_ipv4_pseudo_hdr pseudo;
    pseudo.src_addr   = src_ip;
    pseudo.dst_addr   = dst_ip;
    pseudo.zero       = 0;
    pseudo.protocol   = IPPROTO_TCP_NUM;
    pseudo.tcp_length = (uint16_t)(((tcp_len & 0xFF) << 8) |
                                   ((tcp_len >> 8) & 0xFF));

    /* Accumulate: pseudo-header, then the entire TCP segment. */
    uint32_t sum = 0;
    sum = checksum_partial_internal(&pseudo, sizeof(pseudo), sum);
    sum = checksum_partial_internal(tcp_segment, tcp_len, sum);
    return checksum_fold_internal(sum);
}

int nfs_tcp_verify_checksum(uint32_t src_ip, uint32_t dst_ip,
                            const uint8_t *tcp_segment, size_t tcp_len) {
    if (!tcp_segment || tcp_len < 20)
        return 0;

    /* Compute checksum over pseudo-header + entire TCP segment
     * (which already includes the checksum field).  If the segment
     * is intact, the result folds to 0x0000. */
    struct nfs_ipv4_pseudo_hdr pseudo;
    pseudo.src_addr   = src_ip;
    pseudo.dst_addr   = dst_ip;
    pseudo.zero       = 0;
    pseudo.protocol   = IPPROTO_TCP_NUM;
    pseudo.tcp_length = (uint16_t)(((tcp_len & 0xFF) << 8) |
                                   ((tcp_len >> 8) & 0xFF));

    uint32_t sum = 0;
    sum = checksum_partial_internal(&pseudo, sizeof(pseudo), sum);
    sum = checksum_partial_internal(tcp_segment, tcp_len, sum);
    uint16_t verify = checksum_fold_internal(sum);

    return (verify == 0x0000) ? 1 : 0;
}
