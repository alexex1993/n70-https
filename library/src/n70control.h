#ifndef N70_CONTROL_H
#define N70_CONTROL_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 0=continue, 1=cancel, 2=deadline. Monotonic modulo-32-bit tick arithmetic. */
int n70_deadline(uint32_t start,uint32_t now,uint32_t tick_us,uint32_t timeout_ms,int cancelled);
#ifdef __cplusplus
}
#endif
#endif
