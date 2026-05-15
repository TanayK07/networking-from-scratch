/* Demo driver for authoritative DNS server lesson. */

#include "dns_auth.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

static void print_hex(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (len % 16 != 0) printf("\n");
}

int main(void)
{
    /* Create a zone for example.com */
    struct nfs_dns_zone zone;
    nfs_dns_zone_init(&zone, "example.com");

    /* Add records */
    nfs_dns_zone_add_a(&zone, "example.com", 3600, 93, 184, 216, 34);
    nfs_dns_zone_add_a(&zone, "www.example.com", 300, 93, 184, 216, 34);
    nfs_dns_zone_add_ns(&zone, "example.com", 86400, "ns1.example.com");
    nfs_dns_zone_add_ns(&zone, "example.com", 86400, "ns2.example.com");
    nfs_dns_zone_add_a(&zone, "ns1.example.com", 86400, 198, 51, 100, 1);
    nfs_dns_zone_add_a(&zone, "ns2.example.com", 86400, 198, 51, 100, 2);

    struct nfs_dns_soa soa = {
        .mname = "ns1.example.com",
        .rname = "admin.example.com",
        .serial = 2024010101,
        .refresh = 3600,
        .retry = 900,
        .expire = 604800,
        .minimum = 86400
    };
    nfs_dns_zone_add_soa(&zone, "example.com", 86400, &soa);

    printf("Zone: %s (%u records)\n\n", zone.origin, zone.rr_count);

    /* Build a query for www.example.com A */
    uint8_t qbuf[512];
    memset(qbuf, 0, sizeof(qbuf));
    struct nfs_dns_hdr qhdr;
    memset(&qhdr, 0, sizeof(qhdr));
    qhdr.id = htons(0xBEEF);
    qhdr.flags = htons(0x0100); /* RD=1 */
    qhdr.qdcount = htons(1);
    memcpy(qbuf, &qhdr, 12);
    size_t off = 12;
    int nlen = nfs_dns_name_encode("www.example.com", qbuf + off, sizeof(qbuf) - off);
    off += (size_t)nlen;
    uint16_t tmp = htons(NFS_DNS_TYPE_A);
    memcpy(qbuf + off, &tmp, 2); off += 2;
    tmp = htons(NFS_DNS_CLASS_IN);
    memcpy(qbuf + off, &tmp, 2); off += 2;

    printf("Query (%zu bytes):\n", off);
    print_hex(qbuf, off);

    /* Build response */
    uint8_t rbuf[512];
    int rlen = nfs_dns_zone_build_response(&zone, qbuf, off, rbuf, sizeof(rbuf));
    if (rlen > 0) {
        printf("\nResponse (%d bytes):\n", rlen);
        print_hex(rbuf, (size_t)rlen);
    }

    /* Lookup test */
    struct nfs_dns_lookup_result result;
    int count = nfs_dns_zone_lookup(&zone, "example.com", NFS_DNS_TYPE_NS, &result);
    printf("\nNS records for example.com: %d\n", count);
    for (uint16_t i = 0; i < result.count; i++) {
        printf("  %s type=%u ttl=%u rdlen=%u\n",
               result.matches[i]->name,
               result.matches[i]->type,
               result.matches[i]->ttl,
               result.matches[i]->rdlength);
    }

    return 0;
}
