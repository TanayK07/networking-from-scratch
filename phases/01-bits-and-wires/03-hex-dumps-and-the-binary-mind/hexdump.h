#ifndef NFS_HEXDUMP_H
#define NFS_HEXDUMP_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * Canonical hex dump formatter.
 *
 * Produces output identical to `xxd` / `hexdump -C`:
 *
 *   00000000  45 00 00 3c 1c 46 40 00  40 06 a6 ec 0a 00 02 0f  |E..<.F@.@.......|
 *
 * Each line shows: 8-digit offset, 16 hex bytes (with extra space
 * after byte 7), and an ASCII pane where non-printable characters
 * are replaced with '.'.
 * --------------------------------------------------------------- */

/* Format a complete hex dump of `data` (len bytes) into `out`.
 * Returns the number of characters written (excluding NUL), or -1
 * if out_sz is too small. */
int nfs_hexdump(const uint8_t *data, size_t len, char *out, size_t out_sz);

/* Format a single hex dump line starting at `offset` within the
 * data.  `len` is the number of bytes available at `data` (1..16).
 * Returns characters written (excluding NUL), or -1 on error. */
int nfs_hexdump_line(const uint8_t *data, size_t len, size_t offset, char *out, size_t out_sz);

/* Parse a hex string (e.g. "4500003c") into raw bytes.
 * Returns the number of bytes written to `out`, or -1 on error.
 * Ignores spaces in the input. max is the size of out buffer. */
int nfs_hex_to_bytes(const char *hex, uint8_t *out, size_t max);

#endif /* NFS_HEXDUMP_H */
