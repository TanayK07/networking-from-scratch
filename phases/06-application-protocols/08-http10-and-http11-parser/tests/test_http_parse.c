/* Unit tests for HTTP/1.0 and HTTP/1.1 parser. */

#include "../http_parse.h"
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

/* ---- Parse GET request ---- */

static void test_parse_get_request(void) {
    const char *data = "GET /index.html HTTP/1.1\r\n"
                       "Host: www.example.com\r\n"
                       "User-Agent: test/1.0\r\n"
                       "\r\n";

    struct nfs_http_request req;
    int consumed = nfs_http_parse_request(data, strlen(data), &req);
    ASSERT_TRUE(consumed > 0);
    ASSERT_TRUE(strcmp(req.method, "GET") == 0);
    ASSERT_TRUE(strcmp(req.uri, "/index.html") == 0);
    ASSERT_EQ(req.version_major, 1);
    ASSERT_EQ(req.version_minor, 1);
    ASSERT_EQ(req.header_count, 2);
    ASSERT_TRUE(strcmp(req.headers[0].name, "Host") == 0);
    ASSERT_TRUE(strcmp(req.headers[0].value, "www.example.com") == 0);
    ASSERT_TRUE(strcmp(req.headers[1].name, "User-Agent") == 0);
    ASSERT_TRUE(strcmp(req.headers[1].value, "test/1.0") == 0);
    ASSERT_EQ(req.has_content_length, 0);
}

/* ---- Parse POST with Content-Length ---- */

static void test_parse_post_with_content_length(void) {
    const char *data = "POST /submit HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: 42\r\n"
                       "\r\n"
                       "{\"key\":\"value\"}";

    struct nfs_http_request req;
    int consumed = nfs_http_parse_request(data, strlen(data), &req);
    ASSERT_TRUE(consumed > 0);
    ASSERT_TRUE(strcmp(req.method, "POST") == 0);
    ASSERT_TRUE(strcmp(req.uri, "/submit") == 0);
    ASSERT_EQ(req.header_count, 3);
    ASSERT_EQ(req.has_content_length, 1);
    ASSERT_EQ(req.content_length, 42);
}

/* ---- Parse HTTP/1.0 vs 1.1 ---- */

static void test_parse_http10(void) {
    const char *data = "GET / HTTP/1.0\r\n"
                       "Host: legacy.example.com\r\n"
                       "\r\n";

    struct nfs_http_request req;
    int consumed = nfs_http_parse_request(data, strlen(data), &req);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ(req.version_major, 1);
    ASSERT_EQ(req.version_minor, 0);
}

static void test_parse_http11(void) {
    const char *data = "GET / HTTP/1.1\r\n"
                       "Host: modern.example.com\r\n"
                       "\r\n";

    struct nfs_http_request req;
    int consumed = nfs_http_parse_request(data, strlen(data), &req);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ(req.version_major, 1);
    ASSERT_EQ(req.version_minor, 1);
}

/* ---- Parse response 200 OK ---- */

static void test_parse_response_200(void) {
    const char *data = "HTTP/1.1 200 OK\r\n"
                       "Content-Type: text/html\r\n"
                       "Content-Length: 1234\r\n"
                       "\r\n";

    struct nfs_http_response resp;
    int consumed = nfs_http_parse_response(data, strlen(data), &resp);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ(resp.version_major, 1);
    ASSERT_EQ(resp.version_minor, 1);
    ASSERT_EQ(resp.status_code, 200);
    ASSERT_TRUE(strcmp(resp.reason, "OK") == 0);
    ASSERT_EQ(resp.header_count, 2);
    ASSERT_EQ(resp.has_content_length, 1);
    ASSERT_EQ(resp.content_length, 1234);
}

/* ---- Parse response 404 ---- */

static void test_parse_response_404(void) {
    const char *data = "HTTP/1.1 404 Not Found\r\n"
                       "Content-Type: text/html\r\n"
                       "\r\n";

    struct nfs_http_response resp;
    int consumed = nfs_http_parse_response(data, strlen(data), &resp);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ(resp.status_code, 404);
    ASSERT_TRUE(strcmp(resp.reason, "Not Found") == 0);
}

/* ---- Case-insensitive header lookup ---- */

static void test_find_header_case_insensitive(void) {
    const char *data = "GET / HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "content-type: text/plain\r\n"
                       "X-Custom-Header: custom-value\r\n"
                       "\r\n";

    struct nfs_http_request req;
    nfs_http_parse_request(data, strlen(data), &req);

    const char *ct = nfs_http_find_header(req.headers, req.header_count, "Content-Type");
    ASSERT_TRUE(ct != NULL);
    ASSERT_TRUE(strcmp(ct, "text/plain") == 0);

    const char *host = nfs_http_find_header(req.headers, req.header_count, "HOST");
    ASSERT_TRUE(host != NULL);
    ASSERT_TRUE(strcmp(host, "example.com") == 0);

    const char *custom = nfs_http_find_header(req.headers, req.header_count, "x-custom-header");
    ASSERT_TRUE(custom != NULL);
    ASSERT_TRUE(strcmp(custom, "custom-value") == 0);

    const char *missing = nfs_http_find_header(req.headers, req.header_count, "X-Missing");
    ASSERT_TRUE(missing == NULL);
}

/* ---- Build request ---- */

static void test_build_request(void) {
    struct nfs_http_header extra[1];
    snprintf(extra[0].name, sizeof(extra[0].name), "Accept");
    snprintf(extra[0].value, sizeof(extra[0].value), "text/html");

    char buf[1024];
    int n = nfs_http_build_request("GET", "/path", "example.com", extra, 1, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strstr(buf, "GET /path HTTP/1.1\r\n") != NULL);
    ASSERT_TRUE(strstr(buf, "Host: example.com\r\n") != NULL);
    ASSERT_TRUE(strstr(buf, "Accept: text/html\r\n") != NULL);
    /* Must end with \r\n\r\n */
    ASSERT_TRUE(n >= 4);
    ASSERT_TRUE(strcmp(buf + n - 4, "\r\n\r\n") == 0);
}

