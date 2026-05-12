#ifndef NFS_SNIFF_H
#define NFS_SNIFF_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>  /* ssize_t */

/* ---- Ethernet II frame header (IEEE 802.3) ---- */

#define NFS_ETH_ALEN       6      /* octets in an Ethernet address */
#define NFS_ETH_HLEN       14     /* total octets in header        */
#define NFS_ETH_FRAME_MIN  60     /* minimum frame size (excl. FCS) */
#define NFS_ETH_FRAME_MAX  1514   /* maximum frame size (excl. FCS) */

/* Common EtherType values (network byte order constants not used here;
 * the struct stores network order, callers use ntohs()). */
#define NFS_ETHERTYPE_IPV4  0x0800
#define NFS_ETHERTYPE_ARP   0x0806
#define NFS_ETHERTYPE_IPV6  0x86DD
#define NFS_ETHERTYPE_VLAN  0x8100

struct nfs_eth_header {
    uint8_t  dst[NFS_ETH_ALEN];   /* destination MAC address */
    uint8_t  src[NFS_ETH_ALEN];   /* source MAC address      */
    uint16_t ethertype;            /* network byte order      */
} __attribute__((packed));

/* ---- Parsed frame view (host byte order, no ownership) ---- */

struct nfs_parsed_frame {
    uint8_t        dst[NFS_ETH_ALEN];
    uint8_t        src[NFS_ETH_ALEN];
    uint16_t       ethertype;       /* host byte order */
    const uint8_t *payload;         /* points into original buffer */
    size_t         payload_len;
};

/* ---- Functions ---- */

/*
 * nfs_create_raw_socket -- create an AF_PACKET / SOCK_RAW socket.
 *
 * @protocol: ETH_P_ALL to capture everything, or a specific EtherType
 *            (in host byte order; the function calls htons() internally).
 * Returns the socket fd on success, or -1 on failure (errno is set).
 */
int nfs_create_raw_socket(uint16_t protocol);

/*
 * nfs_bind_to_interface -- bind an AF_PACKET socket to a named interface.
 *
 * @sockfd:  socket returned by nfs_create_raw_socket().
 * @ifname:  interface name, e.g. "eth0".
 * Returns 0 on success, -1 on failure (errno is set).
 */
int nfs_bind_to_interface(int sockfd, const char *ifname);

/*
 * nfs_parse_eth_frame -- parse raw bytes into a nfs_parsed_frame.
 *
 * @buf:     pointer to the raw frame bytes.
 * @len:     number of bytes available.
 * @out:     receives the parsed result.
 * Returns 0 on success, -1 if the buffer is too short for an Ethernet header.
 */
int nfs_parse_eth_frame(const uint8_t *buf, size_t len,
                        struct nfs_parsed_frame *out);

/*
 * nfs_format_mac -- write a MAC address as "XX:XX:XX:XX:XX:XX".
 *
 * @mac:     6-byte MAC address.
 * @buf:     output buffer, must be >= 18 bytes (17 chars + NUL).
 * @buflen:  size of output buffer.
 * Returns @buf on success, or NULL if buflen < 18.
 */
char *nfs_format_mac(const uint8_t mac[NFS_ETH_ALEN], char *buf, size_t buflen);

/*
 * nfs_ethertype_str -- return a human-readable name for common EtherTypes.
 *
 * @ethertype: host byte order.
 * Returns a static string like "IPv4", "ARP", etc., or "unknown".
 */
const char *nfs_ethertype_str(uint16_t ethertype);

/*
 * nfs_send_raw_frame -- transmit a raw Ethernet frame.
 *
 * @sockfd:    AF_PACKET socket.
 * @ifname:    interface name (used to look up ifindex).
 * @frame:     complete Ethernet frame (header + payload).
 * @frame_len: number of bytes in @frame.
 * Returns the number of bytes sent, or -1 on failure.
 */
ssize_t nfs_send_raw_frame(int sockfd, const char *ifname,
                           const uint8_t *frame, size_t frame_len);

#endif /* NFS_SNIFF_H */
