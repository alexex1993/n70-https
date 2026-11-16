#include "n70https.h"
#include "n70timer.h"
TInt CN70Https::ControlError()
    {
    if(iOptions.iCancel&&iOptions.iCancel->IsCancelled())return KErrCancel;
    return iDeadlineArmed?TimerError(iDeadlineTimer,iDeadlineStatus,iDeadlineRemaining):KErrNone;
    }
void CN70Https::CheckL() { User::LeaveIfError(ControlError()); }
TInt CN70Https::ControlCallback(TAny* self){return static_cast<CN70Https*>(self)->ControlError();}
TInt CN70Https::Wait(TRequestStatus& status,TInt target)
    {
    TInt error=0,remaining=iOptions.iIoTimeoutMs;TRequestStatus ioDeadline;TBool consumed=EFalse;
    ArmTimer(iIoTimer,ioDeadline,remaining);
    for(;;){
        error=ControlError();
        if(!error)error=TimerError(iIoTimer,ioDeadline,remaining);
        if(error||status!=KRequestPending)break;
        TRequestStatus tick;iTimer.After(tick,100000);User::WaitForRequest(status,tick);
        if(status!=KRequestPending){
            // The pair-wait consumed one completion. Drain the other exactly
            // once, including when both requests completed in the same tick.
            consumed=ETrue;iTimer.Cancel();User::WaitForRequest(tick);
        }
        // Otherwise tick completed and its signal was ALREADY consumed by the
        // pair-wait. A second WaitForRequest(tick) would block on a new signal.
    }
    if(status==KRequestPending){
        if(target==1){iResolver.Cancel();iResolver.Close();}
        else if(target==2){iConnection.Close();iConnectionStarted=EFalse;}
        else {
            if(target==3)iSocket.CancelConnect();else iSocket.CancelAll();
            // Do not rely on CancelAll alone completing a firmware TCP request.
            // Close the handle before draining completion of the local status.
            iSocket.Close();
        }
    }
    if(!consumed)User::WaitForRequest(status);
    iIoTimer.Cancel();User::WaitForRequest(ioDeadline);
    return error?error:status.Int();
    }
void CN70Https::DelayL(TInt ms)
    {
    CheckL();if(!ms)return;
    iStage=ERetryWait;TInt left=ms,error=0;TRequestStatus delay;ArmTimer(iIoTimer,delay,left);
    for(;;){
        error=ControlError();if(error)break;
        TInt timer=TimerError(iIoTimer,delay,left);if(timer==KErrTimedOut)break;if(timer){error=timer;break;}
        // Only wait on the poll request. The delay's completion stays unconsumed
        // until TimerError rearms it or the single final drain below.
        TRequestStatus tick;iTimer.After(tick,100000);User::WaitForRequest(tick);
    }
    iIoTimer.Cancel();User::WaitForRequest(delay);User::LeaveIfError(error);CheckL();
    }
