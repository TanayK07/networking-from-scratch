/*
 * wifi_parse — demonstrate IEEE 802.11 frame control parsing and building.
 *
 * Build:  make
 * Run:    ./wifi_parse
 */
#include "wifi.h"

#include <stdio.h>
#include <string.h>

static void mac_fmt(const uint8_t m[6], char *buf, size_t sz) {
    snprintf(buf, sz, "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
}

static void demo_beacon(void) {
    printf("--- Beacon frame ---\n");

    struct nfs_wifi_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));

    hdr.fc.type = NFS_WIFI_TYPE_MGMT;
    hdr.fc.subtype = NFS_WIFI_MGMT_BEACON;
    /* Broadcast destination */
    memset(hdr.addr1, 0xFF, 6);
    /* AP source/BSSID */
    uint8_t ap_mac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    memcpy(hdr.addr2, ap_mac, 6);
    memcpy(hdr.addr3, ap_mac, 6);
    hdr.seq_ctrl = nfs_wifi_seq_ctrl(42, 0);

    uint8_t buf[64];
    int len = nfs_wifi_hdr_build(&hdr, buf, sizeof(buf));
    if (len < 0) {
        fprintf(stderr, "Build failed!\n");
        return;
    }

    printf("  Built %d bytes\n", len);
    printf("  FC raw: 0x%04x\n", nfs_wifi_fc_pack(&hdr.fc));
    printf("  Type:   %s\n", nfs_wifi_type_name(hdr.fc.type));
    printf("  Subtype: %s\n", nfs_wifi_subtype_name(hdr.fc.type, hdr.fc.subtype));

    /* Parse it back */
    struct nfs_wifi_hdr parsed;
    if (nfs_wifi_hdr_parse(buf, (size_t)len, &parsed) == 0) {
        char a1[18], a2[18], a3[18];
        mac_fmt(parsed.addr1, a1, sizeof(a1));
        mac_fmt(parsed.addr2, a2, sizeof(a2));
        mac_fmt(parsed.addr3, a3, sizeof(a3));
        printf("  Parsed: addr1=%s addr2=%s addr3=%s\n", a1, a2, a3);
        printf("  Seq#=%u  Frag#=%u\n", nfs_wifi_seq_num(parsed.seq_ctrl),
               nfs_wifi_frag_num(parsed.seq_ctrl));
    }
}

static void demo_ack(void) {
    printf("\n--- ACK frame ---\n");

    struct nfs_wifi_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));

    hdr.fc.type = NFS_WIFI_TYPE_CTRL;
    hdr.fc.subtype = NFS_WIFI_CTRL_ACK;
    uint8_t ra[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    memcpy(hdr.addr1, ra, 6);

    uint8_t buf[16];
    int len = nfs_wifi_hdr_build(&hdr, buf, sizeof(buf));
    if (len < 0) {
        fprintf(stderr, "Build failed!\n");
        return;
    }

    printf("  Built %d bytes\n", len);
    printf("  FC raw: 0x%04x\n", nfs_wifi_fc_pack(&hdr.fc));
    printf("  Type:   %s / %s\n", nfs_wifi_type_name(hdr.fc.type),
           nfs_wifi_subtype_name(hdr.fc.type, hdr.fc.subtype));
}

static void demo_data_from_ap(void) {
    printf("\n--- Data frame (from AP) ---\n");

    struct nfs_wifi_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));

    hdr.fc.type = NFS_WIFI_TYPE_DATA;
    hdr.fc.subtype = NFS_WIFI_DATA_DATA;
    hdr.fc.from_ds = 1; /* From AP */

    uint8_t da[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    uint8_t bssid[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};
    uint8_t sa[] = {0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC};
    memcpy(hdr.addr1, da, 6);
    memcpy(hdr.addr2, bssid, 6);
    memcpy(hdr.addr3, sa, 6);
    hdr.seq_ctrl = nfs_wifi_seq_ctrl(100, 0);

    uint8_t buf[32];
    int len = nfs_wifi_hdr_build(&hdr, buf, sizeof(buf));
    if (len < 0) {
        fprintf(stderr, "Build failed!\n");
        return;
    }

    printf("  Built %d bytes\n", len);

    struct nfs_wifi_hdr parsed;
    if (nfs_wifi_hdr_parse(buf, (size_t)len, &parsed) == 0) {
        const uint8_t *pda, *psa, *pbssid;
        nfs_wifi_get_da(&parsed, &pda);
        nfs_wifi_get_sa(&parsed, &psa);
        nfs_wifi_get_bssid(&parsed, &pbssid);

        char s1[18], s2[18], s3[18];
        mac_fmt(pda, s1, sizeof(s1));
        mac_fmt(psa, s2, sizeof(s2));
        mac_fmt(pbssid, s3, sizeof(s3));
        printf("  DA=%s  SA=%s  BSSID=%s\n", s1, s2, s3);
    }
}

int main(void) {
    printf("=== IEEE 802.11 Frame Types Demo ===\n\n");
    demo_beacon();
    demo_ack();
    demo_data_from_ap();
    return 0;
}
