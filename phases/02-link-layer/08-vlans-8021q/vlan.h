#ifndef NFS_VLAN_H
#define NFS_VLAN_H

#include <stddef.h>
#include <stdint.h>

/*
 * IEEE 802.1Q VLAN tag.
 *
 * Inserted between the source MAC and the original EtherType in an
 * Ethernet II frame:
 *
 *   [dst 6B][src 6B][TPID 2B][TCI 2B][original ethertype 2B][payload...]
 *
 * TPID (Tag Protocol Identifier) = 0x8100 for standard 802.1Q.
 *
 * TCI (Tag Control Information) layout (16 bits, network order):
 *   bits 15-13 : PCP  (Priority Code Point, 3 bits)
 *   bit  12    : DEI  (Drop Eligible Indicator, 1 bit)
 *   bits 11-0  : VID  (VLAN Identifier, 12 bits, 0-4094)
 */

struct nfs_vlan_tag {
    uint16_t tpid; /* 0x8100 in network byte order */
    uint16_t tci;  /* PCP(3) | DEI(1) | VID(12), network byte order */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_vlan_tag) == 4, "nfs_vlan_tag must be exactly 4 bytes");

/* Standard Ethernet header size (without VLAN). */
#define NFS_ETH_HDR_LEN 14

/* Extract PCP (3 bits) from the TCI.  TCI is in network byte order. */
uint8_t nfs_vlan_pcp(const struct nfs_vlan_tag *t);

/* Extract DEI (1 bit) from the TCI. */
uint8_t nfs_vlan_dei(const struct nfs_vlan_tag *t);

/* Extract VID (12 bits, 0-4094) from the TCI. */
uint16_t nfs_vlan_vid(const struct nfs_vlan_tag *t);

/* Set all fields.  TPID is automatically set to 0x8100. */
void nfs_vlan_set(struct nfs_vlan_tag *t, uint8_t pcp, uint8_t dei, uint16_t vid);

/*
 * Check if a frame is 802.1Q-tagged by inspecting bytes 12-13.
 * Returns 1 if tagged, 0 otherwise.
 */
int nfs_vlan_is_tagged(const uint8_t *frame, size_t len);

/*
 * Insert a VLAN tag into an untagged Ethernet frame.
 * The output frame is 4 bytes longer.
 * Returns the new frame length, or -1 on error.
 */
int nfs_vlan_tag_insert(const uint8_t *frame, size_t frame_len, uint16_t vid, uint8_t pcp,
                        uint8_t *out, size_t out_sz);

/*
 * Strip a VLAN tag from a tagged frame.
 * The output frame is 4 bytes shorter.
 * Returns the new frame length, or -1 on error.
 * If vid_out is non-NULL, the extracted VID is written there.
 */
int nfs_vlan_tag_strip(const uint8_t *frame, size_t frame_len, uint8_t *out, size_t out_sz,
                       uint16_t *vid_out);

/* Format a VLAN tag as "VID=100 PCP=3 DEI=0". */
void nfs_vlan_format(const struct nfs_vlan_tag *t, char *buf, size_t sz);

#endif /* NFS_VLAN_H */
