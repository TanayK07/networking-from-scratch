#ifndef NFS_BSOCK_H
#define NFS_BSOCK_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Berkeley Sockets API Helpers
 *
 * Provides portable helpers for building, parsing, and formatting
 * sockaddr_in structures without requiring actual socket syscalls.
 *
 * struct sockaddr_in layout:
 *   sin_family : AF_INET (2)
 *   sin_port   : 16-bit port in network byte order
 *   sin_addr   : 32-bit IPv4 address in network byte order
 * --------------------------------------------------------------- */

/* AF_INET constant (matches POSIX) */
#define NFS_AF_INET  2

/* Our portable sockaddr_in (matches layout of struct sockaddr_in). */
struct nfs_sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;     /* network byte order */
    uint32_t sin_addr;     /* network byte order */
    uint8_t  sin_zero[8];  /* padding to 16 bytes */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_sockaddr_in) == 16,
               "nfs_sockaddr_in must be 16 bytes");

/* Build a sockaddr_in from an IPv4 dotted-quad string and port.
 * Returns 0 on success, -1 on error. */
int nfs_sockaddr_in_build(struct nfs_sockaddr_in *sa,
                          const char *ip_str, uint16_t port);

/* Parse a sockaddr_in into an IP string and port.
 * ip_buf must be at least 16 bytes.
 * Returns 0 on success, -1 on error. */
int nfs_sockaddr_in_parse(const struct nfs_sockaddr_in *sa,
                          char *ip_buf, size_t ip_buf_sz,
                          uint16_t *port);

/* Format a sockaddr_in as "ip:port" string.
 * Returns bytes written (excluding NUL), or -1 on error. */
int nfs_sockaddr_format(const struct nfs_sockaddr_in *sa,
                        char *out, size_t out_sz);

/* Extract the port (host byte order) from a sockaddr_in. */
uint16_t nfs_sockaddr_port(const struct nfs_sockaddr_in *sa);

/* Extract the 32-bit address (network byte order) from a sockaddr_in. */
uint32_t nfs_sockaddr_addr(const struct nfs_sockaddr_in *sa);

/* Parse a dotted-quad IP string into a network-order 32-bit address.
 * Returns 0 on success, -1 on error. */
int nfs_inet_aton(const char *ip_str, uint32_t *addr);

/* Format a network-order 32-bit address into a dotted-quad string.
 * buf must be at least 16 bytes.
 * Returns 0 on success, -1 on error. */
int nfs_inet_ntoa(uint32_t addr, char *buf, size_t buf_sz);

#endif /* NFS_BSOCK_H */
