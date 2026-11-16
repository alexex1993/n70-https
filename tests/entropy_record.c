#include <stdio.h>
#include <string.h>
#include "n70entropy_core.h"
int main(void)
{
    unsigned char secret[32], decoded[32], record[72], damaged[72];
    size_t i;
    for (i=0; i<32; ++i) secret[i]=(unsigned char)(i*7+11);
    n70_entropy_encode(secret,record);
    if (!n70_entropy_decode(record,72,decoded) || memcmp(secret,decoded,32)) return 1;
    for (i=0; i<72*8; ++i) {
        size_t j;
        memcpy(damaged,record,72); damaged[i/8]^=(unsigned char)(1U<<(i%8));
        memset(decoded,0xFF,32);
        if (n70_entropy_decode(damaged,72,decoded)) return 2;
        for (j=0; j<32; ++j) if (decoded[j]) return 3;
    }
    if (n70_entropy_decode(record,0,decoded) || n70_entropy_decode(record,71,decoded) ||
        n70_entropy_decode(record,73,decoded)) return 4;
    puts("PASS: persistent record round-trip, all 576 single-bit corruptions rejected, invalid lengths rejected, failed outputs cleared.");
    return 0;
}
