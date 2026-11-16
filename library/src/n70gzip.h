#ifndef N70_GZIP_H
#define N70_GZIP_H
#include "n70http.h"
#include <zlib.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {z_stream z;uint64_t bytes,limit;int active,ended,members;unsigned char out[2048];} n70_gzip;
int n70_gzip_init(n70_gzip*,uint64_t,alloc_func,free_func,void*);
int n70_gzip_feed(n70_gzip*,const unsigned char*,size_t,n70_body_fn,void*);
int n70_gzip_eof(const n70_gzip*);
void n70_gzip_close(n70_gzip*);
#ifdef __cplusplus
}
#endif
#endif
