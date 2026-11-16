#include "n70control.h"
int n70_deadline(uint32_t start,uint32_t now,uint32_t tick_us,uint32_t timeout_ms,int cancelled)
{
    uint64_t elapsed;
    if(cancelled)return 1;
    elapsed=(uint64_t)(uint32_t)(now-start)*tick_us;
    return elapsed>=(uint64_t)timeout_ms*1000?2:0;
}
