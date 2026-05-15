#ifndef NFS_URLENC_H
#define NFS_URLENC_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * URL Encoding & MIME Types
 *
 * Percent-encoding (RFC 3986):
 *   Unreserved characters pass through: A-Z a-z 0-9 - _ . ~
 *   Everything else -> %XX (uppercase hex).
 *
 * Form encoding (application/x-www-form-urlencoded):
 *   Space -> '+'
 *   Other reserved -> %XX
 *
 * MIME type: maps file extensions to content types.
 * --------------------------------------------------------------- */

/* Percent-encode a string.
 * Returns bytes written (excluding NUL), or -1 on error. */
int nfs_url_encode(const char *input, char *out, size_t out_sz);

/* Percent-decode a string.
 * Returns bytes written (excluding NUL), or -1 on error. */
int nfs_url_decode(const char *input, char *out, size_t out_sz);

/* Form-encode a string (space -> '+', others -> %XX).
 * Returns bytes written (excluding NUL), or -1 on error. */
int nfs_form_encode(const char *input, char *out, size_t out_sz);

/* Form-decode a string ('+' -> space, %XX -> byte).
 * Returns bytes written (excluding NUL), or -1 on error. */
int nfs_form_decode(const char *input, char *out, size_t out_sz);

/* Look up a MIME type by file extension (without the dot).
 * Returns the MIME type string, or "application/octet-stream" if unknown. */
const char *nfs_mime_type_for_ext(const char *ext);

#endif /* NFS_URLENC_H */
