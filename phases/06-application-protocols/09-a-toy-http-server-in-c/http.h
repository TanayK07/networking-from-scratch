#ifndef NFS_HTTP_H
#define NFS_HTTP_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Toy HTTP server in C.
 *
 * Parses HTTP/1.x requests (method, URI, version, headers, body)
 * and builds HTTP responses (status line, headers, body).
 * --------------------------------------------------------------- */

/* Limits */
#define NFS_HTTP_MAX_METHOD     16
#define NFS_HTTP_MAX_URI        2048
#define NFS_HTTP_MAX_VERSION    16
#define NFS_HTTP_MAX_HEADERS    64
#define NFS_HTTP_MAX_HDR_NAME   128
#define NFS_HTTP_MAX_HDR_VALUE  4096
#define NFS_HTTP_MAX_BODY       65536
#define NFS_HTTP_MAX_RESPONSE   65536

/* Common status codes */
#define NFS_HTTP_200_OK                 200
#define NFS_HTTP_301_MOVED              301
#define NFS_HTTP_400_BAD_REQUEST        400
#define NFS_HTTP_404_NOT_FOUND          404
#define NFS_HTTP_500_INTERNAL_ERROR     500

/* HTTP header (name: value pair) */
struct nfs_http_header {
    char name[NFS_HTTP_MAX_HDR_NAME];
    char value[NFS_HTTP_MAX_HDR_VALUE];
};

/* Parsed HTTP request */
struct nfs_http_request {
    char method[NFS_HTTP_MAX_METHOD];
    char uri[NFS_HTTP_MAX_URI];
    char version[NFS_HTTP_MAX_VERSION];  /* e.g. "HTTP/1.1" */
    uint16_t header_count;
    struct nfs_http_header headers[NFS_HTTP_MAX_HEADERS];
    const char *body;       /* points into the original buffer */
    size_t body_len;
};

/* HTTP response builder */
struct nfs_http_response {
    uint16_t status_code;
    char     status_text[64];
    uint16_t header_count;
    struct nfs_http_header headers[NFS_HTTP_MAX_HEADERS];
    const uint8_t *body;
    size_t body_len;
};

/* --- Request parsing -------------------------------------------- */

/* Parse an HTTP request from raw bytes.
 * Returns 0 on success, -1 on error. */
int nfs_http_request_parse(const char *data, size_t data_len,
                           struct nfs_http_request *req);

/* Find a header value by name (case-insensitive).
 * Returns pointer to the value string, or NULL if not found. */
const char *nfs_http_request_get_header(const struct nfs_http_request *req,
                                        const char *name);

/* --- Response building ------------------------------------------ */

/* Initialize a response with a status code. */
void nfs_http_response_init(struct nfs_http_response *resp,
                            uint16_t status_code);

/* Add a header to the response.
 * Returns 0 on success, -1 if headers are full. */
int nfs_http_response_add_header(struct nfs_http_response *resp,
                                 const char *name, const char *value);

/* Set the response body.  The data is NOT copied -- the pointer
 * must remain valid until the response is built. */
void nfs_http_response_set_body(struct nfs_http_response *resp,
                                const uint8_t *body, size_t body_len);

/* Build the complete HTTP response into a buffer.
 * Returns the total bytes written, or -1 on error. */
int nfs_http_response_build(const struct nfs_http_response *resp,
                            char *out, size_t out_sz);

/* --- Utility ---------------------------------------------------- */

/* Return the standard reason phrase for a status code. */
const char *nfs_http_status_str(uint16_t code);

#endif /* NFS_HTTP_H */
