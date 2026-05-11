/* Unit + property tests for nfs_htonl and friends.
 *
 * No external test framework — just stdlib assertions wrapped in a
 * tiny ASSERT_EQ macro. Keeps the dependency story simple.
 */

#include "../htonl.h"

#include <arpa/inet.h>   /* for the system htonl/htons we compare against */
#include <stdio.h>
#include <stdlib.h>

static int failures = 0;

#define ASSERT_EQ(a, b) do {                                    \
    unsigned long long _a = (unsigned long long)(a);             \
    unsigned long long _b = (unsigned long long)(b);             \
    if (_a != _b) {                                              \
        fprintf(stderr, "FAIL %s:%d  %s (0x%llx) != %s (0x%llx)\n",\
                __FILE__, __LINE__, #a, _a, #b, _b);             \
        failures++;                                              \
    }                                                            \
} while (0)

static void test_swap_pinned_values(void) {
    /* Pure swap functions don't depend on host endianness. */
    ASSERT_EQ(nfs_swap32(0x12345678u), 0x78563412u);
    ASSERT_EQ(nfs_swap32(0x00000000u), 0x00000000u);
    ASSERT_EQ(nfs_swap32(0xFFFFFFFFu), 0xFFFFFFFFu);
    ASSERT_EQ(nfs_swap32(0x000000FFu), 0xFF000000u);
    ASSERT_EQ(nfs_swap32(0xAABBCCDDu), 0xDDCCBBAAu);

    ASSERT_EQ(nfs_swap16((uint16_t)0xABCDu), (uint16_t)0xCDABu);
    ASSERT_EQ(nfs_swap16((uint16_t)0x0000u), (uint16_t)0x0000u);
    ASSERT_EQ(nfs_swap16((uint16_t)0xFFFFu), (uint16_t)0xFFFFu);
    ASSERT_EQ(nfs_swap16((uint16_t)0x00FFu), (uint16_t)0xFF00u);
}

static void test_swap_is_involution(void) {
    /* swap(swap(x)) == x for all x. */
    for (uint64_t x = 0; x < 100000; x++) {
        ASSERT_EQ(nfs_swap32(nfs_swap32((uint32_t)x)), (uint32_t)x);
    }
    for (uint32_t x = 0; x < 65536; x++) {
        ASSERT_EQ(nfs_swap16(nfs_swap16((uint16_t)x)), (uint16_t)x);
    }
}

static void test_round_trip(void) {
    /* htonl and ntohl are inverses, regardless of host endianness. */
    for (uint64_t x = 0; x < 100000; x++) {
        ASSERT_EQ(nfs_ntohl(nfs_htonl((uint32_t)x)), (uint32_t)x);
    }
    for (uint32_t x = 0; x < 65536; x++) {
        ASSERT_EQ(nfs_ntohs(nfs_htons((uint16_t)x)), (uint16_t)x);
    }
}

static void test_matches_libc(void) {
    /* Bit-for-bit agreement with the system <arpa/inet.h>. */
    uint32_t test_values_32[] = {
        0, 1, 0xFF, 0x100, 0xFFFF, 0x10000, 0xFFFFFFFF,
        0x12345678, 0xDEADBEEF, 0xCAFEBABE,
    };
    uint16_t test_values_16[] = {
        0, 1, 0xFF, 0x100, 0xFFFF, 0xABCD, 0x1234,
    };

    for (size_t i = 0; i < sizeof(test_values_32) / sizeof(*test_values_32); i++) {
        uint32_t v = test_values_32[i];
        ASSERT_EQ(nfs_htonl(v), htonl(v));
        ASSERT_EQ(nfs_ntohl(v), ntohl(v));
    }
    for (size_t i = 0; i < sizeof(test_values_16) / sizeof(*test_values_16); i++) {
        uint16_t v = test_values_16[i];
        ASSERT_EQ(nfs_htons(v), htons(v));
        ASSERT_EQ(nfs_ntohs(v), ntohs(v));
    }
}

int main(void) {
    test_swap_pinned_values();
    test_swap_is_involution();
    test_round_trip();
    test_matches_libc();

    if (failures) {
        fprintf(stderr, "\n%d failure(s).\n", failures);
        return 1;
    }
    printf("All tests passed.\n");
    return 0;
}
