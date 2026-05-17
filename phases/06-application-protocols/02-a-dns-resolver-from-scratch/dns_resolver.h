#ifndef NFS_DNS_RESOLVER_H
#define NFS_DNS_RESOLVER_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * DNS Resolver from scratch.
 *
 * Builds DNS query messages (header + question section) and parses
 * DNS response messages (header + answer section with resource
 * records).  Handles QNAME encoding/decoding with length-prefixed
 * labels.
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
#define NFS_DNS_RCODE_NOTIMP   4
#define NFS_DNS_RCODE_REFUSED  5

/* Max sizes */
#define NFS_DNS_MAX_NAME    255
#define NFS_DNS_MAX_LABEL   63
#define NFS_DNS_MAX_MSG     512 /* traditional UDP limit */
#define NFS_DNS_HEADER_SIZE 12
#define NFS_DNS_MAX_RR      32

/* DNS header: 12 bytes on the wire (network byte order). */
struct __attribute__((packed)) nfs_dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
};

_Static_assert(sizeof(struct nfs_dns_header) == 12, "DNS header must be 12 bytes");

/* Parsed resource record */
struct nfs_dns_rr {
    char name[NFS_DNS_MAX_NAME];
    uint16_t type;
    uint16_t rclass;
    uint32_t ttl;
    uint16_t rdlength;
    uint8_t rdata[512];
};

/* Parsed DNS response */
struct nfs_dns_response {
    struct nfs_dns_header header;
    char qname[NFS_DNS_MAX_NAME];
    uint16_t qtype;
    uint16_t qclass;
    uint16_t ancount;
    struct nfs_dns_rr answers[NFS_DNS_MAX_RR];
};

/* --- Name encoding / decoding ----------------------------------- */

/* Encode a dotted name (e.g. "www.google.com") into DNS wire format
 * (length-prefixed labels + 0x00 terminator).
 * Returns the number of bytes written, or -1 on error. */
int nfs_dns_name_encode(const char *name, uint8_t *out, size_t out_sz);

/* Decode a DNS name from wire format starting at `offset` within
 * `data` (total `data_len` bytes).  Handles compression pointers.
 * Writes dotted name to `out`.
 * Returns the number of bytes consumed from data at offset, or -1. */
int nfs_dns_name_decode(const uint8_t *data, size_t data_len, size_t offset, char *out,
                        size_t out_sz);

/* --- Query building --------------------------------------------- */

/* Build a complete DNS query message (header + question section).
 * Returns the total length written to `out`, or -1 on error. */
int nfs_dns_query_build(const char *name, uint16_t qtype, uint16_t id, uint8_t *out, size_t out_sz);

/* --- Response parsing ------------------------------------------- */

/* Parse a complete DNS response from raw bytes.
 * Fills in the nfs_dns_response struct.
 * Returns 0 on success, -1 on error. */
int nfs_dns_response_parse(const uint8_t *data, size_t len, struct nfs_dns_response *resp);

/* --- Flag helpers ----------------------------------------------- */

int nfs_dns_is_response(const struct nfs_dns_header *h);
int nfs_dns_is_query(const struct nfs_dns_header *h);
uint8_t nfs_dns_rcode(const struct nfs_dns_header *h);

/* Returns the string name for a given RCODE. */
const char *nfs_dns_rcode_str(uint8_t rcode);

/* Returns the string name for a given RR type. */
const char *nfs_dns_type_str(uint16_t type);

#endif /* NFS_DNS_RESOLVER_H */
