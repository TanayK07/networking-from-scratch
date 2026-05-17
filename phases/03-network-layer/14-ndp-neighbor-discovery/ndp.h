#ifndef NFS_NDP_H
#define NFS_NDP_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * NDP (Neighbor Discovery Protocol) - RFC 4861
 *
 * Built on ICMPv6 with types 133-137:
 *   133 - Router Solicitation (RS)
 *   134 - Router Advertisement (RA)
 *   135 - Neighbor Solicitation (NS)
 *   136 - Neighbor Advertisement (NA)
 *   137 - Redirect
 *
 * All NDP messages start with an ICMPv6 header:
 *   Type (1B) | Code (1B) | Checksum (2B)
 * followed by message-specific fields and TLV options.
 * --------------------------------------------------------------- */

/* ICMPv6 NDP message types */
#define NFS_NDP_RS       133
#define NFS_NDP_RA       134
#define NFS_NDP_NS       135
#define NFS_NDP_NA       136
#define NFS_NDP_REDIRECT 137

/* NDP Option types */
#define NFS_NDP_OPT_SRC_LLA 1 /* Source Link-Layer Address */
#define NFS_NDP_OPT_TGT_LLA 2 /* Target Link-Layer Address */
#define NFS_NDP_OPT_PREFIX  3 /* Prefix Information */
#define NFS_NDP_OPT_MTU     5 /* MTU */

/* NA flags (in the 32-bit flags field, high byte) */
#define NFS_NDP_NA_FLAG_R 0x80000000 /* Router flag */
#define NFS_NDP_NA_FLAG_S 0x40000000 /* Solicited flag */
#define NFS_NDP_NA_FLAG_O 0x20000000 /* Override flag */

/* RA flags (in the 8-bit flags field) */
#define NFS_NDP_RA_FLAG_M 0x80 /* Managed address configuration */
#define NFS_NDP_RA_FLAG_O 0x40 /* Other configuration */

/* ---- ICMPv6 base header ---- */

struct nfs_icmpv6_hdr {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_icmpv6_hdr) == 4, "ICMPv6 header must be 4 bytes");

/* ---- Neighbor Solicitation (type 135) ---- */

struct nfs_ndp_ns {
    uint8_t type; /* 135 */
    uint8_t code; /* 0 */
    uint16_t checksum;
    uint32_t reserved;
    uint8_t target[16]; /* Target IPv6 address */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ndp_ns) == 24, "NS must be 24 bytes");

/* ---- Neighbor Advertisement (type 136) ---- */

struct nfs_ndp_na {
    uint8_t type; /* 136 */
    uint8_t code; /* 0 */
    uint16_t checksum;
    uint32_t flags;     /* R|S|O + 29 reserved bits */
    uint8_t target[16]; /* Target IPv6 address */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ndp_na) == 24, "NA must be 24 bytes");

/* ---- Router Solicitation (type 133) ---- */

struct nfs_ndp_rs {
    uint8_t type; /* 133 */
    uint8_t code; /* 0 */
    uint16_t checksum;
    uint32_t reserved;
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ndp_rs) == 8, "RS must be 8 bytes");

/* ---- Router Advertisement (type 134) ---- */

struct nfs_ndp_ra {
    uint8_t type; /* 134 */
    uint8_t code; /* 0 */
    uint16_t checksum;
    uint8_t cur_hop_limit;
    uint8_t flags;            /* M|O bits */
    uint16_t router_lifetime; /* in seconds, network byte order */
    uint32_t reachable_time;  /* ms, network byte order */
    uint32_t retrans_timer;   /* ms, network byte order */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ndp_ra) == 16, "RA must be 16 bytes");

/* ---- NDP Option (TLV) ---- */

struct nfs_ndp_option {
    uint8_t type;
    uint8_t length; /* in units of 8 bytes */
    /* variable-length data follows */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ndp_option) == 2, "NDP option header must be 2 bytes");

/* ---- Prefix Information Option ---- */

