#ifndef NFS_WSCALE_H
#define NFS_WSCALE_H

#include <stddef.h>
#include <stdint.h>

/* ---------------------------------------------------------------
 * TCP Window Scaling Option (RFC 7323)
 *
 * The 16-bit TCP window field limits advertised windows to 65535
 * bytes.  The Window Scale option negotiated during the handshake
 * allows scaling the window by a shift count (0..14), extending
 * the effective window to up to 1 GiB (2^30).
 *
 * TCP option format:  Kind=3, Length=3, Shift count (1 byte)
 * --------------------------------------------------------------- */

#define NFS_WSCALE_MAX      14 /* maximum shift count per RFC 7323 */
#define NFS_WSCALE_OPT_KIND 3  /* TCP option kind for window scale */
#define NFS_WSCALE_OPT_LEN  3  /* TCP option length */

/* Apply window scaling: effective_window = (uint32_t)window << shift. */
uint32_t nfs_wscale_apply(uint16_t window, uint8_t shift);

/* Compress a real window size to the 16-bit header field:
 * header_value = real_window >> shift. */
uint16_t nfs_wscale_compress(uint32_t real_window, uint8_t shift);

/* Compute the minimum shift count needed to represent desired_window
 * in a 16-bit field. Returns shift in [0..14]. */
uint8_t nfs_wscale_compute(uint32_t desired_window);

/* Build the TCP window scale option into out[].
 * Format: [kind=3, len=3, shift].
 * Returns 3 on success, -1 if out_sz < 3 or shift > 14. */
int nfs_wscale_build_option(uint8_t shift, uint8_t *out, size_t out_sz);

/* Parse a window scale option from data[].
 * Expects kind=3, len=3, shift.
 * Writes the shift to *shift_out.
 * Returns 0 on success, -1 on error. */
int nfs_wscale_parse_option(const uint8_t *data, size_t len, uint8_t *shift_out);

/* Validate a shift count: returns 1 if shift <= 14, 0 otherwise. */
int nfs_wscale_valid(uint8_t shift);

/* Format a human-readable description into buf.
 * Example: "WScale=7 (multiply by 128)" */
void nfs_wscale_format(uint8_t shift, char *buf, size_t sz);

#endif /* NFS_WSCALE_H */
