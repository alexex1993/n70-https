#include "testrunner.h"
#include "n70https.h"
#include "testanchor.h"
#include "n70testsites.h"
#include "n70testpaths.h"
#include "httpfeatures.h"
#include <f32file.h>
#include <aknnotewrappers.h>

_LIT(KResult,"C:\\system\\apps\\N70Test\\response.txt");
static void TestL(CN70CancelToken* aCancel)
    {
    RFs fs; User::LeaveIfError(fs.Connect()); CleanupClosePushL(fs);
    TInt dirError=fs.MkDirAll(_L("C:\\system\\apps\\N70Test\\"));
    if (dirError!=KErrAlreadyExists) User::LeaveIfError(dirError);
    RFile file;
    HBufC8* response=HBufC8::NewLC(65536);
    TPtr8 out=response->Des();
    CN70Https* client=CN70Https::NewL(); CleanupStack::PushL(client);
    TN70HttpOptions options;options.iCancel=aCancel;client->SetOptionsL(options);
    br_x509_trust_anchor anchors[TEST_ANCHOR_COUNT];
    InitTestAnchors(anchors);
    TRAPD(error,client->GetAutoL(_L8("example.com"),_L8("/"),
        anchors,TEST_ANCHOR_COUNT,KDeviceState,out,ETrue));
    TInt tls=client->TlsError();
    TBuf8<200> header;
    header.Format(_L8("N70Test 0.9.1\r\nSymbian error: %d; BearSSL error: %d\r\nSocket I/O error: %d\r\nStage: "),error,tls,client->NetworkError());
    header.Append(client->StageName());
    header.Append(_L8("\r\n"));
    CleanupStack::PopAndDestroy(client);
    User::LeaveIfError(file.Replace(fs,KResult,EFileWrite)); CleanupClosePushL(file);
    User::LeaveIfError(file.Write(header)); User::LeaveIfError(file.Write(out));
    CleanupStack::PopAndDestroy(&file);
    CleanupStack::PopAndDestroy(response);
    CleanupStack::PopAndDestroy(&fs);
    User::LeaveIfError(error);
    }
static void TestSitesL(TDes& aWorkerStage,CN70CancelToken* aCancel)
    {
    aWorkerStage.Copy(_L("connect file server"));
    RFs fs; User::LeaveIfError(fs.Connect()); CleanupClosePushL(fs);
    aWorkerStage.Copy(_L("create log directory"));
    TInt dirError=fs.MkDirAll(_L("C:\\system\\apps\\N70Test\\"));
    if (dirError!=KErrAlreadyExists) User::LeaveIfError(dirError);
    RFile log;
    aWorkerStage.Copy(_L("open tls-tests.txt for writing"));
    User::LeaveIfError(log.Replace(fs,_L("C:\\system\\apps\\N70Test\\tls-tests.txt"),EFileWrite|EFileShareExclusive));
    CleanupClosePushL(log);
    aWorkerStage.Copy(_L("write test log header"));
    User::LeaveIfError(log.Write(_L8("N70Test 0.9.1: ten separate TLS 1.2 connections\r\nHTTP redirects are recorded, not followed.\r\n")));
    User::LeaveIfError(log.Flush());
    aWorkerStage.Copy(_L("allocate response buffer"));
    HBufC8* response=HBufC8::NewLC(16384); TPtr8 out=response->Des();
    aWorkerStage.Copy(_L("create HTTPS client"));
    CN70Https* client=CN70Https::NewL(); CleanupStack::PushL(client);
    TN70HttpOptions options;options.iCancel=aCancel;client->SetOptionsL(options);
    br_x509_trust_anchor anchors[TEST_ANCHOR_COUNT]; InitTestAnchors(anchors);
    TInt passed=0, attempted=0, finalError=KErrNone;
    for (TInt i=0; i<N70_TEST_SITE_COUNT; ++i)
        {
        TPtrC8 host(reinterpret_cast<const TUint8*>(KN70TestSites[i]));
        TBuf8<384> line; line.Copy(_L8("START ")); line.Append(host); line.Append(_L8("\r\n"));
        aWorkerStage.Copy(_L("write START marker"));
        User::LeaveIfError(log.Write(line)); User::LeaveIfError(log.Flush());
        TTime start; start.UniversalTime();
        aWorkerStage.Copy(_L("HTTPS request"));
        TRAPD(error,client->GetAutoL(host,_L8("/"),anchors,
            TEST_ANCHOR_COUNT,KDeviceState,out,i==0,ETrue));
        TTime end; end.UniversalTime(); TTimeIntervalSeconds duration(0);
        end.SecondsFrom(start,duration);
        ++attempted;
        TBool good=error==KErrNone && client->ProtocolVersion()==BR_TLS12 &&
            out.Length()>=5 && out.Left(5).Compare(_L8("HTTP/"))==0;
        if (good) ++passed;
        TPtrC8 verdict(good ? _L8("PASS") : _L8("FAIL"));
        line.Format(_L8("%S | %S | Symbian=%d BearSSL=%d I/O=%d TLS=0x%04x Seconds=%d\r\nStage: "),
            &host,&verdict,error,client->TlsError(),
            client->NetworkError(),client->ProtocolVersion(),duration.Int());
        line.Append(client->StageName()); line.Append(_L8("\r\n"));
        if (out.Length())
            {
            TInt endLine=out.Find(_L8("\r\n"));
            if (endLine<0 || endLine>100) endLine=Min(out.Length(),100);
            line.Append(out.Left(endLine)); line.Append(_L8("\r\n"));
            }
        aWorkerStage.Copy(_L("write site result"));
        User::LeaveIfError(log.Write(line)); User::LeaveIfError(log.Flush());
        if (error==KErrCancel) { finalError=error;break; }
        }
    TBuf8<80> summary;
    aWorkerStage.Copy(_L("write test summary"));
    summary.Format(_L8("Summary: %d/%d passed (%d configured).\r\n"),passed,attempted,N70_TEST_SITE_COUNT);
    User::LeaveIfError(log.Write(summary)); User::LeaveIfError(log.Flush());
    CleanupStack::PopAndDestroy(client);
    CleanupStack::PopAndDestroy(response);
    CleanupStack::PopAndDestroy(&log);
    CleanupStack::PopAndDestroy(&fs);
    aWorkerStage.Copy(_L("complete"));
    User::LeaveIfError(finalError);
    if (passed!=N70_TEST_SITE_COUNT) User::Leave(KErrGeneral);
    }
