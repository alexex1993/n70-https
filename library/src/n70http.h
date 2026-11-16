#ifndef N70HTTP_CORE_H
#define N70HTTP_CORE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define N70_HTTP_HEADERS_MAX 16384
#define N70_URL_MAX 4096
enum { N70_MORE=0,N70_HEADERS=1,N70_DONE=2,N70_BAD=-1,N70_LIMIT=-2,N70_ENCODING=-3,N70_SINK=-4,N70_MEMORY=-5 };
typedef int (*n70_body_fn)(void*,const unsigned char*,size_t);
typedef struct {
    char headers[N70_HTTP_HEADERS_MAX+1]; size_t hlen,total_headers;
    char line[4096]; size_t llen,trailer_bytes;
    char location[N70_URL_MAX+1];
    uint64_t remaining,body_bytes,max_body;
    int state,status,chunked,has_length,head,interim,crlf,gzip;
    int version11,connection_close,connection_keepalive,self_delimited;
} n70_http;
void n70_http_init(n70_http*,uint64_t max_body,int head);
int n70_http_feed(n70_http*,const unsigned char*,size_t,size_t*,n70_body_fn,void*);
int n70_http_eof(n70_http*);
int n70_http_reusable(const n70_http*,int clean_boundary);
int n70_header_value(const char*,size_t,const char*,size_t,const char**,size_t*);
typedef struct { char host[254],target[3073]; unsigned port; } n70_url;
int n70_url_parse(const char*,n70_url*);
int n70_url_resolve(const n70_url*,const char*,n70_url*);
int n70_url_format(const n70_url*,char*,size_t);
int n70_same_origin(const n70_url*,const n70_url*);
#ifdef __cplusplus
}
#endif
#endif
