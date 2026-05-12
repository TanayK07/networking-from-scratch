#define _POSIX_C_SOURCE 200809L

/* main.c -- CLI sniffer using AF_PACKET raw sockets.
 *
 * Usage:
 *     sudo ./sniff [interface]
 *
 * Captures raw Ethernet frames on the given interface (default: "eth0")
 * and prints the source MAC, destination MAC, EtherType, and payload
 * length for each frame received.
 *
 * Requires root or CAP_NET_RAW.
 */

#include "sniff.h"

#include <net/ethernet.h>   /* ETH_P_ALL */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>     /* recvfrom  */
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void handle_sigint(int sig)
{
    (void)sig;
    running = 0;
}

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [interface]\n", prog);
    fprintf(stderr, "  interface  network interface to sniff (default: eth0)\n");
    fprintf(stderr, "\nRequires root or CAP_NET_RAW capability.\n");
}

int main(int argc, char **argv)
{
    if (argc > 1 && (strcmp(argv[1], "-h") == 0 ||
                     strcmp(argv[1], "--help") == 0)) {
        usage(argv[0]);
        return 0;
    }

    const char *ifname = (argc > 1) ? argv[1] : "eth0";

    /* Install SIGINT handler for clean shutdown. */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);

    /* Create AF_PACKET socket that captures all EtherTypes. */
    int sockfd = nfs_create_raw_socket(ETH_P_ALL);
    if (sockfd < 0) return 1;

    /* Bind to the requested interface. */
    if (nfs_bind_to_interface(sockfd, ifname) < 0) {
        close(sockfd);
        return 1;
    }

    fprintf(stderr, "sniffing on %s ... press Ctrl-C to stop\n", ifname);

    /* Print column header. */
    printf("%-17s  %-17s  %-6s  %-10s  %s\n",
           "SRC MAC", "DST MAC", "TYPE", "ETHERTYPE", "LEN");
    printf("%-17s  %-17s  %-6s  %-10s  %s\n",
           "-----------------", "-----------------",
           "------", "----------", "---");

    uint8_t buf[65536];
    unsigned long count = 0;

    while (running) {
        ssize_t n = recvfrom(sockfd, buf, sizeof(buf), 0, NULL, NULL);
        if (n < 0) {
            if (running) perror("recvfrom");
            break;
        }

        struct nfs_parsed_frame frame;
        if (nfs_parse_eth_frame(buf, (size_t)n, &frame) < 0) {
            continue;
        }

        char src_str[18], dst_str[18];
        nfs_format_mac(frame.src, src_str, sizeof(src_str));
        nfs_format_mac(frame.dst, dst_str, sizeof(dst_str));

        printf("%-17s  %-17s  0x%04x  %-10s  %zu\n",
               src_str, dst_str,
               frame.ethertype,
               nfs_ethertype_str(frame.ethertype),
               frame.payload_len);

        count++;
    }

    fprintf(stderr, "\n%lu packets captured\n", count);
    close(sockfd);
    return 0;
}
