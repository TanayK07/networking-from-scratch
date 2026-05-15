#include "urlenc.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    char buf[1024];

    printf("=== URL Encoding ===\n");
    nfs_url_encode("hello world! foo@bar.com/path?q=1&b=2", buf, sizeof(buf));
    printf("  Encoded: %s\n", buf);

    char decoded[1024];
    nfs_url_decode(buf, decoded, sizeof(decoded));
    printf("  Decoded: %s\n", decoded);

    printf("\n=== Form Encoding ===\n");
    nfs_form_encode("name=John Doe&city=New York", buf, sizeof(buf));
    printf("  Encoded: %s\n", buf);

    nfs_form_decode(buf, decoded, sizeof(decoded));
    printf("  Decoded: %s\n", decoded);

    printf("\n=== MIME Types ===\n");
    const char *exts[] = {"html", "json", "png", "pdf", "mp4", "xyz", NULL};
    for (int i = 0; exts[i]; i++) {
        printf("  .%s -> %s\n", exts[i], nfs_mime_type_for_ext(exts[i]));
    }

    return 0;
}
