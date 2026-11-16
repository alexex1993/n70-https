#include "n70gzip.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
static unsigned tests;
#define CHECK(x) do{++tests;if(!(x)){fprintf(stderr,"FAIL gzip line %d\n",__LINE__);exit(1);}}while(0)
struct output{unsigned char data[131072];size_t len;int fail;};
struct allocator{int calls,fail_at,live;};
static void* allocate(void*ctx,unsigned n,unsigned size)
{struct allocator*a=ctx;void*p;if(++a->calls==a->fail_at)return NULL;p=calloc(n,size);if(p)++a->live;return p;}
static void release(void*ctx,void*p){if(p){--((struct allocator*)ctx)->live;free(p);}}
static int output(void*ctx,const unsigned char*p,size_t n)
{struct output*o=ctx;if(o->fail||n>sizeof o->data-o->len)return 1;memcpy(o->data+o->len,p,n);o->len+=n;return 0;}
static int decode(const unsigned char*in,size_t n,size_t step,uint64_t limit,struct output*out,int fail_at)
{
    struct allocator a={0,fail_at,0};n70_gzip*g=calloc(1,sizeof *g);size_t p;int r;
    CHECK(g!=NULL);r=n70_gzip_init(g,limit,allocate,release,&a);
    if(!r){for(p=0;p<n;p+=step){size_t take=n-p;if(take>step)take=step;r=n70_gzip_feed(g,in+p,take,output,out);if(r<0)break;}
        if(r>=0)r=n70_gzip_eof(g);}
    n70_gzip_close(g);CHECK(a.live==0);free(g);return r;
}
/* Independent fixture encoder: gzip header + uncompressed DEFLATE blocks.
 * CRC uses the direct RFC polynomial, independently of zlib's CRC tables. */
