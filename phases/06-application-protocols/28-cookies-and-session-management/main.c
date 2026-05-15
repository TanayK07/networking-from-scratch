#include "cookie.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    struct nfs_cookie_jar jar;
    nfs_cookie_jar_init(&jar);

    /* Parse Set-Cookie headers */
    const char *headers[] = {
        "session_id=abc123; Domain=example.com; Path=/; HttpOnly; Secure; SameSite=Strict",
        "theme=dark; Domain=example.com; Path=/; Max-Age=86400",
        "lang=en; Domain=.example.com; Path=/api",
        NULL
    };

    printf("=== Parsing Set-Cookie Headers ===\n");
    for (int i = 0; headers[i]; i++) {
        struct nfs_cookie cookie;
        if (nfs_cookie_parse_set_cookie(headers[i], &cookie) == 0) {
            printf("  Cookie: %s=%s\n", cookie.name, cookie.value);
            printf("    Domain=%s Path=%s Secure=%d HttpOnly=%d SameSite=%d\n",
                   cookie.domain, cookie.path, cookie.secure,
                   cookie.http_only, cookie.samesite);
            nfs_cookie_jar_add(&jar, &cookie);
        }
    }

    /* Build Cookie header */
    printf("\n=== Building Cookie Headers ===\n");
    char header[4096];

    if (nfs_cookie_build_header(&jar, "example.com", "/", header, sizeof(header)) >= 0) {
        printf("  For example.com/: %s\n", header);
    }

    if (nfs_cookie_build_header(&jar, "www.example.com", "/api/v1", header, sizeof(header)) >= 0) {
        printf("  For www.example.com/api/v1: %s\n", header);
    }

    printf("\n  Jar has %d cookies\n", jar.count);

    /* Lookup */
    const struct nfs_cookie *found = nfs_cookie_jar_lookup(&jar, "session_id",
                                                           "example.com", "/");
    if (found) {
        printf("  Lookup session_id: %s\n", found->value);
    }

    return 0;
}
