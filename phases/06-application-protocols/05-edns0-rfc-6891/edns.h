#ifndef NFS_EDNS_H
#define NFS_EDNS_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * EDNS(0) -- Extension Mechanisms for DNS (RFC 6891).
 *
 * OPT pseudo-RR in the additional section:
 *   NAME  = 0x00 (root)
 *   TYPE  = 41 (OPT)
 *   CLASS = requestor's UDP payload size
 *   TTL   = extended RCODE(8) | version(8) | DO(1) | Z(15)
 *   RDATA = sequence of {option-code(16), option-length(16), option-data}
 *
 * Common EDNS options:
 *   NSID    = 3
 *   Cookie  = 10
 *   Padding = 12
 * --------------------------------------------------------------- */

/* OPT RR type */
#define NFS_EDNS_OPT_TYPE 41

/* EDNS option codes */
#define NFS_EDNS_OPT_NSID    3
#define NFS_EDNS_OPT_COOKIE  10
#define NFS_EDNS_OPT_PADDING 12

/* Max number of EDNS options in a single OPT RR */
#define NFS_EDNS_MAX_OPTIONS 16

/* Max RDATA size for the OPT RR */
#define NFS_EDNS_MAX_RDATA 4096

/* DNS header size */
#define NFS_EDNS_HDR_SIZE 12

/* Single EDNS option */
struct nfs_edns_option {
    uint16_t code;
    uint16_t length;
    uint8_t data[512];
};

/* Extended flags from TTL field */
struct nfs_edns_ext_flags {
    uint8_t ext_rcode; /* extended RCODE (high 8 bits) */
    uint8_t version;   /* EDNS version (should be 0) */
    int do_bit;        /* DNSSEC OK flag */
    uint16_t z;        /* reserved (must be 0) */
};

/* Parsed OPT RR */
struct nfs_edns_opt {
    uint16_t udp_payload_size;       /* from CLASS field */
    struct nfs_edns_ext_flags flags; /* from TTL field */
    uint16_t option_count;
    struct nfs_edns_option options[NFS_EDNS_MAX_OPTIONS];
};

/* --- OPT RR building -------------------------------------------- */

/* Build an OPT pseudo-RR in wire format.
 * Returns the number of bytes written, or -1 on error. */
int nfs_edns_build_opt(uint16_t udp_payload_size, uint8_t ext_rcode, uint8_t version, int do_bit,
                       const struct nfs_edns_option *options, uint16_t option_count, uint8_t *out,
                       size_t out_sz);

/* --- OPT RR parsing --------------------------------------------- */

/* Parse an OPT pseudo-RR from wire format.
 * `data` points to the start of the OPT RR (after name=0x00).
 * Actually, `data` points to the start of the complete RR
 * (starting with name byte 0x00).
 * Returns bytes consumed, or -1 on error. */
int nfs_edns_parse_opt(const uint8_t *data, size_t data_len, struct nfs_edns_opt *opt);

/* --- Option encoder/decoder ------------------------------------- */

/* Encode a single EDNS option into wire format.
 * Returns bytes written, or -1 on error. */
int nfs_edns_encode_option(uint16_t code, const uint8_t *odata, uint16_t length, uint8_t *out,
                           size_t out_sz);

/* Decode a single EDNS option from wire format.
 * Returns bytes consumed, or -1 on error. */
int nfs_edns_decode_option(const uint8_t *data, size_t data_len, struct nfs_edns_option *opt);

/* --- Convenience builders --------------------------------------- */

/* Build an NSID option (server identity string). */
int nfs_edns_build_nsid(const char *nsid_str, struct nfs_edns_option *opt);

/* Build a Cookie option (8-byte client cookie, optional 8-32 byte server cookie). */
int nfs_edns_build_cookie(const uint8_t *client_cookie, const uint8_t *server_cookie,
                          uint16_t server_cookie_len, struct nfs_edns_option *opt);

/* Build a Padding option (pad to desired alignment). */
int nfs_edns_build_padding(uint16_t padding_len, struct nfs_edns_option *opt);

/* --- Utility ---------------------------------------------------- */

/* Return the string name for an EDNS option code. */
const char *nfs_edns_option_str(uint16_t code);

#endif /* NFS_EDNS_H */
