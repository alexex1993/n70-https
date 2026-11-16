#ifndef N70_HEADERS_H
#define N70_HEADERS_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define N70_REQUEST_HEADERS_MAX 4096
/* Transactional replacement; data is CRLF-terminated fields, not NUL-terminated.
 * Returns 0, N70_BAD (invalid/reserved field), or N70_LIMIT. */
int n70_headers_set(char*,size_t*,size_t,const char*,size_t,const char*,size_t);
int n70_headers_has(const char*,size_t,const char*,size_t);
int n70_bearer_valid(const char*,size_t);
/* Once a redirect changes origin, custom fields stay stripped for all hops. */
int n70_headers_forward(int previously_allowed,int same_origin);
#ifdef __cplusplus
}
#endif
#endif
