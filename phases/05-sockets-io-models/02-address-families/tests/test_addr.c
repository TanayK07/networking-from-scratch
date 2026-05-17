/* Unit tests for Address Families. */

#include "../addr.h"
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

/* ---- AF_INET build tests ---- */

static void test_build_inet_basic(void) {
    struct sockaddr_in addr;
    ASSERT_EQ(nfs_addr_build_inet("192.168.1.1", 8080, &addr), 0);
    ASSERT_EQ(addr.sin_family, AF_INET);
    ASSERT_EQ(ntohs(addr.sin_port), 8080);
    ASSERT_EQ(ntohl(addr.sin_addr.s_addr), 0xC0A80101u);
}

static void test_build_inet_localhost(void) {
    struct sockaddr_in addr;
    ASSERT_EQ(nfs_addr_build_inet("127.0.0.1", 80, &addr), 0);
    ASSERT_EQ(ntohl(addr.sin_addr.s_addr), 0x7F000001u);
}

static void test_build_inet_any(void) {
    struct sockaddr_in addr;
    ASSERT_EQ(nfs_addr_build_inet("0.0.0.0", 0, &addr), 0);
    ASSERT_EQ(addr.sin_addr.s_addr, 0);
    ASSERT_EQ(addr.sin_port, 0);
}

static void test_build_inet_invalid_ip(void) {
    struct sockaddr_in addr;
    ASSERT_EQ(nfs_addr_build_inet("not.an.ip", 80, &addr), -1);
}

static void test_build_inet_null(void) {
    struct sockaddr_in addr;
    ASSERT_EQ(nfs_addr_build_inet(NULL, 80, &addr), -1);
    ASSERT_EQ(nfs_addr_build_inet("127.0.0.1", 80, NULL), -1);
}

/* ---- AF_INET6 build tests ---- */

static void test_build_inet6_basic(void) {
    struct sockaddr_in6 addr;
    ASSERT_EQ(nfs_addr_build_inet6("2001:db8::1", 443, &addr), 0);
    ASSERT_EQ(addr.sin6_family, AF_INET6);
    ASSERT_EQ(ntohs(addr.sin6_port), 443);
}

