/* ICMPv4 echo request/reply -- building, parsing, and checksum.
 *
 * Implements the subset of ICMPv4 (RFC 792) needed for a ping clone:
 * Echo Request (type 8) and Echo Reply (type 0). Operates entirely
 * in user-space on byte buffers.
 */

#include "icmp.h"
#include "checksum.h"

#include <arpa/inet.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Parsing
 * --------------------------------------------------------------- */

int nfs_icmp_parse(const uint8_t *buf, size_t len, struct nfs_icmp_hdr *out) {
    if (!buf || !out || len < sizeof(struct nfs_icmp_hdr))
        return -1;

    /* Copy the raw bytes, then convert multi-byte fields to host
     * order so callers don't have to sprinkle ntohs(). */
    memcpy(out, buf, sizeof(*out));
    out->checksum = ntohs(out->checksum);
    out->id = ntohs(out->id);
    out->seq = ntohs(out->seq);

    return 0;
}

/* ---------------------------------------------------------------
 * Build Echo Request
 * --------------------------------------------------------------- */

size_t nfs_icmp_build_echo_request(uint16_t id, uint16_t seq, const uint8_t *payload,
                                   size_t payload_len, uint8_t *buf, size_t buf_len) {
    size_t total = sizeof(struct nfs_icmp_hdr) + payload_len;
    if (!buf || buf_len < total)
        return 0;

    /* Construct header in network byte order. */
    struct nfs_icmp_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.type = NFS_ICMP_ECHO_REQUEST;
    hdr.code = 0;
    hdr.checksum = 0; /* filled in after payload is in place */
    hdr.id = htons(id);
    hdr.seq = htons(seq);

    /* Write header into buffer. */
    memcpy(buf, &hdr, sizeof(hdr));

    /* Write optional payload. */
    if (payload && payload_len > 0)
        memcpy(buf + sizeof(hdr), payload, payload_len);

    /* Compute checksum over the entire ICMP message. */
    uint16_t cs = internet_checksum(buf, total);
    /* internet_checksum returns in host order; store in network order. */
    buf[2] = (uint8_t)(cs >> 8);
    buf[3] = (uint8_t)(cs & 0xFF);

    return total;
}

/* ---------------------------------------------------------------
 * Checksum validation
 * --------------------------------------------------------------- */

int nfs_icmp_validate_checksum(const uint8_t *buf, size_t len) {
    if (!buf || len < sizeof(struct nfs_icmp_hdr))
        return -1;

    /* RFC 1071: computing the checksum over data that already
     * includes a valid checksum yields zero. */
    uint16_t cs = internet_checksum(buf, len);
    return (cs == 0) ? 0 : -1;
}

/* ---------------------------------------------------------------
 * Type name lookup
 * --------------------------------------------------------------- */

const char *nfs_icmp_type_name(uint8_t type) {
    switch (type) {
    case NFS_ICMP_ECHO_REPLY:
        return "Echo Reply";
    case NFS_ICMP_DEST_UNREACH:
        return "Destination Unreachable";
    case NFS_ICMP_ECHO_REQUEST:
        return "Echo Request";
    case NFS_ICMP_TIME_EXCEEDED:
        return "Time Exceeded";
    default:
        return "Unknown";
    }
}
