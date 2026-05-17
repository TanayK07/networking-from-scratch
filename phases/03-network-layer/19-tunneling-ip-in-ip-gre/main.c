/*
 * main.c -- IP-in-IP and GRE tunneling demo
 */
#include "tunnel.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

static void hex_print(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
}

int main(void) {
    printf("=== IP-in-IP Tunneling ===\n\n");

    /* Build a tiny "inner" IP packet (just a header for demo) */
    uint8_t inner[20];
    struct nfs_ipv4_hdr *ihdr = (struct nfs_ipv4_hdr *)inner;
    memset(ihdr, 0, 20);
    ihdr->ver_ihl = 0x45;
    ihdr->total_len = htons(20);
    ihdr->ttl = 64;
    ihdr->protocol = 6;                 /* TCP */
    ihdr->src_addr = htonl(0xC0A80101); /* 192.168.1.1 */
    ihdr->dst_addr = htonl(0xC0A80201); /* 192.168.2.1 */
    ihdr->checksum = nfs_ip_checksum(ihdr, 20);

    printf("Inner packet (%zu bytes): ", sizeof(inner));
    hex_print(inner, sizeof(inner));

    /* Encapsulate */
    uint8_t tunnel_buf[128];
    int tlen = nfs_ipip_encapsulate(htonl(0x0A000001), htonl(0x0A000002), 128, inner, sizeof(inner),
                                    tunnel_buf, sizeof(tunnel_buf));
    printf("IPIP packet (%d bytes):  ", tlen);
    hex_print(tunnel_buf, (size_t)tlen);

    /* Decapsulate */
    size_t dec_len;
    const uint8_t *dec = nfs_ipip_decapsulate(tunnel_buf, (size_t)tlen, &dec_len);
    printf("Decapsulated (%zu bytes): ", dec_len);
    hex_print(dec, dec_len);
    printf("Roundtrip OK: %s\n\n", memcmp(dec, inner, sizeof(inner)) == 0 ? "yes" : "no");

    /* --- GRE demo --- */
    printf("=== GRE Tunneling ===\n\n");

    uint8_t payload[] = {0x45, 0x00, 0x00, 0x28, 0x01, 0x02, 0x03, 0x04};
    uint8_t gre_buf[128];
    int glen = nfs_gre_encapsulate(NFS_GRE_FLAG_K | NFS_GRE_FLAG_S, NFS_GRE_PROTO_IPV4, 42, 1,
                                   payload, sizeof(payload), gre_buf, sizeof(gre_buf));
    printf("GRE packet (%d bytes): ", glen);
    hex_print(gre_buf, (size_t)glen);

    struct nfs_gre_hdr ghdr;
    if (nfs_gre_parse(gre_buf, (size_t)glen, &ghdr) == 0) {
        printf("  Protocol: 0x%04x\n", ghdr.protocol);
        printf("  Key:      %u\n", ghdr.key);
        printf("  Seq:      %u\n", ghdr.seq);
        printf("  Hdr len:  %zu\n", ghdr.hdr_len);
    }

    return 0;
}
