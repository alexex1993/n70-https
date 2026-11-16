#include "n70trust.h"
#include "testanchor.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{++tests;if(!(x)){fprintf(stderr,"FAIL trust line %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    br_x509_trust_anchor a[TEST_ANCHOR_COUNT],b[TEST_ANCHOR_COUNT],temp;unsigned char before[32],after[32],dn[16384];unsigned tests=0;
    InitTestAnchors(a);memcpy(b,a,sizeof a);CHECK(n70_trust_digest(a,TEST_ANCHOR_COUNT,before));
    CHECK(n70_trust_digest(b,TEST_ANCHOR_COUNT,after)&&!memcmp(before,after,32));
    memcpy(dn,b[0].dn.data,b[0].dn.len);b[0].dn.data=dn;
    CHECK(n70_trust_digest(b,TEST_ANCHOR_COUNT,after)&&!memcmp(before,after,32));
    dn[0]^=1;CHECK(n70_trust_digest(b,TEST_ANCHOR_COUNT,after)&&memcmp(before,after,32));
    memcpy(b,a,sizeof a);b[0].flags^=1;CHECK(n70_trust_digest(b,TEST_ANCHOR_COUNT,after)&&memcmp(before,after,32));
    memcpy(b,a,sizeof a);b[0].pkey.key.rsa.elen-=1;CHECK(n70_trust_digest(b,TEST_ANCHOR_COUNT,after)&&memcmp(before,after,32));
    memcpy(b,a,sizeof a);temp=b[0];b[0]=b[1];b[1]=temp;
    CHECK(n70_trust_digest(b,TEST_ANCHOR_COUNT,after)&&memcmp(before,after,32));
    CHECK(n70_trust_digest(a,TEST_ANCHOR_COUNT-1,after)&&memcmp(before,after,32));
    CHECK(!n70_trust_digest(NULL,TEST_ANCHOR_COUNT,after));CHECK(!n70_trust_digest(a,0,after));
    memcpy(b,a,sizeof a);b[0].pkey.key_type=99;CHECK(!n70_trust_digest(b,TEST_ANCHOR_COUNT,after));
    printf("PASS: %u anchor-content fingerprint checks for connection reuse, including same bytes at a new address and changed trust contents.\n",tests);return 0;
}
