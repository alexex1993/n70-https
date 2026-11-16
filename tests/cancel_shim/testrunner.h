#ifndef N70_CANCEL_SHIM_H
#define N70_CANCEL_SHIM_H
#include "../wait_shim/n70https.h"
class CTestRunner {
public:
    TRequestStatus iStatus;bool active=false;int calls=0;
    void Start(){sim::schedule(iStatus,sim::now+5000,0);active=true;}
    void CancelWork(){++calls;if(iStatus==KRequestPending)for(auto&e:sim::events)if(e.status==&iStatus){e.at=sim::now+10;e.result=KErrCancel;}}
    void DoCancel(); // The actual app implementation is linked from testcancel.cpp.
    // Match CActive::Cancel: DoCancel is followed by one framework-owned wait.
    void Cancel(){if(active){DoCancel();User::WaitForRequest(iStatus);active=false;}}
};
#endif
