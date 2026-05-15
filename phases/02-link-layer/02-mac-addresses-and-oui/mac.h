#ifndef NFS_MAC_H
#define NFS_MAC_H

#include <stddef.h>
#include <stdint.h>

/*
 * MAC address utilities — classification, OUI extraction, formatting.
 *
 * Bit layout of the first byte of a MAC address:
 *
 *   bit 0 (I/G)  — 0 = unicast,  1 = multicast
 *   bit 1 (U/L)  — 0 = globally unique (OUI-enforced),
 *                   1 = locally administered
 *
 * Broadcast (ff:ff:ff:ff:ff:ff) is a special case of multicast.
 */

/* Returns 1 if the MAC is the broadcast address (all 0xFF). */
int nfs_mac_is_broadcast(const uint8_t mac[6]);

/* Returns 1 if the multicast (I/G) bit is set. */
int nfs_mac_is_multicast(const uint8_t mac[6]);

/* Returns 1 if the address is unicast (I/G bit clear). */
int nfs_mac_is_unicast(const uint8_t mac[6]);

/* Returns 1 if the locally-administered (U/L) bit is set. */
int nfs_mac_is_local(const uint8_t mac[6]);

/* Returns 1 if the address is globally unique (U/L bit clear). */
int nfs_mac_is_global(const uint8_t mac[6]);

/* Copy the OUI (first 3 bytes) from mac into oui. */
void nfs_mac_get_oui(const uint8_t mac[6], uint8_t oui[3]);

/* Format a 6-byte MAC as "aa:bb:cc:dd:ee:ff".  Returns 0 on success. */
int nfs_mac_format(const uint8_t mac[6], char *buf, size_t sz);

/*
 * Parse "aa:bb:cc:dd:ee:ff" or "aa-bb-cc-dd-ee-ff" into 6 bytes.
 * Returns 0 on success, -1 on error.
 */
int nfs_mac_parse(const char *str, uint8_t mac[6]);

/*
 * Return a human-readable type string:
 *   "broadcast", "multicast", "unicast/local", "unicast/global"
 */
const char *nfs_mac_type_str(const uint8_t mac[6]);

#endif /* NFS_MAC_H */
