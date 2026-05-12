/* ARP request/reply responder on a TAP device.
 *
 * Setup before running:
 *     sudo ip tuntap add dev tap0 mode tap user $USER
 *     sudo ip link set tap0 up
 *     sudo ip addr add 10.0.0.1/24 dev tap0
 *
 * Then in another shell:
 *     arping -c 3 -I tap0 10.0.0.42
 *
 * Expected outcome:
 *     ip neigh show dev tap0
 *     10.0.0.42 lladdr 02:00:00:00:00:42 REACHABLE
 */

#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include "arp.h"

/* Open a TAP device by name. Returns the fd or -1 on failure. */
static int tap_open(const char *requested_name) {
    int fd = open("/dev/net/tun", O_RDWR);
    if (fd < 0) {
        perror("open /dev/net/tun");
        return -1;
    }

    struct ifreq ifr = {0};
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    strncpy(ifr.ifr_name, requested_name, IFNAMSIZ - 1);

    if (ioctl(fd, TUNSETIFF, &ifr) < 0) {
        perror("ioctl TUNSETIFF");
        close(fd);
        return -1;
    }
    fprintf(stderr, "opened TAP device: %s\n", ifr.ifr_name);
    return fd;
}

static void send_arp_reply(int tap_fd, const struct eth_header *req_eth,
                           const struct arp_packet *req_arp, const uint8_t our_mac[6],
                           const uint8_t our_ip[4]) {
    uint8_t out[sizeof(struct eth_header) + sizeof(struct arp_packet)] = {0};
    struct eth_header *eth = (struct eth_header *)out;
    struct arp_packet *arp = (struct arp_packet *)(out + sizeof *eth);

    /* Ethernet: reply to the request's sender, from us. */
    memcpy(eth->dst, req_eth->src, 6);
    memcpy(eth->src, our_mac, 6);
    eth->ethertype = htons(ETHERTYPE_ARP);

    /* ARP: reply opcode, swap sender/target. */
    arp->hw_type = htons(ARP_HW_ETHER);
    arp->proto_type = htons(ETHERTYPE_IPV4);
    arp->hw_addr_len = 6;
    arp->proto_addr_len = 4;
    arp->opcode = htons(ARP_OP_REPLY);
    memcpy(arp->sender_hw, our_mac, 6);
    memcpy(arp->sender_proto, our_ip, 4);
    memcpy(arp->target_hw, req_arp->sender_hw, 6);
    memcpy(arp->target_proto, req_arp->sender_proto, 4);

    ssize_t w = write(tap_fd, out, sizeof out);
    if (w < 0)
        perror("write");
}

int main(int argc, char **argv) {
    const char *ifname = (argc > 1) ? argv[1] : "tap0";
    int tap_fd = tap_open(ifname);
    if (tap_fd < 0)
        return 1;

    uint8_t our_mac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x42};
    /* 10.0.0.42 in network order */
    uint8_t our_ip[4] = {10, 0, 0, 42};

    fprintf(stderr, "answering ARP for 10.0.0.42 with 02:00:00:00:00:42\n");

    uint8_t buf[2048];
    for (;;) {
        ssize_t n = read(tap_fd, buf, sizeof buf);
        if (n < (ssize_t)(sizeof(struct eth_header) + sizeof(struct arp_packet))) {
            continue;
        }

        struct eth_header *eth = (struct eth_header *)buf;
        if (ntohs(eth->ethertype) != ETHERTYPE_ARP)
            continue;

        struct arp_packet *arp = (struct arp_packet *)(buf + sizeof *eth);
        if (ntohs(arp->opcode) != ARP_OP_REQUEST)
            continue;
        if (memcmp(arp->target_proto, our_ip, 4) != 0)
            continue;

        fprintf(stderr, "ARP req for %u.%u.%u.%u — replying\n", our_ip[0], our_ip[1], our_ip[2],
                our_ip[3]);
        send_arp_reply(tap_fd, eth, arp, our_mac, our_ip);
    }
}
