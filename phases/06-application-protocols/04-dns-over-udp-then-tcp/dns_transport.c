/* DNS over UDP then TCP implementation. */

#include "dns_transport.h"
#include <string.h>
#include <arpa/inet.h>

/* ---------------------------------------------------------------
 * TCP framing
 * --------------------------------------------------------------- */

int nfs_dns_tcp_frame(const uint8_t *msg, uint16_t msg_len,
                      uint8_t *out, size_t out_sz)
{
    if (!msg || !out) return -1;
    if (msg_len == 0) return -1;
    if ((size_t)msg_len + NFS_DNS_TCP_PREFIX > out_sz) return -1;

    /* Write 2-byte length prefix in network byte order */
    uint16_t net_len = htons(msg_len);
    memcpy(out, &net_len, 2);
    memcpy(out + 2, msg, msg_len);

    return (int)(NFS_DNS_TCP_PREFIX + msg_len);
}

int nfs_dns_tcp_unframe(const uint8_t *data, size_t data_len,
                        const uint8_t **msg_out, uint16_t *msg_len)
{
    if (!data || !msg_out || !msg_len) return -1;
    if (data_len < NFS_DNS_TCP_PREFIX) return -1;

    /* Read 2-byte length prefix */
    uint16_t net_len;
    memcpy(&net_len, data, 2);
    uint16_t len = ntohs(net_len);

    if (len == 0) return -1;
    if ((size_t)len + NFS_DNS_TCP_PREFIX > data_len) return -1;

    /* Message must be at least a DNS header */
    if (len < NFS_DNS_HDR_SIZE) return -1;

    *msg_out = data + NFS_DNS_TCP_PREFIX;
    *msg_len = len;

    return (int)(NFS_DNS_TCP_PREFIX + len);
}

/* ---------------------------------------------------------------
 * Truncation detection
 * --------------------------------------------------------------- */

int nfs_dns_is_truncated(const uint8_t *msg, size_t msg_len)
{
    if (!msg || msg_len < NFS_DNS_HDR_SIZE) return 0;

    struct nfs_dns_transport_hdr hdr;
    memcpy(&hdr, msg, NFS_DNS_HDR_SIZE);
    uint16_t flags = ntohs(hdr.flags);

    /* TC bit is bit 9 (0x0200) */
    return (flags & 0x0200) ? 1 : 0;
}

int nfs_dns_set_truncated(uint8_t *msg, size_t msg_len)
{
    if (!msg || msg_len < NFS_DNS_HDR_SIZE) return -1;

    struct nfs_dns_transport_hdr hdr;
    memcpy(&hdr, msg, NFS_DNS_HDR_SIZE);
    uint16_t flags = ntohs(hdr.flags);
    flags |= 0x0200; /* Set TC bit */
    hdr.flags = htons(flags);
    memcpy(msg, &hdr, NFS_DNS_HDR_SIZE);

    return 0;
}

int nfs_dns_clear_truncated(uint8_t *msg, size_t msg_len)
{
    if (!msg || msg_len < NFS_DNS_HDR_SIZE) return -1;

    struct nfs_dns_transport_hdr hdr;
    memcpy(&hdr, msg, NFS_DNS_HDR_SIZE);
    uint16_t flags = ntohs(hdr.flags);
    flags &= (uint16_t)~0x0200; /* Clear TC bit */
    hdr.flags = htons(flags);
    memcpy(msg, &hdr, NFS_DNS_HDR_SIZE);

    return 0;
}

/* ---------------------------------------------------------------
 * Message size validation
 * --------------------------------------------------------------- */

int nfs_dns_fits_udp(size_t msg_len)
{
    return msg_len <= NFS_DNS_UDP_MAX;
}

int nfs_dns_fits_tcp(size_t msg_len)
{
    return msg_len <= NFS_DNS_TCP_MAX;
}

/* ---------------------------------------------------------------
 * Response truncation
 * --------------------------------------------------------------- */

int nfs_dns_truncate_response(const uint8_t *msg, size_t msg_len,
                              uint16_t max_size,
                              uint8_t *out, size_t out_sz)
{
    if (!msg || !out) return -1;
    if (msg_len < NFS_DNS_HDR_SIZE) return -1;
    if (max_size < NFS_DNS_HDR_SIZE) return -1;

    /* If it already fits, just copy */
    if (msg_len <= max_size) {
        if (out_sz < msg_len) return -1;
        memcpy(out, msg, msg_len);
        return (int)msg_len;
    }

    /* Simplified truncation: keep header + question section,
     * set TC bit, zero out answer/authority/additional counts.
     *
     * First, find the end of the question section. */
    struct nfs_dns_transport_hdr hdr;
    memcpy(&hdr, msg, NFS_DNS_HDR_SIZE);
    uint16_t qdcount = ntohs(hdr.qdcount);

    size_t pos = NFS_DNS_HDR_SIZE;

    /* Skip question section */
    for (uint16_t i = 0; i < qdcount; i++) {
        /* Skip QNAME */
        while (pos < msg_len) {
            uint8_t label_len = msg[pos];
            if (label_len == 0) { pos++; break; }
            if ((label_len & 0xC0) == 0xC0) { pos += 2; break; }
            pos += 1 + label_len;
        }
        /* Skip QTYPE + QCLASS */
        pos += 4;
    }

    /* Truncated size: header + question */
    size_t trunc_len = (pos <= max_size) ? pos : NFS_DNS_HDR_SIZE;
    if (out_sz < trunc_len) return -1;

    memcpy(out, msg, trunc_len);

    /* Update header: zero answer/ns/additional, set TC */
    struct nfs_dns_transport_hdr *ohdr = (struct nfs_dns_transport_hdr *)out;
    ohdr->ancount = 0;
    ohdr->nscount = 0;
    ohdr->arcount = 0;

    uint16_t flags = ntohs(ohdr->flags);
    flags |= 0x0200; /* TC */
    ohdr->flags = htons(flags);

    /* If question didn't fit, just keep header */
    if (pos > max_size) {
        ohdr->qdcount = 0;
    }

    return (int)trunc_len;
}

/* ---------------------------------------------------------------
 * Multi-message TCP stream parsing
 * --------------------------------------------------------------- */

int nfs_dns_tcp_parse_stream(const uint8_t *data, size_t data_len,
                             size_t *offsets, uint16_t *lengths,
                             int max_msgs)
{
    if (!data || !offsets || !lengths || max_msgs <= 0) return -1;

    int count = 0;
    size_t pos = 0;

    while (pos + NFS_DNS_TCP_PREFIX <= data_len && count < max_msgs) {
        uint16_t net_len;
        memcpy(&net_len, data + pos, 2);
        uint16_t len = ntohs(net_len);

        if (len == 0) break;
        if (pos + NFS_DNS_TCP_PREFIX + len > data_len) break;

        offsets[count] = pos + NFS_DNS_TCP_PREFIX;
        lengths[count] = len;
        count++;

        pos += NFS_DNS_TCP_PREFIX + len;
    }

    return count;
}
