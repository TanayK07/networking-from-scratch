/* From-scratch implementation of htonl/htons/ntohl/ntohs.
 *
 * No #include <arpa/inet.h>. We're building the conversion ourselves.
 */

#include "htonl.h"

uint32_t nfs_swap32(uint32_t x) {
    return ((x & 0x000000FFu) << 24) | ((x & 0x0000FF00u) << 8) | ((x & 0x00FF0000u) >> 8) |
           ((x & 0xFF000000u) >> 24);
}

uint16_t nfs_swap16(uint16_t x) {
    return (uint16_t)(((x & 0x00FFu) << 8) | ((x & 0xFF00u) >> 8));
}

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

uint32_t nfs_htonl(uint32_t x) {
    return nfs_swap32(x);
}
uint16_t nfs_htons(uint16_t x) {
    return nfs_swap16(x);
}
uint32_t nfs_ntohl(uint32_t x) {
    return nfs_swap32(x);
}
uint16_t nfs_ntohs(uint16_t x) {
    return nfs_swap16(x);
}

#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

uint32_t nfs_htonl(uint32_t x) {
    return x;
}
uint16_t nfs_htons(uint16_t x) {
    return x;
}
uint32_t nfs_ntohl(uint32_t x) {
    return x;
}
uint16_t nfs_ntohs(uint16_t x) {
    return x;
}

#else
#error "Unknown __BYTE_ORDER__. Are you on a PDP-11?"
#endif

int nfs_is_little_endian(void) {
    union {
        uint32_t i;
        uint8_t b[4];
    } u;
    u.i = 1;
    return u.b[0] == 1;
}
