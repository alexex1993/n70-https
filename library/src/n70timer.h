#ifndef N70_TIMER_H
#define N70_TIMER_H
// RTimer is the source of timeout expiry. Long supported intervals are split
// into <=30-minute arms to fit Symbian's signed 32-bit microsecond API.
static inline void ArmTimer(RTimer& timer,TRequestStatus& status,TInt& remaining)
    {TInt ms=Min(remaining,1800000);remaining-=ms;timer.After(status,ms*1000);}
static inline TInt TimerError(RTimer& timer,TRequestStatus& status,TInt& remaining)
    {
    if(status==KRequestPending)return KErrNone;
    if(status.Int()!=KErrNone)return status.Int();
    if(!remaining)return KErrTimedOut;
    User::WaitForRequest(status);ArmTimer(timer,status,remaining);return KErrNone;
    }

#endif
