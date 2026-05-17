#include "bsock.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Parse a dotted-quad IPv4 string into a network-order address.
 * --------------------------------------------------------------- */

int nfs_inet_aton(const char *ip_str, uint32_t *addr) {
    if (!ip_str || !addr)
        return -1;

    unsigned int octets[4];
    int consumed = 0;
    int n =
        sscanf(ip_str, "%u.%u.%u.%u%n", &octets[0], &octets[1], &octets[2], &octets[3], &consumed);

    if (n != 4 || ip_str[consumed] != '\0')
        return -1;

    for (int i = 0; i < 4; i++) {
        if (octets[i] > 255)
            return -1;
    }

    /* Store in network byte order (big-endian) */
    *addr = htonl((octets[0] << 24) | (octets[1] << 16) | (octets[2] << 8) | octets[3]);
    return 0;
}

/* ---------------------------------------------------------------
 * Format a network-order address into a dotted-quad string.
 * --------------------------------------------------------------- */

int nfs_inet_ntoa(uint32_t addr, char *buf, size_t buf_sz) {
    if (!buf || buf_sz < 16)
        return -1;

    uint32_t host = ntohl(addr);
    int n = snprintf(buf, buf_sz, "%u.%u.%u.%u", (host >> 24) & 0xFF, (host >> 16) & 0xFF,
                     (host >> 8) & 0xFF, host & 0xFF);
    if (n < 0 || (size_t)n >= buf_sz)
        return -1;

    return 0;
}

/* ---------------------------------------------------------------
 * Build a sockaddr_in from an IP string and port.
 * --------------------------------------------------------------- */

int nfs_sockaddr_in_build(struct nfs_sockaddr_in *sa, const char *ip_str, uint16_t port) {
    if (!sa || !ip_str)
        return -1;

    memset(sa, 0, sizeof(*sa));
    sa->sin_family = NFS_AF_INET;
    sa->sin_port = htons(port);

    uint32_t addr;
    if (nfs_inet_aton(ip_str, &addr) < 0)
        return -1;
    sa->sin_addr = addr;

    return 0;
}

/* ---------------------------------------------------------------
 * Parse a sockaddr_in into IP string and port.
 * --------------------------------------------------------------- */

int nfs_sockaddr_in_parse(const struct nfs_sockaddr_in *sa, char *ip_buf, size_t ip_buf_sz,
                          uint16_t *port) {
    if (!sa || !ip_buf || !port)
        return -1;

    if (sa->sin_family != NFS_AF_INET)
        return -1;

    if (nfs_inet_ntoa(sa->sin_addr, ip_buf, ip_buf_sz) < 0)
        return -1;

    *port = ntohs(sa->sin_port);
    return 0;
}

/* ---------------------------------------------------------------
 * Format sockaddr_in as "ip:port".
 * --------------------------------------------------------------- */

int nfs_sockaddr_format(const struct nfs_sockaddr_in *sa, char *out, size_t out_sz) {
    if (!sa || !out || out_sz == 0)
        return -1;

    char ip[16];
    uint16_t port;

    if (nfs_sockaddr_in_parse(sa, ip, sizeof(ip), &port) < 0)
        return -1;

    int n = snprintf(out, out_sz, "%s:%u", ip, port);
    if (n < 0 || (size_t)n >= out_sz)
        return -1;

    return n;
}

/* ---------------------------------------------------------------
 * Accessor helpers
 * --------------------------------------------------------------- */

uint16_t nfs_sockaddr_port(const struct nfs_sockaddr_in *sa) {
    if (!sa)
        return 0;
    return ntohs(sa->sin_port);
}

uint32_t nfs_sockaddr_addr(const struct nfs_sockaddr_in *sa) {
    if (!sa)
        return 0;
    return sa->sin_addr;
}
