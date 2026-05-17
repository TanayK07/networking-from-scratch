#ifndef NFS_FRAME_H
#define NFS_FRAME_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

#define NFS_ETH_ALEN     6    /* MAC address length in bytes           */
#define NFS_ETH_HLEN     14   /* Ethernet header: 6 dst + 6 src + 2 ET */
#define NFS_ETH_MIN_DATA 46   /* Minimum payload (padded with zeros)   */
#define NFS_ETH_MAX_DATA 1500 /* Maximum payload (standard MTU)        */
#define NFS_ETH_FCS_LEN  4    /* FCS length (not sent via AF_PACKET)   */

/* EtherType constants (IEEE, host byte order). */
#define NFS_ETHERTYPE_IP   0x0800
#define NFS_ETHERTYPE_ARP  0x0806
#define NFS_ETHERTYPE_IPV6 0x86DD
#define NFS_ETHERTYPE_VLAN 0x8100

/* ------------------------------------------------------------------ */
/*  Structures                                                         */
/* ------------------------------------------------------------------ */

/*
 * Parsed Ethernet II frame.
 *
 * This is NOT a wire-format struct; it stores fields in host byte
 * order for easy inspection.
 */
struct nfs_eth_frame {
    uint8_t dst[NFS_ETH_ALEN];
    uint8_t src[NFS_ETH_ALEN];
    uint16_t ethertype;
    uint8_t payload[NFS_ETH_MAX_DATA];
    size_t payload_len;
};

/*
 * Raw-socket destination address (for AF_PACKET sendto).
 */
struct nfs_raw_addr {
    int ifindex;
    uint8_t dst[NFS_ETH_ALEN];
    uint16_t ethertype;
};

/* ------------------------------------------------------------------ */
/*  Frame builder / parser                                             */
/* ------------------------------------------------------------------ */

/*
 * Build a complete Ethernet II frame ready for AF_PACKET transmission.
 *
 * Writes: dst(6) + src(6) + ethertype(2, network byte order) + payload.
 * If payload < 46 bytes the remaining bytes are zero-padded.
 *
 * Returns total bytes written to `out`, or -1 on error
 * (buffer too small, payload > 1500, or NULL pointers).
 */
int nfs_frame_build(const uint8_t *dst, const uint8_t *src, uint16_t ethertype,
                    const uint8_t *payload, size_t payload_len, uint8_t *out, size_t out_sz);

/*
 * Parse a raw Ethernet frame into an nfs_eth_frame struct.
 *
 * Accepts any frame >= 14 bytes.  The ethertype is stored in host
 * byte order.  Returns 0 on success, -1 on error.
 */
int nfs_frame_parse(const uint8_t *data, size_t len, struct nfs_eth_frame *frame);

/* ------------------------------------------------------------------ */
/*  Frame validation                                                   */
/* ------------------------------------------------------------------ */

/*
 * Validate an Ethernet II frame.
 *
 * Returns 1 if:
 *   - len >= 60  (minimum frame without FCS)
 *   - len <= 1514 (maximum frame without FCS)
 *   - ethertype >= 0x0600 (distinguishes Ethernet II from 802.3 length)
 *
 * Returns 0 otherwise.
 */
int nfs_frame_valid(const uint8_t *data, size_t len);

/* ------------------------------------------------------------------ */
/*  MAC address utilities                                              */
/* ------------------------------------------------------------------ */

/*
 * Parse "AA:BB:CC:DD:EE:FF" (case-insensitive) into 6 bytes.
 * Returns 0 on success, -1 on invalid format.
 */
int nfs_mac_parse(const char *str, uint8_t mac[6]);

/*
 * Format a 6-byte MAC as "aa:bb:cc:dd:ee:ff" (lowercase).
 * Returns pointer to `out` on success, NULL on error.
 */
char *nfs_mac_format(const uint8_t mac[6], char *out, size_t out_sz);

/* Returns 1 if all six bytes are 0xFF (broadcast address). */
int nfs_mac_is_broadcast(const uint8_t mac[6]);

/* Returns 1 if bit 0 of the first byte is set (group/multicast bit). */
int nfs_mac_is_multicast(const uint8_t mac[6]);

/* ------------------------------------------------------------------ */
/*  Raw address helper                                                 */
/* ------------------------------------------------------------------ */

/*
 * Initialise an nfs_raw_addr for use with AF_PACKET sendto().
 * Returns 0 on success, -1 on error.
 */
int nfs_raw_addr_init(struct nfs_raw_addr *addr, int ifindex, const uint8_t *dst,
                      uint16_t ethertype);

#endif /* NFS_FRAME_H */
