#include "vwire.h"
#include <stdio.h>
#include <string.h>

static void print_hex(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
        printf("%02x ", data[i]);
    printf("\n");
}

static void demo_clean_wire(void)
{
    printf("=== Clean Wire (no impairments) ===\n");

    nfs_wire_cfg_t cfg = {0};
    cfg.seed = 42;
    nfs_wire_t wire;
    nfs_wire_init(&wire, &cfg);

    uint8_t frame[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t out[64];
    size_t out_len;

    printf("Input:  ");
    print_hex(frame, sizeof(frame));

    int rc = nfs_wire_transmit(&wire, frame, sizeof(frame), out, sizeof(out), &out_len);
    printf("Output: ");
    print_hex(out, out_len);
    printf("Result: %s\n", rc == 0 ? "delivered" : "dropped");
    printf("Match:  %s\n\n",
           memcmp(frame, out, sizeof(frame)) == 0 ? "YES" : "NO");
}

static void demo_noisy_wire(void)
{
    printf("=== Noisy Wire (BER = 0.01) ===\n");

    nfs_wire_cfg_t cfg = {0};
    cfg.ber = 0.01;
    cfg.seed = 123;
    nfs_wire_t wire;
    nfs_wire_init(&wire, &cfg);

    uint8_t frame[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t out[64];
    size_t out_len;

    printf("Input:  ");
    print_hex(frame, sizeof(frame));

    nfs_wire_transmit(&wire, frame, sizeof(frame), out, sizeof(out), &out_len);
    printf("Output: ");
    print_hex(out, out_len);

    char stats[256];
    nfs_wire_stats_string(&wire, stats, sizeof(stats));
    printf("Stats:  %s\n\n", stats);
}

static void demo_lossy_wire(void)
{
    printf("=== Lossy Wire (drop_prob = 0.3) ===\n");

    nfs_wire_cfg_t cfg = {0};
    cfg.drop_prob = 0.3;
    cfg.seed = 7;
    nfs_wire_t wire;
    nfs_wire_init(&wire, &cfg);

    uint8_t frame[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t out[64];
    size_t out_len;
    int delivered = 0, dropped_count = 0;

    for (int i = 0; i < 20; i++) {
        int rc = nfs_wire_transmit(&wire, frame, sizeof(frame),
                                   out, sizeof(out), &out_len);
        printf("  Frame %2d: %s\n", i + 1, rc == 0 ? "delivered" : "DROPPED");
        if (rc == 0)
            delivered++;
        else
            dropped_count++;
    }

    printf("Delivered: %d/20, Dropped: %d/20\n", delivered, dropped_count);
    printf("Loss rate: %.2f\n\n", nfs_wire_loss_rate(&wire));
}

static void demo_delay_jitter(void)
{
    printf("=== Delay + Jitter (1000us +/- 200us) ===\n");

    nfs_wire_cfg_t cfg = {0};
    cfg.delay_us = 1000;
    cfg.jitter_us = 200;
    cfg.seed = 99;
    nfs_wire_t wire;
    nfs_wire_init(&wire, &cfg);

    for (int i = 0; i < 10; i++) {
        uint32_t d = nfs_wire_delay_us(&wire);
        printf("  Frame %2d delay: %u us\n", i + 1, d);
    }
    printf("\n");
}

static void demo_full_channel(void)
{
    printf("=== Full Channel (BER=0.001, drop=0.05, delay=500us, jitter=50us) ===\n");

    nfs_wire_cfg_t cfg = {
        .ber = 0.001,
        .drop_prob = 0.05,
        .delay_us = 500,
        .jitter_us = 50,
        .reorder_prob = 0.02,
        .seed = 42
    };
    nfs_wire_t wire;
    nfs_wire_init(&wire, &cfg);

    uint8_t frame[64];
    uint8_t out[64];
    size_t out_len;

    for (int i = 0; i < 100; i++) {
        /* Fill frame with a pattern */
        memset(frame, (uint8_t)i, sizeof(frame));
        nfs_wire_transmit(&wire, frame, sizeof(frame), out, sizeof(out), &out_len);
    }

    char stats[256];
    nfs_wire_stats_string(&wire, stats, sizeof(stats));
    printf("After 100 frames:\n  %s\n\n", stats);
}

int main(void)
{
    printf("Virtual Wire Simulator - Capstone Demo\n");
    printf("=======================================\n\n");

    demo_clean_wire();
    demo_noisy_wire();
    demo_lossy_wire();
    demo_delay_jitter();
    demo_full_channel();

    return 0;
}
