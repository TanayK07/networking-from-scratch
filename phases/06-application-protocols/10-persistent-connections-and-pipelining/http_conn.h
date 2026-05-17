#ifndef NFS_HTTP_CONN_H
#define NFS_HTTP_CONN_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Persistent connections and pipelining.
 *
 * HTTP/1.0: Connection: keep-alive (opt-in persistent)
 * HTTP/1.1: persistent by default, Connection: close to end
 * Pipelining: multiple requests queued without waiting for
 * responses.
 *
 * Connection state machine:
 *   IDLE -> ACTIVE -> (KEEP_ALIVE | CLOSE)
 * --------------------------------------------------------------- */

/* Connection states */
typedef enum {
    NFS_HTTP_CONN_IDLE,
    NFS_HTTP_CONN_ACTIVE,
    NFS_HTTP_CONN_KEEP_ALIVE,
    NFS_HTTP_CONN_CLOSE
} nfs_http_conn_state_t;

/* HTTP version */
typedef enum { NFS_HTTP_VER_10, NFS_HTTP_VER_11 } nfs_http_version_t;

/* Maximum pipelined requests */
#define NFS_HTTP_MAX_PIPELINE 16
#define NFS_HTTP_MAX_METHOD   16
#define NFS_HTTP_MAX_URI      2048

/* A single pipelined request entry */
struct nfs_http_pipeline_entry {
    char method[NFS_HTTP_MAX_METHOD];
    char uri[NFS_HTTP_MAX_URI];
    int responded; /* 1 if response has been sent */
};

/* Connection context */
struct nfs_http_conn {
    nfs_http_conn_state_t state;
    nfs_http_version_t version;
    int keep_alive_explicit; /* explicit Connection: keep-alive */
    int close_explicit;      /* explicit Connection: close */
    uint32_t request_count;
    uint32_t max_requests; /* 0 = unlimited */

    /* Pipeline queue */
    struct nfs_http_pipeline_entry pipeline[NFS_HTTP_MAX_PIPELINE];
    int pipeline_head; /* next to respond */
    int pipeline_tail; /* next to enqueue */
    int pipeline_count;
};

/* --- Connection management -------------------------------------- */

/* Initialize a connection context.
 * `version` sets the HTTP version (affects default keep-alive).
 * `max_requests` = 0 means unlimited. */
void nfs_http_conn_init(struct nfs_http_conn *conn, nfs_http_version_t version,
                        uint32_t max_requests);

/* Notify the connection that a request has been received.
 * `connection_header` is the value of the Connection header
 * (or NULL if not present).
 * Returns the new state. */
nfs_http_conn_state_t nfs_http_conn_on_request(struct nfs_http_conn *conn,
                                               const char *connection_header);

/* Notify the connection that a response has been sent.
 * Returns the new state (KEEP_ALIVE or CLOSE). */
nfs_http_conn_state_t nfs_http_conn_on_response(struct nfs_http_conn *conn);

/* Check if the connection should be closed after current exchange.
 * Returns non-zero if the connection should close. */
int nfs_http_conn_should_close(const struct nfs_http_conn *conn);

/* Get the current connection state. */
nfs_http_conn_state_t nfs_http_conn_get_state(const struct nfs_http_conn *conn);

/* Determine the correct Connection header value for a response.
 * Returns "keep-alive", "close", or NULL (omit header). */
const char *nfs_http_conn_response_header(const struct nfs_http_conn *conn);

/* --- Pipeline management ---------------------------------------- */

/* Add a request to the pipeline queue.
 * Returns 0 on success, -1 if queue is full. */
int nfs_http_pipeline_enqueue(struct nfs_http_conn *conn, const char *method, const char *uri);

/* Mark the next pending request as responded.
 * Returns 0 on success, -1 if queue is empty. */
int nfs_http_pipeline_dequeue(struct nfs_http_conn *conn);

/* Get the number of pending (not yet responded) requests. */
int nfs_http_pipeline_pending(const struct nfs_http_conn *conn);

/* Peek at the next request to respond to.
 * Returns pointer to the entry, or NULL if none pending. */
const struct nfs_http_pipeline_entry *nfs_http_pipeline_peek(const struct nfs_http_conn *conn);

/* Reset the pipeline queue. */
void nfs_http_pipeline_reset(struct nfs_http_conn *conn);

#endif /* NFS_HTTP_CONN_H */
