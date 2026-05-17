/* Network Address Translation — interactive demonstration. */

#include "nat.h"
#include <stdio.h>

static void print_ip(uint32_t ip) {
    printf("%u.%u.%u.%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF);
}

int main(void) {
    printf("=== NAT: What Really Happens ===\n\n");

    /* External (public) IP: 203.0.113.1 */
    uint32_t ext_ip = (203u << 24) | (0u << 16) | (113u << 8) | 1;

    struct nfs_nat_table nat;
    nfs_nat_init(&nat, 64, ext_ip, 10000);

    /* --- Two internal hosts making outbound connections --- */
    uint32_t host_a = (192u << 24) | (168u << 16) | (1u << 8) | 10;
    uint32_t host_b = (192u << 24) | (168u << 16) | (1u << 8) | 20;
    uint32_t server = (93u << 24) | (184u << 16) | (216u << 8) | 34;

    uint32_t new_ip;
    uint16_t new_port;

    printf("Host A (192.168.1.10:4000) -> 93.184.216.34:80 (TCP)\n");
    nfs_nat_outbound(&nat, host_a, 4000, server, 80, NFS_PROTO_TCP, &new_ip, &new_port);
    printf("  NAT rewrites to: ");
    print_ip(new_ip);
    printf(":%u\n", new_port);

    printf("Host B (192.168.1.20:5000) -> 93.184.216.34:80 (TCP)\n");
    nfs_nat_outbound(&nat, host_b, 5000, server, 80, NFS_PROTO_TCP, &new_ip, &new_port);
    printf("  NAT rewrites to: ");
    print_ip(new_ip);
    printf(":%u\n", new_port);

    printf("Host A again (same flow): ");
    nfs_nat_outbound(&nat, host_a, 4000, server, 80, NFS_PROTO_TCP, &new_ip, &new_port);
    printf("reuses port %u\n\n", new_port);

    /* --- Inbound reply --- */
    printf("Inbound reply from 93.184.216.34:80 -> external port 10000:\n");
    uint32_t orig_ip;
    uint16_t orig_port;
    int rc = nfs_nat_inbound(&nat, server, 80, 10000, NFS_PROTO_TCP, &orig_ip, &orig_port);
    if (rc == 0) {
        printf("  Translated to: ");
        print_ip(orig_ip);
        printf(":%u\n", orig_port);
    }

    printf("Inbound on unknown port 9999:\n");
    rc = nfs_nat_inbound(&nat, server, 80, 9999, NFS_PROTO_TCP, &orig_ip, &orig_port);
    printf("  Result: %s\n\n", rc == 0 ? "found" : "dropped (no mapping)");

    /* --- Format table --- */
    char buf[2048];
    nfs_nat_format(&nat, buf, sizeof(buf));
    printf("%s\n", buf);

    /* --- Expire old entries --- */
    printf("Expiring entries older than 300s (now=400):\n");
    int removed = nfs_nat_expire(&nat, 400.0, 300.0);
    printf("  Removed %d entries\n", removed);

    nfs_nat_format(&nat, buf, sizeof(buf));
    printf("%s", buf);

    nfs_nat_free(&nat);
    printf("\nDone.\n");
    return 0;
}
