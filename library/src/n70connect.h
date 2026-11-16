#ifndef N70_CONNECT_H
#define N70_CONNECT_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    void* context;
    int (*start)(void*);
    int (*resolve)(void*,int first,uint32_t* address);
    int (*connect)(void*,uint32_t address);
    void (*reset)(void*);
    int (*control)(void*);
} n70_connect_ops;
/* Sequential IPv4 fallback, deduplication, bounded answers, one bearer restart.
 * All callbacks use Symbian error values; start/resolve/connect must be bounded. */
int n70_connect_run(const n70_connect_ops*,int max_answers);
#ifdef __cplusplus
}
#endif
#endif
