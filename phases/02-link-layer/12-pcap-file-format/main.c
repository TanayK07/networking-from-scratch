/*
 * pcap_tool — demonstrate PCAP file writing and reading.
 *
 * Build:  make
 * Run:    ./pcap_tool
 *
 * Creates a temporary PCAP file, writes two fake Ethernet frames,
 * then reads them back and prints a summary.
 */
#include "pcap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("=== PCAP File Format Demo ===\n\n");

    const char *fname = "/tmp/nfs_demo.pcap";
    FILE *fp = fopen(fname, "wb");
    if (!fp) {
        perror("fopen write");
        return 1;
    }

    /* Write global header. */
    if (nfs_pcap_write_header(fp, 65535, NFS_PCAP_LINKTYPE_ETHERNET) != 0) {
        fprintf(stderr, "Failed to write PCAP header\n");
        fclose(fp);
        return 1;
    }

    /* Write two fake packets. */
    uint8_t pkt1[60];
    memset(pkt1, 0, sizeof(pkt1));
    memset(pkt1, 0xFF, 6);
    pkt1[12] = 0x08;
    pkt1[13] = 0x00;

    uint8_t pkt2[100];
    memset(pkt2, 0xAA, sizeof(pkt2));
    pkt2[12] = 0x08;
    pkt2[13] = 0x06;

    nfs_pcap_write_packet(fp, 1700000000, 0, pkt1, sizeof(pkt1));
    nfs_pcap_write_packet(fp, 1700000001, 500000, pkt2, sizeof(pkt2));
    fclose(fp);
    printf("Wrote %s\n\n", fname);

    /* Read it back. */
    fp = fopen(fname, "rb");
    if (!fp) {
        perror("fopen read");
        return 1;
    }

    struct nfs_pcap_global_hdr ghdr;
    if (nfs_pcap_read_header(fp, &ghdr) != 0) {
        fprintf(stderr, "Failed to read PCAP header\n");
        fclose(fp);
        return 1;
    }

    char summary[256];
    nfs_pcap_format_header(&ghdr, summary, sizeof(summary));
    printf("Header: %s\n\n", summary);

    struct nfs_pcap_pkt_hdr phdr;
    uint8_t buf[65536];
    int pkt_num = 0;
    int rc;
    while ((rc = nfs_pcap_read_packet(fp, &phdr, buf, sizeof(buf))) == 0) {
        pkt_num++;
        printf("Packet %d: ts=%u.%06u  len=%u  orig=%u\n", pkt_num, phdr.ts_sec, phdr.ts_usec,
               phdr.incl_len, phdr.orig_len);
    }
    if (rc == -1) {
        fprintf(stderr, "Error reading packet\n");
    }

    printf("\nTotal: %d packets\n", pkt_num);
    fclose(fp);
    return 0;
}
