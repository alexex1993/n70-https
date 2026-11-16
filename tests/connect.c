#include "n70connect.h"
#include <stdio.h>
#include <string.h>
struct net{uint32_t ips[32],connected[64];int errs[64],answers,pos,starts,resets,calls,control,start_error,next_error,cancel_on_connect;};
static int start(void*p){struct net*n=p;++n->starts;return n->starts==1?n->start_error:0;}
static int resolve(void*p,int first,uint32_t*ip){struct net*n=p;if(first)n->pos=0;if(n->pos>=n->answers)return n->next_error?n->next_error:-25;*ip=n->ips[n->pos++];return 0;}
static int connect_ip(void*p,uint32_t ip){struct net*n=p;int index=n->calls;n->connected[n->calls++]=ip;if(n->cancel_on_connect)n->control=n->cancel_on_connect;return n->errs[index];}
static void reset(void*p){++((struct net*)p)->resets;}
static int control(void*p){return ((struct net*)p)->control;}
static int run(struct net*n,int max){n70_connect_ops o={n,start,resolve,connect_ip,reset,control};return n70_connect_run(&o,max);}
#define CHECK(x) do{++tests;if(!(x)){fprintf(stderr,"FAIL connect line %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    struct net n;unsigned tests=0,i;
    memset(&n,0,sizeof n);n.answers=2;n.ips[0]=111;n.ips[1]=222;n.errs[0]=-33;
    CHECK(run(&n,8)==0);CHECK(n.calls==2&&n.connected[0]==111&&n.connected[1]==222&&n.resets==0);
    memset(&n,0,sizeof n);n.answers=2;n.ips[0]=111;n.ips[1]=222;n.errs[0]=-191;
    CHECK(run(&n,8)==0);CHECK(n.calls==2&&n.resets==0);
    memset(&n,0,sizeof n);n.start_error=-5109;n.answers=1;n.ips[0]=333;
    CHECK(run(&n,8)==0);CHECK(n.starts==2&&n.resets==1);
    memset(&n,0,sizeof n);n.answers=3;n.ips[0]=n.ips[1]=111;n.ips[2]=222;n.errs[0]=-34;
    CHECK(run(&n,8)==0);CHECK(n.calls==2&&n.pos==3);
    memset(&n,0,sizeof n);n.start_error=-18;n.answers=1;n.ips[0]=333;
    CHECK(run(&n,8)==0);CHECK(n.starts==2&&n.resets==1&&n.calls==1);
    memset(&n,0,sizeof n);n.answers=1;n.ips[0]=111;n.errs[0]=-36;
    CHECK(run(&n,8)==0);CHECK(n.starts==2&&n.resets==1&&n.calls==2);
    memset(&n,0,sizeof n);n.answers=1;n.ips[0]=111;n.errs[0]=n.errs[1]=-36;
    CHECK(run(&n,8)==-36);CHECK(n.resets==1&&n.calls==2);
    memset(&n,0,sizeof n);n.answers=2;n.ips[0]=111;n.ips[1]=222;n.errs[0]=-33;n.cancel_on_connect=-3;
    CHECK(run(&n,8)==-3);CHECK(n.calls==1&&!n.resets);
    n.calls=n.pos=n.starts=0;n.control=0;n.cancel_on_connect=-33;
    CHECK(run(&n,8)==-33);CHECK(n.calls==1&&!n.resets);
    memset(&n,0,sizeof n);n.answers=2;n.ips[0]=111;n.ips[1]=222;n.errs[0]=-33;
    CHECK(run(&n,1)==-33);CHECK(n.calls==1&&n.pos==1);
    memset(&n,0,sizeof n);n.next_error=-1;CHECK(run(&n,8)==-1);CHECK(!n.calls&&!n.resets);
    memset(&n,0,sizeof n);n.start_error=-21;CHECK(run(&n,8)==-21);CHECK(!n.calls&&!n.resets);
    memset(&n,0,sizeof n);n.answers=32;for(i=0;i<32;++i){n.ips[i]=i+1;n.errs[i]=-34;}
    CHECK(run(&n,32)==-34);CHECK(n.calls==32);
    memset(&n,0,sizeof n);n.answers=32;for(i=0;i<32;++i)n.ips[i]=1;n.errs[0]=-34;
    CHECK(run(&n,8)==-34);CHECK(n.calls==1&&n.pos==8);
    CHECK(run(&n,0)==-6);CHECK(run(&n,33)==-6);
    printf("PASS: %u DNS/recovery checks: alternate IPv4, deduplication, one restart, cancellation/deadline, bounded lookups, permanent errors.\n",tests);return 0;
}
