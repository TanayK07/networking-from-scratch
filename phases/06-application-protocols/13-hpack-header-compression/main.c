#include "hpack.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* Demo: integer encoding/decoding */
    printf("=== HPACK Integer Encoding ===\n");

    uint8_t buf[16];
    int n;

    /* Encode 10 with 5-bit prefix */
    n = nfs_hpack_encode_int(buf, sizeof(buf), 10, 5);
    printf("Encode 10 (5-bit prefix): %d byte(s) -> 0x%02x\n", n, buf[0]);

    /* Encode 1337 with 5-bit prefix */
    n = nfs_hpack_encode_int(buf, sizeof(buf), 1337, 5);
    printf("Encode 1337 (5-bit prefix): %d byte(s) -> ", n);
    for (int i = 0; i < n; i++) printf("0x%02x ", buf[i]);
    printf("\n");

    /* Decode it back */
    uint64_t val;
    int consumed = nfs_hpack_decode_int(buf, (size_t)n, &val, 5);
    printf("Decode back: value=%llu, consumed=%d bytes\n",
           (unsigned long long)val, consumed);

    /* Demo: static table */
    printf("\n=== HPACK Static Table ===\n");
    const char *name, *value;
    for (int i = 1; i <= 8; i++) {
        if (nfs_hpack_static_lookup(i, &name, &value) == 0)
            printf("  [%2d] %s: %s\n", i, name, value);
    }

    /* Find :method GET */
    int idx = nfs_hpack_static_find(":method", "GET");
    printf("  Find ':method: GET' -> index %d\n", idx);

    /* Demo: dynamic table */
    printf("\n=== HPACK Dynamic Table ===\n");
    struct nfs_hpack_dynamic_table dt;
    nfs_hpack_dyn_init(&dt, 4096);

    nfs_hpack_dyn_add(&dt, "custom-header", "custom-value");
    nfs_hpack_dyn_add(&dt, "x-request-id", "abc123");

    printf("Dynamic table has %d entries\n", nfs_hpack_dyn_count(&dt));
    for (int i = 0; i < nfs_hpack_dyn_count(&dt); i++) {
        if (nfs_hpack_dyn_lookup(&dt, i, &name, &value) == 0)
            printf("  [dyn %d] %s: %s\n", i, name, value);
    }

    return 0;
}
