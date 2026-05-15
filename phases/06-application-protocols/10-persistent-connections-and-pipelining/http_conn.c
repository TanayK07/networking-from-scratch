/* Persistent connections and pipelining implementation. */

#include "http_conn.h"
#include <string.h>
#include <strings.h>

/* ---------------------------------------------------------------
 * Connection management
 * --------------------------------------------------------------- */

void nfs_http_conn_init(struct nfs_http_conn *conn,
                        nfs_http_version_t version,
                        uint32_t max_requests)
{
    if (!conn) return;
    memset(conn, 0, sizeof(*conn));
    conn->state = NFS_HTTP_CONN_IDLE;
    conn->version = version;
    conn->max_requests = max_requests;
}

nfs_http_conn_state_t nfs_http_conn_on_request(
    struct nfs_http_conn *conn,
    const char *connection_header)
{
    if (!conn) return NFS_HTTP_CONN_CLOSE;

    conn->state = NFS_HTTP_CONN_ACTIVE;
    conn->request_count++;

    /* Parse Connection header */
    conn->keep_alive_explicit = 0;
    conn->close_explicit = 0;

    if (connection_header) {
        if (strcasecmp(connection_header, "keep-alive") == 0) {
            conn->keep_alive_explicit = 1;
        } else if (strcasecmp(connection_header, "close") == 0) {
            conn->close_explicit = 1;
        }
    }

    return conn->state;
}

nfs_http_conn_state_t nfs_http_conn_on_response(
    struct nfs_http_conn *conn)
{
    if (!conn) return NFS_HTTP_CONN_CLOSE;

    /* Check if max requests reached */
    if (conn->max_requests > 0 &&
        conn->request_count >= conn->max_requests) {
        conn->state = NFS_HTTP_CONN_CLOSE;
        return conn->state;
    }

    /* Explicit close requested */
    if (conn->close_explicit) {
        conn->state = NFS_HTTP_CONN_CLOSE;
        return conn->state;
    }

    /* HTTP/1.1: persistent by default unless Connection: close */
    if (conn->version == NFS_HTTP_VER_11) {
        conn->state = NFS_HTTP_CONN_KEEP_ALIVE;
        return conn->state;
    }

    /* HTTP/1.0: close by default unless Connection: keep-alive */
    if (conn->version == NFS_HTTP_VER_10) {
        if (conn->keep_alive_explicit) {
            conn->state = NFS_HTTP_CONN_KEEP_ALIVE;
        } else {
            conn->state = NFS_HTTP_CONN_CLOSE;
        }
        return conn->state;
    }

    conn->state = NFS_HTTP_CONN_CLOSE;
    return conn->state;
}

int nfs_http_conn_should_close(const struct nfs_http_conn *conn)
{
    if (!conn) return 1;

    /* If explicitly in CLOSE state */
    if (conn->state == NFS_HTTP_CONN_CLOSE) return 1;

    /* Check max requests */
    if (conn->max_requests > 0 &&
        conn->request_count >= conn->max_requests) {
        return 1;
    }

    /* If we're active (mid-request), predict the outcome */
    if (conn->close_explicit) return 1;

    if (conn->version == NFS_HTTP_VER_10 && !conn->keep_alive_explicit) {
        return 1;
    }

    return 0;
}

nfs_http_conn_state_t nfs_http_conn_get_state(
    const struct nfs_http_conn *conn)
{
    if (!conn) return NFS_HTTP_CONN_CLOSE;
    return conn->state;
}

const char *nfs_http_conn_response_header(
    const struct nfs_http_conn *conn)
{
    if (!conn) return "close";

    /* HTTP/1.0 with keep-alive: send Connection: keep-alive */
    if (conn->version == NFS_HTTP_VER_10 && conn->keep_alive_explicit &&
        !conn->close_explicit) {
        return "keep-alive";
    }

    /* HTTP/1.1 with close: send Connection: close */
    if (conn->version == NFS_HTTP_VER_11 && conn->close_explicit) {
        return "close";
    }

    /* HTTP/1.0 default: send Connection: close */
    if (conn->version == NFS_HTTP_VER_10 && !conn->keep_alive_explicit) {
        return "close";
    }

    /* HTTP/1.1 default: no explicit header needed */
    return NULL;
}

/* ---------------------------------------------------------------
 * Pipeline management
 * --------------------------------------------------------------- */

int nfs_http_pipeline_enqueue(struct nfs_http_conn *conn,
                              const char *method, const char *uri)
{
    if (!conn || !method || !uri) return -1;
    if (conn->pipeline_count >= NFS_HTTP_MAX_PIPELINE) return -1;

    struct nfs_http_pipeline_entry *e =
        &conn->pipeline[conn->pipeline_tail];

    size_t mlen = strlen(method);
    size_t ulen = strlen(uri);
    if (mlen >= NFS_HTTP_MAX_METHOD || ulen >= NFS_HTTP_MAX_URI) return -1;

    memcpy(e->method, method, mlen + 1);
    memcpy(e->uri, uri, ulen + 1);
    e->responded = 0;

    conn->pipeline_tail = (conn->pipeline_tail + 1) % NFS_HTTP_MAX_PIPELINE;
    conn->pipeline_count++;

    return 0;
}

int nfs_http_pipeline_dequeue(struct nfs_http_conn *conn)
{
    if (!conn || conn->pipeline_count == 0) return -1;

    conn->pipeline[conn->pipeline_head].responded = 1;
    conn->pipeline_head = (conn->pipeline_head + 1) % NFS_HTTP_MAX_PIPELINE;
    conn->pipeline_count--;

    return 0;
}

int nfs_http_pipeline_pending(const struct nfs_http_conn *conn)
{
    if (!conn) return 0;
    return conn->pipeline_count;
}

const struct nfs_http_pipeline_entry *nfs_http_pipeline_peek(
    const struct nfs_http_conn *conn)
{
    if (!conn || conn->pipeline_count == 0) return NULL;
    return &conn->pipeline[conn->pipeline_head];
}

void nfs_http_pipeline_reset(struct nfs_http_conn *conn)
{
    if (!conn) return;
    conn->pipeline_head = 0;
    conn->pipeline_tail = 0;
    conn->pipeline_count = 0;
}
