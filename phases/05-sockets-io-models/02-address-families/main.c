/*
 * main.c -- Address families demo
 */
#include "addr.h"
#include <stdio.h>

int main(void)
{
    char str[NFS_ADDR_STR_MAX];

    printf("=== Address Families ===\n\n");

    /* AF_INET */
    printf("--- AF_INET (IPv4) ---\n");
    struct sockaddr_in addr4;
    nfs_addr_build_inet("192.168.1.100", 8080, &addr4);
    printf("  %s\n", nfs_addr_format_inet(&addr4, str, sizeof(str)));
    printf("  Family: %s\n", nfs_addr_family_name(addr4.sin_family));
    printf("  Port:   %d\n", nfs_addr_get_port((struct sockaddr *)&addr4));
    printf("  Loopback: %d\n\n", nfs_addr_is_loopback4(&addr4));

    struct sockaddr_in lo4;
    nfs_addr_build_inet("127.0.0.1", 80, &lo4);
    printf("  %s (loopback=%d)\n\n",
           nfs_addr_format_inet(&lo4, str, sizeof(str)),
           nfs_addr_is_loopback4(&lo4));

    /* AF_INET6 */
    printf("--- AF_INET6 (IPv6) ---\n");
    struct sockaddr_in6 addr6;
    nfs_addr_build_inet6("2001:db8::1", 443, &addr6);
    printf("  %s\n", nfs_addr_format_inet6(&addr6, str, sizeof(str)));
    printf("  Family: %s\n", nfs_addr_family_name(addr6.sin6_family));
    printf("  Port:   %d\n\n", nfs_addr_get_port((struct sockaddr *)&addr6));

    struct sockaddr_in6 lo6;
    nfs_addr_build_inet6("::1", 0, &lo6);
    printf("  %s (loopback=%d)\n\n",
           nfs_addr_format_inet6(&lo6, str, sizeof(str)),
           nfs_addr_is_loopback6(&lo6));

    /* AF_UNIX */
    printf("--- AF_UNIX ---\n");
    struct sockaddr_un addru;
    nfs_addr_build_unix("/tmp/my.sock", &addru);
    printf("  %s\n", nfs_addr_format_unix(&addru, str, sizeof(str)));
    printf("  Family: %s\n\n", nfs_addr_family_name(addru.sun_family));

    /* Generic format */
    printf("--- Generic formatting ---\n");
    printf("  IPv4: %s\n",
           nfs_addr_format((struct sockaddr *)&addr4, sizeof(addr4), str, sizeof(str)));
    printf("  IPv6: %s\n",
           nfs_addr_format((struct sockaddr *)&addr6, sizeof(addr6), str, sizeof(str)));
    printf("  Unix: %s\n",
           nfs_addr_format((struct sockaddr *)&addru, sizeof(addru), str, sizeof(str)));

    return 0;
}
