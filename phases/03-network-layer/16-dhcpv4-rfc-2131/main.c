/* DHCPv4 (RFC 2131) — interactive demonstration. */

#include "dhcp.h"
#include <stdio.h>
#include <string.h>

static void print_ip(uint32_t ip) {
    /* Assume network byte order stored as big-endian bytes */
    printf("%u.%u.%u.%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
}

int main(void) {
    printf("=== DHCPv4 (RFC 2131) ===\n\n");

    /* --- Build a DHCP DISCOVER --- */
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint32_t xid = 0x12345678;
    uint8_t buf[1024];

    int len = nfs_dhcp_build_discover(mac, xid, buf, sizeof(buf));
    printf("Built DHCP DISCOVER: %d bytes\n", len);

    struct nfs_dhcp_msg *msg = (struct nfs_dhcp_msg *)buf;
    printf("  op:    %u (%s)\n", msg->op, msg->op == 1 ? "BOOTREQUEST" : "BOOTREPLY");
    printf("  htype: %u (Ethernet)\n", msg->htype);
    printf("  hlen:  %u\n", msg->hlen);
    printf("  xid:   0x%08x\n", msg->xid);
    printf("  chaddr: %02x:%02x:%02x:%02x:%02x:%02x\n", msg->chaddr[0], msg->chaddr[1],
           msg->chaddr[2], msg->chaddr[3], msg->chaddr[4], msg->chaddr[5]);

    /* --- Parse the options from the DISCOVER we just built --- */
    struct nfs_dhcp_option opts[32];
    size_t nfound = 0;
    int rc = nfs_dhcp_parse_options(msg->options, sizeof(msg->options), opts, 32, &nfound);
    printf("\nOption parsing: %s (%zu options found)\n", rc == 0 ? "OK" : "FAIL", nfound);

    for (size_t i = 0; i < nfound; i++) {
        printf("  Option %3u  len=%u", opts[i].code, opts[i].length);
        if (opts[i].code == NFS_DHCP_OPT_MSG_TYPE) {
            int mt = opts[i].data[0];
            printf("  -> %s", nfs_dhcp_msg_type_str(mt));
        }
        printf("\n");
    }

    int mtype = nfs_dhcp_get_msg_type(opts, nfound);
    printf("\nMessage type: %d (%s)\n", mtype, nfs_dhcp_msg_type_str(mtype));

    /* --- Simulate a DHCP OFFER response --- */
    printf("\n--- Simulating server OFFER ---\n");
    struct nfs_dhcp_msg offer;
    memset(&offer, 0, sizeof(offer));
    offer.op = NFS_DHCP_BOOTREPLY;
    offer.htype = NFS_DHCP_HTYPE_ETH;
    offer.hlen = NFS_DHCP_HLEN_ETH;
    offer.xid = xid;
    offer.yiaddr = (192u << 24) | (168u << 16) | (1u << 8) | 100;
    offer.siaddr = (192u << 24) | (168u << 16) | (1u << 8) | 1;
    memcpy(offer.chaddr, mac, 6);

    /* Build options */
    size_t opos = 0;
    offer.options[opos++] = 0x63;
    offer.options[opos++] = 0x82;
    offer.options[opos++] = 0x53;
    offer.options[opos++] = 0x63;

    /* Message type = OFFER */
    offer.options[opos++] = NFS_DHCP_OPT_MSG_TYPE;
    offer.options[opos++] = 1;
    offer.options[opos++] = NFS_DHCP_OFFER;

    /* Subnet mask */
    offer.options[opos++] = NFS_DHCP_OPT_SUBNET;
    offer.options[opos++] = 4;
    offer.options[opos++] = 255;
    offer.options[opos++] = 255;
    offer.options[opos++] = 255;
    offer.options[opos++] = 0;

    /* Router */
    offer.options[opos++] = NFS_DHCP_OPT_ROUTER;
    offer.options[opos++] = 4;
    offer.options[opos++] = 192;
    offer.options[opos++] = 168;
    offer.options[opos++] = 1;
    offer.options[opos++] = 1;

    /* DNS server */
    offer.options[opos++] = NFS_DHCP_OPT_DNS;
    offer.options[opos++] = 4;
    offer.options[opos++] = 8;
    offer.options[opos++] = 8;
    offer.options[opos++] = 8;
    offer.options[opos++] = 8;

    /* End */
    offer.options[opos++] = NFS_DHCP_OPT_END;

    nfound = 0;
    rc = nfs_dhcp_parse_options(offer.options, sizeof(offer.options), opts, 32, &nfound);
    printf("Parsed OFFER: %s (%zu options)\n", rc == 0 ? "OK" : "FAIL", nfound);
    printf("  Your IP: ");
    print_ip(offer.yiaddr);
    printf("\n");
    printf("  Server:  ");
    print_ip(offer.siaddr);
    printf("\n");

    for (size_t i = 0; i < nfound; i++) {
        printf("  Option %3u  len=%u", opts[i].code, opts[i].length);
        if (opts[i].code == NFS_DHCP_OPT_MSG_TYPE)
            printf("  -> %s", nfs_dhcp_msg_type_str(opts[i].data[0]));
        else if (opts[i].code == NFS_DHCP_OPT_SUBNET && opts[i].length == 4)
            printf("  -> %u.%u.%u.%u", opts[i].data[0], opts[i].data[1], opts[i].data[2],
                   opts[i].data[3]);
        else if (opts[i].code == NFS_DHCP_OPT_ROUTER && opts[i].length == 4)
            printf("  -> %u.%u.%u.%u", opts[i].data[0], opts[i].data[1], opts[i].data[2],
                   opts[i].data[3]);
        else if (opts[i].code == NFS_DHCP_OPT_DNS && opts[i].length == 4)
            printf("  -> %u.%u.%u.%u", opts[i].data[0], opts[i].data[1], opts[i].data[2],
                   opts[i].data[3]);
        printf("\n");
    }

    /* Message type strings */
    printf("\nAll DHCP message types:\n");
    for (int i = 1; i <= 7; i++)
        printf("  %d = %s\n", i, nfs_dhcp_msg_type_str(i));

    printf("\nDone.\n");
    return 0;
}
