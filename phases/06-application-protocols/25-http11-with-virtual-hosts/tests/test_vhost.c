/* Unit tests for HTTP/1.1 with virtual hosts. */

#include "../vhost.h"
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

/* ---- Table init tests ---- */

static void test_table_init(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    ASSERT_EQ(nfs_vhost_count(&table), 0);
    ASSERT_TRUE(nfs_vhost_get_default(&table) == NULL);
}

/* ---- Add host tests ---- */

static void test_add_host(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    int idx = nfs_vhost_add(&table, "example.com", "/var/www/example", 0);
    ASSERT_TRUE(idx >= 0);
    ASSERT_EQ(nfs_vhost_count(&table), 1);
    ASSERT_TRUE(strcmp(table.hosts[idx].hostname, "example.com") == 0);
    ASSERT_TRUE(strcmp(table.hosts[idx].docroot, "/var/www/example") == 0);
}

static void test_add_multiple_hosts(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    nfs_vhost_add(&table, "alpha.com", "/alpha", 1);
    nfs_vhost_add(&table, "beta.com", "/beta", 0);
    nfs_vhost_add(&table, "gamma.com", "/gamma", 0);
    ASSERT_EQ(nfs_vhost_count(&table), 3);
}

static void test_add_host_null(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    ASSERT_EQ(nfs_vhost_add(&table, NULL, "/x", 0), -1);
    ASSERT_EQ(nfs_vhost_add(&table, "x.com", NULL, 0), -1);
    ASSERT_EQ(nfs_vhost_add(NULL, "x.com", "/x", 0), -1);
}

static void test_first_host_is_default(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    nfs_vhost_add(&table, "first.com", "/first", 0);
    const struct nfs_vhost *def = nfs_vhost_get_default(&table);
    ASSERT_TRUE(def != NULL);
    ASSERT_TRUE(strcmp(def->hostname, "first.com") == 0);
}

static void test_explicit_default(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    nfs_vhost_add(&table, "first.com", "/first", 0);
    nfs_vhost_add(&table, "second.com", "/second", 1);
    const struct nfs_vhost *def = nfs_vhost_get_default(&table);
    ASSERT_TRUE(def != NULL);
    ASSERT_TRUE(strcmp(def->hostname, "second.com") == 0);
}

/* ---- Alias tests ---- */

static void test_add_alias(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    int idx = nfs_vhost_add(&table, "example.com", "/www", 0);
    ASSERT_EQ(nfs_vhost_add_alias(&table, idx, "www.example.com"), 0);
    ASSERT_EQ(table.hosts[idx].alias_count, 1);
    ASSERT_TRUE(strcmp(table.hosts[idx].aliases[0], "www.example.com") == 0);
}

static void test_add_alias_invalid_idx(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    ASSERT_EQ(nfs_vhost_add_alias(&table, 0, "x.com"), -1);
    ASSERT_EQ(nfs_vhost_add_alias(&table, -1, "x.com"), -1);
}

/* ---- Lookup tests ---- */

static void test_lookup_primary(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    nfs_vhost_add(&table, "example.com", "/www", 0);
    nfs_vhost_add(&table, "other.com", "/other", 0);

    const struct nfs_vhost *vh = nfs_vhost_lookup(&table, "other.com");
    ASSERT_TRUE(vh != NULL);
    ASSERT_TRUE(strcmp(vh->hostname, "other.com") == 0);
}

static void test_lookup_alias(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    int idx = nfs_vhost_add(&table, "example.com", "/www", 0);
    nfs_vhost_add_alias(&table, idx, "www.example.com");

    const struct nfs_vhost *vh = nfs_vhost_lookup(&table, "www.example.com");
    ASSERT_TRUE(vh != NULL);
    ASSERT_TRUE(strcmp(vh->hostname, "example.com") == 0);
}

static void test_lookup_case_insensitive(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    nfs_vhost_add(&table, "Example.COM", "/www", 0);

    const struct nfs_vhost *vh = nfs_vhost_lookup(&table, "example.com");
    ASSERT_TRUE(vh != NULL);
    ASSERT_TRUE(strcmp(vh->docroot, "/www") == 0);
}

static void test_lookup_fallback_default(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    nfs_vhost_add(&table, "example.com", "/www", 1);

    const struct nfs_vhost *vh = nfs_vhost_lookup(&table, "unknown.com");
    ASSERT_TRUE(vh != NULL);
    ASSERT_TRUE(strcmp(vh->hostname, "example.com") == 0);
}

static void test_lookup_null(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    ASSERT_TRUE(nfs_vhost_lookup(&table, NULL) == NULL);
    ASSERT_TRUE(nfs_vhost_lookup(NULL, "x.com") == NULL);
}

/* ---- Host header extraction tests ---- */

static void test_extract_host_basic(void) {
    const char *req = "GET / HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "\r\n";
    char host[256];
    ASSERT_EQ(nfs_vhost_extract_host(req, strlen(req), host, sizeof(host)), 0);
    ASSERT_TRUE(strcmp(host, "example.com") == 0);
}

