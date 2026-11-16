#include "n70trust.h"
static void integer(br_sha256_context*h,unsigned v)
{unsigned char b[4];int i;for(i=0;i<4;++i){b[i]=(unsigned char)v;v>>=8;}br_sha256_update(h,b,4);}
static int bytes(br_sha256_context*h,const unsigned char*p,size_t n)
{if(n>16384||(!p&&n))return 0;integer(h,(unsigned)n);if(n)br_sha256_update(h,p,n);return 1;}
int n70_trust_digest(const br_x509_trust_anchor*a,size_t count,unsigned char out[32])
{
    br_sha256_context h;size_t i;if(!a||!count||count>256)return 0;
    br_sha256_init(&h);integer(&h,(unsigned)count);
    for(i=0;i<count;++i){
        integer(&h,a[i].flags);integer(&h,a[i].pkey.key_type);
        if(!bytes(&h,a[i].dn.data,a[i].dn.len))return 0;
        if(a[i].pkey.key_type==BR_KEYTYPE_RSA){
            if(!bytes(&h,a[i].pkey.key.rsa.n,a[i].pkey.key.rsa.nlen)||!bytes(&h,a[i].pkey.key.rsa.e,a[i].pkey.key.rsa.elen))return 0;
        }else if(a[i].pkey.key_type==BR_KEYTYPE_EC){
            integer(&h,a[i].pkey.key.ec.curve);if(!bytes(&h,a[i].pkey.key.ec.q,a[i].pkey.key.ec.qlen))return 0;
        }else return 0;
    }br_sha256_out(&h,out);return 1;
}
