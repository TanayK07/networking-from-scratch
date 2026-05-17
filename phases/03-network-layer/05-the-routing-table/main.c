#include "rtable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void demo(void) {
    struct nfs_rtable rt;
    nfs_rtable_init(&rt, 64);

    printf("=== Routing Table Demo ===\n\n");

    /* Add some routes */
    nfs_rtable_add(&rt, nfs_ip4(0, 0, 0, 0), 0, nfs_ip4(192, 168, 1, 1), 0, 100,
                   NFS_RTF_UP | NFS_RTF_GATEWAY);

    nfs_rtable_add(&rt, nfs_ip4(192, 168, 1, 0), 24, 0, 0, 0, NFS_RTF_UP);

    nfs_rtable_add(&rt, nfs_ip4(10, 0, 0, 0), 8, nfs_ip4(192, 168, 1, 254), 1, 50,
                   NFS_RTF_UP | NFS_RTF_GATEWAY);

    nfs_rtable_add(&rt, nfs_ip4(10, 0, 1, 0), 24, nfs_ip4(10, 0, 0, 1), 1, 10,
                   NFS_RTF_UP | NFS_RTF_GATEWAY);

    /* Display */
    char buf[4096];
    nfs_rtable_format(&rt, buf, sizeof(buf));
    printf("Routing table:\n%s\n", buf);

    /* Lookup examples */
    const struct nfs_rt_entry *e;

    e = nfs_rtable_lookup(&rt, nfs_ip4(10, 0, 1, 50));
    if (e)
        printf("10.0.1.50   -> if%d (/%u, metric %u)\n", e->iface, e->prefix_len, e->metric);

    e = nfs_rtable_lookup(&rt, nfs_ip4(10, 0, 2, 50));
    if (e)
        printf("10.0.2.50   -> if%d (/%u, metric %u)\n", e->iface, e->prefix_len, e->metric);

    e = nfs_rtable_lookup(&rt, nfs_ip4(8, 8, 8, 8));
    if (e)
        printf("8.8.8.8     -> if%d (default, metric %u)\n", e->iface, e->metric);

    nfs_rtable_free(&rt);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    demo();
    return 0;
}
