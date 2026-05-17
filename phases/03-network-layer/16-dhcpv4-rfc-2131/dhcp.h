#ifndef NFS_DHCP_H
#define NFS_DHCP_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * DHCPv4 (RFC 2131)
 *
 * DHCP extends BOOTP to provide automatic IP address allocation.
 * A client broadcasts a DISCOVER, servers reply with OFFERs, the
 * client sends a REQUEST for one, and the server confirms with ACK.
 *
 * The wire format starts with a fixed 236-byte header (inherited
 * from BOOTP) followed by a variable-length options field that
 * begins with the magic cookie 0x63825363.
 *
 * This module implements DHCP message construction, option parsing,
 * and message type identification.
 * --------------------------------------------------------------- */

/* DHCP operations */
#define NFS_DHCP_BOOTREQUEST 1
#define NFS_DHCP_BOOTREPLY   2

/* Hardware type: Ethernet */
#define NFS_DHCP_HTYPE_ETH 1
#define NFS_DHCP_HLEN_ETH  6

/* Magic cookie (network byte order in the options field) */
#define NFS_DHCP_MAGIC_COOKIE 0x63825363

/* DHCP option codes */
#define NFS_DHCP_OPT_PAD          0
#define NFS_DHCP_OPT_SUBNET       1
#define NFS_DHCP_OPT_ROUTER       3
#define NFS_DHCP_OPT_DNS          6
#define NFS_DHCP_OPT_HOSTNAME     12
#define NFS_DHCP_OPT_DOMAIN       15
#define NFS_DHCP_OPT_BROADCAST    28
#define NFS_DHCP_OPT_REQUESTED_IP 50
#define NFS_DHCP_OPT_LEASE_TIME   51
#define NFS_DHCP_OPT_MSG_TYPE     53
#define NFS_DHCP_OPT_SERVER_ID    54
#define NFS_DHCP_OPT_END          255

/* DHCP message types (value of option 53) */
#define NFS_DHCP_DISCOVER 1
#define NFS_DHCP_OFFER    2
#define NFS_DHCP_REQUEST  3
#define NFS_DHCP_DECLINE  4
#define NFS_DHCP_ACK      5
#define NFS_DHCP_NAK      6
#define NFS_DHCP_RELEASE  7

/* Fixed-size DHCP message structure (BOOTP-compatible).
 * The options field is 312 bytes (RFC 2131 minimum). */
struct nfs_dhcp_msg {
    uint8_t op;           /* 1=BOOTREQUEST, 2=BOOTREPLY            */
    uint8_t htype;        /* hardware type (1=Ethernet)            */
    uint8_t hlen;         /* hardware address length (6 for Eth)   */
    uint8_t hops;         /* relay hops                            */
    uint32_t xid;         /* transaction ID                        */
    uint16_t secs;        /* seconds since client started trying   */
    uint16_t flags;       /* flags (bit 0 = broadcast)             */
    uint32_t ciaddr;      /* client IP (if known)                  */
    uint32_t yiaddr;      /* 'your' IP (offered by server)         */
    uint32_t siaddr;      /* server IP                             */
    uint32_t giaddr;      /* relay agent IP                        */
    uint8_t chaddr[16];   /* client hardware address               */
    uint8_t sname[64];    /* server host name (optional)           */
    uint8_t file[128];    /* boot file name (optional)             */
    uint8_t options[312]; /* options (starts with magic cookie)    */
} __attribute__((packed));

_Static_assert(sizeof(struct nfs_dhcp_msg) == 548, "DHCP message must be 548 bytes");

/* A parsed DHCP option. */
struct nfs_dhcp_option {
    uint8_t code;
    uint8_t length;
    uint8_t data[255];
};

/* Parse the options field after the magic cookie.
 * `opts` points to the raw options bytes (including magic cookie).
 * `len` is the total length of the options field.
 * Parsed options are written to `out` (up to max_opts).
 * `nfound` receives the number of options found.
 * Returns 0 on success, -1 on error (bad magic cookie, truncated). */
int nfs_dhcp_parse_options(const uint8_t *opts, size_t len, struct nfs_dhcp_option *out,
                           size_t max_opts, size_t *nfound);

/* Build a DHCP DISCOVER message.
 * `mac` is the 6-byte client MAC address.
 * `xid` is the transaction ID.
 * `out` is the output buffer, `out_sz` its size.
 * Returns total message length, or -1 on error. */
int nfs_dhcp_build_discover(const uint8_t mac[6], uint32_t xid, uint8_t *out, size_t out_sz);

/* Find the DHCP message type (option 53) in a parsed option list.
 * Returns the message type (1-7), or -1 if not found. */
int nfs_dhcp_get_msg_type(const struct nfs_dhcp_option *opts, size_t count);

/* Return a human-readable string for a DHCP message type. */
const char *nfs_dhcp_msg_type_str(int type);

#endif /* NFS_DHCP_H */
