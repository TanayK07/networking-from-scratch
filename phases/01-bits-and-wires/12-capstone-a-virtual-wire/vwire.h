#ifndef NFS_VWIRE_H
#define NFS_VWIRE_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Virtual Wire Simulator
 *
 * Models a physical communication channel with configurable
 * impairments: bit errors (BER), packet drops, propagation
 * delay, jitter, and frame reordering.
 *
 * All randomness uses a seeded xorshift32 PRNG for reproducible
 * simulations.
 * --------------------------------------------------------------- */

/* Wire configuration — all impairment parameters */
typedef struct {
    double   ber;           /* Bit Error Rate: probability of flipping each bit (0.0-1.0) */
    double   drop_prob;     /* Frame drop probability (0.0-1.0) */
    uint32_t delay_us;      /* Base propagation delay in microseconds */
    uint32_t jitter_us;     /* Jitter std deviation in microseconds (added to delay) */
    double   reorder_prob;  /* Probability of swapping adjacent frames (0.0-1.0) */
    uint32_t seed;          /* RNG seed for reproducibility */
} nfs_wire_cfg_t;

/* Wire state — configuration + statistics + PRNG state */
typedef struct {
    nfs_wire_cfg_t cfg;
    uint64_t tx_frames;     /* frames submitted */
    uint64_t rx_frames;     /* frames delivered */
    uint64_t dropped;       /* frames dropped */
    uint64_t bit_errors;    /* total bits flipped */
    uint64_t reordered;     /* frames reordered */
    uint32_t rng_state;     /* PRNG state (xorshift32) */
} nfs_wire_t;

/* ---- Initialization ---- */

/* Initialize wire with given config, seed PRNG, zero stats. */
void nfs_wire_init(nfs_wire_t *w, const nfs_wire_cfg_t *cfg);

/* Zero all counters (tx_frames, rx_frames, dropped, bit_errors, reordered). */
void nfs_wire_reset_stats(nfs_wire_t *w);

/* ---- PRNG ---- */

/* xorshift32: return next pseudo-random uint32_t, advance state. */
uint32_t nfs_wire_rand(nfs_wire_t *w);

/* Return a pseudo-random double in [0.0, 1.0). */
double nfs_wire_rand_double(nfs_wire_t *w);

/* ---- Channel impairments ---- */

/* Flip bits in `frame` (len bytes) with probability `ber` per bit.
 * Returns the number of bits flipped. */
size_t nfs_wire_apply_ber(nfs_wire_t *w, uint8_t *frame, size_t len);

/* Return 1 if the current frame should be dropped, 0 otherwise. */
int nfs_wire_should_drop(nfs_wire_t *w);

/* Compute propagation delay (microseconds) for this frame.
 * Includes base delay + Gaussian jitter (Box-Muller), clamped >= 0. */
uint32_t nfs_wire_delay_us(nfs_wire_t *w);

/* Return 1 if the current frame should be reordered (swapped with next). */
int nfs_wire_should_reorder(nfs_wire_t *w);

/* ---- Frame transmission ---- */

/* Process one frame through the virtual wire.
 *   in/in_len  — input frame
 *   out/out_sz — output buffer
 *   *out_len   — set to output frame length on success
 *
 * Returns:  0 = frame delivered
 *           1 = frame dropped
 *          -1 = error (NULL pointer, buffer too small) */
int nfs_wire_transmit(nfs_wire_t *w, const uint8_t *in, size_t in_len,
                      uint8_t *out, size_t out_sz, size_t *out_len);

/* ---- Statistics ---- */

/* Write a human-readable stats summary into buf (at most sz bytes). */
void nfs_wire_stats_string(const nfs_wire_t *w, char *buf, size_t sz);

/* Return the frame loss rate: dropped / tx_frames (0.0 if no frames sent). */
double nfs_wire_loss_rate(const nfs_wire_t *w);

#endif /* NFS_VWIRE_H */
