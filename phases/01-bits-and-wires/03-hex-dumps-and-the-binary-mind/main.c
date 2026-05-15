#include "hexdump.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_INPUT  4096
#define MAX_OUTPUT 32768

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [hex-string]\n", prog);
    fprintf(stderr, "  Reads hex from argument or stdin, prints canonical hex dump.\n");
    fprintf(stderr, "  Example: %s \"45 00 00 3c 1c 46 40 00 40 06 a6 ec 0a 00 02 0f\"\n", prog);
}

int main(int argc, char *argv[])
{
    char input[MAX_INPUT];
    uint8_t raw[MAX_INPUT / 2];
    char output[MAX_OUTPUT];

    if (argc > 2) {
        usage(argv[0]);
        return 1;
    }

    if (argc == 2) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
        snprintf(input, sizeof(input), "%s", argv[1]);
    } else {
        /* Read from stdin */
        size_t total = 0;
        size_t n;
        while ((n = fread(input + total, 1, sizeof(input) - total - 1, stdin)) > 0) {
            total += n;
        }
        input[total] = '\0';
    }

    int nbytes = nfs_hex_to_bytes(input, raw, sizeof(raw));
    if (nbytes < 0) {
        fprintf(stderr, "Error: invalid hex input\n");
        return 1;
    }

    if (nbytes == 0) {
        printf("(empty)\n");
        return 0;
    }

    int written = nfs_hexdump(raw, (size_t)nbytes, output, sizeof(output));
    if (written < 0) {
        fprintf(stderr, "Error: hexdump formatting failed\n");
        return 1;
    }

    printf("%s", output);
    return 0;
}
