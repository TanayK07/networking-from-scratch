/* DHCPv4 (RFC 2131) — implementation. */

#include "dhcp.h"
#include <string.h>
#include <stdio.h>

int nfs_dhcp_parse_options(const uint8_t *opts, size_t len,
                           struct nfs_dhcp_option *out, size_t max_opts,
                           size_t *nfound)
{
    *nfound = 0;

    if (len < 4)
        return -1;

    /* Verify magic cookie: 0x63825363 in network byte order */
    if (opts[0] != 0x63 || opts[1] != 0x82 ||
        opts[2] != 0x53 || opts[3] != 0x63)
        return -1;

    size_t pos = 4;  /* skip magic cookie */

    while (pos < len) {
        uint8_t code = opts[pos++];

        if (code == NFS_DHCP_OPT_END)
            break;

        if (code == NFS_DHCP_OPT_PAD)
            continue;

        /* TLV: code already read, now read length */
        if (pos >= len)
            return -1;
        uint8_t optlen = opts[pos++];

        if (pos + optlen > len)
            return -1;

        if (*nfound < max_opts) {
            out[*nfound].code   = code;
            out[*nfound].length = optlen;
            memcpy(out[*nfound].data, &opts[pos], optlen);
            (*nfound)++;
        }

        pos += optlen;
    }

    return 0;
}

int nfs_dhcp_build_discover(const uint8_t mac[6], uint32_t xid,
                            uint8_t *out, size_t out_sz)
{
    if (out_sz < sizeof(struct nfs_dhcp_msg))
        return -1;

    memset(out, 0, sizeof(struct nfs_dhcp_msg));

    struct nfs_dhcp_msg *msg = (struct nfs_dhcp_msg *)out;
    msg->op    = NFS_DHCP_BOOTREQUEST;
    msg->htype = NFS_DHCP_HTYPE_ETH;
    msg->hlen  = NFS_DHCP_HLEN_ETH;
    msg->hops  = 0;
    msg->xid   = xid;
    msg->secs  = 0;
    msg->flags = 0x0080;  /* broadcast flag (network byte order: 0x8000) */
    memcpy(msg->chaddr, mac, 6);

    /* Options field: magic cookie + DHCP message type + end */
    size_t opos = 0;
    /* Magic cookie */
    msg->options[opos++] = 0x63;
    msg->options[opos++] = 0x82;
    msg->options[opos++] = 0x53;
    msg->options[opos++] = 0x63;

    /* Option 53: DHCP Message Type = DISCOVER */
    msg->options[opos++] = NFS_DHCP_OPT_MSG_TYPE;
    msg->options[opos++] = 1;
    msg->options[opos++] = NFS_DHCP_DISCOVER;

    /* End */
    msg->options[opos++] = NFS_DHCP_OPT_END;

    return (int)sizeof(struct nfs_dhcp_msg);
}

int nfs_dhcp_get_msg_type(const struct nfs_dhcp_option *opts, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (opts[i].code == NFS_DHCP_OPT_MSG_TYPE && opts[i].length >= 1)
            return (int)opts[i].data[0];
    }
    return -1;
}

const char *nfs_dhcp_msg_type_str(int type)
{
    switch (type) {
    case NFS_DHCP_DISCOVER: return "DISCOVER";
    case NFS_DHCP_OFFER:    return "OFFER";
    case NFS_DHCP_REQUEST:  return "REQUEST";
    case NFS_DHCP_DECLINE:  return "DECLINE";
    case NFS_DHCP_ACK:      return "ACK";
    case NFS_DHCP_NAK:      return "NAK";
    case NFS_DHCP_RELEASE:  return "RELEASE";
    default:                return "UNKNOWN";
    }
}
