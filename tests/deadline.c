#include <stdio.h>
#include "n70control.h"
#define EXPECT(expr,value) do {if((expr)!=(value)){fprintf(stderr,"FAIL line %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    EXPECT(n70_deadline(100,100,1000,30000,0),0);
    EXPECT(n70_deadline(100,30099,1000,30000,0),0);
    EXPECT(n70_deadline(100,30100,1000,30000,0),2);
    EXPECT(n70_deadline(100,100,1000,30000,1),1);
    EXPECT(n70_deadline(0,999999,1000,1,1),1);
    EXPECT(n70_deadline(0xfffffff0U,0x10,1000,33,0),0);
    EXPECT(n70_deadline(0xfffffff0U,0x10,1000,32,0),2);
    EXPECT(n70_deadline(0,86400000U,1000,86400000U,0),2);
    EXPECT(n70_deadline(0,86399999U,1000,86400000U,0),0);
    /* A fresh per-operation clock must not extend the whole redirect chain. */
    EXPECT(n70_deadline(100,101,1000,30000,0),0);
    EXPECT(n70_deadline(0,101,1000,100,0),2);
    puts("PASS: cancellation precedence, deadline boundaries, tick wrap, 64-bit duration and whole-request deadline.");
    return 0;
}
