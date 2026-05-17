/* Unit tests for toy HTTP server. */

#include "../http.h"
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

/* ---- Request parsing tests ---- */

static void test_parse_get_request(void) {
    const char *raw = "GET / HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "\r\n";
    struct nfs_http_request req;
    ASSERT_EQ(nfs_http_request_parse(raw, strlen(raw), &req), 0);
    ASSERT_TRUE(strcmp(req.method, "GET") == 0);
    ASSERT_TRUE(strcmp(req.uri, "/") == 0);
    ASSERT_TRUE(strcmp(req.version, "HTTP/1.1") == 0);
    ASSERT_EQ(req.header_count, 1);
    ASSERT_TRUE(strcmp(req.headers[0].name, "Host") == 0);
    ASSERT_TRUE(strcmp(req.headers[0].value, "example.com") == 0);
    ASSERT_EQ(req.body_len, 0);
}

static void test_parse_post_with_body(void) {
    const char *raw = "POST /submit HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "Content-Type: application/json\r\n"
                      "Content-Length: 13\r\n"
                      "\r\n"
                      "{\"key\":\"val\"}";
    struct nfs_http_request req;
    ASSERT_EQ(nfs_http_request_parse(raw, strlen(raw), &req), 0);
    ASSERT_TRUE(strcmp(req.method, "POST") == 0);
    ASSERT_TRUE(strcmp(req.uri, "/submit") == 0);
    ASSERT_EQ(req.header_count, 3);
    ASSERT_EQ(req.body_len, 13);
    ASSERT_TRUE(memcmp(req.body, "{\"key\":\"val\"}", 13) == 0);
}

static void test_parse_multiple_headers(void) {
    const char *raw = "GET /page HTTP/1.1\r\n"
                      "Host: test.com\r\n"
                      "Accept: text/html\r\n"
                      "Accept-Language: en-US\r\n"
                      "Connection: keep-alive\r\n"
                      "User-Agent: nfs/1.0\r\n"
                      "\r\n";
    struct nfs_http_request req;
    ASSERT_EQ(nfs_http_request_parse(raw, strlen(raw), &req), 0);
    ASSERT_EQ(req.header_count, 5);
}

static void test_parse_http10(void) {
    const char *raw = "GET /old HTTP/1.0\r\n"
                      "Host: legacy.com\r\n"
                      "\r\n";
    struct nfs_http_request req;
    ASSERT_EQ(nfs_http_request_parse(raw, strlen(raw), &req), 0);
    ASSERT_TRUE(strcmp(req.version, "HTTP/1.0") == 0);
}

static void test_parse_null_input(void) {
    struct nfs_http_request req;
    ASSERT_EQ(nfs_http_request_parse(NULL, 100, &req), -1);
}

static void test_parse_empty_input(void) {
    struct nfs_http_request req;
    ASSERT_EQ(nfs_http_request_parse("", 0, &req), -1);
}

static void test_parse_no_crlf(void) {
    struct nfs_http_request req;
    ASSERT_EQ(nfs_http_request_parse("GET / HTTP/1.1", 14, &req), -1);
}

static void test_get_header_case_insensitive(void) {
    const char *raw = "GET / HTTP/1.1\r\n"
                      "Content-Type: text/plain\r\n"
                      "X-Custom: hello\r\n"
                      "\r\n";
    struct nfs_http_request req;
    nfs_http_request_parse(raw, strlen(raw), &req);

    const char *ct = nfs_http_request_get_header(&req, "content-type");
    ASSERT_TRUE(ct != NULL);
    ASSERT_TRUE(strcmp(ct, "text/plain") == 0);

    const char *xc = nfs_http_request_get_header(&req, "X-CUSTOM");
    ASSERT_TRUE(xc != NULL);
    ASSERT_TRUE(strcmp(xc, "hello") == 0);
}

static void test_get_header_not_found(void) {
    const char *raw = "GET / HTTP/1.1\r\n"
                      "Host: test.com\r\n"
                      "\r\n";
    struct nfs_http_request req;
    nfs_http_request_parse(raw, strlen(raw), &req);

    ASSERT_TRUE(nfs_http_request_get_header(&req, "X-Missing") == NULL);
}

static void test_parse_head_method(void) {
    const char *raw = "HEAD /resource HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "\r\n";
    struct nfs_http_request req;
    ASSERT_EQ(nfs_http_request_parse(raw, strlen(raw), &req), 0);
    ASSERT_TRUE(strcmp(req.method, "HEAD") == 0);
}

static void test_parse_delete_method(void) {
    const char *raw = "DELETE /item/42 HTTP/1.1\r\n"
                      "Host: api.example.com\r\n"
                      "\r\n";
    struct nfs_http_request req;
    ASSERT_EQ(nfs_http_request_parse(raw, strlen(raw), &req), 0);
    ASSERT_TRUE(strcmp(req.method, "DELETE") == 0);
    ASSERT_TRUE(strcmp(req.uri, "/item/42") == 0);
}

