#ifndef NFS_CHECKSUM_H
#define NFS_CHECKSUM_H

#include <stddef.h>
#include <stdint.h>

/* RFC 1071 Internet checksum.
 *
 * Computes the 16-bit one's-complement sum of `len` bytes at `data`.
 * Returns the result in HOST byte order. Caller is responsible for
 * htons() if writing it back to a packet.
 */
uint16_t internet_checksum(const void *data, size_t len);

/* Continue an existing checksum with more data. Useful for
 * pseudo-header + L4 header + payload across non-contiguous buffers.
 *
 * Pass `seed = 0` for the first call.
 */
uint32_t internet_checksum_partial(const void *data, size_t len, uint32_t seed);

/* Fold a 32-bit partial sum into the final 16-bit checksum
 * with the bitwise NOT applied (RFC 1071, §1).
 */
uint16_t internet_checksum_fold(uint32_t partial);

#endif /* NFS_CHECKSUM_H */
