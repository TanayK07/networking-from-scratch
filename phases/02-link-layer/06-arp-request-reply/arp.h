#ifndef NFS_ARP_H
#define NFS_ARP_H

#include <stdint.h>

#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV4 0x0800

#define ARP_HW_ETHER   1
#define ARP_OP_REQUEST 1
#define ARP_OP_REPLY   2

/* Ethernet II header. */
struct eth_header {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t ethertype; /* network byte order */
} __attribute__((packed));

/* ARP packet for IPv4-over-Ethernet. */
struct arp_packet {
    uint16_t hw_type;       /* 1 = Ethernet, network order */
    uint16_t proto_type;    /* 0x0800 = IPv4, network order */
    uint8_t hw_addr_len;    /* 6 */
    uint8_t proto_addr_len; /* 4 */
    uint16_t opcode;        /* 1 = req, 2 = reply, network order */
    uint8_t sender_hw[6];
    uint8_t sender_proto[4];
    uint8_t target_hw[6];
    uint8_t target_proto[4];
} __attribute__((packed));

#endif /* NFS_ARP_H */