static void test_build_inet6_loopback(void) {
    struct sockaddr_in6 addr;
    ASSERT_EQ(nfs_addr_build_inet6("::1", 0, &addr), 0);
    static const uint8_t lo[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
    ASSERT_TRUE(memcmp(&addr.sin6_addr, lo, 16) == 0);
}

static void test_build_inet6_full(void) {
    struct sockaddr_in6 addr;
    ASSERT_EQ(nfs_addr_build_inet6_full("fe80::1", 80, 0x12345, 2, &addr), 0);
    ASSERT_EQ(ntohl(addr.sin6_flowinfo), 0x12345u);
    ASSERT_EQ(addr.sin6_scope_id, 2u);
}

static void test_build_inet6_invalid(void) {
    struct sockaddr_in6 addr;
    ASSERT_EQ(nfs_addr_build_inet6("not-ipv6", 80, &addr), -1);
}

static void test_build_inet6_null(void) {
    struct sockaddr_in6 addr;
    ASSERT_EQ(nfs_addr_build_inet6(NULL, 80, &addr), -1);
}

/* ---- AF_UNIX build tests ---- */

static void test_build_unix_basic(void) {
    struct sockaddr_un addr;
    ASSERT_EQ(nfs_addr_build_unix("/tmp/test.sock", &addr), 0);
    ASSERT_EQ(addr.sun_family, AF_UNIX);
    ASSERT_TRUE(strcmp(addr.sun_path, "/tmp/test.sock") == 0);
}

static void test_build_unix_null(void) {
    struct sockaddr_un addr;
    ASSERT_EQ(nfs_addr_build_unix(NULL, &addr), -1);
    ASSERT_EQ(nfs_addr_build_unix("/tmp/x", NULL), -1);
}

static void test_build_unix_too_long(void) {
    char long_path[256];
    memset(long_path, 'a', sizeof(long_path));
    long_path[255] = '\0';
    struct sockaddr_un addr;
    ASSERT_EQ(nfs_addr_build_unix(long_path, &addr), -1);
}

/* ---- Format tests ---- */

static void test_format_inet(void) {
    struct sockaddr_in addr;
    nfs_addr_build_inet("10.0.0.1", 8080, &addr);
    char str[64];
    nfs_addr_format_inet(&addr, str, sizeof(str));
    ASSERT_TRUE(strcmp(str, "10.0.0.1:8080") == 0);
}

static void test_format_inet6(void) {
    struct sockaddr_in6 addr;
    nfs_addr_build_inet6("::1", 443, &addr);
    char str[64];
    nfs_addr_format_inet6(&addr, str, sizeof(str));
    ASSERT_TRUE(strcmp(str, "[::1]:443") == 0);
}

static void test_format_unix(void) {
    struct sockaddr_un addr;
    nfs_addr_build_unix("/tmp/my.sock", &addr);
    char str[64];
    nfs_addr_format_unix(&addr, str, sizeof(str));
    ASSERT_TRUE(strcmp(str, "/tmp/my.sock") == 0);
}

static void test_format_generic_inet(void) {
    struct sockaddr_in addr;
    nfs_addr_build_inet("1.2.3.4", 22, &addr);
    char str[64];
    nfs_addr_format((struct sockaddr *)&addr, sizeof(addr), str, sizeof(str));
    ASSERT_TRUE(strcmp(str, "1.2.3.4:22") == 0);
}

static void test_format_generic_inet6(void) {
    struct sockaddr_in6 addr;
    nfs_addr_build_inet6("::1", 80, &addr);
    char str[64];
    nfs_addr_format((struct sockaddr *)&addr, sizeof(addr), str, sizeof(str));
    ASSERT_TRUE(strcmp(str, "[::1]:80") == 0);
}

static void test_format_null(void) {
    char str[64];
    nfs_addr_format_inet(NULL, str, sizeof(str));
    ASSERT_EQ(str[0], '\0');
}

/* ---- Comparison tests ---- */

static void test_cmp_inet_equal(void) {
    struct sockaddr_in a, b;
    nfs_addr_build_inet("10.0.0.1", 80, &a);
    nfs_addr_build_inet("10.0.0.1", 80, &b);
    ASSERT_EQ(nfs_addr_cmp_inet(&a, &b), 0);
}

static void test_cmp_inet_diff_port(void) {
    struct sockaddr_in a, b;
    nfs_addr_build_inet("10.0.0.1", 80, &a);
    nfs_addr_build_inet("10.0.0.1", 443, &b);
    ASSERT_TRUE(nfs_addr_cmp_inet(&a, &b) != 0);
}

static void test_cmp_inet_diff_ip(void) {
    struct sockaddr_in a, b;
    nfs_addr_build_inet("10.0.0.1", 80, &a);
    nfs_addr_build_inet("10.0.0.2", 80, &b);
    ASSERT_TRUE(nfs_addr_cmp_inet(&a, &b) != 0);
}

static void test_cmp_inet6_equal(void) {
    struct sockaddr_in6 a, b;
    nfs_addr_build_inet6("2001:db8::1", 443, &a);
    nfs_addr_build_inet6("2001:db8::1", 443, &b);
    ASSERT_EQ(nfs_addr_cmp_inet6(&a, &b), 0);
}

static void test_cmp_inet6_diff(void) {
    struct sockaddr_in6 a, b;
    nfs_addr_build_inet6("2001:db8::1", 443, &a);
    nfs_addr_build_inet6("2001:db8::2", 443, &b);
    ASSERT_TRUE(nfs_addr_cmp_inet6(&a, &b) != 0);
}

static void test_cmp_unix_equal(void) {
    struct sockaddr_un a, b;
    nfs_addr_build_unix("/tmp/a.sock", &a);
    nfs_addr_build_unix("/tmp/a.sock", &b);
    ASSERT_EQ(nfs_addr_cmp_unix(&a, &b), 0);
}

static void test_cmp_unix_diff(void) {
    struct sockaddr_un a, b;
    nfs_addr_build_unix("/tmp/a.sock", &a);
    nfs_addr_build_unix("/tmp/b.sock", &b);
    ASSERT_TRUE(nfs_addr_cmp_unix(&a, &b) != 0);
}

static void test_cmp_null(void) {
    struct sockaddr_in addr;
    nfs_addr_build_inet("1.2.3.4", 80, &addr);
    ASSERT_EQ(nfs_addr_cmp_inet(NULL, &addr), -1);
    ASSERT_EQ(nfs_addr_cmp_inet(&addr, NULL), -1);
}

/* ---- Query tests ---- */

static void test_family(void) {
    struct sockaddr_in addr4;
    nfs_addr_build_inet("1.2.3.4", 80, &addr4);
    ASSERT_EQ(nfs_addr_family((struct sockaddr *)&addr4), AF_INET);

    struct sockaddr_in6 addr6;
    nfs_addr_build_inet6("::1", 80, &addr6);
    ASSERT_EQ(nfs_addr_family((struct sockaddr *)&addr6), AF_INET6);
}

static void test_get_port(void) {
    struct sockaddr_in addr4;
    nfs_addr_build_inet("1.2.3.4", 12345, &addr4);
    ASSERT_EQ(nfs_addr_get_port((struct sockaddr *)&addr4), 12345);

    struct sockaddr_in6 addr6;
    nfs_addr_build_inet6("::1", 443, &addr6);
    ASSERT_EQ(nfs_addr_get_port((struct sockaddr *)&addr6), 443);
}

static void test_get_port_unix(void) {
    struct sockaddr_un addru;
    nfs_addr_build_unix("/tmp/x", &addru);
    ASSERT_EQ(nfs_addr_get_port((struct sockaddr *)&addru), -1);
}

static void test_loopback4(void) {
    struct sockaddr_in addr;
    nfs_addr_build_inet("127.0.0.1", 80, &addr);
    ASSERT_EQ(nfs_addr_is_loopback4(&addr), 1);

    nfs_addr_build_inet("127.255.0.1", 80, &addr);
    ASSERT_EQ(nfs_addr_is_loopback4(&addr), 1);

    nfs_addr_build_inet("10.0.0.1", 80, &addr);
    ASSERT_EQ(nfs_addr_is_loopback4(&addr), 0);
}

static void test_loopback6(void) {
    struct sockaddr_in6 addr;
    nfs_addr_build_inet6("::1", 0, &addr);
    ASSERT_EQ(nfs_addr_is_loopback6(&addr), 1);

    nfs_addr_build_inet6("2001:db8::1", 0, &addr);
    ASSERT_EQ(nfs_addr_is_loopback6(&addr), 0);
}

static void test_family_name(void) {
    ASSERT_TRUE(strcmp(nfs_addr_family_name(AF_INET), "AF_INET") == 0);
    ASSERT_TRUE(strcmp(nfs_addr_family_name(AF_INET6), "AF_INET6") == 0);
    ASSERT_TRUE(strcmp(nfs_addr_family_name(AF_UNIX), "AF_UNIX") == 0);
    ASSERT_TRUE(strcmp(nfs_addr_family_name(AF_UNSPEC), "AF_UNSPEC") == 0);
}

int main(void) {
    printf("Address Families Tests\n");

    test_build_inet_basic();
    test_build_inet_localhost();
    test_build_inet_any();
    test_build_inet_invalid_ip();
    test_build_inet_null();
    test_build_inet6_basic();
    test_build_inet6_loopback();
    test_build_inet6_full();
    test_build_inet6_invalid();
    test_build_inet6_null();
    test_build_unix_basic();
    test_build_unix_null();
    test_build_unix_too_long();
    test_format_inet();
    test_format_inet6();
    test_format_unix();
    test_format_generic_inet();
    test_format_generic_inet6();
    test_format_null();
    test_cmp_inet_equal();
    test_cmp_inet_diff_port();
    test_cmp_inet_diff_ip();
    test_cmp_inet6_equal();
    test_cmp_inet6_diff();
    test_cmp_unix_equal();
    test_cmp_unix_diff();
    test_cmp_null();
    test_family();
    test_get_port();
    test_get_port_unix();
    test_loopback4();
    test_loopback6();
    test_family_name();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