struct nfs_ndp_prefix_info {
    uint8_t type;   /* 3 */
    uint8_t length; /* 4 (32 bytes) */
    uint8_t prefix_len;
    uint8_t flags;               /* L|A bits */
    uint32_t valid_lifetime;     /* network byte order */
    uint32_t preferred_lifetime; /* network byte order */
    uint32_t reserved2;
    uint8_t prefix[16];
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_ndp_prefix_info) == 32, "Prefix info must be 32 bytes");

#define NFS_NDP_PREFIX_FLAG_L 0x80 /* On-link */
#define NFS_NDP_PREFIX_FLAG_A 0x40 /* Autonomous address config */

/* ---- Parsed option result ---- */

struct nfs_ndp_parsed_option {
    uint8_t type;
    uint8_t total_len;   /* in bytes (length * 8) */
    const uint8_t *data; /* pointer to data after type+length */
    uint16_t data_len;   /* total_len - 2 */
};

/* ---- API ---- */

/* Build a Neighbor Solicitation message.
 * `target` must be 16 bytes (IPv6 address).
 * `src_mac` if non-NULL adds Source LLA option (6 bytes).
 * Returns bytes written, or -1 on error. */
int nfs_ndp_build_ns(const uint8_t *target, const uint8_t *src_mac, uint8_t *out, size_t out_sz);

/* Build a Neighbor Advertisement message.
 * `flags` = combination of NFS_NDP_NA_FLAG_R/S/O.
 * `tgt_mac` if non-NULL adds Target LLA option.
 * Returns bytes written, or -1 on error. */
int nfs_ndp_build_na(const uint8_t *target, uint32_t flags, const uint8_t *tgt_mac, uint8_t *out,
                     size_t out_sz);

/* Build a Router Solicitation message.
 * `src_mac` if non-NULL adds Source LLA option.
 * Returns bytes written, or -1 on error. */
int nfs_ndp_build_rs(const uint8_t *src_mac, uint8_t *out, size_t out_sz);

/* Build a Router Advertisement message.
 * Returns bytes written, or -1 on error. */
int nfs_ndp_build_ra(uint8_t cur_hop_limit, uint8_t flags, uint16_t router_lifetime,
                     uint32_t reachable_time, uint32_t retrans_timer, const uint8_t *src_mac,
                     uint8_t *out, size_t out_sz);

/* Parse an NDP message, returning the type (133-137) or -1 on error.
 * Validates minimum length for the detected type. */
int nfs_ndp_parse_type(const uint8_t *data, size_t len);

/* Parse a Neighbor Solicitation from raw bytes.
 * Returns 0 on success, -1 on error. */
int nfs_ndp_parse_ns(const uint8_t *data, size_t len, struct nfs_ndp_ns *ns);

/* Parse a Neighbor Advertisement from raw bytes.
 * Returns 0 on success, -1 on error. */
int nfs_ndp_parse_na(const uint8_t *data, size_t len, struct nfs_ndp_na *na);

/* Parse a Router Solicitation from raw bytes. */
int nfs_ndp_parse_rs(const uint8_t *data, size_t len, struct nfs_ndp_rs *rs);

/* Parse a Router Advertisement from raw bytes. */
int nfs_ndp_parse_ra(const uint8_t *data, size_t len, struct nfs_ndp_ra *ra);

/* Iterate NDP options in the data following the fixed header.
 * `opt_data` points to the first option byte, `opt_len` is total option area size.
 * `offset` is in/out: start at 0, updated to next option.
 * Returns 0 on success (option parsed), -1 when done or error. */
int nfs_ndp_option_next(const uint8_t *opt_data, size_t opt_len, size_t *offset,
                        struct nfs_ndp_parsed_option *opt);

/* Format an IPv6 address (16 bytes) as string. Returns `out`. */
char *nfs_ndp_format_ipv6(const uint8_t *addr, char *out, size_t out_sz);

#endif /* NFS_NDP_H */
