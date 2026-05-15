#include "ip_opts.h"
#include <stdio.h>
#include <string.h>

static void print_hex(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
}

static void demo(void)
{
    printf("=== IP Options Demo ===\n\n");

    /* Build a Record Route option with 4 hops */
    uint8_t rr[40];
    int rr_len = nfs_ip_opts_build_rr(4, rr, sizeof(rr));
    printf("Record Route option (%d bytes): ", rr_len);
    print_hex(rr, (size_t)rr_len);

    /* Build a Timestamp option with 3 entries */
    uint8_t ts[40];
    int ts_len = nfs_ip_opts_build_ts(3, ts, sizeof(ts));
    printf("Timestamp option (%d bytes): ", ts_len);
    print_hex(ts, (size_t)ts_len);

    /* Pad to 4-byte boundary */
    uint8_t padded[40];
    memcpy(padded, rr, (size_t)rr_len);
    size_t padded_len;
    nfs_ip_opts_pad(padded, (size_t)rr_len, &padded_len);
    printf("Padded RR (%zu bytes): ", padded_len);
    print_hex(padded, padded_len);

    /* Parse back */
    struct nfs_ip_option opts[10];
    size_t nfound;
    nfs_ip_opts_parse(rr, (size_t)rr_len, opts, 10, &nfound);
    printf("\nParsed %zu option(s) from Record Route:\n", nfound);
    for (size_t i = 0; i < nfound; i++) {
        printf("  [%zu] type=%u (%s), length=%u\n",
               i, opts[i].type, nfs_ip_opt_name(opts[i].type), opts[i].length);
    }

    /* Parse a NOP+EOL stream */
    uint8_t nop_eol[] = {NFS_IPOPT_NOP, NFS_IPOPT_NOP, NFS_IPOPT_EOL};
    nfs_ip_opts_parse(nop_eol, sizeof(nop_eol), opts, 10, &nfound);
    printf("\nParsed NOP+NOP+EOL: %zu option(s)\n", nfound);
    for (size_t i = 0; i < nfound; i++) {
        printf("  [%zu] %s\n", i, nfs_ip_opt_name(opts[i].type));
    }
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    demo();
    return 0;
}
