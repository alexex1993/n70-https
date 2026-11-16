#include <stdio.h>
#include <string.h>
#include "n70entropy_core.h"
static void put32(unsigned char* p,unsigned long x)
{ p[0]=(unsigned char)x;p[1]=(unsigned char)(x>>8);p[2]=(unsigned char)(x>>16);p[3]=(unsigned char)(x>>24); }
int main(void)
{
    unsigned char samples[2048], bad[2048], seed[32];
    unsigned long ticks=100,fast=500;
    int i;
    for(i=0;i<128;i++) {
        ticks+=2+i%23;fast+=100+i*i;
        put32(samples+16*i,fast);put32(samples+16*i+4,ticks);
        put32(samples+16*i+8,'0'+i%10);put32(samples+16*i+12,i*31);
    }
    if(!n70_entropy_from_timing(samples,sizeof samples,seed))return 1;
    if(n70_entropy_from_timing(samples,sizeof samples-1,seed))return 2;
    memcpy(bad,samples,sizeof bad);
    for(i=0;i<128;i++)put32(bad+i*16+8,'1');
    if(n70_entropy_from_timing(bad,sizeof bad,seed))return 3;
    memcpy(bad,samples,sizeof bad);
    for(i=0;i<128;i++)put32(bad+i*16,i*100);
    if(n70_entropy_from_timing(bad,sizeof bad,seed))return 4;
    memcpy(bad,samples,sizeof bad);
    for(i=0;i<128;i++)put32(bad+i*16+4,i);
    if(n70_entropy_from_timing(bad,sizeof bad,seed))return 5;
    memcpy(bad,samples,sizeof bad);put32(bad+8,'X');
    if(n70_entropy_from_timing(bad,sizeof bad,seed))return 6;
    for(i=0;i<32;i++)if(seed[i])return 7;
    puts("PASS: timing input validation; accepts diverse synthetic input, rejects bad lengths, one-key input, fixed timing and too-close events. NOT an entropy assessment.");
    return 0;
}
