#include "cookie.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* ---------------------------------------------------------------
 * Helpers
 * --------------------------------------------------------------- */

/* Skip leading whitespace. */
static const char *skip_ws(const char *s)
{
    while (*s && (*s == ' ' || *s == '\t')) s++;
    return s;
}

/* Case-insensitive compare (C11-safe). */
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

/* Case-insensitive compare with length. */
static int str_ncasecmp(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int ca = tolower((unsigned char)a[i]);
        int cb = tolower((unsigned char)b[i]);
        if (ca != cb) return ca - cb;
        if (ca == 0) return 0;
    }
    return 0;
}

/* Copy at most max-1 chars from src (length slen) into dst, NUL-terminate. */
static void copy_field(char *dst, size_t max, const char *src, size_t slen)
{
    if (slen >= max) slen = max - 1;
    memcpy(dst, src, slen);
    dst[slen] = '\0';
}

/* ---------------------------------------------------------------
 * Parse Set-Cookie header
 *
 * Format: name=value[; attr[=val]]*
 * --------------------------------------------------------------- */

int nfs_cookie_parse_set_cookie(const char *header,
                                struct nfs_cookie *out)
{
    if (!header || !out)
        return -1;

    memset(out, 0, sizeof(*out));
    out->max_age = -1; /* not set */

    const char *p = skip_ws(header);

    /* Parse name=value */
    const char *eq = strchr(p, '=');
    if (!eq || eq == p)
        return -1;

    copy_field(out->name, sizeof(out->name), p, (size_t)(eq - p));

    p = eq + 1;

    /* Value ends at ';' or end of string */
    const char *semi = strchr(p, ';');
    if (semi) {
        copy_field(out->value, sizeof(out->value), p, (size_t)(semi - p));
        p = semi + 1;
    } else {
        copy_field(out->value, sizeof(out->value), p, strlen(p));
        return 0; /* no attributes */
    }

    /* Parse attributes */
    while (*p) {
        p = skip_ws(p);
        if (*p == '\0') break;

        /* Find end of this attribute */
        const char *attr_end = strchr(p, ';');
        size_t attr_len = attr_end ? (size_t)(attr_end - p) : strlen(p);

        /* Find '=' within attribute */
        const char *attr_eq = NULL;
        for (size_t i = 0; i < attr_len; i++) {
            if (p[i] == '=') {
                attr_eq = &p[i];
                break;
            }
        }

        if (str_ncasecmp(p, "Domain", 6) == 0 && attr_eq) {
            const char *v = skip_ws(attr_eq + 1);
            size_t vlen = attr_len - (size_t)(v - p);
            /* Strip trailing whitespace */
            while (vlen > 0 && (v[vlen-1] == ' ' || v[vlen-1] == '\t'))
                vlen--;
            copy_field(out->domain, sizeof(out->domain), v, vlen);
        } else if (str_ncasecmp(p, "Path", 4) == 0 && attr_eq) {
            const char *v = skip_ws(attr_eq + 1);
            size_t vlen = attr_len - (size_t)(v - p);
            while (vlen > 0 && (v[vlen-1] == ' ' || v[vlen-1] == '\t'))
                vlen--;
            copy_field(out->path, sizeof(out->path), v, vlen);
        } else if (str_ncasecmp(p, "Max-Age", 7) == 0 && attr_eq) {
            out->max_age = (int64_t)atoll(attr_eq + 1);
        } else if (str_ncasecmp(p, "Secure", 6) == 0) {
            out->secure = 1;
        } else if (str_ncasecmp(p, "HttpOnly", 8) == 0) {
            out->http_only = 1;
        } else if (str_ncasecmp(p, "SameSite", 8) == 0 && attr_eq) {
            const char *v = skip_ws(attr_eq + 1);
            if (str_ncasecmp(v, "Strict", 6) == 0)
                out->samesite = NFS_COOKIE_SAMESITE_STRICT;
            else if (str_ncasecmp(v, "Lax", 3) == 0)
                out->samesite = NFS_COOKIE_SAMESITE_LAX;
            else
                out->samesite = NFS_COOKIE_SAMESITE_NONE;
        }
        /* Skip Expires and unknown attributes */

        p += attr_len;
        if (*p == ';') p++;
    }

    return 0;
}

/* ---------------------------------------------------------------
 * Build Cookie header from jar
 * --------------------------------------------------------------- */

/* Check if a cookie's domain matches the request domain.
 * Simple suffix match: cookie domain ".example.com" matches "www.example.com". */
