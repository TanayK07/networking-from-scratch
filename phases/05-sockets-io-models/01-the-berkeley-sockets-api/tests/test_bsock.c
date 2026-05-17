/* Unit tests for Berkeley Sockets API helpers. */

#include "../bsock.h"
#include <arpa/inet.h>
#include <stdio.h>
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

/* ---- inet_aton / inet_ntoa ---- */

static void test_inet_aton_loopback(void) {
    uint32_t addr;
    ASSERT_EQ(nfs_inet_aton("127.0.0.1", &addr), 0);
    ASSERT_EQ(addr, htonl(0x7F000001));
}

static void test_inet_aton_zeros(void) {
    uint32_t addr;
    ASSERT_EQ(nfs_inet_aton("0.0.0.0", &addr), 0);
    ASSERT_EQ(addr, 0);
}

static void test_inet_aton_broadcast(void) {
    uint32_t addr;
    ASSERT_EQ(nfs_inet_aton("255.255.255.255", &addr), 0);
    ASSERT_EQ(addr, htonl(0xFFFFFFFF));
}

static void test_inet_aton_private(void) {
    uint32_t addr;
    ASSERT_EQ(nfs_inet_aton("192.168.1.100", &addr), 0);
    ASSERT_EQ(addr, htonl(0xC0A80164));
}

static void test_inet_aton_invalid(void) {
    uint32_t addr;
    ASSERT_EQ(nfs_inet_aton("256.0.0.1", &addr), -1);
    ASSERT_EQ(nfs_inet_aton("abc", &addr), -1);
    ASSERT_EQ(nfs_inet_aton("", &addr), -1);
    ASSERT_EQ(nfs_inet_aton(NULL, &addr), -1);
}

static void test_inet_ntoa_loopback(void) {
    char buf[16];
    ASSERT_EQ(nfs_inet_ntoa(htonl(0x7F000001), buf, sizeof(buf)), 0);
    ASSERT_TRUE(strcmp(buf, "127.0.0.1") == 0);
}

static void test_inet_ntoa_broadcast(void) {
    char buf[16];
    ASSERT_EQ(nfs_inet_ntoa(htonl(0xFFFFFFFF), buf, sizeof(buf)), 0);
    ASSERT_TRUE(strcmp(buf, "255.255.255.255") == 0);
}

static void test_inet_roundtrip(void) {
    const char *ips[] = {"10.0.0.1", "172.16.0.1", "192.168.0.1", "1.2.3.4", NULL};
    for (int i = 0; ips[i]; i++) {
        uint32_t addr;
        char buf[16];
        ASSERT_EQ(nfs_inet_aton(ips[i], &addr), 0);
        ASSERT_EQ(nfs_inet_ntoa(addr, buf, sizeof(buf)), 0);
        ASSERT_TRUE(strcmp(buf, ips[i]) == 0);
    }
}

/* ---- sockaddr_in build/parse ---- */

static void test_build_parse_roundtrip(void) {
    struct nfs_sockaddr_in sa;
    ASSERT_EQ(nfs_sockaddr_in_build(&sa, "192.168.1.100", 8080), 0);
    ASSERT_EQ(sa.sin_family, NFS_AF_INET);

    char ip[16];
    uint16_t port;
    ASSERT_EQ(nfs_sockaddr_in_parse(&sa, ip, sizeof(ip), &port), 0);
    ASSERT_TRUE(strcmp(ip, "192.168.1.100") == 0);
    ASSERT_EQ(port, 8080);
}

static void test_build_port_zero(void) {
    struct nfs_sockaddr_in sa;
    ASSERT_EQ(nfs_sockaddr_in_build(&sa, "10.0.0.1", 0), 0);
    ASSERT_EQ(nfs_sockaddr_port(&sa), 0);
}

static void test_build_port_max(void) {
    struct nfs_sockaddr_in sa;
    ASSERT_EQ(nfs_sockaddr_in_build(&sa, "10.0.0.1", 65535), 0);
    ASSERT_EQ(nfs_sockaddr_port(&sa), 65535);
}

static void test_build_port_80(void) {
    struct nfs_sockaddr_in sa;
    ASSERT_EQ(nfs_sockaddr_in_build(&sa, "10.0.0.1", 80), 0);
    ASSERT_EQ(sa.sin_port, htons(80));
    ASSERT_EQ(nfs_sockaddr_port(&sa), 80);
}

static void test_build_invalid_ip(void) {
    struct nfs_sockaddr_in sa;
    ASSERT_EQ(nfs_sockaddr_in_build(&sa, "999.0.0.1", 80), -1);
}

static void test_build_null(void) {
    ASSERT_EQ(nfs_sockaddr_in_build(NULL, "1.2.3.4", 80), -1);
    struct nfs_sockaddr_in sa;
    ASSERT_EQ(nfs_sockaddr_in_build(&sa, NULL, 80), -1);
}

static void test_parse_wrong_family(void) {
    struct nfs_sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = 99; /* not AF_INET */

    char ip[16];
    uint16_t port;
    ASSERT_EQ(nfs_sockaddr_in_parse(&sa, ip, sizeof(ip), &port), -1);
}

/* ---- sockaddr_format ---- */

static void test_format(void) {
    struct nfs_sockaddr_in sa;
    nfs_sockaddr_in_build(&sa, "127.0.0.1", 443);

    char out[32];
    int n = nfs_sockaddr_format(&sa, out, sizeof(out));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strcmp(out, "127.0.0.1:443") == 0);
}

static void test_format_high_port(void) {
    struct nfs_sockaddr_in sa;
    nfs_sockaddr_in_build(&sa, "10.0.0.1", 65535);

    char out[32];
    nfs_sockaddr_format(&sa, out, sizeof(out));
    ASSERT_TRUE(strcmp(out, "10.0.0.1:65535") == 0);
}

static void test_format_null(void) {
    ASSERT_EQ(nfs_sockaddr_format(NULL, NULL, 0), -1);
}

/* ---- Accessor helpers ---- */

static void test_port_accessor(void) {
    struct nfs_sockaddr_in sa;
    nfs_sockaddr_in_build(&sa, "1.2.3.4", 9090);
    ASSERT_EQ(nfs_sockaddr_port(&sa), 9090);
}

static void test_addr_accessor(void) {
    struct nfs_sockaddr_in sa;
    nfs_sockaddr_in_build(&sa, "10.20.30.40", 80);
    ASSERT_EQ(nfs_sockaddr_addr(&sa), htonl(0x0A141E28));
}

static void test_struct_size(void) {
    ASSERT_EQ(sizeof(struct nfs_sockaddr_in), 16);
}

int main(void) {
    printf("Berkeley Sockets API Tests\n");

    test_inet_aton_loopback();
    test_inet_aton_zeros();
    test_inet_aton_broadcast();
    test_inet_aton_private();
    test_inet_aton_invalid();
    test_inet_ntoa_loopback();
    test_inet_ntoa_broadcast();
    test_inet_roundtrip();
    test_build_parse_roundtrip();
    test_build_port_zero();
    test_build_port_max();
    test_build_port_80();
    test_build_invalid_ip();
    test_build_null();
    test_parse_wrong_family();
    test_format();
    test_format_high_port();
    test_format_null();
    test_port_accessor();
    test_addr_accessor();
    test_struct_size();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
