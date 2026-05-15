/* Demo driver for DNS resolver lesson. */

#include "dns_resolver.h"
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
    uint8_t buf[NFS_DNS_MAX_MSG];

    /* Build a DNS query for www.example.com A record */
    printf("=== DNS Query Builder ===\n");
    int qlen = nfs_dns_query_build("www.example.com", NFS_DNS_TYPE_A,
                                    0x1234, buf, sizeof(buf));
    if (qlen < 0) {
        fprintf(stderr, "Failed to build query\n");
        return 1;
    }
    printf("Built %d-byte query for www.example.com A:\n", qlen);
    print_hex(buf, (size_t)qlen);

    /* Demonstrate name encoding */
    printf("\n=== Name Encoding ===\n");
    uint8_t encoded[64];
    int elen = nfs_dns_name_encode("www.google.com", encoded, sizeof(encoded));
    if (elen > 0) {
        printf("www.google.com -> ");
        print_hex(encoded, (size_t)elen);
    }

    /* Demonstrate name decoding */
    printf("\n=== Name Decoding ===\n");
    char decoded[NFS_DNS_MAX_NAME];
    int consumed = nfs_dns_name_decode(encoded, (size_t)elen, 0,
                                        decoded, sizeof(decoded));
    if (consumed > 0) {
        printf("Decoded: %s (consumed %d bytes)\n", decoded, consumed);
    }

    /* Build and parse a fake response */
    printf("\n=== Fake Response Parsing ===\n");
    uint8_t resp_buf[256];
    memset(resp_buf, 0, sizeof(resp_buf));

    /* Header: response with 1 question, 1 answer */
    struct nfs_dns_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.id = htons(0x1234);
    hdr.flags = htons(0x8180); /* QR=1, RD=1, RA=1 */
    hdr.qdcount = htons(1);
    hdr.ancount = htons(1);
    memcpy(resp_buf, &hdr, 12);

    /* Question: www.example.com A IN */
    size_t off = 12;
    int nlen = nfs_dns_name_encode("www.example.com", resp_buf + off, sizeof(resp_buf) - off);
    off += (size_t)nlen;
    uint16_t tmp;
    tmp = htons(NFS_DNS_TYPE_A);
    memcpy(resp_buf + off, &tmp, 2); off += 2;
    tmp = htons(NFS_DNS_CLASS_IN);
    memcpy(resp_buf + off, &tmp, 2); off += 2;

    /* Answer: www.example.com A IN TTL=300 RDATA=93.184.216.34 */
    nlen = nfs_dns_name_encode("www.example.com", resp_buf + off, sizeof(resp_buf) - off);
    off += (size_t)nlen;
    tmp = htons(NFS_DNS_TYPE_A);
    memcpy(resp_buf + off, &tmp, 2); off += 2;
    tmp = htons(NFS_DNS_CLASS_IN);
    memcpy(resp_buf + off, &tmp, 2); off += 2;
    uint32_t ttl = htonl(300);
    memcpy(resp_buf + off, &ttl, 4); off += 4;
    tmp = htons(4); /* RDLENGTH = 4 for IPv4 */
    memcpy(resp_buf + off, &tmp, 2); off += 2;
    uint8_t ip[] = {93, 184, 216, 34};
    memcpy(resp_buf + off, ip, 4); off += 4;

    struct nfs_dns_response resp;
    if (nfs_dns_response_parse(resp_buf, off, &resp) == 0) {
        printf("Response ID: 0x%04x, RCODE: %s\n",
               ntohs(resp.header.id),
               nfs_dns_rcode_str(nfs_dns_rcode(&resp.header)));
        printf("Question: %s type=%s\n", resp.qname,
               nfs_dns_type_str(resp.qtype));
        printf("Answers: %u\n", resp.ancount);
        for (uint16_t i = 0; i < resp.ancount; i++) {
            struct nfs_dns_rr *rr = &resp.answers[i];
            printf("  %s type=%s TTL=%u rdlen=%u",
                   rr->name, nfs_dns_type_str(rr->type),
                   rr->ttl, rr->rdlength);
            if (rr->type == NFS_DNS_TYPE_A && rr->rdlength == 4) {
                printf(" -> %u.%u.%u.%u",
                       rr->rdata[0], rr->rdata[1],
                       rr->rdata[2], rr->rdata[3]);
            }
            printf("\n");
        }
    }

    return 0;
}
