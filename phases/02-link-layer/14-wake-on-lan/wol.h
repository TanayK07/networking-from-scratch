#ifndef NFS_WOL_H
#define NFS_WOL_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Wake-on-LAN magic packet builder and validator.
 *
 * A WoL magic packet consists of:
 *   - 6 bytes of 0xFF (synchronisation stream)
 *   - The target MAC address repeated 16 times (96 bytes)
 * Total: 102 bytes.
 *
 * An optional 4-byte or 6-byte password ("SecureOn") may follow.
 * --------------------------------------------------------------- */

#define NFS_WOL_SYNC_LEN     6
#define NFS_WOL_MAC_REPEATS  16
#define NFS_WOL_MAGIC_SIZE   102   /* 6 + 6*16 */

/* Build a standard magic packet (102 bytes).
 * Returns 102 on success, -1 if out_sz < 102 or args are NULL. */
int nfs_wol_build(const uint8_t mac[6], uint8_t *out, size_t out_sz);

/* Build a magic packet with an optional SecureOn password appended.
 * pw_len must be 0 (no password), 4, or 6.
 * Returns total bytes written (102, 106, or 108), or -1 on error. */
int nfs_wol_build_with_password(const uint8_t mac[6],
                                const uint8_t *password, size_t pw_len,
                                uint8_t *out, size_t out_sz);

/* Validate a magic packet and extract the target MAC into mac_out.
 * Checks: sync bytes, and all 16 MAC copies are identical.
 * Returns 0 if valid, -1 if invalid. */
int nfs_wol_validate(const uint8_t *pkt, size_t len, uint8_t mac_out[6]);

/* Parse a MAC address string in "aa:bb:cc:dd:ee:ff" or "aa-bb-cc-dd-ee-ff"
 * format.  Returns 0 on success, -1 on parse error. */
int nfs_wol_mac_parse(const char *str, uint8_t mac[6]);

/* Format a MAC address as "aa:bb:cc:dd:ee:ff".
 * buf must be at least 18 bytes.  Returns 0. */
int nfs_wol_mac_format(const uint8_t mac[6], char *buf, size_t sz);

#endif /* NFS_WOL_H */
