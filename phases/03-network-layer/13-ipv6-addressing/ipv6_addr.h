#ifndef NFS_IPV6_ADDR_H
#define NFS_IPV6_ADDR_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * IPv6 address classification, construction, and formatting.
 *
 * An IPv6 address is 128 bits (16 bytes) stored in network byte
 * order (big-endian).  Key address types:
 *
 *   ::1              Loopback
 *   ::               Unspecified
 *   fe80::/10        Link-local
 *   ff00::/8         Multicast
 *   2000::/3         Global unicast
 *   fc00::/7         Unique local (ULA)
 * --------------------------------------------------------------- */

/* Address type classification (return 1 for match, 0 otherwise). */
int nfs_ipv6_addr_is_loopback(const uint8_t addr[16]);
int nfs_ipv6_addr_is_unspecified(const uint8_t addr[16]);
int nfs_ipv6_addr_is_link_local(const uint8_t addr[16]);
int nfs_ipv6_addr_is_multicast(const uint8_t addr[16]);
int nfs_ipv6_addr_is_global_unicast(const uint8_t addr[16]);
int nfs_ipv6_addr_is_ula(const uint8_t addr[16]);

/* Return a human-readable type string: "loopback", "unspecified",
 * "link-local", "multicast", "global unicast", "unique local",
 * or "other".  Never returns NULL. */
const char *nfs_ipv6_addr_type_str(const uint8_t addr[16]);

/* Construct an IPv6 interface address from a 48-bit MAC and a 64-bit
 * prefix using Modified EUI-64:
 *   - Insert 0xFF, 0xFE in the middle of the MAC
 *   - Flip bit 6 (universal/local) of the first byte
 *   - Combine with the prefix (first 8 bytes) */
void nfs_ipv6_addr_from_eui64(const uint8_t mac[6], const uint8_t prefix[8], uint8_t addr[16]);

/* Compute the solicited-node multicast address for `addr`:
 *   ff02::1:ffXX:XXXX   (last 24 bits of addr) */
int nfs_ipv6_addr_solicited_node(const uint8_t addr[16], uint8_t out[16]);

/* Format an IPv6 address in full expanded form (no :: compression):
 *   "2001:0db8:0000:0000:0000:0000:0000:0001"
 * Returns 0 on success, -1 if buffer is too small.
 * Needs at least 40 bytes (39 chars + NUL). */
int nfs_ipv6_addr_format(const uint8_t addr[16], char *buf, size_t sz);

/* Parse a full-form IPv6 address string (8 groups of 4 hex digits
 * separated by colons).  Returns 0 on success, -1 on error. */
int nfs_ipv6_addr_parse(const char *str, uint8_t addr[16]);

#endif /* NFS_IPV6_ADDR_H */
