/* Demo driver for persistent connections and pipelining lesson. */

#include "http_conn.h"
#include <stdio.h>

static const char *state_str(nfs_http_conn_state_t s)
{
    switch (s) {
    case NFS_HTTP_CONN_IDLE:       return "IDLE";
    case NFS_HTTP_CONN_ACTIVE:     return "ACTIVE";
    case NFS_HTTP_CONN_KEEP_ALIVE: return "KEEP_ALIVE";
    case NFS_HTTP_CONN_CLOSE:      return "CLOSE";
    default:                       return "UNKNOWN";
    }
}

int main(void)
{
    /* HTTP/1.1 persistent connection (default) */
    printf("=== HTTP/1.1 Persistent (default) ===\n");
    struct nfs_http_conn conn;
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 0);
    printf("State: %s\n", state_str(nfs_http_conn_get_state(&conn)));

    nfs_http_conn_on_request(&conn, NULL);
    printf("After request: %s\n", state_str(nfs_http_conn_get_state(&conn)));

    nfs_http_conn_on_response(&conn);
    printf("After response: %s, should_close=%d\n",
           state_str(nfs_http_conn_get_state(&conn)),
           nfs_http_conn_should_close(&conn));

    /* HTTP/1.1 with Connection: close */
    printf("\n=== HTTP/1.1 Connection: close ===\n");
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 0);
    nfs_http_conn_on_request(&conn, "close");
    nfs_http_conn_on_response(&conn);
    printf("State: %s, should_close=%d\n",
           state_str(nfs_http_conn_get_state(&conn)),
           nfs_http_conn_should_close(&conn));

    /* HTTP/1.0 without keep-alive */
    printf("\n=== HTTP/1.0 (default close) ===\n");
    nfs_http_conn_init(&conn, NFS_HTTP_VER_10, 0);
    nfs_http_conn_on_request(&conn, NULL);
    nfs_http_conn_on_response(&conn);
    printf("State: %s, should_close=%d\n",
           state_str(nfs_http_conn_get_state(&conn)),
           nfs_http_conn_should_close(&conn));

    /* HTTP/1.0 with keep-alive */
    printf("\n=== HTTP/1.0 Connection: keep-alive ===\n");
    nfs_http_conn_init(&conn, NFS_HTTP_VER_10, 0);
    nfs_http_conn_on_request(&conn, "keep-alive");
    nfs_http_conn_on_response(&conn);
    printf("State: %s, should_close=%d\n",
           state_str(nfs_http_conn_get_state(&conn)),
           nfs_http_conn_should_close(&conn));

    /* Pipeline demo */
    printf("\n=== Pipelining ===\n");
    nfs_http_conn_init(&conn, NFS_HTTP_VER_11, 0);
    nfs_http_pipeline_enqueue(&conn, "GET", "/page1");
    nfs_http_pipeline_enqueue(&conn, "GET", "/page2");
    nfs_http_pipeline_enqueue(&conn, "GET", "/page3");
    printf("Pending: %d\n", nfs_http_pipeline_pending(&conn));

    while (nfs_http_pipeline_pending(&conn) > 0) {
        const struct nfs_http_pipeline_entry *e = nfs_http_pipeline_peek(&conn);
        printf("Responding to: %s %s\n", e->method, e->uri);
        nfs_http_pipeline_dequeue(&conn);
    }
    printf("Pending after all: %d\n", nfs_http_pipeline_pending(&conn));

    return 0;
}
