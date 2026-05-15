/* Demo driver for EDNS(0) lesson. */

#include "edns.h"
#include <stdio.h>
#include <string.h>

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
    /* Build an OPT RR with NSID and Padding options */
    struct nfs_edns_option opts[2];

    nfs_edns_build_nsid("my-resolver-01", &opts[0]);
    nfs_edns_build_padding(16, &opts[1]);

    uint8_t wire[256];
    int wlen = nfs_edns_build_opt(4096, 0, 0, 1, opts, 2, wire, sizeof(wire));

    printf("=== OPT RR (%d bytes) ===\n", wlen);
    print_hex(wire, (size_t)wlen);

    /* Parse it back */
    struct nfs_edns_opt parsed;
    int consumed = nfs_edns_parse_opt(wire, (size_t)wlen, &parsed);
    printf("\n=== Parsed OPT RR (consumed %d bytes) ===\n", consumed);
    printf("UDP payload size: %u\n", parsed.udp_payload_size);
    printf("Extended RCODE:   %u\n", parsed.flags.ext_rcode);
    printf("EDNS version:     %u\n", parsed.flags.version);
    printf("DO bit:           %d\n", parsed.flags.do_bit);
    printf("Options:          %u\n", parsed.option_count);

    for (uint16_t i = 0; i < parsed.option_count; i++) {
        printf("  Option %u: %s (code=%u, len=%u)\n",
               i, nfs_edns_option_str(parsed.options[i].code),
               parsed.options[i].code, parsed.options[i].length);
        if (parsed.options[i].code == NFS_EDNS_OPT_NSID) {
            printf("    NSID: %.*s\n",
                   parsed.options[i].length, parsed.options[i].data);
        }
    }

    /* Build with cookie */
    printf("\n=== Cookie Example ===\n");
    uint8_t client_cookie[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    uint8_t server_cookie[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    struct nfs_edns_option cookie_opt;
    nfs_edns_build_cookie(client_cookie, server_cookie, 8, &cookie_opt);

    uint8_t wire2[128];
    int wlen2 = nfs_edns_build_opt(1232, 0, 0, 0, &cookie_opt, 1, wire2, sizeof(wire2));
    printf("OPT with Cookie (%d bytes):\n", wlen2);
    print_hex(wire2, (size_t)wlen2);

    return 0;
}
