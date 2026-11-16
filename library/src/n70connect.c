#include "n70connect.h"
#include "n70policy.h"
int n70_connect_run(const n70_connect_ops*o,int max)
{
    uint32_t seen[32],ip;int pass,error=0,last=0,answers,nseen,i,control;
    if(max<1||max>32)return -6;
    for(pass=0;pass<2;++pass){
        if((control=o->control(o->context))!=0)return control;
        error=o->start(o->context);if(error)goto failed;
        nseen=0;last=0;
        for(answers=0;answers<max;++answers){
            if((control=o->control(o->context))!=0)return control;
            error=o->resolve(o->context,answers==0,&ip);if(error){if(error==-25||error==-1)error=last?last:error;break;}
            for(i=0;i<nseen;++i)if(seen[i]==ip)break;if(i<nseen)continue;seen[nseen++]=ip;
            if((control=o->control(o->context))!=0)return control;
            error=o->connect(o->context,ip);if(!error)return 0;last=error;
            if((control=o->control(o->context))!=0)return control;
            if(!n70_transport_error(error))break;
        }
        if(!error)error=last?last:-34;
failed:
        if((control=o->control(o->context))!=0)return control;
        if(pass||!n70_bearer_error(error))return error;
        o->reset(o->context);
    }return error;
}
