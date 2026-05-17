/* Unit tests for URL encoding & MIME types. */

#include "../urlenc.h"
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

/* ---- URL encoding tests ---- */

static void test_url_encode_passthrough(void) {
    char out[64];
    int n = nfs_url_encode("hello", out, sizeof(out));
    ASSERT_EQ(n, 5);
    ASSERT_TRUE(strcmp(out, "hello") == 0);
}

static void test_url_encode_unreserved(void) {
    char out[128];
    int n = nfs_url_encode("A-Z_a-z.0~9", out, sizeof(out));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strcmp(out, "A-Z_a-z.0~9") == 0);
}

static void test_url_encode_space(void) {
    char out[64];
    nfs_url_encode("hello world", out, sizeof(out));
    ASSERT_TRUE(strcmp(out, "hello%20world") == 0);
}

static void test_url_encode_special(void) {
    char out[64];
    nfs_url_encode("a=1&b=2", out, sizeof(out));
    ASSERT_TRUE(strcmp(out, "a%3D1%26b%3D2") == 0);
}

static void test_url_encode_at_sign(void) {
    char out[64];
    nfs_url_encode("user@host", out, sizeof(out));
    ASSERT_TRUE(strcmp(out, "user%40host") == 0);
}

static void test_url_encode_slash(void) {
    char out[64];
    nfs_url_encode("/path/to", out, sizeof(out));
    ASSERT_TRUE(strcmp(out, "%2Fpath%2Fto") == 0);
}

static void test_url_encode_empty(void) {
    char out[64];
    int n = nfs_url_encode("", out, sizeof(out));
    ASSERT_EQ(n, 0);
    ASSERT_TRUE(strcmp(out, "") == 0);
}

/* ---- URL decoding tests ---- */

static void test_url_decode_passthrough(void) {
    char out[64];
    int n = nfs_url_decode("hello", out, sizeof(out));
    ASSERT_EQ(n, 5);
    ASSERT_TRUE(strcmp(out, "hello") == 0);
}

static void test_url_decode_space(void) {
    char out[64];
    nfs_url_decode("hello%20world", out, sizeof(out));
    ASSERT_TRUE(strcmp(out, "hello world") == 0);
}

static void test_url_decode_special(void) {
    char out[64];
    nfs_url_decode("a%3D1%26b%3D2", out, sizeof(out));
    ASSERT_TRUE(strcmp(out, "a=1&b=2") == 0);
}

static void test_url_decode_lowercase_hex(void) {
    char out[64];
    nfs_url_decode("%2f%3a", out, sizeof(out));
    ASSERT_TRUE(strcmp(out, "/:") == 0);
}

static void test_url_decode_invalid_hex(void) {
    char out[64];
    ASSERT_EQ(nfs_url_decode("%ZZ", out, sizeof(out)), -1);
}

static void test_url_decode_truncated_percent(void) {
    char out[64];
    ASSERT_EQ(nfs_url_decode("abc%2", out, sizeof(out)), -1);
}

/* ---- URL encode/decode roundtrip ---- */

static void test_url_roundtrip(void) {
    const char *input = "hello world! foo@bar.com/path?q=1&b=2#frag";
    char encoded[256];
    char decoded[256];

    int n1 = nfs_url_encode(input, encoded, sizeof(encoded));
    ASSERT_TRUE(n1 > 0);

    int n2 = nfs_url_decode(encoded, decoded, sizeof(decoded));
    ASSERT_TRUE(n2 > 0);

    ASSERT_TRUE(strcmp(decoded, input) == 0);
}

/* ---- Form encoding tests ---- */

static void test_form_encode_space_to_plus(void) {
    char out[64];
    nfs_form_encode("hello world", out, sizeof(out));
    ASSERT_TRUE(strcmp(out, "hello+world") == 0);
}

static void test_form_encode_special(void) {
    char out[64];
    nfs_form_encode("a=1&b=2", out, sizeof(out));
    ASSERT_TRUE(strcmp(out, "a%3D1%26b%3D2") == 0);
}

