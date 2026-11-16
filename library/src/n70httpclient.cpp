#include "n70httpclient.h"
#include "n70http.h"
TPtrC8 N70HeaderValue(const TDesC8& headers,const TDesC8& name)
    {
    const char* value=NULL;size_t len=0;
    if(!n70_header_value(reinterpret_cast<const char*>(headers.Ptr()),headers.Length(),
        reinterpret_cast<const char*>(name.Ptr()),name.Length(),&value,&len))return _L8("");
    return TPtrC8(reinterpret_cast<const TUint8*>(value),len);
    }
CN70CancelToken::CN70CancelToken() : iCancelled(EFalse) {}
CN70CancelToken* CN70CancelToken::NewL()
    {
    CN70CancelToken* p=new(ELeave) CN70CancelToken;
    CleanupStack::PushL(p); User::LeaveIfError(p->iLock.CreateLocal());
    CleanupStack::Pop(p); return p;
    }
CN70CancelToken::~CN70CancelToken() { iLock.Close(); }
void CN70CancelToken::Cancel() { iLock.Wait(); iCancelled=ETrue; iLock.Signal(); }
void CN70CancelToken::Reset() { iLock.Wait(); iCancelled=EFalse; iLock.Signal(); }
TBool CN70CancelToken::IsCancelled() const
    { iLock.Wait();TBool value=iCancelled;iLock.Signal();return value; }
TN70HttpOptions::TN70HttpOptions() : iTotalTimeoutMs(120000),iIoTimeoutMs(30000),
    iMaxBodyBytes(8*1024*1024),iMaxWireBytes(8*1024*1024),iMaxRedirects(5),iMaxAddresses(8),
    iKeepAlive(ETrue),iKeepAliveIdleMs(15000),iMaxRetries(2),iRetryDelayMs(500),iMaxRetryDelayMs(10000),
    iRetryIdempotent(EFalse),iCancel(NULL),iPromptForAccessPoint(EFalse) {}
TN70HttpResult::TN70HttpResult() : iStatus(0),iRedirects(0),iBodyBytes(0),iWireBodyBytes(0),
    iAttempts(0),iRetries(0),iNewConnections(0),iReusedConnections(0),iAddressAttempts(0),iRetryAfterMs(-1) {}
