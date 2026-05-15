/*
 * main.c -- Longest-Prefix Match routing demo
 *
 * Demonstrates building a routing table and looking up destinations
 * to show how the most specific (longest prefix) route wins.
 */

#include "lpm.h"

#include <stdio.h>
#include <stdint.h>

/* Build an IPv4 address in host byte order from four octets */
static uint32_t ip(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return ((uint32_t)a << 24) | ((uint32_t)b << 16) |
           ((uint32_t)c << 8)  | (uint32_t)d;
}

static void print_ip(uint32_t addr)
{
    printf("%u.%u.%u.%u",
           (addr >> 24) & 0xFF, (addr >> 16) & 0xFF,
           (addr >> 8) & 0xFF, addr & 0xFF);
}

static void do_lookup(const struct nfs_route_table *t, uint32_t dst)
{
    printf("  Lookup ");
    print_ip(dst);
    printf(" -> ");

    const struct nfs_route *r = nfs_route_lookup(t, dst);
    if (r) {
        printf("next-hop ");
        print_ip(r->next_hop);
        printf(" via iface %d (matched /%u)\n", r->interface_id, r->prefix_len);
    } else {
        printf("no route found\n");
    }
}

int main(void)
{
    struct nfs_route_table table;
    nfs_route_table_init(&table, 16);

    printf("=== Longest-Prefix Match Routing Demo ===\n\n");

    /* Build a sample routing table */
    nfs_route_add(&table, ip(0, 0, 0, 0),     0,  ip(10, 0, 0, 1), 0);  /* default */
    nfs_route_add(&table, ip(10, 0, 0, 0),     8,  ip(10, 0, 0, 1), 1);  /* 10/8    */
    nfs_route_add(&table, ip(10, 1, 0, 0),    16,  ip(10, 1, 0, 1), 2);  /* 10.1/16 */
    nfs_route_add(&table, ip(10, 1, 2, 0),    24,  ip(10, 1, 2, 1), 3);  /* 10.1.2/24 */
    nfs_route_add(&table, ip(192, 168, 1, 0), 24,  ip(192, 168, 1, 1), 4);

    /* Print the table */
    printf("Routing table:\n");
    char buf[1024];
    nfs_route_table_format(&table, buf, sizeof(buf));
    printf("%s\n", buf);

    /* Perform lookups */
    printf("Lookups:\n");
    do_lookup(&table, ip(10, 1, 2, 42));   /* matches /24 */
    do_lookup(&table, ip(10, 1, 3, 42));   /* matches /16 */
    do_lookup(&table, ip(10, 2, 0, 1));    /* matches /8  */
    do_lookup(&table, ip(8, 8, 8, 8));     /* matches /0  */
    do_lookup(&table, ip(192, 168, 1, 100)); /* matches 192.168.1/24 */

    printf("\n--- Removing 10.1.2.0/24 ---\n\n");
    nfs_route_remove(&table, ip(10, 1, 2, 0), 24);

    printf("Lookups after removal:\n");
    do_lookup(&table, ip(10, 1, 2, 42));   /* now matches /16 */
    do_lookup(&table, ip(10, 1, 3, 42));   /* still /16 */

    nfs_route_table_free(&table);
    return 0;
}
