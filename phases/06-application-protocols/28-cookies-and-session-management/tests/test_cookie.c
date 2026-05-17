/* Unit tests for Cookies and Session Management. */

#include "../cookie.h"
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

/* ---- Set-Cookie parsing ---- */

static void test_parse_simple(void) {
    struct nfs_cookie c;
    ASSERT_EQ(nfs_cookie_parse_set_cookie("name=value", &c), 0);
    ASSERT_TRUE(strcmp(c.name, "name") == 0);
    ASSERT_TRUE(strcmp(c.value, "value") == 0);
}

static void test_parse_with_domain_path(void) {
    struct nfs_cookie c;
    ASSERT_EQ(nfs_cookie_parse_set_cookie("sid=abc; Domain=example.com; Path=/app", &c), 0);
    ASSERT_TRUE(strcmp(c.name, "sid") == 0);
    ASSERT_TRUE(strcmp(c.value, "abc") == 0);
    ASSERT_TRUE(strcmp(c.domain, "example.com") == 0);
    ASSERT_TRUE(strcmp(c.path, "/app") == 0);
}

static void test_parse_secure_httponly(void) {
    struct nfs_cookie c;
    ASSERT_EQ(nfs_cookie_parse_set_cookie("tok=xyz; Secure; HttpOnly", &c), 0);
    ASSERT_EQ(c.secure, 1);
    ASSERT_EQ(c.http_only, 1);
}

static void test_parse_max_age(void) {
    struct nfs_cookie c;
    ASSERT_EQ(nfs_cookie_parse_set_cookie("pref=dark; Max-Age=86400", &c), 0);
    ASSERT_EQ(c.max_age, 86400);
}

static void test_parse_max_age_zero(void) {
    struct nfs_cookie c;
    ASSERT_EQ(nfs_cookie_parse_set_cookie("old=val; Max-Age=0", &c), 0);
    ASSERT_EQ(c.max_age, 0);
}

static void test_parse_samesite_strict(void) {
    struct nfs_cookie c;
    ASSERT_EQ(nfs_cookie_parse_set_cookie("s=1; SameSite=Strict", &c), 0);
    ASSERT_EQ(c.samesite, NFS_COOKIE_SAMESITE_STRICT);
}

static void test_parse_samesite_lax(void) {
    struct nfs_cookie c;
    ASSERT_EQ(nfs_cookie_parse_set_cookie("s=1; SameSite=Lax", &c), 0);
    ASSERT_EQ(c.samesite, NFS_COOKIE_SAMESITE_LAX);
}

static void test_parse_samesite_none(void) {
    struct nfs_cookie c;
    ASSERT_EQ(nfs_cookie_parse_set_cookie("s=1; SameSite=None", &c), 0);
    ASSERT_EQ(c.samesite, NFS_COOKIE_SAMESITE_NONE);
}

static void test_parse_full(void) {
    struct nfs_cookie c;
    ASSERT_EQ(nfs_cookie_parse_set_cookie("session=abc123; Domain=.example.com; Path=/; "
                                          "Max-Age=3600; Secure; HttpOnly; SameSite=Strict",
                                          &c),
              0);
    ASSERT_TRUE(strcmp(c.name, "session") == 0);
    ASSERT_TRUE(strcmp(c.value, "abc123") == 0);
    ASSERT_TRUE(strcmp(c.domain, ".example.com") == 0);
    ASSERT_TRUE(strcmp(c.path, "/") == 0);
    ASSERT_EQ(c.max_age, 3600);
    ASSERT_EQ(c.secure, 1);
    ASSERT_EQ(c.http_only, 1);
    ASSERT_EQ(c.samesite, NFS_COOKIE_SAMESITE_STRICT);
}

static void test_parse_null(void) {
    struct nfs_cookie c;
    ASSERT_EQ(nfs_cookie_parse_set_cookie(NULL, &c), -1);
}