static void test_extract_host_with_port(void) {
    const char *req = "GET / HTTP/1.1\r\n"
                      "Host: example.com:8080\r\n"
                      "\r\n";
    char host[256];
    ASSERT_EQ(nfs_vhost_extract_host(req, strlen(req), host, sizeof(host)), 0);
    ASSERT_TRUE(strcmp(host, "example.com") == 0);
}

static void test_extract_host_missing(void) {
    const char *req = "GET / HTTP/1.1\r\n"
                      "User-Agent: test\r\n"
                      "\r\n";
    char host[256];
    ASSERT_EQ(nfs_vhost_extract_host(req, strlen(req), host, sizeof(host)), -1);
}

static void test_extract_host_case_insensitive(void) {
    const char *req = "GET / HTTP/1.1\r\n"
                      "host: mixed.EXAMPLE.com\r\n"
                      "\r\n";
    char host[256];
    ASSERT_EQ(nfs_vhost_extract_host(req, strlen(req), host, sizeof(host)), 0);
    ASSERT_TRUE(strcmp(host, "mixed.EXAMPLE.com") == 0);
}

/* ---- Parse host value tests ---- */

static void test_parse_host_simple(void) {
    char hostname[256];
    uint16_t port;
    ASSERT_EQ(nfs_vhost_parse_host_value("example.com", hostname, sizeof(hostname), &port), 0);
    ASSERT_TRUE(strcmp(hostname, "example.com") == 0);
    ASSERT_EQ(port, 80);
}

static void test_parse_host_with_port(void) {
    char hostname[256];
    uint16_t port;
    ASSERT_EQ(nfs_vhost_parse_host_value("example.com:443", hostname, sizeof(hostname), &port), 0);
    ASSERT_TRUE(strcmp(hostname, "example.com") == 0);
    ASSERT_EQ(port, 443);
}

static void test_parse_host_null(void) {
    char hostname[256];
    ASSERT_EQ(nfs_vhost_parse_host_value(NULL, hostname, sizeof(hostname), NULL), -1);
}

/* ---- Request routing tests ---- */

static void test_route_basic(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    nfs_vhost_add(&table, "alpha.com", "/alpha", 1);
    nfs_vhost_add(&table, "beta.com", "/beta", 0);

    const char *req = "GET / HTTP/1.1\r\n"
                      "Host: beta.com\r\n"
                      "\r\n";
    const struct nfs_vhost *vh = nfs_vhost_route(&table, req, strlen(req));
    ASSERT_TRUE(vh != NULL);
    ASSERT_TRUE(strcmp(vh->hostname, "beta.com") == 0);
}

static void test_route_no_host_uses_default(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    nfs_vhost_add(&table, "default.com", "/default", 1);

    const char *req = "GET / HTTP/1.0\r\n"
                      "\r\n";
    const struct nfs_vhost *vh = nfs_vhost_route(&table, req, strlen(req));
    ASSERT_TRUE(vh != NULL);
    ASSERT_TRUE(strcmp(vh->hostname, "default.com") == 0);
}

static void test_route_null(void) {
    struct nfs_vhost_table table;
    nfs_vhost_table_init(&table);
    ASSERT_TRUE(nfs_vhost_route(&table, NULL, 0) == NULL);
}

/* ---- Validation tests ---- */

static void test_validate_http11_with_host(void) {
    const char *req = "GET / HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "\r\n";
    ASSERT_EQ(nfs_vhost_validate_request(req, strlen(req)), 0);
}

static void test_validate_http11_without_host(void) {
    const char *req = "GET / HTTP/1.1\r\n"
                      "\r\n";
    ASSERT_EQ(nfs_vhost_validate_request(req, strlen(req)), -1);
}

static void test_validate_http10_without_host(void) {
    const char *req = "GET / HTTP/1.0\r\n"
                      "\r\n";
    /* HTTP/1.0 does not require Host */
    ASSERT_EQ(nfs_vhost_validate_request(req, strlen(req)), 0);
}

static void test_validate_null(void) {
    ASSERT_EQ(nfs_vhost_validate_request(NULL, 0), -1);
}

int main(void) {
    printf("Virtual Hosts Tests\n");

    test_table_init();
    test_add_host();
    test_add_multiple_hosts();
    test_add_host_null();
    test_first_host_is_default();
    test_explicit_default();
    test_add_alias();
    test_add_alias_invalid_idx();
    test_lookup_primary();
    test_lookup_alias();
    test_lookup_case_insensitive();
    test_lookup_fallback_default();
    test_lookup_null();
    test_extract_host_basic();
    test_extract_host_with_port();
    test_extract_host_missing();
    test_extract_host_case_insensitive();
    test_parse_host_simple();
    test_parse_host_with_port();
    test_parse_host_null();
    test_route_basic();
    test_route_no_host_uses_default();
    test_route_null();
    test_validate_http11_with_host();
    test_validate_http11_without_host();
    test_validate_http10_without_host();
    test_validate_null();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
