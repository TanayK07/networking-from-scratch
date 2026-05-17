/* Unit tests for PCAP file format reading and writing. */

#include "../pcap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;

#define ASSERT_EQ(expr, expected)                                                                  \
    do {                                                                                           \
        tests_run++;                                                                               \
        long long _got = (long long)(expr);                                                        \
        long long _exp = (long long)(expected);                                                    \
        if (_got != _exp) {                                                                        \
            fprintf(stderr, "  FAIL %s:%d: %s == 0x%llx, want 0x%llx\n", __FILE__, __LINE__,       \
                    #expr, _got, _exp);                                                            \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

#define ASSERT_TRUE(expr)                                                                          \
    do {                                                                                           \
        tests_run++;                                                                               \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "  FAIL %s:%d: %s is false\n", __FILE__, __LINE__, #expr);             \
            return;                                                                                \
        }                                                                                          \
        tests_passed++;                                                                            \
    } while (0)

/* ------------------------------------------------------------------ */
/*  Test: struct sizes                                                 */
/* ------------------------------------------------------------------ */
static void test_struct_sizes(void) {
    ASSERT_EQ(sizeof(struct nfs_pcap_global_hdr), 24);
    ASSERT_EQ(sizeof(struct nfs_pcap_pkt_hdr), 16);
}

/* ------------------------------------------------------------------ */
/*  Test: write + read global header roundtrip                         */
/* ------------------------------------------------------------------ */
static void test_header_roundtrip(void) {
    FILE *fp = tmpfile();
    ASSERT_TRUE(fp != NULL);

    ASSERT_EQ(nfs_pcap_write_header(fp, 65535, NFS_PCAP_LINKTYPE_ETHERNET), 0);
    rewind(fp);

    struct nfs_pcap_global_hdr hdr;
    ASSERT_EQ(nfs_pcap_read_header(fp, &hdr), 0);
    ASSERT_EQ(hdr.magic, NFS_PCAP_MAGIC);
    ASSERT_EQ(hdr.version_major, 2);
    ASSERT_EQ(hdr.version_minor, 4);
    ASSERT_EQ(hdr.snaplen, 65535);
    ASSERT_EQ(hdr.linktype, NFS_PCAP_LINKTYPE_ETHERNET);
    ASSERT_EQ(hdr.thiszone, 0);
    ASSERT_EQ(hdr.sigfigs, 0);

    fclose(fp);
}

/* ------------------------------------------------------------------ */
/*  Test: write + read single packet                                   */
/* ------------------------------------------------------------------ */
static void test_single_packet(void) {
    FILE *fp = tmpfile();
    ASSERT_TRUE(fp != NULL);

    ASSERT_EQ(nfs_pcap_write_header(fp, 65535, 1), 0);

    uint8_t pkt[60];
    memset(pkt, 0xAA, sizeof(pkt));
    ASSERT_EQ(nfs_pcap_write_packet(fp, 1700000000, 123456, pkt, 60), 0);

    rewind(fp);

    struct nfs_pcap_global_hdr ghdr;
    ASSERT_EQ(nfs_pcap_read_header(fp, &ghdr), 0);

    struct nfs_pcap_pkt_hdr phdr;
    uint8_t buf[256];
    ASSERT_EQ(nfs_pcap_read_packet(fp, &phdr, buf, sizeof(buf)), 0);
    ASSERT_EQ(phdr.ts_sec, 1700000000u);
    ASSERT_EQ(phdr.ts_usec, 123456u);
    ASSERT_EQ(phdr.incl_len, 60u);
    ASSERT_EQ(phdr.orig_len, 60u);
    ASSERT_TRUE(memcmp(buf, pkt, 60) == 0);

    /* Next read should be EOF. */
    ASSERT_EQ(nfs_pcap_read_packet(fp, &phdr, buf, sizeof(buf)), 1);

    fclose(fp);
}

/* ------------------------------------------------------------------ */
/*  Test: multiple packets                                             */
/* ------------------------------------------------------------------ */
static void test_multiple_packets(void) {
    FILE *fp = tmpfile();
    ASSERT_TRUE(fp != NULL);

    ASSERT_EQ(nfs_pcap_write_header(fp, 65535, 1), 0);

    uint8_t pkt1[40];
    memset(pkt1, 0x11, sizeof(pkt1));
    uint8_t pkt2[100];
    memset(pkt2, 0x22, sizeof(pkt2));
    uint8_t pkt3[200];
    memset(pkt3, 0x33, sizeof(pkt3));

    ASSERT_EQ(nfs_pcap_write_packet(fp, 100, 0, pkt1, 40), 0);
    ASSERT_EQ(nfs_pcap_write_packet(fp, 101, 500000, pkt2, 100), 0);
    ASSERT_EQ(nfs_pcap_write_packet(fp, 102, 999999, pkt3, 200), 0);

    rewind(fp);

    struct nfs_pcap_global_hdr ghdr;
    ASSERT_EQ(nfs_pcap_read_header(fp, &ghdr), 0);

    struct nfs_pcap_pkt_hdr phdr;
    uint8_t buf[256];

    /* Packet 1 */
    ASSERT_EQ(nfs_pcap_read_packet(fp, &phdr, buf, sizeof(buf)), 0);
    ASSERT_EQ(phdr.ts_sec, 100u);
    ASSERT_EQ(phdr.incl_len, 40u);
    ASSERT_EQ(buf[0], 0x11);

    /* Packet 2 */
    ASSERT_EQ(nfs_pcap_read_packet(fp, &phdr, buf, sizeof(buf)), 0);
    ASSERT_EQ(phdr.ts_sec, 101u);
    ASSERT_EQ(phdr.ts_usec, 500000u);
    ASSERT_EQ(phdr.incl_len, 100u);
    ASSERT_EQ(buf[0], 0x22);

    /* Packet 3 */
    ASSERT_EQ(nfs_pcap_read_packet(fp, &phdr, buf, sizeof(buf)), 0);
    ASSERT_EQ(phdr.ts_sec, 102u);
    ASSERT_EQ(phdr.incl_len, 200u);
    ASSERT_EQ(buf[0], 0x33);

    /* EOF */
    ASSERT_EQ(nfs_pcap_read_packet(fp, &phdr, buf, sizeof(buf)), 1);

    fclose(fp);
}

/* ------------------------------------------------------------------ */
/*  Test: invalid magic                                                */
/* ------------------------------------------------------------------ */
static void test_invalid_magic(void) {
    FILE *fp = tmpfile();
    ASSERT_TRUE(fp != NULL);

    /* Write garbage. */
    uint8_t garbage[24];
    memset(garbage, 0x42, sizeof(garbage));
    fwrite(garbage, 1, sizeof(garbage), fp);
    rewind(fp);

    struct nfs_pcap_global_hdr hdr;
    ASSERT_EQ(nfs_pcap_read_header(fp, &hdr), -1);

    fclose(fp);
}

/* ------------------------------------------------------------------ */
/*  Test: empty pcap (header only, no packets)                         */
/* ------------------------------------------------------------------ */
static void test_empty_pcap(void) {
    FILE *fp = tmpfile();
    ASSERT_TRUE(fp != NULL);

    ASSERT_EQ(nfs_pcap_write_header(fp, 65535, 1), 0);
    rewind(fp);

    struct nfs_pcap_global_hdr ghdr;
    ASSERT_EQ(nfs_pcap_read_header(fp, &ghdr), 0);

    struct nfs_pcap_pkt_hdr phdr;
    uint8_t buf[256];
    ASSERT_EQ(nfs_pcap_read_packet(fp, &phdr, buf, sizeof(buf)), 1); /* EOF */

    fclose(fp);
}

/* ------------------------------------------------------------------ */
/*  Test: read header from truncated file                              */
/* ------------------------------------------------------------------ */
static void test_truncated_header(void) {
    FILE *fp = tmpfile();
    ASSERT_TRUE(fp != NULL);

    /* Write only 10 bytes (less than 24-byte header). */
    uint8_t partial[10] = {0};
    fwrite(partial, 1, sizeof(partial), fp);
    rewind(fp);

    struct nfs_pcap_global_hdr hdr;
    ASSERT_EQ(nfs_pcap_read_header(fp, &hdr), -1);

    fclose(fp);
}

/* ------------------------------------------------------------------ */
/*  Test: packet data buffer too small                                 */
/* ------------------------------------------------------------------ */
static void test_buffer_too_small(void) {
    FILE *fp = tmpfile();
    ASSERT_TRUE(fp != NULL);

    ASSERT_EQ(nfs_pcap_write_header(fp, 65535, 1), 0);

    uint8_t pkt[100];
    memset(pkt, 0xBB, sizeof(pkt));
    ASSERT_EQ(nfs_pcap_write_packet(fp, 0, 0, pkt, 100), 0);

    rewind(fp);

    struct nfs_pcap_global_hdr ghdr;
    ASSERT_EQ(nfs_pcap_read_header(fp, &ghdr), 0);

    struct nfs_pcap_pkt_hdr phdr;
    uint8_t small_buf[50]; /* too small for 100-byte packet */
    ASSERT_EQ(nfs_pcap_read_packet(fp, &phdr, small_buf, sizeof(small_buf)), -1);

    fclose(fp);
}

/* ------------------------------------------------------------------ */
/*  Test: byte-swapped magic header                                    */
/* ------------------------------------------------------------------ */
static void test_swapped_magic(void) {
    FILE *fp = tmpfile();
    ASSERT_TRUE(fp != NULL);

    /* Manually write a header with swapped magic. */
    struct nfs_pcap_global_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = NFS_PCAP_MAGIC_SWAPPED;
    /* Swap the other fields too (simulating a big-endian writer on LE host). */
    hdr.version_major = (uint16_t)((2 >> 8) | (2 << 8)); /* swap16(2) */
    hdr.version_minor = (uint16_t)((4 >> 8) | (4 << 8)); /* swap16(4) */
    hdr.snaplen = ((65535u >> 24) & 0xFF) | ((65535u >> 8) & 0xFF00) | ((65535u << 8) & 0xFF0000) |
                  ((65535u << 24) & 0xFF000000u);
    hdr.linktype = ((1u >> 24) & 0xFF) | ((1u >> 8) & 0xFF00) | ((1u << 8) & 0xFF0000) |
                   ((1u << 24) & 0xFF000000u);

    fwrite(&hdr, sizeof(hdr), 1, fp);
    rewind(fp);

    struct nfs_pcap_global_hdr read_hdr;
    ASSERT_EQ(nfs_pcap_read_header(fp, &read_hdr), 0);
    ASSERT_EQ(read_hdr.magic, NFS_PCAP_MAGIC);
    ASSERT_EQ(read_hdr.version_major, 2);
    ASSERT_EQ(read_hdr.version_minor, 4);
    ASSERT_EQ(read_hdr.snaplen, 65535u);
    ASSERT_EQ(read_hdr.linktype, 1u);

    fclose(fp);
}

/* ------------------------------------------------------------------ */
/*  Test: format header                                                */
/* ------------------------------------------------------------------ */
static void test_format_header(void) {
    struct nfs_pcap_global_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = NFS_PCAP_MAGIC;
    hdr.version_major = 2;
    hdr.version_minor = 4;
    hdr.snaplen = 65535;
    hdr.linktype = 1;

    char buf[256];
    nfs_pcap_format_header(&hdr, buf, sizeof(buf));
    ASSERT_TRUE(strstr(buf, "PCAP") != NULL);
    ASSERT_TRUE(strstr(buf, "2.4") != NULL);
    ASSERT_TRUE(strstr(buf, "65535") != NULL);
}

/* ------------------------------------------------------------------ */
/*  Test: write with NULL file pointer                                 */
/* ------------------------------------------------------------------ */
static void test_null_fp(void) {
    ASSERT_EQ(nfs_pcap_write_header(NULL, 65535, 1), -1);

    uint8_t data[10] = {0};
    ASSERT_EQ(nfs_pcap_write_packet(NULL, 0, 0, data, 10), -1);

    struct nfs_pcap_global_hdr ghdr;
    ASSERT_EQ(nfs_pcap_read_header(NULL, &ghdr), -1);

    struct nfs_pcap_pkt_hdr phdr;
    ASSERT_EQ(nfs_pcap_read_packet(NULL, &phdr, data, 10), -1);
}

/* ------------------------------------------------------------------ */
/*  Test: custom snaplen and linktype                                  */
/* ------------------------------------------------------------------ */
static void test_custom_snaplen(void) {
    FILE *fp = tmpfile();
    ASSERT_TRUE(fp != NULL);

    ASSERT_EQ(nfs_pcap_write_header(fp, 1500, 6), 0); /* linktype 6 = IEEE 802.5 */
    rewind(fp);

    struct nfs_pcap_global_hdr hdr;
    ASSERT_EQ(nfs_pcap_read_header(fp, &hdr), 0);
    ASSERT_EQ(hdr.snaplen, 1500u);
    ASSERT_EQ(hdr.linktype, 6u);

    fclose(fp);
}

/* ------------------------------------------------------------------ */
/*  Test: large packet data integrity                                  */
/* ------------------------------------------------------------------ */
static void test_large_packet(void) {
    FILE *fp = tmpfile();
    ASSERT_TRUE(fp != NULL);

    ASSERT_EQ(nfs_pcap_write_header(fp, 65535, 1), 0);

    uint8_t pkt[1500];
    for (int i = 0; i < 1500; i++)
        pkt[i] = (uint8_t)(i & 0xFF);

    ASSERT_EQ(nfs_pcap_write_packet(fp, 999, 888, pkt, 1500), 0);
    rewind(fp);

    struct nfs_pcap_global_hdr ghdr;
    ASSERT_EQ(nfs_pcap_read_header(fp, &ghdr), 0);

    struct nfs_pcap_pkt_hdr phdr;
    uint8_t buf[1500];
    ASSERT_EQ(nfs_pcap_read_packet(fp, &phdr, buf, sizeof(buf)), 0);
    ASSERT_EQ(phdr.incl_len, 1500u);
    ASSERT_TRUE(memcmp(buf, pkt, 1500) == 0);

    fclose(fp);
}

/* ------------------------------------------------------------------ */

int main(void) {
    test_struct_sizes();
    test_header_roundtrip();
    test_single_packet();
    test_multiple_packets();
    test_invalid_magic();
    test_empty_pcap();
    test_truncated_header();
    test_buffer_too_small();
    test_swapped_magic();
    test_format_header();
    test_null_fp();
    test_custom_snaplen();
    test_large_packet();

    printf("\n  %d / %d tests passed\n", tests_passed, tests_run);
    if (tests_passed != tests_run) {
        fprintf(stderr, "  SOME TESTS FAILED\n");
        return 1;
    }
    printf("  All tests passed.\n");
    return 0;
}
