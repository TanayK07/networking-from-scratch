/* Authoritative DNS server implementation. */

#include "dns_auth.h"
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>

/* ---------------------------------------------------------------
 * Name encoding / decoding (same as lesson 02, self-contained)
 * --------------------------------------------------------------- */

int nfs_dns_name_encode(const char *name, uint8_t *out, size_t out_sz) {
    if (!name || !out)
        return -1;

    if (name[0] == '\0' || (name[0] == '.' && name[1] == '\0')) {
        if (out_sz < 1)
            return -1;
        out[0] = 0;
        return 1;
    }

    size_t name_len = strlen(name);
    size_t pos = 0;
    size_t i = 0;

    while (i < name_len) {
        const char *dot = strchr(name + i, '.');
        size_t label_len = dot ? (size_t)(dot - (name + i)) : (name_len - i);

        if (label_len == 0) {
            i++;
            continue;
        }
        if (label_len > NFS_DNS_MAX_LABEL)
            return -1;
        if (pos + 1 + label_len >= out_sz)
            return -1;

        out[pos++] = (uint8_t)label_len;
        memcpy(out + pos, name + i, label_len);
        pos += label_len;
        i += label_len;
        if (dot)
            i++;
    }

    if (pos >= out_sz)
        return -1;
    out[pos++] = 0;
    return (int)pos;
}

int nfs_dns_name_decode(const uint8_t *data, size_t data_len, size_t offset, char *out,
                        size_t out_sz) {
    if (!data || !out || out_sz == 0 || offset >= data_len)
        return -1;

    size_t out_pos = 0;
    size_t cur = offset;
    int jumped = 0;
    int bytes_consumed = 0;
    int jumps = 0;

    while (cur < data_len) {
        uint8_t len = data[cur];

        if (len == 0) {
            if (!jumped)
                bytes_consumed = (int)(cur - offset + 1);
            break;
        }

        if ((len & 0xC0) == 0xC0) {
            if (cur + 1 >= data_len)
                return -1;
            if (!jumped)
                bytes_consumed = (int)(cur - offset + 2);
            uint16_t ptr = (uint16_t)(((len & 0x3F) << 8) | data[cur + 1]);
            if (ptr >= data_len)
                return -1;
            cur = ptr;
            jumped = 1;
            if (++jumps > 128)
                return -1;
            continue;
        }

        if ((len & 0xC0) != 0)
            return -1;
        cur++;
        if (cur + len > data_len)
            return -1;

        if (out_pos > 0) {
            if (out_pos >= out_sz - 1)
                return -1;
            out[out_pos++] = '.';
        }

        if (out_pos + len >= out_sz)
            return -1;
        memcpy(out + out_pos, data + cur, len);
        out_pos += len;
        cur += len;
    }

    if (out_pos >= out_sz)
        return -1;
    out[out_pos] = '\0';
    if (!jumped && bytes_consumed == 0)
        return -1;
    return bytes_consumed;
}

/* ---------------------------------------------------------------
 * Case-insensitive name comparison
 * --------------------------------------------------------------- */

static int name_eq(const char *a, const char *b) {
    return strcasecmp(a, b) == 0;
}

/* ---------------------------------------------------------------
 * Zone management
 * --------------------------------------------------------------- */

int nfs_dns_zone_init(struct nfs_dns_zone *zone, const char *origin) {
    if (!zone || !origin)
        return -1;
    memset(zone, 0, sizeof(*zone));
    size_t len = strlen(origin);
    if (len >= NFS_DNS_MAX_NAME)
        return -1;
    memcpy(zone->origin, origin, len + 1);
    return 0;
}

int nfs_dns_zone_add_rr(struct nfs_dns_zone *zone, const char *name, uint16_t type, uint16_t rclass,
                        uint32_t ttl, const uint8_t *rdata, uint16_t rdlength) {
    if (!zone || !name)
        return -1;
    if (zone->rr_count >= NFS_DNS_MAX_RR)
        return -1;
    if (rdlength > 0 && !rdata)
        return -1;
    if (rdlength > sizeof(((struct nfs_dns_rr *)0)->rdata))
        return -1;

    struct nfs_dns_rr *rr = &zone->rrs[zone->rr_count];
    size_t nlen = strlen(name);
    if (nlen >= NFS_DNS_MAX_NAME)
        return -1;
    memcpy(rr->name, name, nlen + 1);
    rr->type = type;
    rr->rclass = rclass;
    rr->ttl = ttl;
    rr->rdlength = rdlength;
    if (rdlength > 0) {
        memcpy(rr->rdata, rdata, rdlength);
    }

    zone->rr_count++;
    return 0;
}

