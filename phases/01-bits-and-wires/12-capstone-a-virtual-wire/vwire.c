#include "vwire.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- Initialization ---- */

void nfs_wire_init(nfs_wire_t *w, const nfs_wire_cfg_t *cfg) {
    if (!w)
        return;
    if (cfg)
        w->cfg = *cfg;
    else
        memset(&w->cfg, 0, sizeof(w->cfg));
    w->tx_frames = 0;
    w->rx_frames = 0;
    w->dropped = 0;
    w->bit_errors = 0;
    w->reordered = 0;
    w->rng_state = w->cfg.seed ? w->cfg.seed : 1; /* avoid zero state */
}

void nfs_wire_reset_stats(nfs_wire_t *w) {
    if (!w)
        return;
    w->tx_frames = 0;
    w->rx_frames = 0;
    w->dropped = 0;
    w->bit_errors = 0;
    w->reordered = 0;
}

/* ---- PRNG (xorshift32) ---- */

uint32_t nfs_wire_rand(nfs_wire_t *w) {
    uint32_t s = w->rng_state;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    w->rng_state = s;
    return s;
}

double nfs_wire_rand_double(nfs_wire_t *w) {
    uint32_t r = nfs_wire_rand(w);
    return (double)(r >> 1) / (double)(UINT32_MAX >> 1);
}

/* ---- Channel impairments ---- */

size_t nfs_wire_apply_ber(nfs_wire_t *w, uint8_t *frame, size_t len) {
    if (w->cfg.ber <= 0.0 || len == 0)
        return 0;

    size_t flipped = 0;
    size_t total_bits = len * 8;

    for (size_t i = 0; i < total_bits; i++) {
        double r = nfs_wire_rand_double(w);
        if (r < w->cfg.ber) {
            size_t byte_idx = i / 8;
            uint8_t bit_mask = (uint8_t)(1 << (7 - (i % 8)));
            frame[byte_idx] ^= bit_mask;
            flipped++;
        }
    }

    return flipped;
}

int nfs_wire_should_drop(nfs_wire_t *w) {
    if (w->cfg.drop_prob <= 0.0)
        return 0;
    if (w->cfg.drop_prob >= 1.0)
        return 1;
    return nfs_wire_rand_double(w) < w->cfg.drop_prob ? 1 : 0;
}

uint32_t nfs_wire_delay_us(nfs_wire_t *w) {
    if (w->cfg.jitter_us == 0)
        return w->cfg.delay_us;

    /* Box-Muller transform: generate Gaussian from two uniform samples */
    double u1 = nfs_wire_rand_double(w);
    double u2 = nfs_wire_rand_double(w);

    /* Avoid log(0) */
    if (u1 < 1e-15)
        u1 = 1e-15;

    double gaussian = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    double jitter = gaussian * (double)w->cfg.jitter_us;

    double total = (double)w->cfg.delay_us + jitter;
    if (total < 0.0)
        total = 0.0;

    return (uint32_t)total;
}

int nfs_wire_should_reorder(nfs_wire_t *w) {
    if (w->cfg.reorder_prob <= 0.0)
        return 0;
    if (w->cfg.reorder_prob >= 1.0)
        return 1;
    return nfs_wire_rand_double(w) < w->cfg.reorder_prob ? 1 : 0;
}

/* ---- Frame transmission ---- */

int nfs_wire_transmit(nfs_wire_t *w, const uint8_t *in, size_t in_len, uint8_t *out, size_t out_sz,
                      size_t *out_len) {
    if (!w || !out_len)
        return -1;

    /* Allow zero-length frames (valid), but NULL in with non-zero len is an error */
    if (!in && in_len > 0)
        return -1;

    if (out_sz < in_len)
        return -1;

    w->tx_frames++;

    /* Check for drop */
    if (nfs_wire_should_drop(w)) {
        w->dropped++;
        *out_len = 0;
        return 1;
    }

    /* Copy frame to output */
    if (in_len > 0)
        memcpy(out, in, in_len);
    *out_len = in_len;

    /* Apply bit errors */
    size_t flips = nfs_wire_apply_ber(w, out, in_len);
    w->bit_errors += flips;

    w->rx_frames++;
    return 0;
}

/* ---- Statistics ---- */

void nfs_wire_stats_string(const nfs_wire_t *w, char *buf, size_t sz) {
    if (!w || !buf || sz == 0)
        return;
    snprintf(buf, sz,
             "TX: %llu  RX: %llu  Dropped: %llu  "
             "Bit errors: %llu  Reordered: %llu  "
             "Loss rate: %.4f",
             (unsigned long long)w->tx_frames, (unsigned long long)w->rx_frames,
             (unsigned long long)w->dropped, (unsigned long long)w->bit_errors,
             (unsigned long long)w->reordered, nfs_wire_loss_rate(w));
}

double nfs_wire_loss_rate(const nfs_wire_t *w) {
    if (!w || w->tx_frames == 0)
        return 0.0;
    return (double)w->dropped / (double)w->tx_frames;
}
