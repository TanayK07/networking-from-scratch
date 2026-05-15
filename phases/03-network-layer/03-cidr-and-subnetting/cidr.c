#include "cidr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int nfs_ip_parse(const char *str, uint32_t *out)
{
    if (!str || !out)
        return -1;

    unsigned int a, b, c, d;
    char trail;
    int n = sscanf(str, "%u.%u.%u.%u%c", &a, &b, &c, &d, &trail);
    if (n != 4)
        return -1;
    if (a > 255 || b > 255 || c > 255 || d > 255)
        return -1;

    *out = (a << 24) | (b << 16) | (c << 8) | d;
    return 0;
}

void nfs_ip_format(uint32_t ip, char *buf, size_t sz)
{
    snprintf(buf, sz, "%u.%u.%u.%u",
             (ip >> 24) & 0xFF,
             (ip >> 16) & 0xFF,
             (ip >> 8)  & 0xFF,
              ip        & 0xFF);
}

int nfs_cidr_parse(const char *str, struct nfs_cidr *out)
{
    if (!str || !out)
        return -1;

    /* Find the '/' separator. */
    const char *slash = strchr(str, '/');
    if (!slash)
        return -1;

    /* Parse IP portion. */
    size_t ip_len = (size_t)(slash - str);
    if (ip_len >= 16)
        return -1;

    char ip_str[16];
    memcpy(ip_str, str, ip_len);
    ip_str[ip_len] = '\0';

    if (nfs_ip_parse(ip_str, &out->addr) != 0)
        return -1;

    /* Parse prefix length. */
    char *end;
    long prefix = strtol(slash + 1, &end, 10);
    if (end == slash + 1 || *end != '\0')
        return -1;
    if (prefix < 0 || prefix > 32)
        return -1;

    out->prefix_len = (uint8_t)prefix;
    return 0;
}

uint32_t nfs_cidr_mask(uint8_t prefix_len)
{
    if (prefix_len == 0)
        return 0;
    return 0xFFFFFFFFU << (32 - prefix_len);
}

uint32_t nfs_cidr_network(const struct nfs_cidr *c)
{
    return c->addr & nfs_cidr_mask(c->prefix_len);
}

uint32_t nfs_cidr_broadcast(const struct nfs_cidr *c)
{
    uint32_t mask = nfs_cidr_mask(c->prefix_len);
    return nfs_cidr_network(c) | ~mask;
}

uint32_t nfs_cidr_host_count(uint8_t prefix_len)
{
    if (prefix_len >= 32)
        return 1;
    if (prefix_len == 31)
        return 2;
    if (prefix_len == 0)
        return 0xFFFFFFFEU;

    uint32_t total = 1U << (32 - prefix_len);
    return total - 2;
}

int nfs_cidr_contains(const struct nfs_cidr *net, uint32_t ip)
{
    uint32_t mask = nfs_cidr_mask(net->prefix_len);
    return (ip & mask) == (net->addr & mask);
}

int nfs_cidr_overlap(const struct nfs_cidr *a, const struct nfs_cidr *b)
{
    /* Two CIDRs overlap if and only if one contains the network address
     * of the other (using the shorter prefix). Equivalently, use the
     * shorter prefix to compare both network addresses. */
    uint8_t shorter = a->prefix_len < b->prefix_len ? a->prefix_len : b->prefix_len;
    uint32_t mask = nfs_cidr_mask(shorter);
    return (a->addr & mask) == (b->addr & mask);
}

void nfs_cidr_format(const struct nfs_cidr *c, char *buf, size_t sz)
{
    char ip_buf[16];
    nfs_ip_format(c->addr, ip_buf, sizeof(ip_buf));
    snprintf(buf, sz, "%s/%u", ip_buf, c->prefix_len);
}