static void test_parse_no_equals(void) {
    struct nfs_cookie c;
    ASSERT_EQ(nfs_cookie_parse_set_cookie("noequalshere", &c), -1);
}

/* ---- Cookie jar ---- */

static void test_jar_init(void) {
    struct nfs_cookie_jar jar;
    nfs_cookie_jar_init(&jar);
    ASSERT_EQ(jar.count, 0);
}

static void test_jar_add_and_lookup(void) {
    struct nfs_cookie_jar jar;
    nfs_cookie_jar_init(&jar);

    struct nfs_cookie c;
    nfs_cookie_parse_set_cookie("sid=123; Domain=example.com; Path=/", &c);
    ASSERT_EQ(nfs_cookie_jar_add(&jar, &c), 0);
    ASSERT_EQ(jar.count, 1);

    const struct nfs_cookie *found = nfs_cookie_jar_lookup(&jar, "sid", "example.com", "/");
    ASSERT_TRUE(found != NULL);
    ASSERT_TRUE(strcmp(found->value, "123") == 0);
}

static void test_jar_update_existing(void) {
    struct nfs_cookie_jar jar;
    nfs_cookie_jar_init(&jar);

    struct nfs_cookie c;
    nfs_cookie_parse_set_cookie("sid=old; Domain=example.com; Path=/", &c);
    nfs_cookie_jar_add(&jar, &c);

    nfs_cookie_parse_set_cookie("sid=new; Domain=example.com; Path=/", &c);
    nfs_cookie_jar_add(&jar, &c);

    ASSERT_EQ(jar.count, 1); /* should replace, not add */

    const struct nfs_cookie *found = nfs_cookie_jar_lookup(&jar, "sid", "example.com", "/");
    ASSERT_TRUE(found != NULL);
    ASSERT_TRUE(strcmp(found->value, "new") == 0);
}

static void test_jar_lookup_not_found(void) {
    struct nfs_cookie_jar jar;
    nfs_cookie_jar_init(&jar);
    ASSERT_TRUE(nfs_cookie_jar_lookup(&jar, "x", "y.com", "/") == NULL);
}

static void test_jar_expire(void) {
    struct nfs_cookie_jar jar;
    nfs_cookie_jar_init(&jar);

    struct nfs_cookie c;
    nfs_cookie_parse_set_cookie("alive=yes; Domain=a.com; Path=/; Max-Age=100", &c);
    nfs_cookie_jar_add(&jar, &c);

    nfs_cookie_parse_set_cookie("dead=no; Domain=a.com; Path=/; Max-Age=0", &c);
    nfs_cookie_jar_add(&jar, &c);

    ASSERT_EQ(jar.count, 2);

    int removed = nfs_cookie_jar_expire(&jar);
    ASSERT_EQ(removed, 1);
    ASSERT_EQ(jar.count, 1);
    ASSERT_TRUE(strcmp(jar.cookies[0].name, "alive") == 0);
}

/* ---- Cookie header building ---- */

static void test_build_header_single(void) {
    struct nfs_cookie_jar jar;
    nfs_cookie_jar_init(&jar);

    struct nfs_cookie c;
    nfs_cookie_parse_set_cookie("sid=abc; Domain=example.com; Path=/", &c);
    nfs_cookie_jar_add(&jar, &c);

    char out[256];
    int n = nfs_cookie_build_header(&jar, "example.com", "/page", out, sizeof(out));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strcmp(out, "sid=abc") == 0);
}

static void test_build_header_multiple(void) {
    struct nfs_cookie_jar jar;
    nfs_cookie_jar_init(&jar);

    struct nfs_cookie c;
    nfs_cookie_parse_set_cookie("a=1; Domain=example.com; Path=/", &c);
    nfs_cookie_jar_add(&jar, &c);
    nfs_cookie_parse_set_cookie("b=2; Domain=example.com; Path=/", &c);
    nfs_cookie_jar_add(&jar, &c);

    char out[256];
    nfs_cookie_build_header(&jar, "example.com", "/", out, sizeof(out));
    ASSERT_TRUE(strstr(out, "a=1") != NULL);
    ASSERT_TRUE(strstr(out, "b=2") != NULL);
    ASSERT_TRUE(strstr(out, "; ") != NULL);
}

