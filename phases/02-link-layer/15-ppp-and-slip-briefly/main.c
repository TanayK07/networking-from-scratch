/*
 * main.c -- SLIP and PPP framing demo
 */
#include "slip_ppp.h"
#include <stdio.h>
#include <string.h>

static void hex_print(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
}

int main(void)
{
    /* --- SLIP demo --- */
    printf("=== SLIP Framing ===\n\n");

    uint8_t ip_pkt[] = {0x45, 0x00, 0xC0, 0xDB, 0x01, 0x02};
    printf("Original IP packet (%zu bytes): ", sizeof(ip_pkt));
    hex_print(ip_pkt, sizeof(ip_pkt));

    uint8_t slip_frame[64];
    int slip_len = nfs_slip_encode(ip_pkt, sizeof(ip_pkt),
                                   slip_frame, sizeof(slip_frame));
    printf("SLIP encoded (%d bytes):        ", slip_len);
    hex_print(slip_frame, (size_t)slip_len);

    uint8_t decoded[64];
    int dec_len = nfs_slip_decode(slip_frame, (size_t)slip_len,
                                  decoded, sizeof(decoded));
    printf("SLIP decoded (%d bytes):        ", dec_len);
    hex_print(decoded, (size_t)dec_len);
    printf("Roundtrip OK: %s\n\n",
           (dec_len == (int)sizeof(ip_pkt) &&
            memcmp(decoded, ip_pkt, sizeof(ip_pkt)) == 0) ? "yes" : "no");

    /* --- PPP demo --- */
    printf("=== PPP Framing ===\n\n");

    const char *msg = "Hello PPP";
    uint8_t ppp_buf[128];
    int ppp_len = nfs_ppp_frame_build(NFS_PPP_PROTO_IPV4,
                                      (const uint8_t *)msg, strlen(msg),
                                      ppp_buf, sizeof(ppp_buf));
    printf("PPP frame (%d bytes): ", ppp_len);
    hex_print(ppp_buf, (size_t)ppp_len);

    struct nfs_ppp_frame parsed;
    if (nfs_ppp_frame_parse(ppp_buf, (size_t)ppp_len, &parsed) == 0) {
        printf("  Address:  0x%02X\n", parsed.address);
        printf("  Control:  0x%02X\n", parsed.control);
        printf("  Protocol: 0x%04X (%s)\n",
               parsed.protocol, nfs_ppp_protocol_name(parsed.protocol));
        printf("  Payload:  %.*s\n", parsed.payload_len, parsed.payload);
        printf("  FCS:      0x%04X\n", parsed.fcs);
    }

    printf("\nFCS-16 test: 0x%04X\n",
           nfs_ppp_fcs16((const uint8_t *)"123456789", 9));

    return 0;
}