int nfs_dns_zone_add_a(struct nfs_dns_zone *zone, const char *name, uint32_t ttl, uint8_t a,
                       uint8_t b, uint8_t c, uint8_t d) {
    uint8_t rdata[4] = {a, b, c, d};
    return nfs_dns_zone_add_rr(zone, name, NFS_DNS_TYPE_A, NFS_DNS_CLASS_IN, ttl, rdata, 4);
}

int nfs_dns_zone_add_ns(struct nfs_dns_zone *zone, const char *name, uint32_t ttl,
                        const char *nsname) {
    if (!nsname)
        return -1;
    uint8_t encoded[NFS_DNS_MAX_NAME];
    int elen = nfs_dns_name_encode(nsname, encoded, sizeof(encoded));
    if (elen < 0)
        return -1;
    return nfs_dns_zone_add_rr(zone, name, NFS_DNS_TYPE_NS, NFS_DNS_CLASS_IN, ttl, encoded,
                               (uint16_t)elen);
}

int nfs_dns_zone_add_soa(struct nfs_dns_zone *zone, const char *name, uint32_t ttl,
                         const struct nfs_dns_soa *soa) {
    if (!soa)
        return -1;

    uint8_t rdata[512];
    size_t pos = 0;

    /* MNAME */
    int n = nfs_dns_name_encode(soa->mname, rdata + pos, sizeof(rdata) - pos);
    if (n < 0)
        return -1;
    pos += (size_t)n;

    /* RNAME */
    n = nfs_dns_name_encode(soa->rname, rdata + pos, sizeof(rdata) - pos);
    if (n < 0)
        return -1;
    pos += (size_t)n;

    /* Serial, Refresh, Retry, Expire, Minimum (5 x 4 bytes) */
    if (pos + 20 > sizeof(rdata))
        return -1;
    uint32_t vals[5] = {htonl(soa->serial), htonl(soa->refresh), htonl(soa->retry),
                        htonl(soa->expire), htonl(soa->minimum)};
    memcpy(rdata + pos, vals, 20);
    pos += 20;

    return nfs_dns_zone_add_rr(zone, name, NFS_DNS_TYPE_SOA, NFS_DNS_CLASS_IN, ttl, rdata,
                               (uint16_t)pos);
}

/* ---------------------------------------------------------------
 * Lookup
 * --------------------------------------------------------------- */

int nfs_dns_zone_lookup(const struct nfs_dns_zone *zone, const char *qname, uint16_t qtype,
                        struct nfs_dns_lookup_result *result) {
    if (!zone || !qname || !result)
        return -1;

    memset(result, 0, sizeof(*result));

    for (uint16_t i = 0; i < zone->rr_count; i++) {
        const struct nfs_dns_rr *rr = &zone->rrs[i];
        if (name_eq(rr->name, qname)) {
            if (qtype == 255 || rr->type == qtype) {
                if (result->count < NFS_DNS_MAX_RR) {
                    result->matches[result->count++] = rr;
                }
            }
        }
    }

    return (int)result->count;
}

/* ---------------------------------------------------------------
 * Helper: write a RR to wire format
 * --------------------------------------------------------------- */

static int write_rr_wire(const struct nfs_dns_rr *rr, uint8_t *out, size_t out_sz) {
    size_t pos = 0;

    /* Name */
    int nlen = nfs_dns_name_encode(rr->name, out + pos, out_sz - pos);
    if (nlen < 0)
        return -1;
    pos += (size_t)nlen;

    /* TYPE(2) + CLASS(2) + TTL(4) + RDLENGTH(2) + RDATA */
    if (pos + 10 + rr->rdlength > out_sz)
        return -1;

    uint16_t tmp;
    tmp = htons(rr->type);
    memcpy(out + pos, &tmp, 2);
    pos += 2;
    tmp = htons(rr->rclass);
    memcpy(out + pos, &tmp, 2);
    pos += 2;
    uint32_t t = htonl(rr->ttl);
    memcpy(out + pos, &t, 4);
    pos += 4;
    tmp = htons(rr->rdlength);
    memcpy(out + pos, &tmp, 2);
    pos += 2;
    if (rr->rdlength > 0) {
        memcpy(out + pos, rr->rdata, rr->rdlength);
        pos += rr->rdlength;
    }

    return (int)pos;
}

