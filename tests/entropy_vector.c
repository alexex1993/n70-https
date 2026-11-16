/* Independent-reference test driver for the portable derivation core. */
#include <stdio.h>
#include "n70entropy_core.h"
int main(void)
{
    unsigned char seed[32], out[64];
    static const char context[] = "fixed test context";
    int i;
    for (i=0; i<32; ++i) seed[i]=(unsigned char)i;
    n70_entropy_derive(seed,context,sizeof context-1,out);
    for (i=0; i<64; ++i) printf("%02x",out[i]);
    printf("\n");
    return 0;
}