static void test_form_decode_plus_to_space(void) {
    char out[64];
    nfs_form_decode("hello+world", out, sizeof(out));
    ASSERT_TRUE(strcmp(out, "hello world") == 0);
}

static void test_form_decode_percent(void) {
    char out[64];
    nfs_form_decode("a%3D1%26b%3D2", out, sizeof(out));
    ASSERT_TRUE(strcmp(out, "a=1&b=2") == 0);
}

static void test_form_roundtrip(void) {
    const char *input = "name=John Doe&city=New York";
    char encoded[256];
    char decoded[256];

    nfs_form_encode(input, encoded, sizeof(encoded));
    nfs_form_decode(encoded, decoded, sizeof(decoded));
    ASSERT_TRUE(strcmp(decoded, input) == 0);
}

/* ---- MIME type tests ---- */

static void test_mime_html(void) {
    ASSERT_TRUE(strcmp(nfs_mime_type_for_ext("html"), "text/html") == 0);
}

static void test_mime_json(void) {
    ASSERT_TRUE(strcmp(nfs_mime_type_for_ext("json"), "application/json") == 0);
}

static void test_mime_png(void) {
    ASSERT_TRUE(strcmp(nfs_mime_type_for_ext("png"), "image/png") == 0);
}

static void test_mime_pdf(void) {
    ASSERT_TRUE(strcmp(nfs_mime_type_for_ext("pdf"), "application/pdf") == 0);
}

static void test_mime_css(void) {
    ASSERT_TRUE(strcmp(nfs_mime_type_for_ext("css"), "text/css") == 0);
}

static void test_mime_js(void) {
    ASSERT_TRUE(strcmp(nfs_mime_type_for_ext("js"), "application/javascript") == 0);
}

static void test_mime_unknown(void) {
    ASSERT_TRUE(strcmp(nfs_mime_type_for_ext("xyz"), "application/octet-stream") == 0);
}

static void test_mime_null(void) {
    ASSERT_TRUE(strcmp(nfs_mime_type_for_ext(NULL), "application/octet-stream") == 0);
}

static void test_mime_case_insensitive(void) {
    ASSERT_TRUE(strcmp(nfs_mime_type_for_ext("HTML"), "text/html") == 0);
    ASSERT_TRUE(strcmp(nfs_mime_type_for_ext("Json"), "application/json") == 0);
}

/* ---- Error cases ---- */

static void test_url_encode_null(void) {
    ASSERT_EQ(nfs_url_encode(NULL, NULL, 0), -1);
}

static void test_url_decode_null(void) {
    ASSERT_EQ(nfs_url_decode(NULL, NULL, 0), -1);
}

static void test_url_encode_buffer_small(void) {
    char out[3]; /* "a b" would encode to "a%20b" (5 chars + nul) */
    ASSERT_EQ(nfs_url_encode("a b", out, sizeof(out)), -1);
}

int main(void) {
    printf("URL Encoding & MIME Types Tests\n");

    test_url_encode_passthrough();
    test_url_encode_unreserved();
    test_url_encode_space();
    test_url_encode_special();
    test_url_encode_at_sign();
    test_url_encode_slash();
    test_url_encode_empty();
    test_url_decode_passthrough();
    test_url_decode_space();
    test_url_decode_special();
    test_url_decode_lowercase_hex();
    test_url_decode_invalid_hex();
    test_url_decode_truncated_percent();
    test_url_roundtrip();
    test_form_encode_space_to_plus();
    test_form_encode_special();
    test_form_decode_plus_to_space();
    test_form_decode_percent();
    test_form_roundtrip();
    test_mime_html();
    test_mime_json();
    test_mime_png();
    test_mime_pdf();
    test_mime_css();
    test_mime_js();
    test_mime_unknown();
    test_mime_null();
    test_mime_case_insensitive();
    test_url_encode_null();
    test_url_decode_null();
    test_url_encode_buffer_small();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