/* ---------------------------------------------------------------
 * Response building
 * --------------------------------------------------------------- */

int nfs_dns_zone_build_response(const struct nfs_dns_zone *zone, const uint8_t *query,
                                size_t query_len, uint8_t *out, size_t out_sz) {
    if (!zone || !query || !out)
        return -1;
    if (query_len < NFS_DNS_HEADER_SIZE)
        return -1;
    if (out_sz < NFS_DNS_HEADER_SIZE)
        return -1;

    /* Parse query header */
    struct nfs_dns_hdr qhdr;
    memcpy(&qhdr, query, NFS_DNS_HEADER_SIZE);

    uint16_t qdcount = ntohs(qhdr.qdcount);
    if (qdcount == 0)
        return -1;

    /* Parse first question */
    char qname[NFS_DNS_MAX_NAME];
    int consumed = nfs_dns_name_decode(query, query_len, NFS_DNS_HEADER_SIZE, qname, sizeof(qname));
    if (consumed < 0)
        return -1;

    size_t qoff = NFS_DNS_HEADER_SIZE + (size_t)consumed;
    if (qoff + 4 > query_len)
        return -1;

    uint16_t qtype, qclass;
    memcpy(&qtype, query + qoff, 2);
    memcpy(&qclass, query + qoff + 2, 2);
    qtype = ntohs(qtype);
    qclass = ntohs(qclass);

    /* Lookup answers */
    struct nfs_dns_lookup_result answers;
    nfs_dns_zone_lookup(zone, qname, qtype, &answers);

    /* Lookup NS records for authority section */
    struct nfs_dns_lookup_result authority;
    nfs_dns_zone_lookup(zone, zone->origin, NFS_DNS_TYPE_NS, &authority);

    /* Determine RCODE */
    uint8_t rcode = NFS_DNS_RCODE_NOERROR;
    if (answers.count == 0) {
        /* Check if name exists at all */
        struct nfs_dns_lookup_result any;
        nfs_dns_zone_lookup(zone, qname, 255, &any);
        if (any.count == 0) {
            rcode = NFS_DNS_RCODE_NXDOMAIN;
        }
    }

    /* Build response header */
    struct nfs_dns_hdr rhdr;
    memset(&rhdr, 0, sizeof(rhdr));
    rhdr.id = qhdr.id;
    /* QR=1, AA=1, RD copied from query */
    uint16_t qflags = ntohs(qhdr.flags);
    uint16_t rflags = 0x8400; /* QR=1, AA=1 */
    if (qflags & 0x0100)
        rflags |= 0x0100; /* RD */
    rflags |= rcode;
    rhdr.flags = htons(rflags);
    rhdr.qdcount = htons(1);
    rhdr.ancount = htons(answers.count);
    rhdr.nscount = htons(authority.count);

    memcpy(out, &rhdr, NFS_DNS_HEADER_SIZE);
    size_t pos = NFS_DNS_HEADER_SIZE;

    /* Copy question section */
    int nlen = nfs_dns_name_encode(qname, out + pos, out_sz - pos);
    if (nlen < 0)
        return -1;
    pos += (size_t)nlen;

    if (pos + 4 > out_sz)
        return -1;
    uint16_t tmp;
    tmp = htons(qtype);
    memcpy(out + pos, &tmp, 2);
    pos += 2;
    tmp = htons(qclass);
    memcpy(out + pos, &tmp, 2);
    pos += 2;

    /* Write answer RRs */
    for (uint16_t i = 0; i < answers.count; i++) {
        int rlen = write_rr_wire(answers.matches[i], out + pos, out_sz - pos);
        if (rlen < 0)
            return -1;
        pos += (size_t)rlen;
    }

    /* Write authority (NS) RRs */
    for (uint16_t i = 0; i < authority.count; i++) {
        int rlen = write_rr_wire(authority.matches[i], out + pos, out_sz - pos);
        if (rlen < 0)
            return -1;
        pos += (size_t)rlen;
    }

    return (int)pos;
}
