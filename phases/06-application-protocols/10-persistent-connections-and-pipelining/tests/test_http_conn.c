/* Unit tests for persistent connections and pipelining. */

#include "../http_conn.h"
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

/* ---- HTTP/1.1 connection tests ---- */

static void test_http11_default_persistent(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 0);
    ASSERT_EQ(nfs_http_conn_get_state(&conn), NFS_HTTP_CONN_IDLE);

    nfs_http_conn_on_request(&conn, NULL);
    ASSERT_EQ(nfs_http_conn_get_state(&conn), NFS_HTTP_CONN_ACTIVE);

    nfs_http_conn_on_response(&conn);
    ASSERT_EQ(nfs_http_conn_get_state(&conn), NFS_HTTP_CONN_KEEP_ALIVE);
    ASSERT_EQ(nfs_http_conn_should_close(&conn), 0);
}

static void test_http11_connection_close(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 0);

    nfs_http_conn_on_request(&conn, "close");
    nfs_http_conn_on_response(&conn);
    ASSERT_EQ(nfs_http_conn_get_state(&conn), NFS_HTTP_CONN_CLOSE);
    ASSERT_EQ(nfs_http_conn_should_close(&conn), 1);
}

static void test_http11_multiple_requests(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 0);

    for (int i = 0; i < 5; i++) {
        nfs_http_conn_on_request(&conn, NULL);
        nfs_http_conn_on_response(&conn);
        ASSERT_EQ(nfs_http_conn_get_state(&conn), NFS_HTTP_CONN_KEEP_ALIVE);
    }
    ASSERT_EQ(conn.request_count, 5);
}

static void test_http11_response_header_default(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 0);
    nfs_http_conn_on_request(&conn, NULL);
    /* HTTP/1.1 default: no Connection header needed */
    ASSERT_TRUE(nfs_http_conn_response_header(&conn) == NULL);
}

static void test_http11_response_header_close(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 0);
    nfs_http_conn_on_request(&conn, "close");
    const char *hdr = nfs_http_conn_response_header(&conn);
    ASSERT_TRUE(hdr != NULL);
    ASSERT_TRUE(strcmp(hdr, "close") == 0);
}

/* ---- HTTP/1.0 connection tests ---- */

static void test_http10_default_close(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_10, 0);

    nfs_http_conn_on_request(&conn, NULL);
    nfs_http_conn_on_response(&conn);
    ASSERT_EQ(nfs_http_conn_get_state(&conn), NFS_HTTP_CONN_CLOSE);
    ASSERT_EQ(nfs_http_conn_should_close(&conn), 1);
}

static void test_http10_keep_alive(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_10, 0);

    nfs_http_conn_on_request(&conn, "keep-alive");
    nfs_http_conn_on_response(&conn);
    ASSERT_EQ(nfs_http_conn_get_state(&conn), NFS_HTTP_CONN_KEEP_ALIVE);
    ASSERT_EQ(nfs_http_conn_should_close(&conn), 0);
}

static void test_http10_response_header_close(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_10, 0);
    nfs_http_conn_on_request(&conn, NULL);
    const char *hdr = nfs_http_conn_response_header(&conn);
    ASSERT_TRUE(hdr != NULL);
    ASSERT_TRUE(strcmp(hdr, "close") == 0);
}

static void test_http10_response_header_keep_alive(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_10, 0);
    nfs_http_conn_on_request(&conn, "keep-alive");
    const char *hdr = nfs_http_conn_response_header(&conn);
    ASSERT_TRUE(hdr != NULL);
    ASSERT_TRUE(strcmp(hdr, "keep-alive") == 0);
}

/* ---- Max requests tests ---- */

static void test_max_requests(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 3);

    nfs_http_conn_on_request(&conn, NULL);
    nfs_http_conn_on_response(&conn);
    ASSERT_EQ(nfs_http_conn_get_state(&conn), NFS_HTTP_CONN_KEEP_ALIVE);

    nfs_http_conn_on_request(&conn, NULL);
    nfs_http_conn_on_response(&conn);
    ASSERT_EQ(nfs_http_conn_get_state(&conn), NFS_HTTP_CONN_KEEP_ALIVE);

    nfs_http_conn_on_request(&conn, NULL);
    nfs_http_conn_on_response(&conn);
    /* Third request: should close */
    ASSERT_EQ(nfs_http_conn_get_state(&conn), NFS_HTTP_CONN_CLOSE);
    ASSERT_EQ(nfs_http_conn_should_close(&conn), 1);
}

/* ---- Pipeline tests ---- */

