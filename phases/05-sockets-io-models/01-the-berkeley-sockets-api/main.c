#include "bsock.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("=== Berkeley Sockets API Helpers ===\n\n");

    /* Build a sockaddr_in */
    struct nfs_sockaddr_in sa;
    if (nfs_sockaddr_in_build(&sa, "192.168.1.100", 8080) == 0) {
        printf("Built sockaddr_in:\n");
        printf("  family: %u\n", sa.sin_family);
        printf("  port (network): 0x%04x\n", sa.sin_port);
        printf("  addr (network): 0x%08x\n", sa.sin_addr);
    }

    /* Parse it back */
    char ip[16];
    uint16_t port;
    if (nfs_sockaddr_in_parse(&sa, ip, sizeof(ip), &port) == 0) {
        printf("\nParsed back:\n");
        printf("  IP: %s\n", ip);
        printf("  Port: %u\n", port);
    }

    /* Format */
    char formatted[32];
    if (nfs_sockaddr_format(&sa, formatted, sizeof(formatted)) > 0) {
        printf("  Formatted: %s\n", formatted);
    }

    /* Helpers */
    printf("\n  Port via helper: %u\n", nfs_sockaddr_port(&sa));

    /* More addresses */
    printf("\n=== Various Addresses ===\n");
    const char *addrs[] = {"0.0.0.0", "127.0.0.1", "255.255.255.255", "10.0.0.1", NULL};
    for (int i = 0; addrs[i]; i++) {
        struct nfs_sockaddr_in tmp;
        nfs_sockaddr_in_build(&tmp, addrs[i], 80);
        char f[32];
        nfs_sockaddr_format(&tmp, f, sizeof(f));
        printf("  %s\n", f);
    }

    return 0;
}
