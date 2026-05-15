#ifndef NFS_IP_OPTS_H
#define NFS_IP_OPTS_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * IPv4 options parser and builder.
 *
 * IPv4 options live in the variable-length part of the IP header
 * (bytes 20..59, up to 40 bytes).  Each option has a type byte:
 *
 *   +-------+-------+-----------+
 *   | copied | class | number   |
 *   | 1 bit  | 2 bit | 5 bits   |
 *   +-------+-------+-----------+
 *
 * Single-byte options (EOL, NOP) have no length/data fields.
 * Multi-byte options have: type(1) + length(1) + data(length-2).
 * --------------------------------------------------------------- */

/* Well-known option types */
#define NFS_IPOPT_EOL   0       /* End of Options List */
#define NFS_IPOPT_NOP   1       /* No Operation */
#define NFS_IPOPT_RR    7       /* Record Route */
#define NFS_IPOPT_TS    68      /* Internet Timestamp (0x44) */
#define NFS_IPOPT_LSRR  131    /* Loose Source and Record Route (0x83) */
#define NFS_IPOPT_SSRR  137    /* Strict Source and Record Route (0x89) */

/* Maximum option data size (40 bytes max for all options combined,
 * minus type and length bytes gives 38 for the largest single option). */
#define NFS_IPOPT_MAX_DATA  38

struct nfs_ip_option {
    uint8_t type;
    uint8_t length;                     /* Total length including type+length bytes (0 for EOL/NOP) */
    uint8_t data[NFS_IPOPT_MAX_DATA];   /* Option data (length - 2 bytes used) */
};

/* Parse options from raw bytes.
 * `data` points to the start of the options area, `len` is its length.
 * Parsed options are written to `opts` (at most `max_opts`).
 * `*nfound` is set to the number of options parsed.
 * Returns 0 on success, -1 on malformed input. */
int nfs_ip_opts_parse(const uint8_t *data, size_t len,
                      struct nfs_ip_option *opts, size_t max_opts,
                      size_t *nfound);

/* Build a Record Route option.
 * `max_hops` is the number of IP address slots to allocate (each 4 bytes).
 * Writes to `out` (at most `out_sz` bytes).
 * Returns bytes written, or -1 on error. */
int nfs_ip_opts_build_rr(int max_hops, uint8_t *out, size_t out_sz);

/* Build an Internet Timestamp option.
 * `max_entries` is the number of timestamp slots (each 4 bytes).
 * Writes to `out` (at most `out_sz` bytes).
 * Returns bytes written, or -1 on error. */
int nfs_ip_opts_build_ts(int max_entries, uint8_t *out, size_t out_sz);

/* Pad options to a 4-byte boundary with NOPs and a trailing EOL.
 * `opts` is the options buffer, `len` is the current length.
 * `*padded_len` receives the padded length.
 * Returns 0 on success, -1 if the result would exceed 40 bytes. */
int nfs_ip_opts_pad(uint8_t *opts, size_t len, size_t *padded_len);

/* Return a human-readable name for an option type.
 * Returns a static string (never NULL). */
const char *nfs_ip_opt_name(uint8_t type);

#endif /* NFS_IP_OPTS_H */
