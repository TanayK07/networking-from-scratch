/* Toy HTTP server implementation. */

#include "http.h"
#include <string.h>
#include <stdio.h>
#include <strings.h>

/* ---------------------------------------------------------------
 * Helper: find CRLF in a buffer
 * --------------------------------------------------------------- */

static const char *find_crlf(const char *data, size_t len)
{
    for (size_t i = 0; i + 1 < len; i++) {
        if (data[i] == '\r' && data[i + 1] == '\n') {
            return data + i;
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------
 * Request parsing
 * --------------------------------------------------------------- */

int nfs_http_request_parse(const char *data, size_t data_len,
                           struct nfs_http_request *req)
{
    if (!data || !req || data_len == 0) return -1;

    memset(req, 0, sizeof(*req));

    const char *pos = data;
    const char *end = data + data_len;

    /* Parse request line: METHOD SP URI SP VERSION CRLF */
    const char *line_end = find_crlf(pos, (size_t)(end - pos));
    if (!line_end) return -1;

    /* METHOD */
    const char *sp = memchr(pos, ' ', (size_t)(line_end - pos));
    if (!sp) return -1;
    size_t method_len = (size_t)(sp - pos);
    if (method_len == 0 || method_len >= NFS_HTTP_MAX_METHOD) return -1;
    memcpy(req->method, pos, method_len);
    req->method[method_len] = '\0';
    pos = sp + 1;

    /* URI */
    sp = memchr(pos, ' ', (size_t)(line_end - pos));
    if (!sp) return -1;
    size_t uri_len = (size_t)(sp - pos);
    if (uri_len == 0 || uri_len >= NFS_HTTP_MAX_URI) return -1;
    memcpy(req->uri, pos, uri_len);
    req->uri[uri_len] = '\0';
    pos = sp + 1;

    /* VERSION */
    size_t ver_len = (size_t)(line_end - pos);
    if (ver_len == 0 || ver_len >= NFS_HTTP_MAX_VERSION) return -1;
    memcpy(req->version, pos, ver_len);
    req->version[ver_len] = '\0';

    pos = line_end + 2; /* skip CRLF */

    /* Parse headers */
    req->header_count = 0;
    while (pos < end) {
        /* Empty line = end of headers */
        if (pos + 1 < end && pos[0] == '\r' && pos[1] == '\n') {
            pos += 2;
            break;
        }

        line_end = find_crlf(pos, (size_t)(end - pos));
        if (!line_end) return -1;

        /* Find colon */
        const char *colon = memchr(pos, ':', (size_t)(line_end - pos));
        if (!colon) return -1;

        size_t name_len = (size_t)(colon - pos);
        if (name_len == 0 || name_len >= NFS_HTTP_MAX_HDR_NAME) return -1;

        /* Skip optional whitespace after colon */
        const char *val_start = colon + 1;
        while (val_start < line_end && *val_start == ' ') val_start++;

        size_t val_len = (size_t)(line_end - val_start);
        if (val_len >= NFS_HTTP_MAX_HDR_VALUE) return -1;

        if (req->header_count >= NFS_HTTP_MAX_HEADERS) return -1;

        struct nfs_http_header *h = &req->headers[req->header_count];
        memcpy(h->name, pos, name_len);
        h->name[name_len] = '\0';
        memcpy(h->value, val_start, val_len);
        h->value[val_len] = '\0';

        req->header_count++;
        pos = line_end + 2;
    }

    /* Body is everything after the blank line */
    if (pos < end) {
        req->body = pos;
        req->body_len = (size_t)(end - pos);
    }

    return 0;
}

const char *nfs_http_request_get_header(const struct nfs_http_request *req,
                                        const char *name)
{
    if (!req || !name) return NULL;

    for (uint16_t i = 0; i < req->header_count; i++) {
        if (strcasecmp(req->headers[i].name, name) == 0) {
            return req->headers[i].value;
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------
 * Response building
 * --------------------------------------------------------------- */

const char *nfs_http_status_str(uint16_t code)
{
    switch (code) {
    case 200: return "OK";
    case 201: return "Created";
    case 204: return "No Content";
    case 301: return "Moved Permanently";
    case 302: return "Found";
    case 304: return "Not Modified";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 500: return "Internal Server Error";
    case 502: return "Bad Gateway";
    case 503: return "Service Unavailable";
    default:  return "Unknown";
    }
}

void nfs_http_response_init(struct nfs_http_response *resp,
                            uint16_t status_code)
{
    if (!resp) return;
    memset(resp, 0, sizeof(*resp));
    resp->status_code = status_code;
    const char *text = nfs_http_status_str(status_code);
    size_t tlen = strlen(text);
    if (tlen >= sizeof(resp->status_text)) tlen = sizeof(resp->status_text) - 1;
    memcpy(resp->status_text, text, tlen);
    resp->status_text[tlen] = '\0';
}

int nfs_http_response_add_header(struct nfs_http_response *resp,
                                 const char *name, const char *value)
{
    if (!resp || !name || !value) return -1;
    if (resp->header_count >= NFS_HTTP_MAX_HEADERS) return -1;

    struct nfs_http_header *h = &resp->headers[resp->header_count];
    size_t nlen = strlen(name);
    size_t vlen = strlen(value);
    if (nlen >= NFS_HTTP_MAX_HDR_NAME || vlen >= NFS_HTTP_MAX_HDR_VALUE) return -1;

    memcpy(h->name, name, nlen + 1);
    memcpy(h->value, value, vlen + 1);
    resp->header_count++;

    return 0;
}

void nfs_http_response_set_body(struct nfs_http_response *resp,
                                const uint8_t *body, size_t body_len)
{
    if (!resp) return;
    resp->body = body;
    resp->body_len = body_len;
}

int nfs_http_response_build(const struct nfs_http_response *resp,
                            char *out, size_t out_sz)
{
    if (!resp || !out || out_sz == 0) return -1;

    size_t pos = 0;

    /* Status line: HTTP/1.1 CODE REASON\r\n */
    int n = snprintf(out + pos, out_sz - pos, "HTTP/1.1 %u %s\r\n",
                     resp->status_code, resp->status_text);
    if (n < 0 || (size_t)n >= out_sz - pos) return -1;
    pos += (size_t)n;

    /* Headers */
    for (uint16_t i = 0; i < resp->header_count; i++) {
        n = snprintf(out + pos, out_sz - pos, "%s: %s\r\n",
                     resp->headers[i].name, resp->headers[i].value);
        if (n < 0 || (size_t)n >= out_sz - pos) return -1;
        pos += (size_t)n;
    }

    /* Blank line */
    if (pos + 2 >= out_sz) return -1;
    out[pos++] = '\r';
    out[pos++] = '\n';

    /* Body */
    if (resp->body && resp->body_len > 0) {
        if (pos + resp->body_len > out_sz) return -1;
        memcpy(out + pos, resp->body, resp->body_len);
        pos += resp->body_len;
    }

    return (int)pos;
}
