/*
 * main.c -- Demo driver for TLS Record Layer
 */

#include "tls_record.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    uint8_t buf[256];

    /* Build a handshake record */
    const uint8_t hs_data[] = {0x01, 0x00, 0x00, 0x05, 0x03, 0x03, 0x00, 0x00, 0x00};
    int n = nfs_tls_record_build(buf, sizeof(buf),
                                 NFS_TLS_CT_HANDSHAKE,
                                 NFS_TLS_VERSION_12,
                                 hs_data, sizeof(hs_data));
    printf("Built TLS record: %d bytes\n", n);

    struct nfs_tls_record rec;
    if (nfs_tls_record_parse(buf, (size_t)n, &rec) > 0) {
        printf("  content_type: %s (%u)\n",
               nfs_tls_content_type_name(rec.content_type), rec.content_type);
        printf("  version:      %s (0x%04x)\n",
               nfs_tls_version_name(rec.version), rec.version);
        printf("  length:       %u\n", rec.length);
    }

    /* Build an alert record */
    uint8_t alert_frag[2];
    nfs_tls_alert_build(alert_frag, sizeof(alert_frag),
                        NFS_TLS_ALERT_FATAL,
                        NFS_TLS_ALERT_HANDSHAKE_FAILURE);

    n = nfs_tls_record_build(buf, sizeof(buf),
                             NFS_TLS_CT_ALERT,
                             NFS_TLS_VERSION_12,
                             alert_frag, 2);
    printf("\nBuilt alert record: %d bytes\n", n);

    if (nfs_tls_record_parse(buf, (size_t)n, &rec) > 0) {
        struct nfs_tls_alert alert;
        if (nfs_tls_alert_parse(rec.fragment, rec.length, &alert) == 0) {
            printf("  level:       %s (%u)\n",
                   nfs_tls_alert_level_name(alert.level), alert.level);
            printf("  description: %s (%u)\n",
                   nfs_tls_alert_desc_name(alert.description), alert.description);
        }
    }

    return 0;
}
