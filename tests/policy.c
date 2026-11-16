#include "n70policy.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{++tests;if(!(x)){fprintf(stderr,"FAIL policy line %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    static const char*const verbs[]={"GET","HEAD","POST","PUT","PATCH","DELETE","OPTIONS","get",""};
    static const int errors[]={-3,-4,-6,-9,-18,-20,-21,-25,-29,-33,-34,-36,-13,-15,-8,-1,-2,0,-190,-191,-5100,-5104,-5105,-5106,-5109,-192,-5107};
    unsigned tests=0,i,j;int id,delivered,control,attempt,after;
    uint64_t now=n70_epoch_seconds(1994,11,6,8,49,30);
    CHECK(now==784111770U);CHECK(n70_epoch_seconds(1970,1,1,0,0,0)==0);
    CHECK(n70_epoch_seconds(2000,1,1,0,0,0)==946684800U);
    CHECK(n70_epoch_seconds(2000,2,29,0,0,0)==951782400U);
    CHECK(n70_epoch_seconds(2100,2,29,0,0,0)==UINT64_MAX);
    for(i=0;i<sizeof verbs/sizeof verbs[0];++i){
        CHECK(n70_method_valid(verbs[i])==(i<6));
        for(j=0;j<sizeof errors/sizeof errors[0];++j)for(id=0;id<2;++id)for(delivered=0;delivered<2;++delivered)for(control=0;control<2;++control){
            int e=errors[j],net=e==-18||e==-25||e==-29||e==-33||e==-34||e==-36||e==-13||e==-15||e==-8||
                e==-190||e==-191||e==-5100||e==-5104||e==-5105||e==-5106||e==-5109;
            int method=i<2||(id&&(i==3||i==5));
            CHECK(n70_retry_allowed(verbs[i],id,delivered,e,control)==(!control&&!delivered&&method&&net));
        }
    }
    CHECK(n70_method_has_body("PUT"));CHECK(n70_method_has_body("PATCH"));CHECK(n70_method_has_body("DELETE"));
    CHECK(!n70_method_has_body("GET"));CHECK(!n70_method_has_body("HEAD"));
    for(i=100;i<=599;++i)CHECK(n70_retry_status(i)==(i==408||i==429||i==502||i==503||i==504));
#define AFTER(s,n) CHECK(n70_retry_after(s,sizeof(s)-1,now,1994)==(n))
    AFTER("0",0);AFTER("2",2000);AFTER("0002",2000);AFTER("2147483",2147483000);
    AFTER("2147484",2147483647);AFTER("99999999999999999999999999",2147483647);
    AFTER("",-1);AFTER("-1",-1);AFTER("+1",-1);AFTER("1.5",-1);AFTER("12abc",-1);AFTER("1\0",-1);
    AFTER("Sun, 06 Nov 1994 08:49:37 GMT",7000);
    AFTER("Sunday, 06-Nov-94 08:49:37 GMT",7000);
    AFTER("Sun Nov  6 08:49:37 1994",7000);
    AFTER("Sun, 06 Nov 1994 08:49:29 GMT",0);
    AFTER("Sun, 31 Apr 1994 08:49:37 GMT",-1);
    AFTER("Sun, 06 Xxx 1994 08:49:37 GMT",-1);
    AFTER("Sun, 06 Nov 1994 25:49:37 GMT",-1);
    AFTER("Sun, 06 Nov 1994 08:49:37 UTC",-1);
    AFTER("Sun, 06 Nov 9999 08:49:37 GMT",2147483647);
    CHECK(n70_retry_after("Sunday, 06-Nov-94 08:49:37 GMT",30,now,2026)==7000);
    for(attempt=0;attempt<11;++attempt)for(after=-1;after<=12000;after+=101){
        int d=n70_retry_delay(500,10000,attempt,after,123456U);
        if(after>10000)CHECK(d==-1);else{CHECK(d>=500&&d<=10000);CHECK(d>=after);}
    }
    CHECK(n70_retry_delay(500,10000,0,20000,0)==-1);
    CHECK(n70_retry_delay(500,10000,0,10000,0)==10000);
    CHECK(n70_retry_delay(0,10000,2,2000,0)==2000);
    CHECK(n70_retry_delay(500,10000,0,-1,0)==500);
    CHECK(n70_retry_delay(500,10000,1,-1,0)==1000);
    CHECK(n70_retry_delay(500,10000,20,-1,0)==10000);
    printf("PASS: %u policy checks: methods, conservative retries, delivered-response/cancel guards, three Retry-After date forms, overflow, exponential jitter and server-delay cap.\n",tests);return 0;
}
