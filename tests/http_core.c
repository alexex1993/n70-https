#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "n70http.h"
static unsigned tests;
static int last_reusable;
struct output { char data[65536];size_t len;int fail; };
static int body(void*p,const unsigned char*d,size_t n)
{ struct output*o=p;if(o->fail)return 1;if(n>sizeof o->data-o->len)return 1;memcpy(o->data+o->len,d,n);o->len+=n;return 0; }
static int parse(const char*wire,size_t len,size_t step,uint64_t limit,int head,struct output*out,int*status)
{
    n70_http*p=malloc(sizeof *p);size_t offset=0;int event=0;
    if(!p)exit(2);n70_http_init(p,limit,head);
    while(offset<len){size_t left=len-offset;if(left>step)left=step;
        while(left){size_t used=0;event=n70_http_feed(p,(const unsigned char*)wire+offset,left,&used,body,out);
            offset+=used;left-=used;if(event<0||event==N70_DONE)goto done;
            if(!used&&event!=N70_HEADERS){event=N70_BAD;goto done;}
        }
    }
    event=n70_http_eof(p);
done:*status=p->status;last_reusable=n70_http_reusable(p,offset==len);free(p);return event;
}
static void fixture(const char*wire,uint64_t limit,int head,int expected,const char*text,int status)
{
    size_t step;
    for(step=1;step<=strlen(wire)+1;step++){
        struct output out;int s=0,r;memset(&out,0,sizeof out);
        r=parse(wire,strlen(wire),step,limit,head,&out,&s);++tests;
        if(r!=expected||(expected==N70_DONE&&(s!=status||out.len!=strlen(text)||memcmp(out.data,text,out.len)))){
            fprintf(stderr,"FAIL step=%u result=%d expected=%d status=%d wire=%.120s\n",(unsigned)step,r,expected,s,wire);exit(1);
        }
    }
}
static void url(const char*base,const char*relative,const char*expected)
{
    n70_url a,b;char formatted[4097];int ok;
    if(!n70_url_parse(base,&a))exit(2);ok=n70_url_resolve(&a,relative,&b);++tests;
    if(!expected){if(ok){fprintf(stderr,"unexpected URL accepted: %s\n",relative);exit(1);}return;}
    if(!ok||!n70_url_format(&b,formatted,sizeof formatted)||strcmp(formatted,expected)){
        fprintf(stderr,"URL fail: %s + %s -> %s\n",base,relative,ok?formatted:"rejected");exit(1);
    }
}
static void reuse(const char*wire,int head,int expected)
{
    size_t step;
    for(step=1;step<=strlen(wire)+1;++step){struct output out;int status,r;memset(&out,0,sizeof out);
        r=parse(wire,strlen(wire),step,100,head,&out,&status);++tests;
        if(r!=N70_DONE||last_reusable!=expected){fprintf(stderr,"reuse failure step=%u r=%d reusable=%d\n",(unsigned)step,r,last_reusable);exit(11);}
    }
}
int main(void)
{
    const char*fixed="HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nhello";
    const char*chunked="HTTP/1.1 200 OK\r\nTransfer-Encoding: Chunked\r\n\r\n2;note=abc\r\nhe\r\n3\r\nllo\r\n0\r\nX-Result: yes\r\n\r\n";
    size_t n;
    {const char* h="HTTP/1.1 200 OK\r\nContent-Type:\tapplication/json \t\r\nEmpty:\r\n\r\n";const char*v;size_t len;
     ++tests;if(!n70_header_value(h,strlen(h),"content-type",12,&v,&len)||len!=16||memcmp(v,"application/json",16))return 7;
     ++tests;if(n70_header_value(h,strlen(h),"Content",7,&v,&len))return 8;
     ++tests;if(!n70_header_value(h,strlen(h),"empty",5,&v,&len)||len)return 9;}
    fixture(fixed,5,0,N70_DONE,"hello",200);
    reuse(fixed,0,1);reuse(chunked,0,1);
    reuse("HTTP/1.1 200 OK\r\nContent-Length: 1\r\nConnection: cLoSe\r\n\r\nx",0,0);
    reuse("HTTP/1.1 200 OK\r\nContent-Length: 1\r\nConnection: keep-alive, close\r\n\r\nx",0,0);
    reuse("HTTP/1.1 200 OK\r\nContent-Length: 1\r\nConnection: close\r\nConnection: keep-alive\r\n\r\nx",0,0);
    reuse("HTTP/1.0 200 OK\r\nContent-Length: 1\r\n\r\nx",0,0);
    reuse("HTTP/1.0 200 OK\r\nContent-Length: 1\r\nConnection: keep-alive\r\n\r\nx",0,1);
    reuse("HTTP/1.1 200 OK\r\n\r\nclose-delimited",0,0);
    reuse("HTTP/1.1 204 Empty\r\n\r\n",0,1);
    reuse("HTTP/1.1 200 OK\r\nContent-Length: 999\r\n\r\n",1,1);
    reuse("HTTP/1.1 408 Timeout\r\nContent-Length: 0\r\n\r\n",0,0);
    reuse("HTTP/1.1 100 Continue\r\nConnection: close\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n",0,0);
    reuse("HTTP/1.1 200 OK\r\nContent-Length: 1\r\n\r\nxunexpected",0,0);
    fixture("HTTP/1.1 200 OK\r\nConnection: keep-alive close\r\n\r\n",100,0,N70_BAD,"",0);
    fixture("HTTP/1.1 503 Busy\r\nRetry-After: 0\r\nRetry-After: 1\r\n\r\n",100,0,N70_BAD,"",0);
    fixture(chunked,5,0,N70_DONE,"hello",200);
    fixture("HTTP/1.0 404 Missing\r\nX: y\r\n\r\nnot found",100,0,N70_DONE,"not found",404);
    fixture("HTTP/1.1 100 Continue\r\n\r\nHTTP/1.1 103 Early Hints\r\nLink: </x>\r\n\r\nHTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n",0,0,N70_DONE,"",200);
    fixture("HTTP/1.1 204 No Content\r\n\r\n",0,0,N70_DONE,"",204);
    fixture("HTTP/1.1 304 Not Modified\r\nContent-Length: 999\r\n\r\n",0,0,N70_DONE,"",304);
    fixture("HTTP/1.1 200 OK\r\nContent-Length: 999\r\n\r\n",0,1,N70_DONE,"",200);
    fixture(fixed,4,0,N70_LIMIT,"",0);fixture(chunked,4,0,N70_LIMIT,"",0);
    fixture("HTTP/1.1 200 OK\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\nhello",100,0,N70_BAD,"",0);
    fixture("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\nContent-Length: 0\r\n\r\n",100,0,N70_BAD,"",0);
    fixture("HTTP/1.1 200 OK\r\nContent-Length : 1\r\n\r\nx",100,0,N70_BAD,"",0);
    fixture("HTTP/1.1 200 OK\r\n folded: x\r\n\r\n",100,0,N70_BAD,"",0);
    fixture("HTTP/1.1 200 OK\r\nContent-Encoding: br\r\n\r\n",100,0,N70_ENCODING,"",0);
    fixture("HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 999\r\n\r\n",0,1,N70_DONE,"",200);
    fixture("HTTP/1.1 204 No Content\r\nContent-Encoding: gzip\r\n\r\n",0,0,N70_DONE,"",204);
    fixture("HTTP/1.1 200 OK\r\nContent-Encoding: gzip, br\r\n\r\n",100,0,N70_ENCODING,"",0);
    fixture("HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Encoding: gzip\r\n\r\n",100,0,N70_ENCODING,"",0);
    {n70_http*p=malloc(sizeof *p);size_t used;int r;
     const char*h="HTTP/1.1 200 OK\r\nContent-Encoding: GZIP\r\nContent-Length: 0\r\n\r\n";
     n70_http_init(p,100,0);r=n70_http_feed(p,(const unsigned char*)h,strlen(h),&used,NULL,NULL);
     ++tests;if(r!=N70_HEADERS||!p->gzip)exit(10);free(p);}
    fixture("HTTP/1.1 200 OK\r\nTransfer-Encoding: gzip, chunked\r\n\r\n",100,0,N70_ENCODING,"",0);
    fixture("HTTP/1.1 200 OK\r\nContent-Length: 18446744073709551616\r\n\r\n",100,0,N70_BAD,"",0);
    fixture("HTTP/1.1 200 OK\r\nContent-Length: -1\r\n\r\n",100,0,N70_BAD,"",0);
    fixture("HTTP/1.1 302 Found\r\nLocation: \r\nLocation: /other\r\n\r\n",100,0,N70_BAD,"",0);
    fixture("HTTP/1.1 101 Switching Protocols\r\n\r\n",100,0,N70_BAD,"",0);
    fixture("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n1\r\nx!\n0\r\n\r\n",100,0,N70_BAD,"",0);
    fixture("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nZ\r\n",100,0,N70_BAD,"",0);
    fixture("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n0\r\nContent-Length: 1\r\n\r\n",100,0,N70_BAD,"",0);
    for(n=0;n<strlen(fixed);n++){struct output o;int s;memset(&o,0,sizeof o);++tests;if(parse(fixed,n,1,100,0,&o,&s)!=N70_BAD)exit(3);}
    for(n=0;n<strlen(chunked);n++){struct output o;int s;memset(&o,0,sizeof o);++tests;if(parse(chunked,n,1,100,0,&o,&s)!=N70_BAD)exit(4);}
    {struct output o;int s;memset(&o,0,sizeof o);o.fail=1;++tests;if(parse(fixed,strlen(fixed),8,100,0,&o,&s)!=N70_SINK)exit(5);}
    url("https://Example.COM/a/b?old=1","../c?x=2#f","https://example.com/c?x=2");
    url("https://example.com/a/b","?x=https://other/","https://example.com/a/b?x=https://other/");
    url("https://example.com/a/b","/a//b/./../c","https://example.com/a//c");
    url("https://example.com/a/b","//other.test:8443/x","https://other.test:8443/x");
    url("https://example.com/a/b","https://OTHER.test/x#f","https://other.test/x");
    url("https://example.com/a/b","http://other.test/",NULL);
    url("https://example.com/a/b","https://user:pass@other.test/",NULL);
    url("https://example.com/a/b","https://other.test:65536/",NULL);
    url("https://example.com/a/b","https://other.test\\@evil/",NULL);
    url("https://example.com/a/b","/x\r\nInjected: x",NULL);
    fixture("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n10000000000000000\r\n",100,0,N70_BAD,"",0);
    {char huge[18000];size_t prefix=strlen("HTTP/1.1 200 OK\r\nX: ");
     strcpy(huge,"HTTP/1.1 200 OK\r\nX: ");memset(huge+prefix,'a',17000);
     strcpy(huge+prefix+17000,"\r\n\r\n");
     {struct output o;int s;memset(&o,0,sizeof o);++tests;
      if(parse(huge,strlen(huge),17,100,0,&o,&s)!=N70_LIMIT)exit(6);}}
    {size_t i;for(i=0;i<strlen(chunked);i++){
        unsigned bit;for(bit=0;bit<8;bit++){
            char mutated[256];struct output o;int s;
            strcpy(mutated,chunked);mutated[i]^=(char)(1U<<bit);
            memset(&o,0,sizeof o);parse(mutated,strlen(chunked),1,100,0,&o,&s);++tests;
        }
    }}
    printf("PASS: %u HTTP framing/stream fragmentation, truncation, limits, sink failure and URL cases\n",tests);
    return 0;
}
