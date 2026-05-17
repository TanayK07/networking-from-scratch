#ifndef NFS_ETHERNET_H
#define NFS_ETHERNET_H

#include <stddef.h>
#include <stdint.h>

/*
 * Ethernet II frame header (DIX framing).
 *
 * Wire layout (14 bytes):
 *   [dst MAC 6B][src MAC 6B][EtherType 2B][payload...][FCS 4B]
 *
 * This struct covers only the first 14 bytes.  FCS is typically
 * stripped by the NIC before userspace ever sees it.
 */
struct nfs_eth_hdr {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t ethertype; /* network byte order */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_eth_hdr) == 14, "nfs_eth_hdr must be exactly 14 bytes");

/* Minimum frame size we accept (header only; we don't require padding). */
#define NFS_ETH_HDR_LEN 14

/* Minimum Ethernet payload to satisfy the 64-byte minimum frame rule
 * (64 - 14 header - 4 FCS = 46).  nfs_eth_build pads to this. */
#define NFS_ETH_MIN_PAYLOAD 46

/* Well-known EtherTypes (network byte order written big-endian). */
#define NFS_ETHERTYPE_IPV4 0x0800
#define NFS_ETHERTYPE_ARP  0x0806
#define NFS_ETHERTYPE_IPV6 0x86DD
#define NFS_ETHERTYPE_VLAN 0x8100

/*
 * Parse a raw Ethernet frame into its header and payload.
 *
 * Returns 0 on success, -1 if the frame is too short.
 * On success *payload points inside `frame` and *payload_len is set.
 * The ethertype in `hdr` is converted to host byte order.
 */
int nfs_eth_parse(const uint8_t *frame, size_t len, struct nfs_eth_hdr *hdr,
                  const uint8_t **payload, size_t *payload_len);

/*
 * Build an Ethernet II frame.
 *
 * Payload is padded with zeros to NFS_ETH_MIN_PAYLOAD if shorter.
 * Returns total frame length written to `out`, or -1 on error.
 */
int nfs_eth_build(const uint8_t dst[6], const uint8_t src[6], uint16_t ethertype,
                  const uint8_t *payload, size_t payload_len, uint8_t *out, size_t out_sz);

/* Return a human-readable name for a host-byte-order ethertype. */
const char *nfs_ethertype_name(uint16_t et);

/* Format header as "aa:bb:cc:dd:ee:ff -> 11:22:33:44:55:66 [IPv4]". */
void nfs_eth_format(const struct nfs_eth_hdr *h, char *buf, size_t sz);

/* Format a 6-byte MAC to "aa:bb:cc:dd:ee:ff".  Returns 0 on success. */
int nfs_mac_format(const uint8_t mac[6], char *buf, size_t sz);

/* Parse "aa:bb:cc:dd:ee:ff" into 6 bytes.  Returns 0 on success. */
int nfs_mac_parse(const char *str, uint8_t mac[6]);

#endif /* NFS_ETHERNET_H */
