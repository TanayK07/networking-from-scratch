#ifndef NFS_ADDR_H
#define NFS_ADDR_H

#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>

/* ---------------------------------------------------------------
 * Address Families
 *
 * struct sockaddr_in  (AF_INET):
 *   sin_family (2B), sin_port (2B), sin_addr (4B), sin_zero (8B)
 *
 * struct sockaddr_in6 (AF_INET6):
 *   sin6_family (2B), sin6_port (2B), sin6_flowinfo (4B),
 *   sin6_addr (16B), sin6_scope_id (4B)
 *
 * struct sockaddr_un  (AF_UNIX):
 *   sun_family (2B), sun_path (up to 108B)
 * --------------------------------------------------------------- */

/* Maximum formatted address string length */
#define NFS_ADDR_STR_MAX 128

/* ---- Build functions ---- */

/* Build a sockaddr_in from IPv4 dotted-decimal string and port.
 * Returns 0 on success, -1 on error. */
int nfs_addr_build_inet(const char *ip_str, uint16_t port, struct sockaddr_in *addr);

/* Build a sockaddr_in6 from IPv6 colon-hex string and port.
 * Returns 0 on success, -1 on error. */
int nfs_addr_build_inet6(const char *ip6_str, uint16_t port, struct sockaddr_in6 *addr);

/* Build a sockaddr_in6 with scope_id and flowinfo. */
int nfs_addr_build_inet6_full(const char *ip6_str, uint16_t port, uint32_t flowinfo,
                              uint32_t scope_id, struct sockaddr_in6 *addr);

/* Build a sockaddr_un from a path string.
 * Returns 0 on success, -1 on error (path too long). */
int nfs_addr_build_unix(const char *path, struct sockaddr_un *addr);

/* ---- Parse / format functions ---- */

/* Format a sockaddr_in to "IP:port" string. Returns `out`. */
char *nfs_addr_format_inet(const struct sockaddr_in *addr, char *out, size_t out_sz);

/* Format a sockaddr_in6 to "[IPv6]:port" string. Returns `out`. */
char *nfs_addr_format_inet6(const struct sockaddr_in6 *addr, char *out, size_t out_sz);

/* Format a sockaddr_un to its path. Returns `out`. */
char *nfs_addr_format_unix(const struct sockaddr_un *addr, char *out, size_t out_sz);

/* Format any sockaddr (generic). Returns `out`. */
char *nfs_addr_format(const struct sockaddr *sa, socklen_t sa_len, char *out, size_t out_sz);

/* ---- Comparison functions ---- */

/* Compare two sockaddr_in addresses (IP + port).
 * Returns 0 if equal, non-zero if different. */
int nfs_addr_cmp_inet(const struct sockaddr_in *a, const struct sockaddr_in *b);

/* Compare two sockaddr_in6 addresses (IP + port + scope).
 * Returns 0 if equal, non-zero if different. */
int nfs_addr_cmp_inet6(const struct sockaddr_in6 *a, const struct sockaddr_in6 *b);

/* Compare two sockaddr_un addresses (path).
 * Returns 0 if equal, non-zero if different. */
int nfs_addr_cmp_unix(const struct sockaddr_un *a, const struct sockaddr_un *b);

/* ---- Query functions ---- */

/* Get the address family from a generic sockaddr. */
sa_family_t nfs_addr_family(const struct sockaddr *sa);

/* Get the port from AF_INET or AF_INET6 sockaddr (host byte order).
 * Returns port, or -1 if not an inet family. */
int nfs_addr_get_port(const struct sockaddr *sa);

/* Check if an IPv4 address is loopback (127.0.0.0/8). */
int nfs_addr_is_loopback4(const struct sockaddr_in *addr);

/* Check if an IPv6 address is loopback (::1). */
int nfs_addr_is_loopback6(const struct sockaddr_in6 *addr);

/* Return the name of an address family. */
const char *nfs_addr_family_name(sa_family_t family);

#endif /* NFS_ADDR_H */
