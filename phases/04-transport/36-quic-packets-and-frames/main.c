/*
 * main.c -- QUIC packets and frames demo
 */
#include "quic.h"
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
    printf("=== QUIC Packets and Frames ===\n\n");

    /* Variable-length integer encoding */
    printf("--- Variable-Length Integers ---\n");
    uint64_t test_vals[] = {0, 37, 15293, 494878333, 151288809941952652ULL};
    for (size_t i = 0; i < sizeof(test_vals)/sizeof(test_vals[0]); i++) {
        uint8_t buf[8];
        int n = nfs_quic_varint_encode(test_vals[i], buf, sizeof(buf));
        printf("  %20llu -> %d bytes: ", (unsigned long long)test_vals[i], n);
        hex_print(buf, (size_t)n);

        uint64_t decoded;
        nfs_quic_varint_decode(buf, (size_t)n, &decoded);
        printf("  Decoded: %llu (match=%s)\n",
               (unsigned long long)decoded,
               decoded == test_vals[i] ? "yes" : "no");
    }

    /* Long header */
    printf("\n--- Long Header (Initial) ---\n");
    struct nfs_quic_long_hdr lhdr = {
        .first_byte = NFS_QUIC_HEADER_FORM_BIT | NFS_QUIC_FIXED_BIT |
                      (NFS_QUIC_LONG_TYPE_INITIAL << 4),
        .version = NFS_QUIC_VERSION_1,
        .dcid_len = 8,
        .dcid = {0x83, 0x94, 0xc8, 0xf0, 0x3e, 0x51, 0x57, 0x08},
        .scid_len = 0,
    };
    uint8_t buf[128];
    int n = nfs_quic_long_hdr_build(&lhdr, buf, sizeof(buf));
    printf("  Built (%d bytes): ", n);
    hex_print(buf, (size_t)n);
    printf("  Type: %s\n", nfs_quic_long_type_name(nfs_quic_long_hdr_type(lhdr.first_byte)));

    /* Short header */
    printf("\n--- Short Header ---\n");
    struct nfs_quic_short_hdr shdr = {
        .first_byte = NFS_QUIC_FIXED_BIT | NFS_QUIC_SHORT_SPIN_BIT,
        .dcid_len = 4,
        .dcid = {0x01, 0x02, 0x03, 0x04},
    };
    n = nfs_quic_short_hdr_build(&shdr, buf, sizeof(buf));
    printf("  Built (%d bytes): ", n);
    hex_print(buf, (size_t)n);
    printf("  Spin: %d, Key Phase: %d\n",
           nfs_quic_short_hdr_spin(shdr.first_byte),
           nfs_quic_short_hdr_key_phase(shdr.first_byte));

    /* Frame types */
    printf("\n--- Frame Types ---\n");
    uint8_t frames[] = {0x00, 0x01, 0x02, 0x03, 0x08, 0x0B, 0x0F};
    for (size_t i = 0; i < sizeof(frames); i++) {
        printf("  0x%02x: %s", frames[i], nfs_quic_frame_name(frames[i]));
        if (nfs_quic_is_stream_frame(frames[i])) {
            printf(" (FIN=%d, LEN=%d, OFF=%d)",
                   nfs_quic_stream_has_fin(frames[i]),
                   nfs_quic_stream_has_len(frames[i]),
                   nfs_quic_stream_has_off(frames[i]));
        }
        printf("\n");
    }

    return 0;
}
