#ifndef NFS_DNS_AUTH_H
#define NFS_DNS_AUTH_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Authoritative DNS server.
 *
 * Manages zone data: a collection of resource records (RRs).
 * Supports lookup by QNAME + QTYPE, building DNS response
 * messages with answer and authority sections, and SOA records.
 * --------------------------------------------------------------- */

/* DNS record types */
#define NFS_DNS_TYPE_A     1
#define NFS_DNS_TYPE_NS    2
#define NFS_DNS_TYPE_CNAME 5
#define NFS_DNS_TYPE_SOA   6
#define NFS_DNS_TYPE_PTR   12
#define NFS_DNS_TYPE_MX    15
#define NFS_DNS_TYPE_TXT   16
#define NFS_DNS_TYPE_AAAA  28

/* DNS classes */
#define NFS_DNS_CLASS_IN 1

/* RCODE values */
#define NFS_DNS_RCODE_NOERROR  0
#define NFS_DNS_RCODE_FORMERR  1
#define NFS_DNS_RCODE_SERVFAIL 2
#define NFS_DNS_RCODE_NXDOMAIN 3

/* Limits */
#define NFS_DNS_MAX_NAME    255
#define NFS_DNS_MAX_LABEL   63
#define NFS_DNS_MAX_RR      128
#define NFS_DNS_HEADER_SIZE 12
#define NFS_DNS_MAX_MSG     512

/* SOA record data (parsed form) */
struct nfs_dns_soa {
    char mname[NFS_DNS_MAX_NAME]; /* primary NS */
    char rname[NFS_DNS_MAX_NAME]; /* admin email (dotted) */
    uint32_t serial;
    uint32_t refresh;
    uint32_t retry;
    uint32_t expire;
    uint32_t minimum;
};

/* Resource record */
struct nfs_dns_rr {
    char name[NFS_DNS_MAX_NAME];
    uint16_t type;
    uint16_t rclass;
    uint32_t ttl;
    uint16_t rdlength;
    uint8_t rdata[512];
};

/* Zone data */
struct nfs_dns_zone {
    char origin[NFS_DNS_MAX_NAME]; /* zone apex, e.g. "example.com" */
    struct nfs_dns_rr rrs[NFS_DNS_MAX_RR];
    uint16_t rr_count;
};

/* DNS header (wire format, network byte order) */
struct __attribute__((packed)) nfs_dns_hdr {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

_Static_assert(sizeof(struct nfs_dns_hdr) == 12, "DNS header must be 12 bytes");

/* Lookup result */
struct nfs_dns_lookup_result {
    const struct nfs_dns_rr *matches[NFS_DNS_MAX_RR];
    uint16_t count;
};

/* --- Zone management -------------------------------------------- */

/* Initialize a zone with the given origin (e.g. "example.com").
 * Returns 0 on success, -1 on error. */
int nfs_dns_zone_init(struct nfs_dns_zone *zone, const char *origin);

/* Add a resource record to the zone.
 * Returns 0 on success, -1 if zone is full or input invalid. */
int nfs_dns_zone_add_rr(struct nfs_dns_zone *zone, const char *name, uint16_t type, uint16_t rclass,
                        uint32_t ttl, const uint8_t *rdata, uint16_t rdlength);

/* Convenience: add an A record (4-byte IPv4 address). */
int nfs_dns_zone_add_a(struct nfs_dns_zone *zone, const char *name, uint32_t ttl, uint8_t a,
                       uint8_t b, uint8_t c, uint8_t d);

/* Convenience: add an NS record (nameserver name). */
int nfs_dns_zone_add_ns(struct nfs_dns_zone *zone, const char *name, uint32_t ttl,
                        const char *nsname);

/* Convenience: add a SOA record. */
int nfs_dns_zone_add_soa(struct nfs_dns_zone *zone, const char *name, uint32_t ttl,
                         const struct nfs_dns_soa *soa);

/* --- Lookup ------------------------------------------------------ */

/* Find all RRs matching the given QNAME + QTYPE.
 * Type 255 (ANY) matches all types for the name.
 * Returns the number of matches found. */
int nfs_dns_zone_lookup(const struct nfs_dns_zone *zone, const char *qname, uint16_t qtype,
                        struct nfs_dns_lookup_result *result);

/* --- Name encoding/decoding (needed for wire format) ------------ */

int nfs_dns_name_encode(const char *name, uint8_t *out, size_t out_sz);

int nfs_dns_name_decode(const uint8_t *data, size_t data_len, size_t offset, char *out,
                        size_t out_sz);

/* --- Response building ------------------------------------------ */

/* Build a complete DNS response message for a given query.
 * `query` is the raw query bytes, `query_len` its length.
 * Writes the response to `out` (up to `out_sz` bytes).
 * Returns the response length, or -1 on error. */
int nfs_dns_zone_build_response(const struct nfs_dns_zone *zone, const uint8_t *query,
                                size_t query_len, uint8_t *out, size_t out_sz);

#endif /* NFS_DNS_AUTH_H */