/* ---- Multiple headers ---- */

static void test_multiple_headers(void) {
    const char *data = "GET / HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "Accept: text/html\r\n"
                       "Accept-Language: en\r\n"
                       "Accept-Encoding: gzip\r\n"
                       "Connection: keep-alive\r\n"
                       "\r\n";

    struct nfs_http_request req;
    int consumed = nfs_http_parse_request(data, strlen(data), &req);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ(req.header_count, 5);
}

/* ---- Empty header value ---- */

static void test_empty_header_value(void) {
    const char *data = "GET / HTTP/1.1\r\n"
                       "Host: example.com\r\n"
                       "X-Empty:\r\n"
                       "\r\n";

    struct nfs_http_request req;
    int consumed = nfs_http_parse_request(data, strlen(data), &req);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ(req.header_count, 2);

    const char *val = nfs_http_find_header(req.headers, req.header_count, "X-Empty");
    ASSERT_TRUE(val != NULL);
    ASSERT_TRUE(strcmp(val, "") == 0);
}

/* ---- Malformed request rejected ---- */

static void test_malformed_request_no_version(void) {
    const char *data = "GET /\r\n\r\n";
    struct nfs_http_request req;
    int consumed = nfs_http_parse_request(data, strlen(data), &req);
    ASSERT_EQ(consumed, -1);
}

static void test_malformed_request_no_crlf(void) {
    const char *data = "GET / HTTP/1.1\nHost: x\n\n";
    struct nfs_http_request req;
    int consumed = nfs_http_parse_request(data, strlen(data), &req);
    ASSERT_EQ(consumed, -1);
}

/* ---- Status reason lookup ---- */

static void test_status_reasons(void) {
    ASSERT_TRUE(strcmp(nfs_http_status_reason(200), "OK") == 0);
    ASSERT_TRUE(strcmp(nfs_http_status_reason(301), "Moved Permanently") == 0);
    ASSERT_TRUE(strcmp(nfs_http_status_reason(404), "Not Found") == 0);
    ASSERT_TRUE(strcmp(nfs_http_status_reason(500), "Internal Server Error") == 0);
    ASSERT_TRUE(strcmp(nfs_http_status_reason(999), "Unknown") == 0);
}

/* ---- Response with HTTP/1.0 ---- */

static void test_parse_response_http10(void) {
    const char *data = "HTTP/1.0 302 Found\r\n"
                       "Location: http://example.com/new\r\n"
                       "\r\n";

    struct nfs_http_response resp;
    int consumed = nfs_http_parse_response(data, strlen(data), &resp);
    ASSERT_TRUE(consumed > 0);
    ASSERT_EQ(resp.version_major, 1);
    ASSERT_EQ(resp.version_minor, 0);
    ASSERT_EQ(resp.status_code, 302);
    ASSERT_TRUE(strcmp(resp.reason, "Found") == 0);

    const char *loc = nfs_http_find_header(resp.headers, resp.header_count, "Location");
    ASSERT_TRUE(loc != NULL);
    ASSERT_TRUE(strcmp(loc, "http://example.com/new") == 0);
}

/* ---- Build request no extra headers ---- */

static void test_build_request_no_extras(void) {
    char buf[512];
    int n = nfs_http_build_request("HEAD", "/", "test.com", NULL, 0, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);
    ASSERT_TRUE(strstr(buf, "HEAD / HTTP/1.1\r\n") != NULL);
    ASSERT_TRUE(strstr(buf, "Host: test.com\r\n") != NULL);
}

/* ---- PUT request ---- */

static void test_parse_put_request(void) {
    const char *data = "PUT /resource HTTP/1.1\r\n"
                       "Host: api.example.com\r\n"
                       "Content-Type: application/json\r\n"
                       "Content-Length: 15\r\n"
                       "\r\n";

    struct nfs_http_request req;
    int consumed = nfs_http_parse_request(data, strlen(data), &req);
    ASSERT_TRUE(consumed > 0);
    ASSERT_TRUE(strcmp(req.method, "PUT") == 0);
    ASSERT_TRUE(strcmp(req.uri, "/resource") == 0);
    ASSERT_EQ(req.has_content_length, 1);
    ASSERT_EQ(req.content_length, 15);
}

/* ---- DELETE request ---- */

static void test_parse_delete_request(void) {
    const char *data = "DELETE /item/42 HTTP/1.1\r\n"
                       "Host: api.example.com\r\n"
                       "\r\n";

    struct nfs_http_request req;
    int consumed = nfs_http_parse_request(data, strlen(data), &req);
    ASSERT_TRUE(consumed > 0);
    ASSERT_TRUE(strcmp(req.method, "DELETE") == 0);
    ASSERT_TRUE(strcmp(req.uri, "/item/42") == 0);
}

int main(void) {
    printf("HTTP/1.0 & HTTP/1.1 Parser Tests\n");

    test_parse_get_request();
    test_parse_post_with_content_length();
    test_parse_http10();
    test_parse_http11();
    test_parse_response_200();
    test_parse_response_404();
    test_find_header_case_insensitive();
    test_build_request();
    test_multiple_headers();
    test_empty_header_value();
    test_malformed_request_no_version();
    test_malformed_request_no_crlf();
    test_status_reasons();
    test_parse_response_http10();
    test_build_request_no_extras();
    test_parse_put_request();
    test_parse_delete_request();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
