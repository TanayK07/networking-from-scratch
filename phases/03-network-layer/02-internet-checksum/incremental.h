#ifndef NFS_INCREMENTAL_H
#define NFS_INCREMENTAL_H

#include <stdint.h>

/* RFC 1624: update a 16-bit Internet checksum when one 16-bit word
 * changes. Returns the new checksum.
 *
 *   old_check  -- the existing 16-bit checksum (host order)
 *   old_word   -- the 16-bit word being replaced (host order)
 *   new_word   -- the new value (host order)
 */
uint16_t internet_checksum_update(uint16_t old_check,
                                  uint16_t old_word,
                                  uint16_t new_word);

/* Same idea, but for an N-byte change (N must be even).
 * Used for IP-address fields (4 bytes each) in NAT. */
uint16_t internet_checksum_update_n(uint16_t old_check,
                                    const void *old_data,
                                    const void *new_data,
                                    unsigned int n);

#endif /* NFS_INCREMENTAL_H */