static void test_pipeline_enqueue_dequeue(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 0);

    ASSERT_EQ(nfs_http_pipeline_enqueue(&conn, "GET", "/a"), 0);
    ASSERT_EQ(nfs_http_pipeline_enqueue(&conn, "GET", "/b"), 0);
    ASSERT_EQ(nfs_http_pipeline_pending(&conn), 2);

    const struct nfs_http_pipeline_entry *e = nfs_http_pipeline_peek(&conn);
    ASSERT_TRUE(e != NULL);
    ASSERT_TRUE(strcmp(e->method, "GET") == 0);
    ASSERT_TRUE(strcmp(e->uri, "/a") == 0);

    ASSERT_EQ(nfs_http_pipeline_dequeue(&conn), 0);
    ASSERT_EQ(nfs_http_pipeline_pending(&conn), 1);

    e = nfs_http_pipeline_peek(&conn);
    ASSERT_TRUE(strcmp(e->uri, "/b") == 0);
}

static void test_pipeline_empty(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 0);

    ASSERT_EQ(nfs_http_pipeline_pending(&conn), 0);
    ASSERT_TRUE(nfs_http_pipeline_peek(&conn) == NULL);
    ASSERT_EQ(nfs_http_pipeline_dequeue(&conn), -1);
}

static void test_pipeline_full(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 0);

    for (int i = 0; i < NFS_HTTP_MAX_PIPELINE; i++) {
        ASSERT_EQ(nfs_http_pipeline_enqueue(&conn, "GET", "/x"), 0);
    }
    /* Queue full */
    ASSERT_EQ(nfs_http_pipeline_enqueue(&conn, "GET", "/y"), -1);
}

static void test_pipeline_fifo_order(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 0);

    nfs_http_pipeline_enqueue(&conn, "GET", "/first");
    nfs_http_pipeline_enqueue(&conn, "POST", "/second");
    nfs_http_pipeline_enqueue(&conn, "PUT", "/third");

    const struct nfs_http_pipeline_entry *e;

    e = nfs_http_pipeline_peek(&conn);
    ASSERT_TRUE(strcmp(e->uri, "/first") == 0);
    nfs_http_pipeline_dequeue(&conn);

    e = nfs_http_pipeline_peek(&conn);
    ASSERT_TRUE(strcmp(e->uri, "/second") == 0);
    ASSERT_TRUE(strcmp(e->method, "POST") == 0);
    nfs_http_pipeline_dequeue(&conn);

    e = nfs_http_pipeline_peek(&conn);
    ASSERT_TRUE(strcmp(e->uri, "/third") == 0);
    nfs_http_pipeline_dequeue(&conn);

    ASSERT_EQ(nfs_http_pipeline_pending(&conn), 0);
}

static void test_pipeline_reset(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 0);

    nfs_http_pipeline_enqueue(&conn, "GET", "/a");
    nfs_http_pipeline_enqueue(&conn, "GET", "/b");
    ASSERT_EQ(nfs_http_pipeline_pending(&conn), 2);

    nfs_http_pipeline_reset(&conn);
    ASSERT_EQ(nfs_http_pipeline_pending(&conn), 0);
    ASSERT_TRUE(nfs_http_pipeline_peek(&conn) == NULL);
}

static void test_pipeline_null(void) {
    ASSERT_EQ(nfs_http_pipeline_enqueue(NULL, "GET", "/x"), -1);
    ASSERT_EQ(nfs_http_pipeline_pending(NULL), 0);
}

/* ---- Init tests ---- */

static void test_init_defaults(void) {
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 100);
    ASSERT_EQ(conn.state, NFS_HTTP_CONN_IDLE);
    ASSERT_EQ(conn.version, NFS_HTTP_VER_11);
    ASSERT_EQ(conn.max_requests, 100);
    ASSERT_EQ(conn.request_count, 0);
    ASSERT_EQ(conn.pipeline_count, 0);
}

int main(void) {
    printf("Persistent Connections Tests\n");

    test_http11_default_persistent();
    test_http11_connection_close();
    test_http11_multiple_requests();
    test_http11_response_header_default();
    test_http11_response_header_close();
    test_http10_default_close();
    test_http10_keep_alive();
    test_http10_response_header_close();
    test_http10_response_header_keep_alive();
    test_max_requests();
    test_pipeline_enqueue_dequeue();
    test_pipeline_empty();
    test_pipeline_full();
    test_pipeline_fifo_order();
    test_pipeline_reset();
    test_pipeline_null();
    test_init_defaults();

    printf("\n%d/%d passed\n", tests_passed, tests_run);
    if (tests_passed == tests_run) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}
