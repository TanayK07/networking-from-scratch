/* IP Fragmentation Demo
 *
 * Demonstrates fragmenting a 4000-byte IP packet at MTU=1500
 * and reassembling the fragments back into the original packet.
 */

#include "fragment.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

/* Simple helper to compute a checksum for building demo packets. */
static uint16_t demo_checksum(const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0;

    for (size_t i = 0; i + 1 < len; i += 2)
        sum += (uint32_t)((uint16_t)p[i] << 8 | p[i + 1]);
    if (len & 1)
        sum += (uint32_t)p[len - 1] << 8;

    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    return htons((uint16_t)~sum);
}

int main(void)
{
    printf("=== IP Fragmentation Demo ===\n\n");

    /* Build a 4020-byte IP packet: 20-byte header + 4000-byte payload. */
    uint8_t original[4020];
    memset(original, 0, sizeof(original));

    struct nfs_ipv4_hdr *hdr = (struct nfs_ipv4_hdr *)original;
    hdr->ver_ihl   = 0x45;                /* IPv4, IHL=5                */
    hdr->tos       = 0;
    hdr->total_len = htons(4020);          /* 20 + 4000                  */
    hdr->id        = htons(0x1234);        /* identification             */
    hdr->flags_frag = 0;                   /* no flags, offset=0         */
    hdr->ttl       = 64;
    hdr->protocol  = 17;                   /* UDP                        */
    hdr->src_addr  = htonl(0xC0A80001);    /* 192.168.0.1                */
    hdr->dst_addr  = htonl(0xC0A80002);    /* 192.168.0.2                */
    hdr->checksum  = 0;
    hdr->checksum  = demo_checksum(hdr, 20);

    /* Fill payload with a recognizable pattern. */
    for (int i = 0; i < 4000; i++)
        original[20 + i] = (uint8_t)(i & 0xFF);

    printf("Original packet: %u bytes (header=%u, payload=%u)\n",
           ntohs(hdr->total_len), 20, 4000);
    printf("  ID=0x%04x  TTL=%u  Protocol=%u\n",
           ntohs(hdr->id), hdr->ttl, hdr->protocol);
    printf("  Src=0x%08x  Dst=0x%08x\n\n",
           ntohl(hdr->src_addr), ntohl(hdr->dst_addr));

    /* Fragment at MTU=1500. */
    struct nfs_ip_fragment frags[10];
    int nfrags = nfs_ip_fragment_packet(original, sizeof(original),
                                        1500, frags, 10);
    if (nfrags < 0) {
        fprintf(stderr, "Fragmentation failed: %d\n", nfrags);
        return 1;
    }

    printf("Fragmented into %d fragments (MTU=1500):\n", nfrags);
    for (int i = 0; i < nfrags; i++) {
        const struct nfs_ipv4_hdr *fh =
            (const struct nfs_ipv4_hdr *)frags[i].header;
        uint16_t ff = ntohs(fh->flags_frag);
        uint16_t off = nfs_ip_get_frag_offset(ff);
        int mf = (ff & NFS_IP_FLAG_MF) ? 1 : 0;

        printf("  Fragment %d: total_len=%u  offset=%u (bytes=%u)  MF=%d  "
               "payload=%zu bytes\n",
               i, ntohs(fh->total_len), off, off * 8, mf,
               frags[i].payload_len);
    }

    /* Reassemble. */
    uint8_t reassembled[8192];
    int total = nfs_ip_reassemble(frags, (size_t)nfrags,
                                  reassembled, sizeof(reassembled));
    if (total < 0) {
        fprintf(stderr, "\nReassembly failed!\n");
        return 1;
    }

    printf("\nReassembled packet: %d bytes\n", total);

    /* Verify payload matches. */
    if (total == 4020 && memcmp(original + 20, reassembled + 20, 4000) == 0)
        printf("Payload matches original: OK\n");
    else
        printf("Payload mismatch!\n");

    return 0;
}
