#include "n70entropy_core.h"
#include "bearssl.h"
#include <string.h>

static void clear_memory(void *memory, size_t len)
{
    volatile unsigned char *p = memory;
    while (len--) *p++ = 0;
}
static void checksum(const unsigned char data[40], unsigned char hash[32])
{
    br_sha256_context ctx;
    br_sha256_init(&ctx);
    br_sha256_update(&ctx, data, 40);
    br_sha256_out(&ctx, hash);
    clear_memory(&ctx, sizeof ctx);
}
static uint32_t load32(const unsigned char *p)
{ return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24); }
int n70_entropy_from_timing(const unsigned char *samples, size_t len, unsigned char seed[32])
{
    static const char domain[] = "N70 experimental input timing v1";
    br_sha256_context ctx;
    size_t i, j;
    unsigned keys=0, distinct_keys=0, distinct_deltas=0;
    uint32_t delta[127];
    clear_memory(seed, 32);
    if (!samples || len != 128*16) return 0;
    for (i=0; i<128; ++i) {
        uint32_t key=load32(samples+i*16+8);
        if (key<'0' || key>'9') return 0;
        keys|=1U<<(key-'0');
        if (i) {
            if (load32(samples+i*16+4)-load32(samples+(i-1)*16+4)<2) return 0;
            delta[i-1]=load32(samples+i*16)-load32(samples+(i-1)*16);
        }
    }
    for (i=0; i<10; ++i) if (keys&(1U<<i)) ++distinct_keys;
    for (i=0; i<127; ++i) {
        for (j=0; j<i; ++j) if (delta[j]==delta[i]) break;
        if (j==i) ++distinct_deltas;
    }
    clear_memory(delta,sizeof delta);
    if (distinct_keys<4 || distinct_deltas<16) return 0;
    br_sha256_init(&ctx);
    br_sha256_update(&ctx, domain, sizeof domain-1);
    br_sha256_update(&ctx, samples, len);
    br_sha256_out(&ctx, seed);
    clear_memory(&ctx, sizeof ctx);
    return 1;
}
void n70_entropy_encode(const unsigned char secret[32], unsigned char record[72])
{
    memcpy(record, "N70RNG01", 8);
    memcpy(record + 8, secret, 32);
    checksum(record, record + 40);
}
int n70_entropy_decode(const unsigned char *record, size_t len, unsigned char secret[32])
{
    unsigned char hash[32];
    unsigned diff = 0;
    size_t i;
    clear_memory(secret, 32);
    if (len != 72 || memcmp(record, "N70RNG01", 8)) return 0;
    checksum(record, hash);
    for (i = 0; i < 32; ++i) diff |= hash[i] ^ record[40 + i];
    clear_memory(hash, sizeof hash);
    if (diff) return 0;
    memcpy(secret, record + 8, 32);
    return 1;
}

void n70_entropy_derive(const unsigned char old_secret[32],
    const void *context, size_t context_len, unsigned char out[64])
{
    static const char domain[] = "N70 HTTPS persistent seed v1";
    br_hmac_drbg_context rng;
    volatile unsigned char *wipe;
    size_t i;
    br_hmac_drbg_init(&rng, &br_sha256_vtable, old_secret, 32);
    br_hmac_drbg_update(&rng, domain, sizeof domain - 1);
    if (context_len) br_hmac_drbg_update(&rng, context, context_len);
    br_hmac_drbg_generate(&rng, out, 64);
    wipe = (volatile unsigned char *)&rng;
    for (i = 0; i < sizeof rng; i++) wipe[i] = 0;
}
