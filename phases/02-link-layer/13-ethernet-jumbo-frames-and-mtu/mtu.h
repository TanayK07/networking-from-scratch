#ifndef NFS_MTU_H
#define NFS_MTU_H

#include <stddef.h>

/*
 * Ethernet II frame sizes (IEEE 802.3).
 */
#define NFS_ETH_HDR_LEN     14   /* 6 dst + 6 src + 2 ethertype */
#define NFS_ETH_FCS_LEN     4    /* Frame Check Sequence (CRC-32) */
#define NFS_ETH_MIN_PAYLOAD 46   /* Minimum payload (padded if needed) */
#define NFS_ETH_MAX_PAYLOAD 1500 /* Standard MTU */

/* Standard frame limits. */
#define NFS_ETH_MIN_FRAME 64   /* 14 + 46 + 4 = 64 bytes (with FCS) */
#define NFS_ETH_MAX_FRAME 1518 /* 14 + 1500 + 4 = 1518 bytes (with FCS) */

/* Jumbo frame limits (not IEEE standard, vendor convention). */
#define NFS_JUMBO_MTU       9000 /* Most common jumbo MTU */
#define NFS_JUMBO_MAX_FRAME 9018 /* 14 + 9000 + 4 = 9018 */
#define NFS_BABY_JUMBO_MTU  1992 /* Baby jumbo (some switches) */

/* 802.1Q VLAN tag adds 4 bytes. */
#define NFS_VLAN_TAG_LEN   4
#define NFS_VLAN_MAX_FRAME 1522 /* 1518 + 4 = 1522 (baby giant) */

/* Physical layer overhead per frame. */
#define NFS_PREAMBLE_LEN 7
#define NFS_SFD_LEN      1
#define NFS_IFG_LEN      12
#define NFS_PHY_OVERHEAD 20 /* 7 + 1 + 12 = 20 bytes */

/*
 * MTU profile for a link type.
 */
struct nfs_mtu_profile {
    const char *name;
    size_t mtu;
    size_t min_frame;
    size_t max_frame;
};

/*
 * Frame size validation.
 * Returns 1 if valid, 0 otherwise.
 */
int nfs_mtu_frame_valid(size_t frame_len, size_t mtu);

/*
 * Returns 1 if payload_len fits within the given MTU.
 */
int nfs_mtu_payload_fits(size_t payload_len, size_t mtu);

/*
 * Returns 1 if the IP packet exceeds the MTU and needs fragmentation.
 */
int nfs_mtu_needs_fragmentation(size_t ip_packet_len, size_t mtu);

/*
 * Ethernet efficiency: payload bytes / total wire bytes.
 * Returns a value between 0.0 and 1.0.
 * Accounts for PHY overhead (preamble + SFD + IFG) and minimum padding.
 */
double nfs_mtu_efficiency(size_t payload_len);

/*
 * Goodput in bits per second for a given payload size and link speed.
 * speed_mbps is the line rate in megabits per second (e.g. 1000 for 1Gbps).
 */
double nfs_mtu_goodput_bps(size_t payload_len, int speed_mbps);

/*
 * Maximum frames per second for a given frame size and link speed.
 * frame_len is the Ethernet frame length (header + payload + FCS).
 */
size_t nfs_mtu_max_fps(size_t frame_len, int speed_mbps);

/*
 * Returns an nfs_mtu_profile for standard Ethernet (MTU 1500).
 */
struct nfs_mtu_profile nfs_mtu_standard(void);

/*
 * Returns an nfs_mtu_profile for jumbo frames (MTU 9000).
 */
struct nfs_mtu_profile nfs_mtu_jumbo(void);

/*
 * Path MTU discovery: returns the minimum MTU across an array of link MTUs.
 */
size_t nfs_path_mtu(const size_t *link_mtus, int num_links);

/*
 * Clamp desired_mtu to [min_mtu, max_mtu].
 */
size_t nfs_mtu_clamp(size_t desired_mtu, size_t min_mtu, size_t max_mtu);

#endif /* NFS_MTU_H */
