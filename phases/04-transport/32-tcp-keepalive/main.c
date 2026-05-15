/* TCP Keepalive — demo driver. */

#include "keepalive.h"
#include <stdio.h>

int main(void)
{
    printf("=== TCP Keepalive Demo ===\n\n");

    struct nfs_keepalive ka;
    nfs_keepalive_init(&ka, NFS_KA_DEFAULT_IDLE, NFS_KA_DEFAULT_INTERVAL,
                       NFS_KA_DEFAULT_PROBES);
    nfs_keepalive_enable(&ka);
    nfs_keepalive_data_received(&ka, 0.0);

    char buf[256];
    nfs_keepalive_format(&ka, buf, sizeof(buf));
    printf("Initial: %s\n", buf);

    printf("\n--- Simulating idle connection ---\n");
    double t = 0.0;
    for (int i = 0; i < 12; i++) {
        t += 800.0;
        int result = nfs_keepalive_check(&ka, t);
        printf("  t=%.0f: check=%d (probes_sent=%d)\n",
               t, result, ka.probes_sent);
        if (result == -1) {
            printf("  ** Connection declared DEAD **\n");
            break;
        }
    }

    nfs_keepalive_format(&ka, buf, sizeof(buf));
    printf("\nFinal: %s\n", buf);

    printf("\n--- Keepalive probe packet ---\n");
    uint8_t probe[20];
    int n = nfs_keepalive_build_probe(0x12345677, probe, sizeof(probe));
    if (n == 20) {
        printf("  Probe built (%d bytes), seq=0x%02x%02x%02x%02x, "
               "flags=0x%02x\n",
               n, probe[4], probe[5], probe[6], probe[7], probe[13]);
    }

    return 0;
}
