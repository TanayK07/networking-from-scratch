/*
 * addr.c -- Address families implementation
 */
#include "addr.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

/* ---- Build functions ---- */

int nfs_addr_build_inet(const char *ip_str, uint16_t port, struct sockaddr_in *addr) {
    if (!ip_str || !addr)
        return -1;

    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);

    if (inet_pton(AF_INET, ip_str, &addr->sin_addr) != 1)
        return -1;

    return 0;
}

int nfs_addr_build_inet6(const char *ip6_str, uint16_t port, struct sockaddr_in6 *addr) {
    return nfs_addr_build_inet6_full(ip6_str, port, 0, 0, addr);
}

int nfs_addr_build_inet6_full(const char *ip6_str, uint16_t port, uint32_t flowinfo,
                              uint32_t scope_id, struct sockaddr_in6 *addr) {
    if (!ip6_str || !addr)
        return -1;

    memset(addr, 0, sizeof(*addr));
    addr->sin6_family = AF_INET6;
    addr->sin6_port = htons(port);
    addr->sin6_flowinfo = htonl(flowinfo);
    addr->sin6_scope_id = scope_id; /* scope_id is host byte order */

    if (inet_pton(AF_INET6, ip6_str, &addr->sin6_addr) != 1)
        return -1;

    return 0;
}

int nfs_addr_build_unix(const char *path, struct sockaddr_un *addr) {
    if (!path || !addr)
        return -1;

    size_t path_len = strlen(path);
    if (path_len >= sizeof(addr->sun_path))
        return -1;

    memset(addr, 0, sizeof(*addr));
    addr->sun_family = AF_UNIX;
    memcpy(addr->sun_path, path, path_len);
    addr->sun_path[path_len] = '\0';

    return 0;
}

/* ---- Format functions ---- */

char *nfs_addr_format_inet(const struct sockaddr_in *addr, char *out, size_t out_sz) {
    if (!addr || !out || out_sz == 0) {
        if (out && out_sz > 0)
            out[0] = '\0';
        return out;
    }

    char ip[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip))) {
        out[0] = '\0';
        return out;
    }

    snprintf(out, out_sz, "%s:%u", ip, ntohs(addr->sin_port));
    return out;
}

char *nfs_addr_format_inet6(const struct sockaddr_in6 *addr, char *out, size_t out_sz) {
    if (!addr || !out || out_sz == 0) {
        if (out && out_sz > 0)
            out[0] = '\0';
        return out;
    }

    char ip[INET6_ADDRSTRLEN];
    if (!inet_ntop(AF_INET6, &addr->sin6_addr, ip, sizeof(ip))) {
        out[0] = '\0';
        return out;
    }

    snprintf(out, out_sz, "[%s]:%u", ip, ntohs(addr->sin6_port));
    return out;
}

char *nfs_addr_format_unix(const struct sockaddr_un *addr, char *out, size_t out_sz) {
    if (!addr || !out || out_sz == 0) {
        if (out && out_sz > 0)
            out[0] = '\0';
        return out;
    }

    snprintf(out, out_sz, "%s", addr->sun_path);
    return out;
}

char *nfs_addr_format(const struct sockaddr *sa, socklen_t sa_len, char *out, size_t out_sz) {
    if (!sa || !out || out_sz == 0) {
        if (out && out_sz > 0)
            out[0] = '\0';
        return out;
    }

    switch (sa->sa_family) {
    case AF_INET:
        if (sa_len >= sizeof(struct sockaddr_in))
            return nfs_addr_format_inet((const struct sockaddr_in *)sa, out, out_sz);
        break;
    case AF_INET6:
        if (sa_len >= sizeof(struct sockaddr_in6))
            return nfs_addr_format_inet6((const struct sockaddr_in6 *)sa, out, out_sz);
        break;
    case AF_UNIX:
        if (sa_len >= sizeof(sa_family_t))
            return nfs_addr_format_unix((const struct sockaddr_un *)sa, out, out_sz);
        break;
    default:
        break;
    }

    snprintf(out, out_sz, "(unknown family %d)", sa->sa_family);
    return out;
}

/* ---- Comparison functions ---- */

int nfs_addr_cmp_inet(const struct sockaddr_in *a, const struct sockaddr_in *b) {
    if (!a || !b)
        return -1;
    if (a->sin_port != b->sin_port)
        return 1;
    if (a->sin_addr.s_addr != b->sin_addr.s_addr)
        return 1;
    return 0;
}

int nfs_addr_cmp_inet6(const struct sockaddr_in6 *a, const struct sockaddr_in6 *b) {
    if (!a || !b)
        return -1;
    if (a->sin6_port != b->sin6_port)
        return 1;
    if (memcmp(&a->sin6_addr, &b->sin6_addr, 16) != 0)
        return 1;
    if (a->sin6_scope_id != b->sin6_scope_id)
        return 1;
    return 0;
}

int nfs_addr_cmp_unix(const struct sockaddr_un *a, const struct sockaddr_un *b) {
    if (!a || !b)
        return -1;
    return strcmp(a->sun_path, b->sun_path);
}

/* ---- Query functions ---- */

sa_family_t nfs_addr_family(const struct sockaddr *sa) {
    if (!sa)
        return AF_UNSPEC;
    return sa->sa_family;
}

int nfs_addr_get_port(const struct sockaddr *sa) {
    if (!sa)
        return -1;
    switch (sa->sa_family) {
    case AF_INET:
        return ntohs(((const struct sockaddr_in *)sa)->sin_port);
    case AF_INET6:
        return ntohs(((const struct sockaddr_in6 *)sa)->sin6_port);
    default:
        return -1;
    }
}

int nfs_addr_is_loopback4(const struct sockaddr_in *addr) {
    if (!addr)
        return 0;
    /* 127.0.0.0/8: first byte is 127 */
    uint32_t ip = ntohl(addr->sin_addr.s_addr);
    return ((ip >> 24) == 127) ? 1 : 0;
}

int nfs_addr_is_loopback6(const struct sockaddr_in6 *addr) {
    if (!addr)
        return 0;
    /* ::1 */
    static const uint8_t lo6[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    return (memcmp(&addr->sin6_addr, lo6, 16) == 0) ? 1 : 0;
}

const char *nfs_addr_family_name(sa_family_t family) {
    switch (family) {
    case AF_INET:
        return "AF_INET";
    case AF_INET6:
        return "AF_INET6";
    case AF_UNIX:
        return "AF_UNIX";
    case AF_UNSPEC:
        return "AF_UNSPEC";
    default:
        return "UNKNOWN";
    }
}
