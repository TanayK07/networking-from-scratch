#ifndef NFS_SLAAC_H
#define NFS_SLAAC_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * SLAAC (Stateless Address Autoconfiguration) - RFC 4862
 *
 * Generates an IPv6 address from:
 *   1. A /64 prefix (from Router Advertisement)
 *   2. An interface identifier derived from the MAC address (EUI-64)
 *
 * EUI-64 from 48-bit MAC:
 *   - Split MAC into OUI (3 bytes) and NIC (3 bytes)
 *   - Insert 0xFF, 0xFE between them
 *   - Flip the U/L bit (bit 1 of first byte, i.e. XOR with 0x02)
 *
 * Result: 8-byte interface ID appended to 8-byte prefix = 16-byte IPv6 addr
 * --------------------------------------------------------------- */

/* Generate the 8-byte EUI-64 interface identifier from a 6-byte MAC.
 * `mac` is 6 bytes, `eui64` receives 8 bytes.
 * Returns 0 on success, -1 on error. */
int nfs_slaac_eui64(const uint8_t *mac, uint8_t *eui64);

/* Generate a full 16-byte IPv6 address from prefix + MAC.
 * `prefix` is 16 bytes (only first `prefix_len` bits are used).
 * `prefix_len` must be <= 128 (typically 64).
 * `mac` is 6 bytes.
 * `addr_out` receives 16 bytes.
 * Returns 0 on success, -1 on error. */
int nfs_slaac_generate_addr(const uint8_t *prefix, uint8_t prefix_len,
                            const uint8_t *mac, uint8_t *addr_out);

/* Validate that a prefix is suitable for SLAAC.
 * Rules: prefix_len must be exactly 64, prefix must not be link-local
 * (fe80::/10) if reject_linklocal is set, and must not be multicast.
 * Returns 1 if valid, 0 if invalid. */
int nfs_slaac_validate_prefix(const uint8_t *prefix, uint8_t prefix_len,
                              int reject_linklocal);

/* Generate the link-local address (fe80::/64 + EUI-64) from a MAC.
 * `mac` is 6 bytes, `addr_out` receives 16 bytes.
 * Returns 0 on success, -1 on error. */
int nfs_slaac_linklocal(const uint8_t *mac, uint8_t *addr_out);

/* Compute the solicited-node multicast address for a given IPv6 address.
 * ff02::1:ffXX:XXXX where XX:XXXX are the low 24 bits of the address.
 * `addr` is 16 bytes, `mcast_out` receives 16 bytes.
 * Returns 0 on success, -1 on error. */
int nfs_slaac_solicited_node(const uint8_t *addr, uint8_t *mcast_out);

/* Format an IPv6 address (16 bytes) as a colon-hex string. Returns `out`. */
char *nfs_slaac_format_ipv6(const uint8_t *addr, char *out, size_t out_sz);

#endif /* NFS_SLAAC_H */
