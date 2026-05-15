#ifndef NFS_FRAGMENT_H
#define NFS_FRAGMENT_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------------------
 * IPv4 header — 20-byte packed wire format.
 *
 * All multi-byte fields are in NETWORK byte order when on the wire.
 * The fragmentation helpers work on the host-order value of flags_frag.
 * --------------------------------------------------------------------------- */

struct nfs_ipv4_hdr {
    uint8_t  ver_ihl;     /* version (4 bits) | IHL (4 bits)          */
    uint8_t  tos;         /* type of service / DSCP+ECN               */
    uint16_t total_len;   /* total datagram length                    */
    uint16_t id;          /* identification                           */
    uint16_t flags_frag;  /* flags (3 bits) | fragment offset (13 bits) */
    uint8_t  ttl;         /* time to live                             */
    uint8_t  protocol;    /* upper-layer protocol                     */
    uint16_t checksum;    /* header checksum                          */
    uint32_t src_addr;    /* source address                           */
    uint32_t dst_addr;    /* destination address                      */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ipv4_hdr) == 20,
               "nfs_ipv4_hdr must be exactly 20 bytes");

/* Flag / offset constants — applied to the HOST-order value of flags_frag. */
#define NFS_IP_FLAG_DF          0x4000  /* Don't Fragment               */
#define NFS_IP_FLAG_MF          0x2000  /* More Fragments               */
#define NFS_IP_FRAG_OFFSET_MASK 0x1FFF  /* 13-bit offset in 8-byte units */

/* Extract the 3-bit flags field from a host-order flags_frag value. */
uint16_t nfs_ip_get_flags(uint16_t flags_frag_host);

/* Extract the 13-bit fragment offset (in 8-byte units). */
uint16_t nfs_ip_get_frag_offset(uint16_t flags_frag_host);

/* Return non-zero if the packet is a fragment (MF set OR offset != 0). */
int nfs_ip_is_fragment(uint16_t flags_frag_host);

/* A single IP fragment: header (always 20 bytes) + payload (up to 1480). */
struct nfs_ip_fragment {
    uint8_t header[20];
    uint8_t payload[1480];
    size_t  payload_len;
};

/* Fragment an IP packet.
 *
 * orig_pkt  — full IP packet (header + payload), header in network byte order
 * pkt_len   — total length of orig_pkt
 * mtu       — maximum transmission unit (bytes)
 * frags     — output array of fragments
 * max_frags — capacity of frags[]
 *
 * Returns the number of fragments produced (>= 1), or:
 *   -1  if the DF flag is set and the packet exceeds the MTU
 *   -2  if max_frags is too small to hold all fragments
 */
int nfs_ip_fragment_packet(const uint8_t *orig_pkt, size_t pkt_len,
                           uint16_t mtu,
                           struct nfs_ip_fragment *frags, size_t max_frags);

/* Reassemble fragments into the original IP packet.
 *
 * frags  — array of fragments (may be out of order)
 * nfrags — number of fragments
 * out    — output buffer for the reassembled packet
 * out_sz — capacity of out[]
 *
 * Returns the total packet length on success, or -1 on error.
 */
int nfs_ip_reassemble(const struct nfs_ip_fragment *frags, size_t nfrags,
                      uint8_t *out, size_t out_sz);

#endif /* NFS_FRAGMENT_H */
