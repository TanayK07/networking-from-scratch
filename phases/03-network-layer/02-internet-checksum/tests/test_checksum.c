/* Tests for the Internet checksum (RFC 1071) and incremental update (RFC 1624).
 *
 * Three families:
 *   1. Pinned cases — known-answer inputs from real captures.
 *   2. Property tests — sum of (data || cs) should fold to zero.
 *   3. Incremental-update parity — patching one field should equal recompute.
 */

#include "../../../../common/c/checksum.h"
#include "../incremental.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A real IPv4 header from a captured ICMP echo request.
 * The 16-bit field at offset 10 is the checksum itself. If we zero it
 * and recompute, we should get the original value back. */
static void test_real_ipv4_header(void) {
    uint8_t hdr[20] = {
        0x45, 0x00, 0x00, 0x54,    /* version/IHL, ToS, total len */
        0x00, 0x00, 0x40, 0x00,    /* ID, flags+frag offset */
        0x40, 0x01,                 /* TTL=64, proto=ICMP */
        0x00, 0x00,                 /* checksum (zeroed) */
        0x0a, 0x00, 0x00, 0x01,    /* src 10.0.0.1 */
        0x0a, 0x00, 0x00, 0x02     /* dst 10.0.0.2 */
    };
    uint16_t cs = internet_checksum(hdr, sizeof(hdr));
    /* Hand-computed (verified with python: see lesson README). */
    if (cs != 0x26a7) {
        fprintf(stderr, "  FAIL ipv4_header: got 0x%04x want 0x26a7\n", cs);
        exit(1);
    }
    printf("  real IPv4 header... OK (0x%04x)\n", cs);
}

/* Receiver-side: checksum over (data || on-wire cs field) folds to 0. */
static void test_self_check_property(void) {
    srand(42);
    for (int trial = 0; trial < 100; trial++) {
        uint8_t data[64];
        for (size_t i = 0; i < sizeof(data); i++) data[i] = (uint8_t)rand();

        uint16_t cs = internet_checksum(data, sizeof(data));

        uint8_t ext[sizeof(data) + 2];
        memcpy(ext, data, sizeof(data));
        ext[sizeof(data)]     = (uint8_t)(cs >> 8);
        ext[sizeof(data) + 1] = (uint8_t)(cs & 0xFF);

        uint16_t verify = internet_checksum(ext, sizeof(ext));
        /* (~sum + sum) folded == 0xFFFF, complement == 0x0000 */
        if (verify != 0x0000) {
            fprintf(stderr, "  FAIL self_check trial %d: got 0x%04x\n",
                    trial, verify);
            exit(1);
        }
    }
    printf("  self-check property (100 trials)... OK\n");
}

/* Incremental update should produce the same checksum as a full recompute. */
static void test_incremental_parity(void) {
    srand(123);

    for (int trial = 0; trial < 1000; trial++) {
        uint8_t buf[64];
        for (size_t i = 0; i < sizeof(buf); i++) buf[i] = (uint8_t)rand();

        size_t off = (size_t)((rand() & 0x1F) * 2);
        if (off >= sizeof(buf) - 2) off = 0;

        uint16_t old_word = (uint16_t)((buf[off] << 8) | buf[off + 1]);
        uint16_t new_word = (uint16_t)rand();

        uint16_t cs_before = internet_checksum(buf, sizeof(buf));

        buf[off]     = (uint8_t)(new_word >> 8);
        buf[off + 1] = (uint8_t)(new_word & 0xFF);

        uint16_t cs_full        = internet_checksum(buf, sizeof(buf));
        uint16_t cs_incremental =
            internet_checksum_update(cs_before, old_word, new_word);

        if (cs_full != cs_incremental) {
            fprintf(stderr,
                "  FAIL incremental_parity trial %d: full=0x%04x incr=0x%04x\n",
                trial, cs_full, cs_incremental);
            exit(1);
        }
    }
    printf("  incremental update parity (1000 trials)... OK\n");
}

/* Order independence: chunked sum equals whole sum. */
static void test_chunked_parity(void) {
    srand(456);
    uint8_t buf[100];
    for (size_t i = 0; i < sizeof(buf); i++) buf[i] = (uint8_t)rand();

    uint16_t whole = internet_checksum(buf, sizeof(buf));

    /* Use 16-bit-aligned splits. */
    size_t splits[] = {0, 16, 40, 80, sizeof(buf)};
    uint32_t partial = 0;
    for (int i = 0; i < 4; i++) {
        partial = internet_checksum_partial(buf + splits[i],
                                            splits[i + 1] - splits[i],
                                            partial);
    }
    uint16_t chunked = internet_checksum_fold(partial);

    if (whole != chunked) {
        fprintf(stderr, "  FAIL chunked_parity: whole=0x%04x chunked=0x%04x\n",
                whole, chunked);
        exit(1);
    }
    printf("  chunked vs whole parity... OK\n");
}

/* Edge cases: empty, odd-length, all-zeros. */
static void test_edge_cases(void) {
    /* Empty input: sum is 0, complement is 0xFFFF. */
    uint16_t cs = internet_checksum(NULL, 0);
    assert(cs == 0xFFFF);

    /* All zeros: same. */
    uint8_t zeros[20] = {0};
    cs = internet_checksum(zeros, sizeof(zeros));
    assert(cs == 0xFFFF);

    /* Odd length: pad with zero on the right. */
    uint8_t three[] = {0x12, 0x34, 0x56};
    cs = internet_checksum(three, 3);
    /* 0x1234 + 0x5600 = 0x6834, complement = 0x97cb */
    assert(cs == 0x97cb);

    printf("  edge cases (empty/zeros/odd)... OK\n");
}

int main(void) {
    printf("Internet checksum test suite\n");
    test_real_ipv4_header();
    test_self_check_property();
    test_incremental_parity();
    test_chunked_parity();
    test_edge_cases();
    printf("PASS\n");
    return 0;
}