/* ---- Response building tests ---- */

static void test_response_200(void) {
    struct nfs_http_response resp;
    nfs_http_response_init(&resp, 200);
    nfs_http_response_add_header(&resp, "Content-Type", "text/html");

    const char *body = "<h1>OK</h1>";
    nfs_http_response_set_body(&resp, (const uint8_t *)body, strlen(body));

    char buf[4096];
    int len = nfs_http_response_build(&resp, buf, sizeof(buf));
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buf, "HTTP/1.1 200 OK\r\n") != NULL);
    ASSERT_TRUE(strstr(buf, "Content-Type: text/html\r\n") != NULL);
    ASSERT_TRUE(strstr(buf, "<h1>OK</h1>") != NULL);
}

static void test_response_404(void) {
    struct nfs_http_response resp;
    nfs_http_response_init(&resp, 404);

    char buf[4096];
    int len = nfs_http_response_build(&resp, buf, sizeof(buf));
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buf, "HTTP/1.1 404 Not Found\r\n") != NULL);
}

static void test_response_301(void) {
    struct nfs_http_response resp;
    nfs_http_response_init(&resp, 301);
    nfs_http_response_add_header(&resp, "Location", "https://example.com/new");

    char buf[4096];
    int len = nfs_http_response_build(&resp, buf, sizeof(buf));
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buf, "301 Moved Permanently") != NULL);
    ASSERT_TRUE(strstr(buf, "Location: https://example.com/new\r\n") != NULL);
}

static void test_response_500(void) {
    struct nfs_http_response resp;
    nfs_http_response_init(&resp, 500);

    char buf[4096];
    int len = nfs_http_response_build(&resp, buf, sizeof(buf));
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buf, "500 Internal Server Error") != NULL);
}

static void test_response_no_body(void) {
    struct nfs_http_response resp;
    nfs_http_response_init(&resp, 204);

    char buf[4096];
    int len = nfs_http_response_build(&resp, buf, sizeof(buf));
    ASSERT_TRUE(len > 0);
    /* Should end with blank line */
    ASSERT_TRUE(len >= 4);
    ASSERT_TRUE(buf[len - 2] == '\r' && buf[len - 1] == '\n');
}

static void test_response_multiple_headers(void) {
    struct nfs_http_response resp;
    nfs_http_response_init(&resp, 200);
    nfs_http_response_add_header(&resp, "Content-Type", "text/plain");
    nfs_http_response_add_header(&resp, "Content-Length", "5");
    nfs_http_response_add_header(&resp, "Connection", "close");

    const char *body = "hello";
    nfs_http_response_set_body(&resp, (const uint8_t *)body, 5);

    char buf[4096];
    int len = nfs_http_response_build(&resp, buf, sizeof(buf));
    ASSERT_TRUE(len > 0);
    ASSERT_TRUE(strstr(buf, "Content-Type: text/plain\r\n") != NULL);
    ASSERT_TRUE(strstr(buf, "Content-Length: 5\r\n") != NULL);
    ASSERT_TRUE(strstr(buf, "Connection: close\r\n") != NULL);
}

static void test_response_build_null(void) {
    char buf[64];
    ASSERT_EQ(nfs_http_response_build(NULL, buf, sizeof(buf)), -1);
}

static void test_response_add_header_null(void) {
    struct nfs_http_response resp;
    nfs_http_response_init(&resp, 200);
    ASSERT_EQ(nfs_http_response_add_header(&resp, NULL, "val"), -1);
}

/* ---- Status string tests ---- */

static void test_status_str(void) {
    ASSERT_TRUE(strcmp(nfs_http_status_str(200), "OK") == 0);
    ASSERT_TRUE(strcmp(nfs_http_status_str(301), "Moved Permanently") == 0);
    ASSERT_TRUE(strcmp(nfs_http_status_str(400), "Bad Request") == 0);
    ASSERT_TRUE(strcmp(nfs_http_status_str(404), "Not Found") == 0);
    ASSERT_TRUE(strcmp(nfs_http_status_str(500), "Internal Server Error") == 0);
    ASSERT_TRUE(strcmp(nfs_http_status_str(999), "Unknown") == 0);
}

int main(void) {
    printf("HTTP Server Tests\n");

    test_parse_get_request();
    test_parse_post_with_body();
    test_parse_multiple_headers();
    test_parse_http10();
    test_parse_null_input();
    test_parse_empty_input();
    test_parse_no_crlf();
    test_get_header_case_insensitive();
    test_get_header_not_found();
    test_parse_head_method();
    test_parse_delete_method();
    test_response_200();
    test_response_404();
    test_response_301();
    test_response_500();
    test_response_no_body();
    test_response_multiple_headers();
    test_response_build_null();
    test_response_add_header_null();
    test_status_str();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
