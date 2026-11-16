#include "n70https.h"
#include "n70timer.h"
#include <cstdio>
#include <climits>
namespace sim {
    std::vector<Event> events;int64_t now=0,cancelAt=INT64_MAX;int signals=0;
    void finish(TRequestStatus&s,int result){if(s==KRequestPending){s.value=result;++signals;}
        events.erase(std::remove_if(events.begin(),events.end(),[&](const Event&e){return e.status==&s;}),events.end());}
    void schedule(TRequestStatus&s,int64_t at,int result){finish(s,KErrCancel);s.value=KRequestPending;events.push_back({&s,at,result});}
    void advance(){if(events.empty())throw std::runtime_error("empty request semaphore: repeated wait / no future completion");
        auto e=*std::min_element(events.begin(),events.end(),[](const Event&a,const Event&b){return a.at<b.at;});
        now=e.at;
        std::vector<Event> ready;for(const auto&v:events)if(v.at<=now)ready.push_back(v);
        for(const auto&v:ready)finish(*v.status,v.result);
    }
    void wait_signal(){while(!signals)advance();--signals;}
}
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){std::fprintf(stderr,"FAIL wait line %d time=%lld\n",__LINE__,sim::now);return 1;}}while(0)
static void start(CN70Https&c,TRequestStatus&s,int total,int io,int64_t completion=153000)
{
    if(!sim::events.empty()||sim::signals)throw std::runtime_error("previous case leaked a request completion");
    sim::now=0;sim::cancelAt=INT64_MAX;
    c.iOptions.iIoTimeoutMs=io;c.iDeadlineRemaining=total;ArmTimer(c.iDeadlineTimer,c.iDeadlineStatus,c.iDeadlineRemaining);
    sim::schedule(s,completion,0);c.iSocket.status=c.iResolver.status=c.iConnection.status=&s;
}
static void cleanup(CN70Https&c,TRequestStatus&s,bool unusedSocket=false)
{
    c.iDeadlineTimer.Cancel();User::WaitForRequest(c.iDeadlineStatus);
    if(unusedSocket){sim::finish(s,KErrCancel);User::WaitForRequest(s);}
    ++checks;if(!sim::events.empty()||sim::signals)throw std::runtime_error("unbalanced request semaphore after cleanup");
}
static int run_tests()
{
    {TRequestStatus s;sim::schedule(s,10,0);User::WaitForRequest(s);CHECK(sim::signals==0);
     bool blocked=false;try{User::WaitForRequest(s);}catch(const std::runtime_error&){blocked=true;}CHECK(blocked);}
    {TRequestStatus a,b;sim::schedule(a,20,0);sim::schedule(b,15,0);User::WaitForRequest(a);
     CHECK(sim::signals==1);User::WaitForRequest(b);CHECK(sim::signals==0);}
    {TRequestStatus a,b;sim::schedule(a,30,0);sim::schedule(b,30,0);User::WaitForRequest(a,b);
     CHECK(sim::signals==1);User::WaitForRequest(b);CHECK(sim::signals==0);}
    {CN70Https c;TRequestStatus s;start(c,s,120000,30000,12);CHECK(c.Wait(s,3)==0);CHECK(sim::now==12&&!c.iSocket.closed);cleanup(c,s);}
    {CN70Https c;TRequestStatus s;start(c,s,120000,30000,150);CHECK(c.Wait(s,3)==0);CHECK(sim::now==150);cleanup(c,s);}
    {CN70Https c;TRequestStatus s;start(c,s,120000,30000,100);CHECK(c.Wait(s,3)==0);CHECK(sim::now==100);cleanup(c,s);}
    {CN70Https c;TRequestStatus s;start(c,s,120000,30000,0);sim::advance();CHECK(c.Wait(s,3)==0);CHECK(sim::now==0);cleanup(c,s);}
    {CN70Https c;TRequestStatus s;start(c,s,120000,30000);CHECK(c.Wait(s,3)==KErrTimedOut);
     CHECK(sim::now==30000&&c.iSocket.cancelConnect&&c.iSocket.closed&&s!=KRequestPending);cleanup(c,s);}
    {CN70Https c;TRequestStatus s;start(c,s,2000,30000);CHECK(c.Wait(s,3)==KErrTimedOut);CHECK(sim::now==2000&&c.iSocket.closed);cleanup(c,s);}
    {CN70Https c;TRequestStatus s;start(c,s,100,30000,40);CHECK(c.Wait(s,0)==0);
     sim::schedule(s,80,0);CHECK(c.Wait(s,0)==0);sim::schedule(s,153000,0);
     CHECK(c.Wait(s,0)==KErrTimedOut);CHECK(sim::now>=100&&sim::now<200&&c.iSocket.cancelAll&&c.iSocket.closed);cleanup(c,s);}
    {CN70Https c;TRequestStatus s;CN70CancelToken token;start(c,s,120000,30000);c.iOptions.iCancel=&token;sim::cancelAt=350;
     CHECK(c.Wait(s,3)==KErrCancel);CHECK(sim::now==400&&c.iSocket.closed);cleanup(c,s);}
    {CN70Https c;TRequestStatus s;start(c,s,2000,30000);CHECK(c.Wait(s,1)==KErrTimedOut);CHECK(c.iResolver.cancel&&c.iResolver.closed);cleanup(c,s);}
    {CN70Https c;TRequestStatus s;start(c,s,2000,30000);CHECK(c.Wait(s,2)==KErrTimedOut);CHECK(c.iConnection.closed&&!c.iConnectionStarted);cleanup(c,s);}
    {CN70Https c;TRequestStatus s;start(c,s,120000,30000);sim::finish(s,-42);CHECK(c.Wait(s,3)==-42&&!c.iSocket.closed);cleanup(c,s);}
    {CN70Https c;TRequestStatus s;start(c,s,1800100,1800200,1900000);CHECK(c.Wait(s,3)==KErrTimedOut);CHECK(sim::now==1800100);cleanup(c,s);}
    {CN70Https c;TRequestStatus s;CN70CancelToken token;start(c,s,2000,30000);c.iOptions.iCancel=&token;sim::cancelAt=2000;
     CHECK(c.Wait(s,3)==KErrCancel);CHECK(sim::now==2000);cleanup(c,s);}
    {CN70Https c;TRequestStatus s;start(c,s,2000,30000);c.DelayL(500);CHECK(sim::now==500&&c.iStage==CN70Https::ERetryWait);cleanup(c,s,true);}
    {CN70Https c;TRequestStatus s;start(c,s,200,30000);bool left=false;try{c.DelayL(1000);}catch(const User::LeaveError&e){left=e.code==KErrTimedOut;}
     CHECK(left&&sim::now==200);cleanup(c,s,true);}
    {CN70Https c;TRequestStatus s;CN70CancelToken token;start(c,s,2000,30000);c.iOptions.iCancel=&token;sim::cancelAt=150;
     bool left=false;try{c.DelayL(1000);}catch(const User::LeaveError&e){left=e.code==KErrCancel;}CHECK(left&&sim::now==200);cleanup(c,s,true);}
    std::printf("PASS: %u request-semaphore checks of actual n70wait.cpp: double-wait detection, signal restoration/balance, simultaneous/immediate completion, TCP expiry, cancellation, retry delay and long timer rearm.\n",checks);
    return 0;
}
int main()
{
    try{return run_tests();}
    catch(const User::LeaveError&e){std::fprintf(stderr,"FAIL unexpected leave %d\n",e.code);}
    catch(const std::exception&e){std::fprintf(stderr,"FAIL %s at %lld ms\n",e.what(),sim::now);}
    return 1;
}