static void test_build_header_domain_filter(void) {
    struct nfs_cookie_jar jar;
    nfs_cookie_jar_init(&jar);

    struct nfs_cookie c;
    nfs_cookie_parse_set_cookie("a=1; Domain=example.com; Path=/", &c);
    nfs_cookie_jar_add(&jar, &c);
    nfs_cookie_parse_set_cookie("b=2; Domain=other.com; Path=/", &c);
    nfs_cookie_jar_add(&jar, &c);

    char out[256];
    nfs_cookie_build_header(&jar, "example.com", "/", out, sizeof(out));
    ASSERT_TRUE(strstr(out, "a=1") != NULL);
    ASSERT_TRUE(strstr(out, "b=2") == NULL);
}

static void test_build_header_path_filter(void) {
    struct nfs_cookie_jar jar;
    nfs_cookie_jar_init(&jar);

    struct nfs_cookie c;
    nfs_cookie_parse_set_cookie("a=1; Domain=example.com; Path=/api", &c);
    nfs_cookie_jar_add(&jar, &c);

    char out[256];
    /* /api/v1 should match /api prefix */
    nfs_cookie_build_header(&jar, "example.com", "/api/v1", out, sizeof(out));
    ASSERT_TRUE(strstr(out, "a=1") != NULL);

    /* /other should NOT match /api prefix */
    nfs_cookie_build_header(&jar, "example.com", "/other", out, sizeof(out));
    ASSERT_TRUE(strstr(out, "a=1") == NULL);
}

static void test_build_header_subdomain_match(void) {
    struct nfs_cookie_jar jar;
    nfs_cookie_jar_init(&jar);

    struct nfs_cookie c;
    nfs_cookie_parse_set_cookie("a=1; Domain=.example.com; Path=/", &c);
    nfs_cookie_jar_add(&jar, &c);

    char out[256];
    nfs_cookie_build_header(&jar, "www.example.com", "/", out, sizeof(out));
    ASSERT_TRUE(strstr(out, "a=1") != NULL);
}

static void test_build_header_expired_skipped(void) {
    struct nfs_cookie_jar jar;
    nfs_cookie_jar_init(&jar);

    struct nfs_cookie c;
    nfs_cookie_parse_set_cookie("a=1; Domain=example.com; Path=/; Max-Age=0", &c);
    nfs_cookie_jar_add(&jar, &c);

    char out[256];
    int n = nfs_cookie_build_header(&jar, "example.com", "/", out, sizeof(out));
    ASSERT_EQ(n, 0); /* no cookies match (expired) */
}

static void test_build_header_null(void) {
    ASSERT_EQ(nfs_cookie_build_header(NULL, "a", "/", NULL, 0), -1);
}

int main(void) {
    printf("Cookies and Session Management Tests\n");

    test_parse_simple();
    test_parse_with_domain_path();
    test_parse_secure_httponly();
    test_parse_max_age();
    test_parse_max_age_zero();
    test_parse_samesite_strict();
    test_parse_samesite_lax();
    test_parse_samesite_none();
    test_parse_full();
    test_parse_null();
    test_parse_no_equals();
    test_jar_init();
    test_jar_add_and_lookup();
    test_jar_update_existing();
    test_jar_lookup_not_found();
    test_jar_expire();
    test_build_header_single();
    test_build_header_multiple();
    test_build_header_domain_filter();
    test_build_header_path_filter();
    test_build_header_subdomain_match();
    test_build_header_expired_skipped();
    test_build_header_null();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
