/*
 * ndp.c -- NDP (Neighbor Discovery Protocol) implementation
 */
#include "ndp.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

/* ---- Link-Layer Address option builder ---- */

static int build_lla_option(uint8_t type, const uint8_t *mac, uint8_t *out, size_t out_sz) {
    /* Option: type(1) + length(1) + MAC(6) = 8 bytes, length field = 1 (units of 8) */
    if (out_sz < 8)
        return -1;
    out[0] = type;
    out[1] = 1; /* 1 * 8 = 8 bytes */
    memcpy(out + 2, mac, 6);
    return 8;
}

/* ---- Build functions ---- */

int nfs_ndp_build_ns(const uint8_t *target, const uint8_t *src_mac, uint8_t *out, size_t out_sz) {
    if (!target || !out)
        return -1;

    size_t needed = sizeof(struct nfs_ndp_ns);
    if (src_mac)
        needed += 8;
    if (out_sz < needed)
        return -1;

    struct nfs_ndp_ns *ns = (struct nfs_ndp_ns *)out;
    memset(ns, 0, sizeof(*ns));
    ns->type = NFS_NDP_NS;
    ns->code = 0;
    ns->checksum = 0; /* Would be computed with pseudo-header in real stack */
    ns->reserved = 0;
    memcpy(ns->target, target, 16);

    size_t pos = sizeof(struct nfs_ndp_ns);

    if (src_mac) {
        int opt_len = build_lla_option(NFS_NDP_OPT_SRC_LLA, src_mac, out + pos, out_sz - pos);
        if (opt_len < 0)
            return -1;
        pos += (size_t)opt_len;
    }

    return (int)pos;
}

int nfs_ndp_build_na(const uint8_t *target, uint32_t flags, const uint8_t *tgt_mac, uint8_t *out,
                     size_t out_sz) {
    if (!target || !out)
        return -1;

    size_t needed = sizeof(struct nfs_ndp_na);
    if (tgt_mac)
        needed += 8;
    if (out_sz < needed)
        return -1;

    struct nfs_ndp_na *na = (struct nfs_ndp_na *)out;
    memset(na, 0, sizeof(*na));
    na->type = NFS_NDP_NA;
    na->code = 0;
    na->checksum = 0;
    na->flags = htonl(flags);
    memcpy(na->target, target, 16);

    size_t pos = sizeof(struct nfs_ndp_na);

    if (tgt_mac) {
        int opt_len = build_lla_option(NFS_NDP_OPT_TGT_LLA, tgt_mac, out + pos, out_sz - pos);
        if (opt_len < 0)
            return -1;
        pos += (size_t)opt_len;
    }

    return (int)pos;
}

int nfs_ndp_build_rs(const uint8_t *src_mac, uint8_t *out, size_t out_sz) {
    if (!out)
        return -1;

    size_t needed = sizeof(struct nfs_ndp_rs);
    if (src_mac)
        needed += 8;
    if (out_sz < needed)
        return -1;

    struct nfs_ndp_rs *rs = (struct nfs_ndp_rs *)out;
    memset(rs, 0, sizeof(*rs));
    rs->type = NFS_NDP_RS;
    rs->code = 0;
    rs->checksum = 0;
    rs->reserved = 0;

    size_t pos = sizeof(struct nfs_ndp_rs);

    if (src_mac) {
        int opt_len = build_lla_option(NFS_NDP_OPT_SRC_LLA, src_mac, out + pos, out_sz - pos);
        if (opt_len < 0)
            return -1;
        pos += (size_t)opt_len;
    }

    return (int)pos;
}

int nfs_ndp_build_ra(uint8_t cur_hop_limit, uint8_t flags, uint16_t router_lifetime,
                     uint32_t reachable_time, uint32_t retrans_timer, const uint8_t *src_mac,
                     uint8_t *out, size_t out_sz) {
    if (!out)
        return -1;

    size_t needed = sizeof(struct nfs_ndp_ra);
    if (src_mac)
        needed += 8;
    if (out_sz < needed)
        return -1;

    struct nfs_ndp_ra *ra = (struct nfs_ndp_ra *)out;
    memset(ra, 0, sizeof(*ra));
    ra->type = NFS_NDP_RA;
    ra->code = 0;
    ra->checksum = 0;
    ra->cur_hop_limit = cur_hop_limit;
    ra->flags = flags;
    ra->router_lifetime = htons(router_lifetime);
    ra->reachable_time = htonl(reachable_time);
    ra->retrans_timer = htonl(retrans_timer);

    size_t pos = sizeof(struct nfs_ndp_ra);

    if (src_mac) {
        int opt_len = build_lla_option(NFS_NDP_OPT_SRC_LLA, src_mac, out + pos, out_sz - pos);
        if (opt_len < 0)
            return -1;
        pos += (size_t)opt_len;
    }

    return (int)pos;
}

