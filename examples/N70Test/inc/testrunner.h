#ifndef TESTRUNNER_H
#define TESTRUNNER_H
#include <e32base.h>
class CN70CancelToken;
class CTestRunner : public CActive
    {
public:
    static CTestRunner* NewL();
    ~CTestRunner();
    void StartL(TInt aMode=0); // 0=GET,1=sites,2=features,3=deadline,4=cancel,5=API/gzip,6=TCP,7=client
    void CancelWork();
private:
    CTestRunner();
    void RunL();
    void DoCancel();
    static TInt ThreadEntry(TAny* aRunner);
    RThread iThread;
    TInt iMode;
    CN70CancelToken* iCancel;
    TBuf<64> iWorkerStage;
    };
#endif
