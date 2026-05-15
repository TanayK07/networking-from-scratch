/* Demo driver for chunked transfer encoding lesson. */

#include "chunked.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    /* Encode data into chunks */
    const char *msg = "Hello, World! This is chunked transfer encoding in action.";
    size_t msg_len = strlen(msg);

    printf("=== Chunked Encoding ===\n");
    printf("Original: %zu bytes\n", msg_len);

    uint8_t encoded[4096];
    int elen = nfs_chunked_encode((const uint8_t *)msg, msg_len, 16,
                                   encoded, sizeof(encoded));
    if (elen > 0) {
        printf("Encoded (%d bytes):\n%.*s\n", elen, elen, (char *)encoded);
    }

    /* Decode it back */
    printf("=== Chunked Decoding ===\n");
    uint8_t decoded_buf[4096];
    struct nfs_chunked_decoded decoded;
    decoded.data = decoded_buf;

    int consumed = nfs_chunked_decode(encoded, (size_t)elen, &decoded);
    if (consumed > 0) {
        printf("Decoded: %zu bytes\n", decoded.data_len);
        printf("Content: %.*s\n", (int)decoded.data_len, decoded.data);
        printf("Trailers: %u\n", decoded.trailer_count);
    }

    /* Manual chunk encoding */
    printf("\n=== Manual Chunk Encoding ===\n");
    uint8_t manual[256];
    size_t pos = 0;

    const char *c1 = "chunk one";
    int n = nfs_chunked_encode_chunk((const uint8_t *)c1, strlen(c1),
                                      manual + pos, sizeof(manual) - pos);
    pos += (size_t)n;

    const char *c2 = "chunk two";
    n = nfs_chunked_encode_chunk((const uint8_t *)c2, strlen(c2),
                                  manual + pos, sizeof(manual) - pos);
    pos += (size_t)n;

    n = nfs_chunked_encode_last(manual + pos, sizeof(manual) - pos);
    pos += (size_t)n;

    printf("Manual encoded (%zu bytes):\n%.*s\n", pos, (int)pos, (char *)manual);

    return 0;
}
