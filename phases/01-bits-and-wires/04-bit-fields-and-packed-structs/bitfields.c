#include "bitfields.h"
#include <arpa/inet.h>
#include <stdio.h>

/* ---- Field extraction ---- */

uint8_t nfs_ipv4_version(const struct nfs_ipv4_hdr *h) {
    return (h->ver_ihl >> 4) & 0x0F;
}

uint8_t nfs_ipv4_ihl(const struct nfs_ipv4_hdr *h) {
    return h->ver_ihl & 0x0F;
}

uint8_t nfs_ipv4_dscp(const struct nfs_ipv4_hdr *h) {
    return (h->tos >> 2) & 0x3F;
}

uint8_t nfs_ipv4_ecn(const struct nfs_ipv4_hdr *h) {
    return h->tos & 0x03;
}

uint16_t nfs_ipv4_flags(const struct nfs_ipv4_hdr *h) {
    /* flags_frag is in network byte order.
     * After converting to host order: top 3 bits are flags. */
    uint16_t val = ntohs(h->flags_frag);
    return (val >> 13) & 0x07;
}

uint16_t nfs_ipv4_frag_offset(const struct nfs_ipv4_hdr *h) {
    uint16_t val = ntohs(h->flags_frag);
    return val & 0x1FFF;
}

/* ---- Field setters ---- */

void nfs_ipv4_set_ver_ihl(struct nfs_ipv4_hdr *h, uint8_t ver, uint8_t ihl) {
    h->ver_ihl = (uint8_t)((ver << 4) | (ihl & 0x0F));
}

/* ---- Formatting ---- */

void nfs_ipv4_format(const struct nfs_ipv4_hdr *h, char *buf, size_t sz) {
    uint32_t src = ntohl(h->src_addr);
    uint32_t dst = ntohl(h->dst_addr);

    snprintf(buf, sz,
             "IPv4: ver=%u ihl=%u dscp=%u ecn=%u len=%u id=0x%04x "
             "flags=0x%x frag_off=%u ttl=%u proto=%u cksum=0x%04x "
             "src=%u.%u.%u.%u dst=%u.%u.%u.%u",
             nfs_ipv4_version(h), nfs_ipv4_ihl(h), nfs_ipv4_dscp(h), nfs_ipv4_ecn(h),
             ntohs(h->total_len), ntohs(h->ident), nfs_ipv4_flags(h), nfs_ipv4_frag_offset(h),
             h->ttl, h->protocol, ntohs(h->checksum), (src >> 24) & 0xFF, (src >> 16) & 0xFF,
             (src >> 8) & 0xFF, src & 0xFF, (dst >> 24) & 0xFF, (dst >> 16) & 0xFF,
             (dst >> 8) & 0xFF, dst & 0xFF);
}
