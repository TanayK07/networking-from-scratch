#ifndef NFS_PROMISC_H
#define NFS_PROMISC_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  ioctl / setsockopt flag constants (reference values)               */
/* ------------------------------------------------------------------ */

#define NFS_IFF_PROMISC       0x100 /* IFF_PROMISC from net/if.h        */
#define NFS_PACKET_MR_PROMISC 1     /* PACKET_MR_PROMISC for setsockopt */

/* ------------------------------------------------------------------ */
/*  MAC address classification                                         */
/* ------------------------------------------------------------------ */

/* Returns 1 if mac is the broadcast address ff:ff:ff:ff:ff:ff. */
int nfs_mac_is_broadcast(const uint8_t mac[6]);

/* Returns 1 if mac is multicast (bit 0 of byte 0 set) AND not broadcast. */
int nfs_mac_is_multicast(const uint8_t mac[6]);

/* Returns 1 if mac is unicast (bit 0 of byte 0 clear). */
int nfs_mac_is_unicast(const uint8_t mac[6]);

/* Returns 1 if a and b are the same 6-byte MAC address. */
int nfs_mac_equal(const uint8_t a[6], const uint8_t b[6]);

/* ------------------------------------------------------------------ */
/*  Frame filter                                                       */
/* ------------------------------------------------------------------ */

/*
 * Frame filter simulating normal vs promiscuous NIC behavior.
 *
 * Normal mode:  only accepts frames whose dst MAC matches local_mac,
 *               or is broadcast, or is multicast.
 * Promiscuous:  accepts all frames regardless of dst MAC.
 */
struct nfs_frame_filter {
    uint8_t local_mac[6];
    int promiscuous;          /* 0 = normal filtering, 1 = promiscuous */
    uint64_t total_seen;      /* frames evaluated                       */
    uint64_t accepted;        /* frames accepted by filter              */
    uint64_t rejected;        /* frames rejected by filter              */
    uint64_t broadcast_count; /* broadcast frames seen                  */
    uint64_t multicast_count; /* multicast frames seen                  */
    uint64_t unicast_count;   /* unicast frames addressed to us         */
    uint64_t other_unicast;   /* unicast to others (only in promisc)    */
};

/* Initialise a frame filter with the given local MAC and mode. */
void nfs_filter_init(struct nfs_frame_filter *f, const uint8_t local_mac[6], int promiscuous);

/*
 * Evaluate a raw Ethernet frame against the filter.
 *
 * Returns  1  if the frame is accepted,
 *          0  if the frame is rejected (normal mode, wrong unicast dst),
 *         -1  if the frame is too short (< 14 bytes).
 *
 * Updates the statistics counters in `f` on every call (even rejected).
 */
int nfs_filter_accept(struct nfs_frame_filter *f, const uint8_t *frame, size_t len);

/* Reset all statistics counters to zero (mode and local_mac unchanged). */
void nfs_filter_reset_stats(struct nfs_frame_filter *f);

/* Write a human-readable statistics summary into buf (up to sz bytes). */
void nfs_filter_summary(const struct nfs_frame_filter *f, char *buf, size_t sz);

/* ------------------------------------------------------------------ */
/*  Packet socket configuration (struct only, no actual socket)        */
/* ------------------------------------------------------------------ */

struct nfs_packet_cfg {
    int socket_type; /* SOCK_RAW or SOCK_DGRAM                       */
    int protocol;    /* e.g. htons(ETH_P_ALL) = 0x0003, ETH_P_IP    */
    int ifindex;     /* interface index                               */
    int promiscuous; /* 1 to request promiscuous mode                 */
};

/* Initialise a packet socket configuration struct. */
void nfs_packet_cfg_init(struct nfs_packet_cfg *cfg, int ifindex, int protocol, int promiscuous);

#endif /* NFS_PROMISC_H */