static unsigned crc(const unsigned char*p,size_t n)
{unsigned c=~0U;size_t i;for(i=0;i<n;++i){int b;c^=p[i];for(b=0;b<8;++b)c=(c>>1)^((0U-(c&1))&0xedb88320U);}return ~c;}
static void le32(unsigned char*p,unsigned x){int i;for(i=0;i<4;++i){p[i]=(unsigned char)x;x>>=8;}}
static size_t stored(unsigned char*out,const unsigned char*text,size_t n)
{
    static const unsigned char head[]={31,139,8,0,0,0,0,0,0,255};size_t pos=10,p=0;
    memcpy(out,head,10);do{unsigned take=(unsigned)(n-p);if(take>65535)take=65535;
        out[pos++]=(unsigned char)(p+take==n);out[pos++]=(unsigned char)take;out[pos++]=(unsigned char)(take>>8);
        out[pos++]=(unsigned char)~take;out[pos++]=(unsigned char)(~take>>8);memcpy(out+pos,text+p,take);pos+=take;p+=take;
    }while(p<n);le32(out+pos,crc(text,n));le32(out+pos+4,(unsigned)n);return pos+8;
}
int main(void)
{
    /* Fixed-Huffman stream for hello, independently captured from gzip. */
    static const unsigned char fixed[]={31,139,8,0,0,0,0,0,0,3,203,72,205,201,201,7,0,134,166,16,54,5,0,0,0};
    unsigned char wire[150000],mutated[150000],text[100000];size_t n,step,i;struct output out;int r;
    for(step=1;step<=sizeof fixed+1;++step){memset(&out,0,sizeof out);CHECK(decode(fixed,sizeof fixed,step,5,&out,0)==N70_DONE&&out.len==5&&!memcmp(out.data,"hello",5));}
    for(i=0;i<100000;++i)text[i]=(unsigned char)(i*17);n=stored(wire,text,sizeof text);
    for(step=1;step<=4096;step*=2){memset(&out,0,sizeof out);CHECK(decode(wire,n,step,sizeof text,&out,0)==N70_DONE&&out.len==sizeof text&&!memcmp(out.data,text,sizeof text));}
    n=stored(wire,(const unsigned char*)"",0);memset(&out,0,sizeof out);CHECK(decode(wire,n,1,0,&out,0)==N70_DONE&&!out.len);
    n=stored(wire,(const unsigned char*)"hello",5);memcpy(wire+n,fixed,sizeof fixed);
    for(step=1;step<=n+sizeof fixed+1;++step){memset(&out,0,sizeof out);CHECK(decode(wire,n+sizeof fixed,step,10,&out,0)==N70_DONE&&out.len==10&&!memcmp(out.data,"hellohello",10));}
    for(i=0;i<sizeof fixed;++i){memset(&out,0,sizeof out);CHECK(decode(fixed,i,1,100,&out,0)==N70_BAD);}
    for(i=0;i<sizeof fixed;++i){unsigned bit;for(bit=0;bit<8;++bit){memcpy(mutated,fixed,sizeof fixed);mutated[i]^=(unsigned char)(1U<<bit);
        memset(&out,0,sizeof out);r=decode(mutated,sizeof fixed,1,100,&out,0);
        if((i==3&&bit>0)||i>=17)CHECK(r==N70_BAD); /* FTEXT is legal; CRC/ISIZE mutations aren't. */
    }}
    memcpy(mutated,fixed,sizeof fixed);mutated[sizeof fixed]=0;memset(&out,0,sizeof out);
    CHECK(decode(mutated,sizeof fixed+1,1,100,&out,0)==N70_BAD);
    memset(&out,0,sizeof out);CHECK(decode(fixed,sizeof fixed,2,4,&out,0)==N70_LIMIT&&out.len<=4);
    memset(&out,0,sizeof out);out.fail=1;CHECK(decode(fixed,sizeof fixed,1,100,&out,0)==N70_SINK);
    memset(&out,0,sizeof out);CHECK(decode(fixed,sizeof fixed,1,100,&out,1)==N70_MEMORY);
    memset(&out,0,sizeof out);CHECK(decode(fixed,sizeof fixed,1,100,&out,2)==N70_MEMORY);
    /* Optional header fields: extra, filename, comment and FHCRC. */
    memcpy(wire,fixed,10);wire[3]=30;wire[10]=3;wire[11]=0;memcpy(wire+12,"xyzname\0comment",16);
    {unsigned c=crc(wire,28);wire[28]=(unsigned char)c;wire[29]=(unsigned char)(c>>8);}
    memcpy(wire+30,fixed+10,sizeof fixed-10);n=30+sizeof fixed-10;
    memset(&out,0,sizeof out);CHECK(decode(wire,n,1,100,&out,0)==N70_DONE&&out.len==5);
    wire[28]^=1;memset(&out,0,sizeof out);CHECK(decode(wire,n,1,100,&out,0)==N70_BAD);
    n=stored(wire,(const unsigned char*)"",0);for(i=1;i<129;++i)memcpy(wire+n*i,wire,n);
    memset(&out,0,sizeof out);CHECK(decode(wire,n*129,7,0,&out,0)==N70_LIMIT);
    {FILE*f=fopen("tests/fixtures/repeated.gzip","rb");CHECK(f!=NULL);n=fread(wire,1,sizeof wire,f);fclose(f);CHECK(n>0&&n<1000);
     for(step=1;step<=n+1;++step){memset(&out,0,sizeof out);CHECK(decode(wire,n,step,100000,&out,0)==N70_DONE&&out.len==100000);
        for(i=0;i<out.len;++i)if(out.data[i]!='A'){fprintf(stderr,"dynamic output differs\n");return 1;}}
     memset(&out,0,sizeof out);CHECK(decode(wire,n,1,1000,&out,0)==N70_LIMIT&&out.len<=1000);}
    printf("PASS: %u gzip checks: fragmentation, 100 KiB, fixed/stored blocks, members, headers/FHCRC, CRC/ISIZE, truncation, limits, sink errors and allocation cleanup.\n",tests);
    return 0;
}
