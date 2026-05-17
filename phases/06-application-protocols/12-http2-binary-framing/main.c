#include "h2frame.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    uint8_t buf[256];

    /* Build a SETTINGS frame with two entries */
    struct nfs_h2_setting settings[] = {
        {NFS_H2_SETTINGS_HEADER_TABLE_SIZE, 4096},
        {NFS_H2_SETTINGS_MAX_CONCURRENT_STREAMS, 100},
    };
    uint8_t settings_payload[64];
    int slen = nfs_h2_settings_build(settings_payload, sizeof(settings_payload), settings, 2);
    if (slen < 0) {
        fprintf(stderr, "Failed to build settings payload\n");
        return 1;
    }

    int total = nfs_h2_frame_build(buf, sizeof(buf), NFS_H2_SETTINGS, 0, 0, settings_payload,
                                   (uint32_t)slen);
    if (total < 0) {
        fprintf(stderr, "Failed to build frame\n");
        return 1;
    }

    printf("Built %d-byte SETTINGS frame\n", total);
    printf("Wire bytes: ");
    for (int i = 0; i < total; i++)
        printf("%02x ", buf[i]);
    printf("\n\n");

    /* Parse it back */
    struct nfs_h2_frame frame;
    if (nfs_h2_frame_parse(buf, (size_t)total, &frame) == 0) {
        printf("Parsed frame:\n");
        printf("  Length:    %u\n", frame.length);
        printf("  Type:      %s (%u)\n", nfs_h2_frame_type_name(frame.type), frame.type);
        printf("  Flags:     0x%02x\n", frame.flags);
        printf("  Stream ID: %u\n", frame.stream_id);

        /* Parse settings entries */
        struct nfs_h2_setting parsed[16];
        int n = nfs_h2_settings_parse(frame.payload, frame.length, parsed, 16);
        printf("  Settings (%d entries):\n", n);
        for (int i = 0; i < n; i++) {
            printf("    id=0x%04x value=%u\n", parsed[i].id, parsed[i].value);
        }
    }

    /* Build a DATA frame */
    const char *data = "Hello, HTTP/2!";
    total = nfs_h2_frame_build(buf, sizeof(buf), NFS_H2_DATA, NFS_H2_FLAG_END_STREAM, 1,
                               (const uint8_t *)data, (uint32_t)strlen(data));
    printf("\nBuilt %d-byte DATA frame on stream 1\n", total);

    if (nfs_h2_frame_parse(buf, (size_t)total, &frame) == 0) {
        printf("  Type: %s, Flags: 0x%02x, Stream: %u, Payload: %.*s\n",
               nfs_h2_frame_type_name(frame.type), frame.flags, frame.stream_id, frame.length,
               frame.payload);
    }

    return 0;
}
