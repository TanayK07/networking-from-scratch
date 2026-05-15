#ifndef NFS_CIDR_H
#define NFS_CIDR_H

#include <stddef.h>
#include <stdint.h>

/* CIDR block: IP address + prefix length.
 * The address is stored in HOST byte order for ease of bit manipulation. */
struct nfs_cidr {
    uint32_t addr;
    uint8_t  prefix_len;
};

/* Parse a CIDR string "10.0.0.0/24" into a struct nfs_cidr.
 * Returns 0 on success, -1 on error. */
int nfs_cidr_parse(const char *str, struct nfs_cidr *out);

/* Parse a dotted-decimal IPv4 address "10.0.0.1" into a host-order uint32.
 * Returns 0 on success, -1 on error. */
int nfs_ip_parse(const char *str, uint32_t *out);

/* Format a host-order uint32 as "10.0.0.1" into buf.
 * buf must be at least 16 bytes. */
void nfs_ip_format(uint32_t ip, char *buf, size_t sz);

/* Return the subnet mask for a given prefix length.
 *   /24 -> 0xFFFFFF00
 *   /0  -> 0x00000000
 *   /32 -> 0xFFFFFFFF */
uint32_t nfs_cidr_mask(uint8_t prefix_len);

/* Return the network address: addr & mask. */
uint32_t nfs_cidr_network(const struct nfs_cidr *c);

/* Return the broadcast address: network | ~mask. */
uint32_t nfs_cidr_broadcast(const struct nfs_cidr *c);

/* Return the number of usable host addresses.
 *   2^(32-prefix) - 2 for prefix < 31
 *   /31 -> 2  (RFC 3021 point-to-point)
 *   /32 -> 1  (single host) */
uint32_t nfs_cidr_host_count(uint8_t prefix_len);

/* Return 1 if ip falls within the subnet described by net, 0 otherwise. */
int nfs_cidr_contains(const struct nfs_cidr *net, uint32_t ip);

/* Return 1 if the two CIDR blocks overlap (share any address), 0 otherwise. */
int nfs_cidr_overlap(const struct nfs_cidr *a, const struct nfs_cidr *b);

/* Format a CIDR block as "10.0.0.0/24" into buf.
 * buf must be at least 20 bytes. */
void nfs_cidr_format(const struct nfs_cidr *c, char *buf, size_t sz);

#endif /* NFS_CIDR_H */
