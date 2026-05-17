#include "frame.h"

#include <arpa/inet.h> /* htons, ntohs */
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Frame builder                                                      */
/* ------------------------------------------------------------------ */

int nfs_frame_build(const uint8_t *dst, const uint8_t *src, uint16_t ethertype,
                    const uint8_t *payload, size_t payload_len, uint8_t *out, size_t out_sz) {
    if (!dst || !src || !out)
        return -1;
    if (payload_len > NFS_ETH_MAX_DATA)
        return -1;
    if (payload_len > 0 && !payload)
        return -1;

    /* Effective payload after padding. */
    size_t eff_payload = payload_len;
    if (eff_payload < NFS_ETH_MIN_DATA)
        eff_payload = NFS_ETH_MIN_DATA;

    size_t total = NFS_ETH_HLEN + eff_payload;
    if (out_sz < total)
        return -1;

    /* Zero the buffer so padding bytes are 0x00. */
    memset(out, 0, total);

    /* Destination MAC */
    memcpy(out, dst, NFS_ETH_ALEN);
    /* Source MAC */
    memcpy(out + NFS_ETH_ALEN, src, NFS_ETH_ALEN);
    /* EtherType in network byte order */
    uint16_t et_net = htons(ethertype);
    memcpy(out + 12, &et_net, 2);

    /* Payload */
    if (payload && payload_len > 0)
        memcpy(out + NFS_ETH_HLEN, payload, payload_len);

    return (int)total;
}

/* ------------------------------------------------------------------ */
/*  Frame parser                                                       */
/* ------------------------------------------------------------------ */

int nfs_frame_parse(const uint8_t *data, size_t len, struct nfs_eth_frame *frame) {
    if (!data || !frame)
        return -1;
    if (len < NFS_ETH_HLEN)
        return -1;

    memcpy(frame->dst, data, NFS_ETH_ALEN);
    memcpy(frame->src, data + NFS_ETH_ALEN, NFS_ETH_ALEN);

    uint16_t raw;
    memcpy(&raw, data + 12, 2);
    frame->ethertype = ntohs(raw);

    frame->payload_len = len - NFS_ETH_HLEN;
    if (frame->payload_len > NFS_ETH_MAX_DATA)
        frame->payload_len = NFS_ETH_MAX_DATA;

    memset(frame->payload, 0, sizeof(frame->payload));
    if (frame->payload_len > 0)
        memcpy(frame->payload, data + NFS_ETH_HLEN, frame->payload_len);

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Frame validation                                                   */
/* ------------------------------------------------------------------ */

int nfs_frame_valid(const uint8_t *data, size_t len) {
    if (!data)
        return 0;
    if (len < (NFS_ETH_HLEN + NFS_ETH_MIN_DATA)) /* 60 */
        return 0;
    if (len > (NFS_ETH_HLEN + NFS_ETH_MAX_DATA)) /* 1514 */
        return 0;

    /* Extract ethertype from wire (network byte order). */
    uint16_t raw;
    memcpy(&raw, data + 12, 2);
    uint16_t et = ntohs(raw);

    /* Ethernet II: ethertype >= 0x0600.
     * Values below are IEEE 802.3 length fields, not Ethernet II. */
    if (et < 0x0600)
        return 0;

    return 1;
}

/* ------------------------------------------------------------------ */
/*  MAC address utilities                                              */
/* ------------------------------------------------------------------ */

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

char *nfs_mac_format(const uint8_t mac[6], char *out, size_t out_sz) {
    if (!mac || !out || out_sz < 18) /* "aa:bb:cc:dd:ee:ff\0" = 18 chars */
        return NULL;
    snprintf(out, out_sz, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4],
             mac[5]);
    return out;
}

int nfs_mac_is_broadcast(const uint8_t mac[6]) {
    if (!mac)
        return 0;
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0xFF)
            return 0;
    }
    return 1;
}

int nfs_mac_is_multicast(const uint8_t mac[6]) {
    if (!mac)
        return 0;
    /* Bit 0 of the first octet is the group (multicast) bit. */
    return (mac[0] & 0x01) ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/*  Raw address helper                                                 */
/* ------------------------------------------------------------------ */

int nfs_raw_addr_init(struct nfs_raw_addr *addr, int ifindex, const uint8_t *dst,
                      uint16_t ethertype) {
    if (!addr || !dst)
        return -1;
    addr->ifindex = ifindex;
    memcpy(addr->dst, dst, NFS_ETH_ALEN);
    addr->ethertype = ethertype;
    return 0;
}
