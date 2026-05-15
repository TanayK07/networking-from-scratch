#ifndef NFS_TCP_OPTS_H
#define NFS_TCP_OPTS_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP Option Kind constants (RFC 9293, IANA TCP Option Kind Numbers).
 * --------------------------------------------------------------- */

#define NFS_TCPOPT_EOL        0   /* End of Option List */
#define NFS_TCPOPT_NOP        1   /* No-Operation (padding) */
#define NFS_TCPOPT_MSS        2   /* Maximum Segment Size (RFC 9293) */
#define NFS_TCPOPT_WSCALE     3   /* Window Scale (RFC 7323) */
#define NFS_TCPOPT_SACK_PERM  4   /* SACK Permitted (RFC 2018) */
#define NFS_TCPOPT_SACK       5   /* SACK blocks (RFC 2018) */
#define NFS_TCPOPT_TIMESTAMPS 8   /* Timestamps (RFC 7323) */

/* Maximum number of SACK block pairs in a single SACK option.
 * With a 40-byte option space: (40 - 2 header) / 8 = 4 blocks max,
 * but we allow up to 4 (typical limit with timestamps). */
#define NFS_TCPOPT_MAX_SACK_BLOCKS 4

/* ---------------------------------------------------------------
 * Parsed TCP option.
 * --------------------------------------------------------------- */

struct nfs_tcp_option {
    uint8_t kind;
    uint8_t length;     /* total length including kind and length bytes */
    union {
        uint16_t mss;           /* kind=2: MSS value (host order) */
        uint8_t  wscale;        /* kind=3: shift count */
        struct {
            uint32_t ts_val;    /* kind=8: timestamp value */
            uint32_t ts_ecr;    /* kind=8: timestamp echo reply */
        } timestamps;
        struct {
            uint32_t blocks[8]; /* left0, right0, left1, right1, ... */
            size_t   nblocks;   /* number of block pairs */
        } sack;
    } data;
};

/* ---------------------------------------------------------------
 * Parsing
 * --------------------------------------------------------------- */

/* Parse TCP options from the header extension area.
 * data     = pointer to start of options (after the 20-byte fixed header)
 * len      = number of option bytes ((data_off - 5) * 4)
 * opts     = output array
 * max_opts = capacity of opts array
 * nfound   = set to number of parsed options
 * Returns 0 on success, -1 on malformed options. */
int nfs_tcp_opts_parse(const uint8_t *data, size_t len,
                       struct nfs_tcp_option *opts, size_t max_opts,
                       size_t *nfound);

/* ---------------------------------------------------------------
 * Building individual options
 * --------------------------------------------------------------- */

/* Build MSS option (kind=2, len=4, 2-byte MSS in network order).
 * Returns bytes written, or -1 on insufficient space. */
int nfs_tcp_opts_build_mss(uint16_t mss, uint8_t *out, size_t out_sz);

/* Build Window Scale option (kind=3, len=3, 1-byte shift).
 * Returns bytes written, or -1 on insufficient space. */
int nfs_tcp_opts_build_wscale(uint8_t shift, uint8_t *out, size_t out_sz);

/* Build Timestamps option (kind=8, len=10, two 4-byte values).
 * Returns bytes written, or -1 on insufficient space. */
int nfs_tcp_opts_build_timestamps(uint32_t ts_val, uint32_t ts_ecr,
                                  uint8_t *out, size_t out_sz);

/* Build SACK Permitted option (kind=4, len=2).
 * Returns bytes written, or -1 on insufficient space. */
int nfs_tcp_opts_build_sack_perm(uint8_t *out, size_t out_sz);

/* ---------------------------------------------------------------
 * Padding
 * --------------------------------------------------------------- */

/* Pad options with NOPs and EOL to a 4-byte boundary.
 * opts       = buffer containing built options
 * opts_len   = current length of options
 * padded_len = set to final length after padding
 * Returns 0 on success, -1 if buffer would overflow (assumes
 * at least opts_len + 3 bytes available in buffer). */
int nfs_tcp_opts_pad(uint8_t *opts, size_t opts_len, size_t *padded_len);

/* ---------------------------------------------------------------
 * Formatting
 * --------------------------------------------------------------- */

/* Format a parsed option as a human-readable string.
 * Examples: "MSS=1460", "WScale=7", "TS val=123 ecr=456" */
void nfs_tcp_opt_format(const struct nfs_tcp_option *opt, char *buf, size_t sz);

#endif /* NFS_TCP_OPTS_H */