static int domain_matches(const char *cookie_domain, const char *request_domain)
{
    if (!cookie_domain[0])
        return 1; /* no domain restriction */
    if (str_casecmp(cookie_domain, request_domain) == 0)
        return 1;

    /* Suffix match: cookie_domain should be a suffix of request_domain */
    size_t clen = strlen(cookie_domain);
    size_t rlen = strlen(request_domain);
    if (clen > rlen) return 0;

    /* Check if request_domain ends with cookie_domain */
    const char *suffix = request_domain + rlen - clen;
    if (str_casecmp(suffix, cookie_domain) == 0) {
        /* Must be preceded by '.' in request domain (or cookie starts with '.') */
        if (suffix == request_domain)
            return 1;
        if (suffix[-1] == '.' || cookie_domain[0] == '.')
            return 1;
    }

    return 0;
}

/* Check if cookie path matches request path (prefix match). */
static int path_matches(const char *cookie_path, const char *request_path)
{
    if (!cookie_path[0])
        return 1; /* no path restriction */
    size_t clen = strlen(cookie_path);
    if (strncmp(cookie_path, request_path, clen) == 0) {
        /* Exact match or request_path continues with '/' */
        if (request_path[clen] == '\0' || request_path[clen] == '/' ||
            cookie_path[clen - 1] == '/')
            return 1;
    }
    return 0;
}

int nfs_cookie_build_header(const struct nfs_cookie_jar *jar,
                            const char *domain, const char *path,
                            char *out, size_t out_sz)
{
    if (!jar || !domain || !path || !out || out_sz == 0)
        return -1;

    size_t pos = 0;
    int first = 1;

    for (int i = 0; i < jar->count; i++) {
        const struct nfs_cookie *c = &jar->cookies[i];

        if (!domain_matches(c->domain, domain))
            continue;
        if (!path_matches(c->path, path))
            continue;
        if (c->max_age == 0)
            continue; /* expired */

        /* Append "name=value" */
        size_t nlen = strlen(c->name);
        size_t vlen = strlen(c->value);
        size_t need = (first ? 0 : 2) + nlen + 1 + vlen;

        if (pos + need >= out_sz)
            return -1;

        if (!first) {
            out[pos++] = ';';
            out[pos++] = ' ';
        }

        memcpy(out + pos, c->name, nlen);
        pos += nlen;
        out[pos++] = '=';
        memcpy(out + pos, c->value, vlen);
        pos += vlen;

        first = 0;
    }

    out[pos] = '\0';
    return (int)pos;
}

/* ---------------------------------------------------------------
 * Cookie jar operations
 * --------------------------------------------------------------- */

void nfs_cookie_jar_init(struct nfs_cookie_jar *jar)
{
    if (!jar) return;
    jar->count = 0;
}

int nfs_cookie_jar_add(struct nfs_cookie_jar *jar,
                       const struct nfs_cookie *cookie)
{
    if (!jar || !cookie)
        return -1;

    /* Check if existing cookie with same name+domain+path */
    for (int i = 0; i < jar->count; i++) {
        if (strcmp(jar->cookies[i].name, cookie->name) == 0 &&
            str_casecmp(jar->cookies[i].domain, cookie->domain) == 0 &&
            strcmp(jar->cookies[i].path, cookie->path) == 0) {
            jar->cookies[i] = *cookie;
            return 0;
        }
    }

    if (jar->count >= NFS_COOKIE_JAR_MAX)
        return -1;

    jar->cookies[jar->count++] = *cookie;
    return 0;
}

const struct nfs_cookie *nfs_cookie_jar_lookup(const struct nfs_cookie_jar *jar,
                                               const char *name,
                                               const char *domain,
                                               const char *path)
{
    if (!jar || !name || !domain || !path)
        return NULL;

    for (int i = 0; i < jar->count; i++) {
        const struct nfs_cookie *c = &jar->cookies[i];
        if (strcmp(c->name, name) == 0 &&
            str_casecmp(c->domain, domain) == 0 &&
            strcmp(c->path, path) == 0)
            return c;
    }

    return NULL;
}

int nfs_cookie_jar_expire(struct nfs_cookie_jar *jar)
{
    if (!jar) return 0;

    int removed = 0;
    int i = 0;
    while (i < jar->count) {
        if (jar->cookies[i].max_age == 0) {
            /* Remove by swapping with last */
            jar->cookies[i] = jar->cookies[jar->count - 1];
            jar->count--;
            removed++;
        } else {
            i++;
        }
    }

    return removed;
}
