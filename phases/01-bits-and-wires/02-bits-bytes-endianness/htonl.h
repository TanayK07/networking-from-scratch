#ifndef NFS_HTONL_H
#define NFS_HTONL_H

#include <stdint.h>

/* Reimplementation of htonl, htons, ntohl, ntohs from scratch.
 *
 * Prefixed with nfs_ to avoid colliding with <arpa/inet.h>.
 */

uint32_t nfs_swap32(uint32_t x);
uint16_t nfs_swap16(uint16_t x);

uint32_t nfs_htonl(uint32_t x);
uint16_t nfs_htons(uint16_t x);
uint32_t nfs_ntohl(uint32_t x);
uint16_t nfs_ntohs(uint16_t x);

/* Detect host endianness at runtime. */
int nfs_is_little_endian(void);

#endif /* NFS_HTONL_H */
