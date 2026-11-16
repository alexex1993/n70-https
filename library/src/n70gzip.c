#include "n70gzip.h"
#include <string.h>
int n70_gzip_init(n70_gzip*g,uint64_t limit,alloc_func alloc,free_func release,void*ctx)
{
    int r;memset(g,0,sizeof *g);g->limit=limit;g->z.zalloc=alloc;g->z.zfree=release;g->z.opaque=ctx;
    r=inflateInit2(&g->z,31);if(r!=Z_OK)return r==Z_MEM_ERROR?N70_MEMORY:N70_BAD;
    g->active=1;return 0;
}
void n70_gzip_close(n70_gzip*g){if(g->active)inflateEnd(&g->z);g->active=0;}
int n70_gzip_feed(n70_gzip*g,const unsigned char*data,size_t len,n70_body_fn sink,void*ctx)
{
    int r;size_t n;uInt before;
    if(!g->active||len>0x7fffffffU)return N70_BAD;
    g->z.next_in=data;g->z.avail_in=(uInt)len;
    while(g->z.avail_in){
        if(g->ended){
            if(g->members>=128)return N70_LIMIT;
            if(inflateReset(&g->z)!=Z_OK)return N70_BAD;g->ended=0;
        }
        do{
            before=g->z.avail_in;g->z.next_out=g->out;g->z.avail_out=sizeof g->out;
            r=inflate(&g->z,Z_NO_FLUSH);n=sizeof g->out-g->z.avail_out;
            if(n>g->limit-g->bytes)return N70_LIMIT;
            if(n&&sink&&sink(ctx,g->out,n))return N70_SINK;g->bytes+=n;
            if(r==Z_STREAM_END){g->ended=1;++g->members;break;}
            if(r!=Z_OK&&r!=Z_BUF_ERROR)return r==Z_MEM_ERROR?N70_MEMORY:N70_BAD;
            if(before==g->z.avail_in&&!n){if(g->z.avail_in)return N70_BAD;break;}
        }while(g->z.avail_in||g->z.avail_out==0);
    }return N70_MORE;
}
int n70_gzip_eof(const n70_gzip*g){return g->active&&g->ended&&g->members?N70_DONE:N70_BAD;}
