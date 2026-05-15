#ifndef NFS_COOKIE_H
#define NFS_COOKIE_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * HTTP Cookies and Session Management (RFC 6265)
 *
 * Set-Cookie: name=value; Expires=...; Max-Age=...; Domain=...;
 *             Path=...; Secure; HttpOnly; SameSite=...
 *
 * Cookie: name1=value1; name2=value2
 * --------------------------------------------------------------- */

#define NFS_COOKIE_MAX_NAME    128
#define NFS_COOKIE_MAX_VALUE   4096
#define NFS_COOKIE_MAX_DOMAIN  255
#define NFS_COOKIE_MAX_PATH    512

/* SameSite values */
#define NFS_COOKIE_SAMESITE_NONE    0
#define NFS_COOKIE_SAMESITE_LAX     1
#define NFS_COOKIE_SAMESITE_STRICT  2

/* A parsed cookie */
struct nfs_cookie {
    char     name[NFS_COOKIE_MAX_NAME];
    char     value[NFS_COOKIE_MAX_VALUE];
    char     domain[NFS_COOKIE_MAX_DOMAIN];
    char     path[NFS_COOKIE_MAX_PATH];
    int64_t  max_age;       /* -1 = not set, 0 = delete, >0 = seconds */
    uint8_t  secure;        /* 1 if Secure attribute present */
    uint8_t  http_only;     /* 1 if HttpOnly attribute present */
    uint8_t  samesite;      /* NFS_COOKIE_SAMESITE_* */
};

/* Cookie jar: stores multiple cookies */
#define NFS_COOKIE_JAR_MAX  64

struct nfs_cookie_jar {
    struct nfs_cookie cookies[NFS_COOKIE_JAR_MAX];
    int count;
};

/* Parse a Set-Cookie header value into a cookie struct.
 * Returns 0 on success, -1 on error. */
int nfs_cookie_parse_set_cookie(const char *header,
                                struct nfs_cookie *out);

/* Build a Cookie request header from a jar, filtered by domain and path.
 * Format: "name1=value1; name2=value2"
 * Returns bytes written (excluding NUL), or -1 on error. */
int nfs_cookie_build_header(const struct nfs_cookie_jar *jar,
                            const char *domain, const char *path,
                            char *out, size_t out_sz);

/* Initialize a cookie jar. */
void nfs_cookie_jar_init(struct nfs_cookie_jar *jar);

/* Add (or update) a cookie in the jar.
 * If a cookie with the same name+domain+path exists, it is replaced.
 * Returns 0 on success, -1 if jar is full. */
int nfs_cookie_jar_add(struct nfs_cookie_jar *jar,
                       const struct nfs_cookie *cookie);

/* Look up a cookie by name, domain, and path.
 * Returns pointer to the cookie, or NULL if not found. */
const struct nfs_cookie *nfs_cookie_jar_lookup(const struct nfs_cookie_jar *jar,
                                               const char *name,
                                               const char *domain,
                                               const char *path);

/* Remove expired cookies (max_age == 0) from the jar.
 * Returns number of cookies removed. */
int nfs_cookie_jar_expire(struct nfs_cookie_jar *jar);

#endif /* NFS_COOKIE_H */
