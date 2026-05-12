#define _GNU_SOURCE

/* sniff.c -- AF_PACKET raw socket helpers for Linux.
 *
 * Provides functions to create, bind, parse, and send raw Ethernet
 * frames using the Linux AF_PACKET interface.
 *
 * References:
 *   - packet(7)  man page
 *   - IEEE 802.3  Ethernet II framing
 *   - RFC 826     (ARP, for context on L2 addressing)
 */

#include "sniff.h"

#include <arpa/inet.h>       /* htons, ntohs         */
#include <errno.h>
#include <net/ethernet.h>    /* ETH_P_ALL            */
#include <net/if.h>          /* if_nametoindex, IFNAMSIZ */
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>      /* socket, bind, sendto */
#include <linux/if_packet.h> /* struct sockaddr_ll   */
#include <unistd.h>

/* ------------------------------------------------------------------ */
/*  Socket creation                                                    */
/* ------------------------------------------------------------------ */

int nfs_create_raw_socket(uint16_t protocol)
{
    int fd = socket(AF_PACKET, SOCK_RAW, htons(protocol));
    if (fd < 0) {
        perror("socket(AF_PACKET, SOCK_RAW)");
        return -1;
    }
    return fd;
}

/* ------------------------------------------------------------------ */
/*  Bind to a specific network interface                               */
/* ------------------------------------------------------------------ */

int nfs_bind_to_interface(int sockfd, const char *ifname)
{
    unsigned int ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        fprintf(stderr, "if_nametoindex(%s): %s\n", ifname, strerror(errno));
        return -1;
    }

    struct sockaddr_ll sll;
    memset(&sll, 0, sizeof(sll));
    sll.sll_family   = AF_PACKET;
    sll.sll_ifindex  = (int)ifindex;
    sll.sll_protocol = htons(ETH_P_ALL);

    if (bind(sockfd, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        perror("bind(AF_PACKET)");
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Parse an Ethernet II frame                                         */
/* ------------------------------------------------------------------ */

int nfs_parse_eth_frame(const uint8_t *buf, size_t len,
                        struct nfs_parsed_frame *out)
{
    if (!buf || !out) return -1;
    if (len < NFS_ETH_HLEN) return -1;

    const struct nfs_eth_header *eth = (const struct nfs_eth_header *)buf;
    memcpy(out->dst, eth->dst, NFS_ETH_ALEN);
    memcpy(out->src, eth->src, NFS_ETH_ALEN);
    out->ethertype   = ntohs(eth->ethertype);
    out->payload     = buf + NFS_ETH_HLEN;
    out->payload_len = len - NFS_ETH_HLEN;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Format a MAC address to string                                     */
/* ------------------------------------------------------------------ */

char *nfs_format_mac(const uint8_t mac[NFS_ETH_ALEN], char *buf, size_t buflen)
{
    /* "XX:XX:XX:XX:XX:XX" = 17 chars + NUL = 18 bytes */
    if (!mac || !buf || buflen < 18) return NULL;

    snprintf(buf, buflen, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buf;
}

/* ------------------------------------------------------------------ */
/*  Human-readable EtherType name                                      */
/* ------------------------------------------------------------------ */

const char *nfs_ethertype_str(uint16_t ethertype)
{
    switch (ethertype) {
    case NFS_ETHERTYPE_IPV4: return "IPv4";
    case NFS_ETHERTYPE_ARP:  return "ARP";
    case NFS_ETHERTYPE_IPV6: return "IPv6";
    case NFS_ETHERTYPE_VLAN: return "802.1Q VLAN";
    default:                 return "unknown";
    }
}

/* ------------------------------------------------------------------ */
/*  Send a raw Ethernet frame                                          */
/* ------------------------------------------------------------------ */

ssize_t nfs_send_raw_frame(int sockfd, const char *ifname,
                           const uint8_t *frame, size_t frame_len)
{
    if (!ifname || !frame || frame_len < NFS_ETH_HLEN) {
        errno = EINVAL;
        return -1;
    }

    unsigned int ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        fprintf(stderr, "if_nametoindex(%s): %s\n", ifname, strerror(errno));
        return -1;
    }

    const struct nfs_eth_header *eth = (const struct nfs_eth_header *)frame;

    struct sockaddr_ll dest;
    memset(&dest, 0, sizeof(dest));
    dest.sll_family   = AF_PACKET;
    dest.sll_ifindex  = (int)ifindex;
    dest.sll_halen    = NFS_ETH_ALEN;
    dest.sll_protocol = eth->ethertype;   /* already network order */
    memcpy(dest.sll_addr, eth->dst, NFS_ETH_ALEN);

    ssize_t sent = sendto(sockfd, frame, frame_len, 0,
                          (struct sockaddr *)&dest, sizeof(dest));
    if (sent < 0) {
        perror("sendto(AF_PACKET)");
    }
    return sent;
}
