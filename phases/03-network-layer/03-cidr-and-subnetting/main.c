#include "cidr.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <CIDR> [ip-to-check]\n", argv[0]);
        fprintf(stderr, "  Example: %s 192.168.1.0/24 192.168.1.42\n", argv[0]);
        return 1;
    }

    struct nfs_cidr cidr;
    if (nfs_cidr_parse(argv[1], &cidr) != 0) {
        fprintf(stderr, "Error: invalid CIDR \"%s\"\n", argv[1]);
        return 1;
    }

    char buf[20];
    nfs_cidr_format(&cidr, buf, sizeof(buf));
    printf("CIDR:       %s\n", buf);

    char ip_buf[16];
    uint32_t mask = nfs_cidr_mask(cidr.prefix_len);
    nfs_ip_format(mask, ip_buf, sizeof(ip_buf));
    printf("Mask:       %s\n", ip_buf);

    nfs_ip_format(nfs_cidr_network(&cidr), ip_buf, sizeof(ip_buf));
    printf("Network:    %s\n", ip_buf);

    nfs_ip_format(nfs_cidr_broadcast(&cidr), ip_buf, sizeof(ip_buf));
    printf("Broadcast:  %s\n", ip_buf);

    printf("Hosts:      %u\n", nfs_cidr_host_count(cidr.prefix_len));

    if (argc >= 3) {
        uint32_t ip;
        if (nfs_ip_parse(argv[2], &ip) != 0) {
            fprintf(stderr, "Error: invalid IP \"%s\"\n", argv[2]);
            return 1;
        }
        nfs_ip_format(ip, ip_buf, sizeof(ip_buf));
        printf("IP %s %s in %s\n", ip_buf, nfs_cidr_contains(&cidr, ip) ? "is" : "is NOT", buf);
    }

    return 0;
}
