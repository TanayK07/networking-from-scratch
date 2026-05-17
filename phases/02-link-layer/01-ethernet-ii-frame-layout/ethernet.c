#include "ethernet.h"

#include <arpa/inet.h> /* ntohs, htons */
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Parse                                                              */
/* ------------------------------------------------------------------ */

int nfs_eth_parse(const uint8_t *frame, size_t len, struct nfs_eth_hdr *hdr,
                  const uint8_t **payload, size_t *payload_len) {
    if (!frame || !hdr || !payload || !payload_len)
        return -1;
    if (len < NFS_ETH_HDR_LEN)
        return -1;

    memcpy(hdr->dst, frame, 6);
    memcpy(hdr->src, frame + 6, 6);

    /* Store ethertype in host byte order for easy comparison. */
    uint16_t raw;
    memcpy(&raw, frame + 12, 2);
    hdr->ethertype = ntohs(raw);

    *payload = frame + NFS_ETH_HDR_LEN;
    *payload_len = len - NFS_ETH_HDR_LEN;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Build                                                              */
/* ------------------------------------------------------------------ */

int nfs_eth_build(const uint8_t dst[6], const uint8_t src[6], uint16_t ethertype,
                  const uint8_t *payload, size_t payload_len, uint8_t *out, size_t out_sz) {
    if (!dst || !src || !out)
        return -1;

    /* Effective payload length after padding. */
    size_t eff_payload = payload_len;
    if (eff_payload < NFS_ETH_MIN_PAYLOAD)
        eff_payload = NFS_ETH_MIN_PAYLOAD;

    size_t total = NFS_ETH_HDR_LEN + eff_payload;
    if (out_sz < total)
        return -1;

    /* Zero the output so padding bytes are 0x00. */
    memset(out, 0, total);

    memcpy(out, dst, 6);
    memcpy(out + 6, src, 6);

    uint16_t et_net = htons(ethertype);
    memcpy(out + 12, &et_net, 2);

    if (payload && payload_len > 0)
        memcpy(out + NFS_ETH_HDR_LEN, payload, payload_len);

    return (int)total;
}

/* ------------------------------------------------------------------ */
/*  EtherType name                                                     */
/* ------------------------------------------------------------------ */

const char *nfs_ethertype_name(uint16_t et) {
    switch (et) {
    case NFS_ETHERTYPE_IPV4:
        return "IPv4";
    case NFS_ETHERTYPE_ARP:
        return "ARP";
    case NFS_ETHERTYPE_IPV6:
        return "IPv6";
    case NFS_ETHERTYPE_VLAN:
        return "802.1Q";
    default:
        return "Unknown";
    }
}

/* ------------------------------------------------------------------ */
/*  Formatting helpers                                                 */
/* ------------------------------------------------------------------ */

int nfs_mac_format(const uint8_t mac[6], char *buf, size_t sz) {
    if (!mac || !buf || sz < 18) /* "aa:bb:cc:dd:ee:ff\0" = 18 chars */
        return -1;
    snprintf(buf, sz, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
             mac[5]);
    return 0;
}

int nfs_mac_parse(const char *str, uint8_t mac[6]) {
    if (!str || !mac)
        return -1;
    unsigned int b[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6)
        return -1;
    for (int i = 0; i < 6; i++) {
        if (b[i] > 0xFF)
            return -1;
        mac[i] = (uint8_t)b[i];
    }
    return 0;
}

void nfs_eth_format(const struct nfs_eth_hdr *h, char *buf, size_t sz) {
    if (!h || !buf || sz == 0)
        return;

    char src[18], dst[18];
    nfs_mac_format(h->src, src, sizeof(src));
    nfs_mac_format(h->dst, dst, sizeof(dst));
    snprintf(buf, sz, "%s -> %s [%s]", src, dst, nfs_ethertype_name(h->ethertype));
}
