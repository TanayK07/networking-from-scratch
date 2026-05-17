/* IPv6 Header Parsing and Building (RFC 8200)
 *
 * Implements the 40-byte fixed IPv6 header: parse, build, address
 * formatting/parsing, and next-header name lookup.
 */

#include "ipv6.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Field extractors                                                   */
/* ------------------------------------------------------------------ */

uint8_t nfs_ipv6_version(const struct nfs_ipv6_hdr *h) {
    uint32_t host = ntohl(h->vtc_flow);
    return (uint8_t)((host >> 28) & 0x0F);
}

uint8_t nfs_ipv6_traffic_class(const struct nfs_ipv6_hdr *h) {
    uint32_t host = ntohl(h->vtc_flow);
    return (uint8_t)((host >> 20) & 0xFF);
}

uint32_t nfs_ipv6_flow_label(const struct nfs_ipv6_hdr *h) {
    uint32_t host = ntohl(h->vtc_flow);
    return host & 0x000FFFFF;
}

/* ------------------------------------------------------------------ */
/*  Parse                                                              */
/* ------------------------------------------------------------------ */

int nfs_ipv6_parse(const uint8_t *data, size_t len, struct nfs_ipv6_hdr *hdr) {
    if (!data || !hdr || len < 40)
        return -1;

    memcpy(hdr, data, 40);

    /* Validate version == 6. */
    if (nfs_ipv6_version(hdr) != 6)
        return -1;

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Build                                                              */
/* ------------------------------------------------------------------ */

int nfs_ipv6_build(uint8_t tc, uint32_t flow_label, uint8_t next_hdr, uint8_t hop_limit,
                   const uint8_t src[16], const uint8_t dst[16], uint16_t payload_len, uint8_t *out,
                   size_t out_sz) {
    if (!out || out_sz < 40 || !src || !dst)
        return -1;

    /* Mask flow_label to 20 bits. */
    flow_label &= 0x000FFFFF;

    struct nfs_ipv6_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));

    /* Encode vtc_flow: version=6, traffic_class=tc, flow_label. */
    uint32_t vtc = ((uint32_t)6 << 28) | ((uint32_t)tc << 20) | flow_label;
    hdr.vtc_flow = htonl(vtc);
    hdr.payload_len = htons(payload_len);
    hdr.next_hdr = next_hdr;
    hdr.hop_limit = hop_limit;
    memcpy(hdr.src, src, 16);
    memcpy(hdr.dst, dst, 16);

    memcpy(out, &hdr, 40);
    return 40;
}

/* ------------------------------------------------------------------ */
/*  Address formatting                                                 */
/* ------------------------------------------------------------------ */

int nfs_ipv6_addr_format(const uint8_t addr[16], char *buf, size_t sz) {
    if (!addr || !buf || sz < 40)
        return -1;

    int n = snprintf(buf, sz,
                     "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
                     "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                     addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7],
                     addr[8], addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15]);
    return n;
}

/* ------------------------------------------------------------------ */
/*  Address parsing                                                    */
/* ------------------------------------------------------------------ */

int nfs_ipv6_addr_parse(const char *str, uint8_t addr[16]) {
    if (!str || !addr)
        return -1;

    /* Expect exactly "xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx" (39 chars). */
    unsigned int groups[8];
    int matched = sscanf(str, "%4x:%4x:%4x:%4x:%4x:%4x:%4x:%4x", &groups[0], &groups[1], &groups[2],
                         &groups[3], &groups[4], &groups[5], &groups[6], &groups[7]);
    if (matched != 8)
        return -1;

    /* Verify the string is exactly the right length and format. */
    size_t len = strlen(str);
    if (len != 39)
        return -1;

    /* Check colons are in the right places. */
    for (int i = 0; i < 7; i++) {
        if (str[4 + i * 5] != ':')
            return -1;
    }

    for (int i = 0; i < 8; i++) {
        if (groups[i] > 0xFFFF)
            return -1;
        addr[i * 2] = (uint8_t)((groups[i] >> 8) & 0xFF);
        addr[i * 2 + 1] = (uint8_t)(groups[i] & 0xFF);
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Next Header name lookup                                            */
/* ------------------------------------------------------------------ */

const char *nfs_ipv6_next_hdr_name(uint8_t nh) {
    switch (nh) {
    case 0:
        return "Hop-by-Hop";
    case 6:
        return "TCP";
    case 17:
        return "UDP";
    case 43:
        return "Routing";
    case 44:
        return "Fragment";
    case 58:
        return "ICMPv6";
    case 59:
        return "No Next Header";
    default:
        return "Unknown";
    }
}
