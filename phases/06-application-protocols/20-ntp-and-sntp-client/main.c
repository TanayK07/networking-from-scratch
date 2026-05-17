#include "ntp.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    /* Build a client request packet */
    struct nfs_ntp_packet req;
    memset(&req, 0, sizeof(req));
    req.li = NFS_NTP_LI_NONE;
    req.vn = 4;
    req.mode = NFS_NTP_MODE_CLIENT;

    /* Set transmit timestamp (simulated: Unix time 1700000000.5) */
    req.xmit_ts = nfs_unix_to_ntp(1700000000.5);

    uint8_t buf[64];
    int n = nfs_ntp_build(buf, sizeof(buf), &req);
    printf("Built %d-byte NTP client request\n", n);
    printf("Wire bytes: ");
    for (int i = 0; i < n; i++)
        printf("%02x ", buf[i]);
    printf("\n\n");

    /* Parse it back */
    struct nfs_ntp_packet parsed;
    if (nfs_ntp_parse(buf, (size_t)n, &parsed) == 0) {
        printf("Parsed NTP packet:\n");
        printf("  LI=%u VN=%u Mode=%u\n", parsed.li, parsed.vn, parsed.mode);
        printf("  Stratum=%u Poll=%d Precision=%d\n", parsed.stratum, parsed.poll,
               parsed.precision);
        printf("  Transmit TS: sec=%u frac=%u\n", parsed.xmit_ts.seconds, parsed.xmit_ts.fraction);
        printf("  Unix time: %.3f\n", nfs_ntp_to_unix(&parsed.xmit_ts));
    }

    /* Simulate offset/RTT calculation */
    printf("\n=== Offset & RTT Simulation ===\n");
    struct nfs_ntp_ts t1 = nfs_unix_to_ntp(1000.0);
    struct nfs_ntp_ts t2 = nfs_unix_to_ntp(1000.025);
    struct nfs_ntp_ts t3 = nfs_unix_to_ntp(1000.026);
    struct nfs_ntp_ts t4 = nfs_unix_to_ntp(1000.050);

    double offset = nfs_ntp_offset(&t1, &t2, &t3, &t4);
    double rtt = nfs_ntp_rtt(&t1, &t2, &t3, &t4);
    printf("  Offset: %.6f sec\n", offset);
    printf("  RTT:    %.6f sec\n", rtt);

    return 0;
}
