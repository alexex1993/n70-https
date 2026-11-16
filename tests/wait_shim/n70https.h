// Deterministic host adapter for the real src/n70wait.cpp. No phone claims.
#ifndef N70_WAIT_SHIM_H
#define N70_WAIT_SHIM_H
#include <algorithm>
#include <vector>
#include <stdexcept>
#include <cstdint>
typedef int TInt;typedef void TAny;typedef bool TBool;
const TInt KRequestPending=-1000,KErrNone=0,KErrCancel=-3,KErrTimedOut=-33;
const TBool EFalse=false,ETrue=true;
template<class T>T Min(T a,T b){return a<b?a:b;}
struct TRequestStatus{TInt value=0;TInt Int()const{return value;}operator TInt()const{return value;}};
namespace sim {
    struct Event{TRequestStatus* status;int64_t at;int result;};
    extern std::vector<Event> events;extern int64_t now,cancelAt;
    extern int signals;
    void schedule(TRequestStatus&,int64_t,int);
    void finish(TRequestStatus&,int);
    void advance();
    void wait_signal();
}
class RTimer {
    TRequestStatus* status=nullptr;
public:
    void After(TRequestStatus&s,TInt us){status=&s;sim::schedule(s,sim::now+us/1000,0);}
    void Cancel(){if(status){sim::finish(*status,KErrCancel);status=nullptr;}}
};
namespace User {
    struct LeaveError {TInt code;explicit LeaveError(TInt e):code(e){}};
    inline void LeaveIfError(TInt e){if(e<0)throw LeaveError(e);}
    // Match euser/us_exec.cpp: do/while, always consume one semaphore signal,
    // even if status is already complete; restore unrelated completions.
    inline void WaitForRequest(TRequestStatus&a){int restore=-1;do{++restore;sim::wait_signal();}while(a==KRequestPending);sim::signals+=restore;}
    inline void WaitForRequest(TRequestStatus&a,TRequestStatus&b){int restore=-1;do{++restore;sim::wait_signal();}while(a==KRequestPending&&b==KRequestPending);sim::signals+=restore;}
}
class CN70CancelToken {public:bool IsCancelled(){return sim::now>=sim::cancelAt;}};
struct FakeHandle {
    TRequestStatus* status=nullptr;bool closed=false,cancelConnect=false,cancelAll=false,cancel=false;
    // Simulate a firmware request whose cancellation alone doesn't complete it.
    void CancelConnect(){cancelConnect=true;}
    void CancelAll(){cancelAll=true;}
    void Cancel(){cancel=true;}
    void Close(){closed=true;if(status)sim::finish(*status,KErrCancel);}
};
class CN70Https {
public:
    enum {ERetryWait=8};TInt iStage=0;
    struct Options{TInt iIoTimeoutMs=30000;CN70CancelToken* iCancel=nullptr;}iOptions;
    bool iDeadlineArmed=true,iConnectionStarted=true;
    RTimer iDeadlineTimer,iIoTimer,iTimer;
    TRequestStatus iDeadlineStatus;TInt iDeadlineRemaining=0;
    FakeHandle iSocket,iConnection,iResolver;
    TInt ControlError();void CheckL();static TInt ControlCallback(TAny*);
    TInt Wait(TRequestStatus&,TInt);
    void DelayL(TInt);
};
#endif