/* ---- Parse functions ---- */

int nfs_ndp_parse_type(const uint8_t *data, size_t len) {
    if (!data || len < 4)
        return -1;

    uint8_t type = data[0];
    if (type < NFS_NDP_RS || type > NFS_NDP_REDIRECT)
        return -1;

    /* Validate minimum lengths */
    size_t min_len = 0;
    switch (type) {
    case NFS_NDP_RS:
        min_len = sizeof(struct nfs_ndp_rs);
        break;
    case NFS_NDP_RA:
        min_len = sizeof(struct nfs_ndp_ra);
        break;
    case NFS_NDP_NS:
        min_len = sizeof(struct nfs_ndp_ns);
        break;
    case NFS_NDP_NA:
        min_len = sizeof(struct nfs_ndp_na);
        break;
    case NFS_NDP_REDIRECT:
        min_len = 40;
        break; /* minimum redirect size */
    default:
        return -1;
    }

    if (len < min_len)
        return -1;

    /* Code must be 0 for all NDP messages */
    if (data[1] != 0)
        return -1;

    return (int)type;
}

int nfs_ndp_parse_ns(const uint8_t *data, size_t len, struct nfs_ndp_ns *ns) {
    if (!data || !ns)
        return -1;
    if (len < sizeof(struct nfs_ndp_ns))
        return -1;
    if (data[0] != NFS_NDP_NS || data[1] != 0)
        return -1;

    memcpy(ns, data, sizeof(*ns));
    ns->checksum = ntohs(ns->checksum);
    return 0;
}

int nfs_ndp_parse_na(const uint8_t *data, size_t len, struct nfs_ndp_na *na) {
    if (!data || !na)
        return -1;
    if (len < sizeof(struct nfs_ndp_na))
        return -1;
    if (data[0] != NFS_NDP_NA || data[1] != 0)
        return -1;

    memcpy(na, data, sizeof(*na));
    na->checksum = ntohs(na->checksum);
    na->flags = ntohl(na->flags);
    return 0;
}

int nfs_ndp_parse_rs(const uint8_t *data, size_t len, struct nfs_ndp_rs *rs) {
    if (!data || !rs)
        return -1;
    if (len < sizeof(struct nfs_ndp_rs))
        return -1;
    if (data[0] != NFS_NDP_RS || data[1] != 0)
        return -1;

    memcpy(rs, data, sizeof(*rs));
    rs->checksum = ntohs(rs->checksum);
    return 0;
}

int nfs_ndp_parse_ra(const uint8_t *data, size_t len, struct nfs_ndp_ra *ra) {
    if (!data || !ra)
        return -1;
    if (len < sizeof(struct nfs_ndp_ra))
        return -1;
    if (data[0] != NFS_NDP_RA || data[1] != 0)
        return -1;

    memcpy(ra, data, sizeof(*ra));
    ra->checksum = ntohs(ra->checksum);
    ra->router_lifetime = ntohs(ra->router_lifetime);
    ra->reachable_time = ntohl(ra->reachable_time);
    ra->retrans_timer = ntohl(ra->retrans_timer);
    return 0;
}

int nfs_ndp_option_next(const uint8_t *opt_data, size_t opt_len, size_t *offset,
                        struct nfs_ndp_parsed_option *opt) {
    if (!opt_data || !offset || !opt)
        return -1;
    if (*offset + 2 > opt_len)
        return -1;

    uint8_t type = opt_data[*offset];
    uint8_t length = opt_data[*offset + 1];

    /* Length must be > 0 (in units of 8 bytes) */
    if (length == 0)
        return -1;

    size_t total = (size_t)length * 8;
    if (*offset + total > opt_len)
        return -1;

    opt->type = type;
    opt->total_len = (uint8_t)total;
    opt->data = opt_data + *offset + 2;
    opt->data_len = (uint16_t)(total - 2);

    *offset += total;
    return 0;
}

char *nfs_ndp_format_ipv6(const uint8_t *addr, char *out, size_t out_sz) {
    if (!addr || !out || out_sz < 40) {
        if (out && out_sz > 0)
            out[0] = '\0';
        return out;
    }

    snprintf(out, out_sz,
             "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
             "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
             addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7], addr[8],
             addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15]);
    return out;
}
