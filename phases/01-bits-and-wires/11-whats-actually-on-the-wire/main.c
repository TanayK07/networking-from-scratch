#include "wire.h"
#include <stdio.h>
#include <string.h>

static void print_hex(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
}

static void demo_preamble(void)
{
    printf("=== Ethernet Preamble & SFD (IEEE 802.3) ===\n");

    uint8_t preamble[8];
    int n = nfs_preamble_generate(preamble, sizeof(preamble));
    printf("Generated preamble (%d bytes): ", n);
    print_hex(preamble, (size_t)n);

    printf("  Bytes 0-6 (preamble): 7x 0xAA = alternating 10101010\n");
    printf("  Byte  7   (SFD):      0xAB = 10101011 (marks frame start)\n");

    /* Detect preamble in a buffer with some leading noise */
    uint8_t wire[16] = {0x00, 0x42, 0xFF};
    memcpy(wire + 3, preamble, 8);
    int sfd_offset = nfs_preamble_detect(wire, sizeof(wire));
    printf("SFD detected at offset %d (expected 10 = 3 noise + 7 preamble)\n\n",
           sfd_offset);
}

static void demo_ifg(void)
{
    printf("=== Interframe Gap (IFG) ===\n");

    int speeds[] = {10, 100, 1000, 10000};
    const char *names[] = {"10 Mbps", "100 Mbps", "1 Gbps", "10 Gbps"};

    for (int i = 0; i < 4; i++) {
        printf("  %s: %d bytes, %d ns\n",
               names[i], nfs_ifg_bytes(speeds[i]), nfs_ifg_ns(speeds[i]));
    }
    printf("\n");
}

static void demo_8b10b(void)
{
    printf("=== 8b/10b Encoding Concepts ===\n");

    /* Running disparity tracking */
    int rd = -1;
    printf("  Starting RD: %d\n", rd);

    /* Simulate: symbol with 6 ones (unbalanced toward 1s) */
    uint16_t sym_heavy = 0x1F7; /* 0111110111 = 8 ones */
    rd = nfs_rd_update(rd, sym_heavy);
    printf("  After symbol 0x%03X (%d ones): RD = %+d\n",
           sym_heavy, 8, rd);

    /* Symbol with 5 ones (balanced) */
    uint16_t sym_balanced = 0x1A5; /* 5 ones */
    rd = nfs_rd_update(rd, sym_balanced);
    printf("  After symbol 0x%03X (5 ones):  RD = %+d (unchanged)\n",
           sym_balanced, rd);

    /* K28.5 comma detection */
    printf("  K28.5 RD-: 0x%03X is comma? %s\n",
           NFS_K28_5_RDN, nfs_is_comma(NFS_K28_5_RDN) ? "yes" : "no");
    printf("  K28.5 RD+: 0x%03X is comma? %s\n",
           NFS_K28_5_RDP, nfs_is_comma(NFS_K28_5_RDP) ? "yes" : "no");
    printf("  Random 0x155: comma? %s\n\n",
           nfs_is_comma(0x155) ? "yes" : "no");
}

static void demo_frame_overhead(void)
{
    printf("=== Frame on the Wire ===\n");
    printf("  Physical frame layout:\n");
    printf("  [Preamble 7B][SFD 1B][Dest 6B][Src 6B][Type 2B]"
           "[Payload 46-1500B][FCS 4B][IFG 12B]\n");
    printf("  Total overhead per frame: %d bytes\n", nfs_wire_frame_overhead());
    printf("  Min frame (L2): %d bytes\n", nfs_min_frame_size());
    printf("  Max frame (L2): %d bytes\n", nfs_max_frame_size());

    printf("\n  Wire efficiency:\n");
    size_t payloads[] = {46, 576, 1500, 9000};
    for (int i = 0; i < 4; i++) {
        printf("    %5zu-byte payload: %.2f%%\n",
               payloads[i], nfs_wire_efficiency(payloads[i]) * 100.0);
    }
    printf("\n");
}

static void demo_timing(void)
{
    printf("=== Bit Times and Frame Timing ===\n");

    int speeds[] = {10, 100, 1000, 10000};
    const char *names[] = {"10 Mbps", "100 Mbps", "1 Gbps", "10 Gbps"};

    for (int i = 0; i < 4; i++) {
        printf("  %s: bit time = %d ns\n", names[i], nfs_bit_time_ns(speeds[i]));
    }

    printf("\n  Frame transmission times:\n");
    printf("    64B at 100 Mbps:   %lld ns\n",
           nfs_frame_time_ns(64, 100));
    printf("    1518B at 1 Gbps:   %lld ns\n",
           nfs_frame_time_ns(1518, 1000));
    printf("    64B at 10 Gbps:    %lld ns (sub-ns bit time)\n",
           nfs_frame_time_ns(64, 10000));
}

int main(void)
{
    demo_preamble();
    demo_ifg();
    demo_8b10b();
    demo_frame_overhead();
    demo_timing();
    return 0;
}
