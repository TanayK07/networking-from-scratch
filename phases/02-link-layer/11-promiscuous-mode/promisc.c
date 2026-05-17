#include "promisc.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  MAC address classification                                         */
/* ------------------------------------------------------------------ */

int nfs_mac_is_broadcast(const uint8_t mac[6]) {
    static const uint8_t bcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return memcmp(mac, bcast, 6) == 0;
}

int nfs_mac_is_multicast(const uint8_t mac[6]) {
    /* Multicast: bit 0 of byte 0 is set, but exclude broadcast. */
    if (nfs_mac_is_broadcast(mac))
        return 0;
    return (mac[0] & 0x01) != 0;
}

int nfs_mac_is_unicast(const uint8_t mac[6]) {
    return (mac[0] & 0x01) == 0;
}

int nfs_mac_equal(const uint8_t a[6], const uint8_t b[6]) {
    return memcmp(a, b, 6) == 0;
}

/* ------------------------------------------------------------------ */
/*  Frame filter                                                       */
/* ------------------------------------------------------------------ */

void nfs_filter_init(struct nfs_frame_filter *f, const uint8_t local_mac[6], int promiscuous) {
    memset(f, 0, sizeof(*f));
    memcpy(f->local_mac, local_mac, 6);
    f->promiscuous = promiscuous;
}

int nfs_filter_accept(struct nfs_frame_filter *f, const uint8_t *frame, size_t len) {
    if (len < 14)
        return -1;

    const uint8_t *dst = frame; /* first 6 bytes are dst MAC */

    f->total_seen++;

    /* Classify the frame */
    int is_bcast = nfs_mac_is_broadcast(dst);
    int is_mcast = nfs_mac_is_multicast(dst);
    int is_to_us = nfs_mac_equal(dst, f->local_mac);

    /* Update classification counters */
    if (is_bcast) {
        f->broadcast_count++;
    } else if (is_mcast) {
        f->multicast_count++;
    } else if (is_to_us) {
        f->unicast_count++;
    } else {
        f->other_unicast++;
    }

    /* Decide whether to accept */
    if (f->promiscuous || is_bcast || is_mcast || is_to_us) {
        f->accepted++;
        return 1;
    }

    f->rejected++;
    return 0;
}

void nfs_filter_reset_stats(struct nfs_frame_filter *f) {
    f->total_seen = 0;
    f->accepted = 0;
    f->rejected = 0;
    f->broadcast_count = 0;
    f->multicast_count = 0;
    f->unicast_count = 0;
    f->other_unicast = 0;
}

void nfs_filter_summary(const struct nfs_frame_filter *f, char *buf, size_t sz) {
    if (!buf || sz == 0)
        return;

    snprintf(buf, sz,
             "mode=%s  total=%llu  accepted=%llu  rejected=%llu  "
             "bcast=%llu  mcast=%llu  unicast=%llu  other=%llu",
             f->promiscuous ? "promisc" : "normal", (unsigned long long)f->total_seen,
             (unsigned long long)f->accepted, (unsigned long long)f->rejected,
             (unsigned long long)f->broadcast_count, (unsigned long long)f->multicast_count,
             (unsigned long long)f->unicast_count, (unsigned long long)f->other_unicast);
}

/* ------------------------------------------------------------------ */
/*  Packet socket configuration                                        */
/* ------------------------------------------------------------------ */

void nfs_packet_cfg_init(struct nfs_packet_cfg *cfg, int ifindex, int protocol, int promiscuous) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->socket_type = 3; /* SOCK_RAW */
    cfg->protocol = protocol;
    cfg->ifindex = ifindex;
    cfg->promiscuous = promiscuous;
}
