#ifndef N70ENTROPY_CORE_H
#define N70ENTROPY_CORE_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* out[0..31] is next persistent secret, out[32..63] is TLS seed.
 * This does not create entropy: old_secret must already be unpredictable. */
void n70_entropy_derive(const unsigned char old_secret[32],
    const void *context, size_t context_len, unsigned char out[64]);
#define N70_ENTROPY_RECORD_SIZE 72
void n70_entropy_encode(const unsigned char secret[32], unsigned char record[72]);
/* Checksum detects accidental damage, not malicious modification. */
int n70_entropy_decode(const unsigned char *record, size_t len, unsigned char secret[32]);
/* Experimental timing collector; heuristics reject obvious bad input only.
 * Passing them does NOT establish a cryptographic min-entropy estimate. */
int n70_entropy_from_timing(const unsigned char *samples, size_t len, unsigned char seed[32]);
#ifdef __cplusplus
}
#endif
#endif
