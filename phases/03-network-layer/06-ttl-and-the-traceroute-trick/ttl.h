#ifndef NFS_TTL_H
#define NFS_TTL_H

#include <stddef.h>
#include <stdint.h>

/*
 * TTL (Time To Live) manipulation and traceroute simulation.
 *
 * Self-contained IPv4 header struct for TTL operations,
 * plus a traceroute simulator that demonstrates how traceroute
 * uses incrementing TTL values to discover each hop on the path.
 */

/* IPv4 header (20 bytes, no options) -- packed for wire format */
struct nfs_ipv4_hdr {
    uint8_t  ver_ihl;      /* Version (4 bits) + IHL (4 bits) */
    uint8_t  tos;          /* Type of Service                  */
    uint16_t total_len;    /* Total length                     */
    uint16_t id;           /* Identification                   */
    uint16_t flags_frag;   /* Flags (3 bits) + Fragment Offset */
    uint8_t  ttl;          /* Time To Live                     */
    uint8_t  protocol;     /* Protocol                         */
    uint16_t checksum;     /* Header checksum                  */
    uint32_t src_addr;     /* Source address                   */
    uint32_t dst_addr;     /* Destination address              */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ipv4_hdr) == 20,
               "IPv4 header must be exactly 20 bytes");

/*
 * Decrement the TTL field in the IPv4 header.
 *
 * Returns:
 *   -1          if TTL was already 0 (cannot decrement)
 *    0          if TTL becomes 0 after decrement (ICMP Time Exceeded)
 *   >0 (1-254) the new TTL value after decrement
 */
int nfs_ttl_decrement(struct nfs_ipv4_hdr *hdr);

/*
 * Set the TTL field.
 */
void nfs_ttl_set(struct nfs_ipv4_hdr *hdr, uint8_t ttl);

/*
 * Get the TTL field.
 */
uint8_t nfs_ttl_get(const struct nfs_ipv4_hdr *hdr);

/*
 * A single hop in a traceroute result.
 */
struct nfs_traceroute_hop {
    int      hop_num;       /* 1-based hop number              */
    uint32_t responder_ip;  /* IP of the router that responded */
    int      reached_dest;  /* 1 if this hop is the destination */
};

/*
 * Simulate a traceroute from `src` to `dst` through `nhops` intermediate
 * routers whose IPs are given in the `hops` array.
 *
 * The path is: src -> hops[0] -> hops[1] -> ... -> hops[nhops-1] -> dst
 *
 * For each probe TTL = 1, 2, ..., the packet traverses the path.
 * At each hop (router), TTL is decremented.  When TTL hits 0, that
 * router "sends back" a Time Exceeded message (its IP is recorded).
 * If the packet reaches `dst`, that hop is recorded with reached_dest=1
 * and the simulation stops.
 *
 * Results are written to `results` (up to `max_results` entries).
 * Returns the number of hops recorded.
 */
int nfs_traceroute_sim(uint32_t src, uint32_t dst,
                       const uint32_t *hops, size_t nhops,
                       struct nfs_traceroute_hop *results,
                       size_t max_results);

#endif /* NFS_TTL_H */
