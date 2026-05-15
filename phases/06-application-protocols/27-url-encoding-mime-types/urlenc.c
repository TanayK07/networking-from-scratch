#include "urlenc.h"
#include <string.h>
#include <ctype.h>

/* Case-insensitive string comparison (C11-safe, no POSIX dependency). */
static int str_casecmp(const char *a, const char *b)
{
    while (*a && *b) {
        int ca = tolower((unsigned char)*a);
        int cb = tolower((unsigned char)*b);
        if (ca != cb) return ca - cb;
        a++;
        b++;
    }
    return tolower((unsigned char)*a) - tolower((unsigned char)*b);
}

/* ---------------------------------------------------------------
 * Unreserved character check (RFC 3986 Section 2.3)
 * --------------------------------------------------------------- */

static int is_unreserved(unsigned char c)
{
    return isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~';
}

static const char hex_chars[] = "0123456789ABCDEF";

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* ---------------------------------------------------------------
 * Percent-encoding (RFC 3986)
 * --------------------------------------------------------------- */

int nfs_url_encode(const char *input, char *out, size_t out_sz)
{
    if (!input || !out || out_sz == 0)
        return -1;

    size_t pos = 0;
    for (const char *p = input; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (is_unreserved(c)) {
            if (pos + 1 >= out_sz) return -1;
            out[pos++] = (char)c;
        } else {
            if (pos + 3 >= out_sz) return -1;
            out[pos++] = '%';
            out[pos++] = hex_chars[(c >> 4) & 0x0F];
            out[pos++] = hex_chars[c & 0x0F];
        }
    }

    out[pos] = '\0';
    return (int)pos;
}

int nfs_url_decode(const char *input, char *out, size_t out_sz)
{
    if (!input || !out || out_sz == 0)
        return -1;

    size_t pos = 0;
    for (const char *p = input; *p; ) {
        if (pos + 1 >= out_sz) return -1;

        if (*p == '%') {
            p++;
            if (!p[0] || !p[1]) return -1;
            int hi = hex_val(p[0]);
            int lo = hex_val(p[1]);
            if (hi < 0 || lo < 0) return -1;
            out[pos++] = (char)((hi << 4) | lo);
            p += 2;
        } else {
            out[pos++] = *p++;
        }
    }

    out[pos] = '\0';
    return (int)pos;
}

/* ---------------------------------------------------------------
 * Form encoding (application/x-www-form-urlencoded)
 * Space -> '+', others -> %XX
 * --------------------------------------------------------------- */

int nfs_form_encode(const char *input, char *out, size_t out_sz)
{
    if (!input || !out || out_sz == 0)
        return -1;

    size_t pos = 0;
    for (const char *p = input; *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == ' ') {
            if (pos + 1 >= out_sz) return -1;
            out[pos++] = '+';
        } else if (is_unreserved(c)) {
            if (pos + 1 >= out_sz) return -1;
            out[pos++] = (char)c;
        } else {
            if (pos + 3 >= out_sz) return -1;
            out[pos++] = '%';
            out[pos++] = hex_chars[(c >> 4) & 0x0F];
            out[pos++] = hex_chars[c & 0x0F];
        }
    }

    out[pos] = '\0';
    return (int)pos;
}

int nfs_form_decode(const char *input, char *out, size_t out_sz)
{
    if (!input || !out || out_sz == 0)
        return -1;

    size_t pos = 0;
    for (const char *p = input; *p; ) {
        if (pos + 1 >= out_sz) return -1;

        if (*p == '+') {
            out[pos++] = ' ';
            p++;
        } else if (*p == '%') {
            p++;
            if (!p[0] || !p[1]) return -1;
            int hi = hex_val(p[0]);
            int lo = hex_val(p[1]);
            if (hi < 0 || lo < 0) return -1;
            out[pos++] = (char)((hi << 4) | lo);
            p += 2;
        } else {
            out[pos++] = *p++;
        }
    }

    out[pos] = '\0';
    return (int)pos;
}

/* ---------------------------------------------------------------
 * MIME type lookup
 * --------------------------------------------------------------- */

static const struct {
    const char *ext;
    const char *mime;
} mime_table[] = {
    { "html",  "text/html" },
    { "htm",   "text/html" },
    { "css",   "text/css" },
    { "js",    "application/javascript" },
    { "json",  "application/json" },
    { "xml",   "application/xml" },
    { "txt",   "text/plain" },
    { "csv",   "text/csv" },
    { "png",   "image/png" },
    { "jpg",   "image/jpeg" },
    { "jpeg",  "image/jpeg" },
    { "gif",   "image/gif" },
    { "svg",   "image/svg+xml" },
    { "ico",   "image/x-icon" },
    { "webp",  "image/webp" },
    { "pdf",   "application/pdf" },
    { "zip",   "application/zip" },
    { "gz",    "application/gzip" },
    { "tar",   "application/x-tar" },
    { "mp3",   "audio/mpeg" },
    { "mp4",   "video/mp4" },
    { "wav",   "audio/wav" },
    { "webm",  "video/webm" },
    { "woff",  "font/woff" },
    { "woff2", "font/woff2" },
    { "ttf",   "font/ttf" },
    { "otf",   "font/otf" },
    { "wasm",  "application/wasm" },
    { NULL, NULL }
};

const char *nfs_mime_type_for_ext(const char *ext)
{
    if (!ext) return "application/octet-stream";

    for (int i = 0; mime_table[i].ext != NULL; i++) {
        if (str_casecmp(mime_table[i].ext, ext) == 0)
            return mime_table[i].mime;
    }

    return "application/octet-stream";
}
