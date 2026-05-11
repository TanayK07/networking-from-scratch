/* Demo: read a 32-bit hex value from argv, print all four conversions.
 *
 *   $ ./endian 0x12345678
 *   host endianness:  little
 *   input (host):     0x12345678
 *   nfs_htonl:        0x78563412
 *   nfs_ntohl:        0x78563412
 *   nfs_htons (low):  0x3412
 *   nfs_ntohs (low):  0x3412
 */

#include "htonl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <32-bit hex value>\n", argv[0]);
        return 1;
    }

    uint32_t x = (uint32_t)strtoul(argv[1], NULL, 0);
    uint16_t s = (uint16_t)(x & 0xFFFF);

    printf("host endianness:  %s\n", nfs_is_little_endian() ? "little" : "big");
    printf("input (host):     0x%08x\n", x);
    printf("nfs_htonl:        0x%08x\n", nfs_htonl(x));
    printf("nfs_ntohl:        0x%08x\n", nfs_ntohl(x));
    printf("nfs_htons (low):  0x%04x\n", nfs_htons(s));
    printf("nfs_ntohs (low):  0x%04x\n", nfs_ntohs(s));
    return 0;
}