TInt CTestRunner::ThreadEntry(TAny* aRunner)
    {
    CTestRunner* runner=static_cast<CTestRunner*>(aRunner);
    CTrapCleanup* cleanup=CTrapCleanup::New();
    if (!cleanup) return KErrNoMemory;
    TRAPD(error,if(runner->iMode>=2) TestHttpFeaturesL(runner->iWorkerStage,runner->iCancel,runner->iMode); else if(runner->iMode==1) TestSitesL(runner->iWorkerStage,runner->iCancel); else TestL(runner->iCancel));
    delete cleanup; return error;
    }
CTestRunner::CTestRunner() : CActive(EPriorityStandard) { CActiveScheduler::Add(this); }
CTestRunner* CTestRunner::NewL() { CTestRunner* p=new(ELeave) CTestRunner; CleanupStack::PushL(p); p->iCancel=CN70CancelToken::NewL(); CleanupStack::Pop(p); return p; }
CTestRunner::~CTestRunner() { Cancel(); iThread.Close(); delete iCancel; }
void CTestRunner::CancelWork() { if(iCancel) iCancel->Cancel(); }
void CTestRunner::StartL(TInt aMode)
    {
    if (IsActive()) return;
    iThread.Close();
    iMode=aMode; iCancel->Reset();
    iWorkerStage.Copy(_L("worker startup"));
    User::LeaveIfError(iThread.Create(_L("N70HttpsTest"),ThreadEntry,65536,65536,4194304,this));
    iThread.Logon(iStatus); SetActive(); iThread.Resume();
    }
void CTestRunner::RunL()
    {
    TBuf<160> text;
    TPtrC file=iMode==2?_L("http-features.txt"):(iMode==3?_L("timeout.txt"):(iMode==4?_L("cancel.txt"):_L("tls-tests.txt")));
    if(iMode==5)file.Set(_L("api-tests.txt"));
    if(iMode==6)file.Set(_L("tcp-timeout.txt"));
    if(iMode==7)file.Set(_L("client-tests.txt"));
    if (iThread.ExitType()==EExitPanic)
        {
        TExitCategoryName category=iThread.ExitCategory();
        text.Format(_L("%S panic %d. Stage: %S"),&category,iThread.ExitReason(),&iWorkerStage);
        }
    else if (iMode && iStatus.Int()!=KErrNone)
        text.Format(_L("Test: %d. Stage: %S. See %S"),iStatus.Int(),&iWorkerStage,&file);
    else if (iMode>=2) text.Format(_L("%S. See %S"),&iWorkerStage,&file);
    else if (iMode) text.Format(_L("Test passed. See %S"),&file);
    else text.Format(_L("HTTPS result: %d. See response.txt"),iStatus.Int());
    (new(ELeave) CAknInformationNote)->ExecuteLD(text);
    iThread.Close();
    }
