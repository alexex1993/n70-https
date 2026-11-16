#ifndef N70_HTTPCLIENT_H
#define N70_HTTPCLIENT_H
#include <e32base.h>
#include <stdint.h>
// First matching field, case-insensitive; borrowed view, empty if absent.
TPtrC8 N70HeaderValue(const TDesC8& aHeaders,const TDesC8& aName);
// Own this token until the worker has finished. Cancel is safe from another
// thread in this process. Reset only between requests, never during a request.
class CN70CancelToken : public CBase
    {
public:
    static CN70CancelToken* NewL();
    ~CN70CancelToken();
    void Cancel();
    void Reset();
    TBool IsCancelled() const;
private:
    CN70CancelToken();
    mutable RMutex iLock;
    TBool iCancelled;
    };
class TN70HttpOptions
    {
public:
    TN70HttpOptions();
    TInt iTotalTimeoutMs; // Entire call incl. redirects; default 120000.
    TInt iIoTimeoutMs;    // Each socket/DNS/start wait; default 30000.
    TUint iMaxBodyBytes;  // Decoded body, default 8 MiB.
    TUint iMaxWireBytes;  // Body before gzip decoding, default 8 MiB.
    TInt iMaxRedirects;   // Default 5, zero disables following.
    TInt iMaxAddresses;   // IPv4 DNS answers per lookup; default 8 (up to 32).
    TBool iKeepAlive;     // Reuse one fully consumed same-origin TLS connection.
    TInt iKeepAliveIdleMs;// Default 15000, up to 300000.
    TInt iMaxRetries;     // Additional attempts for the entire call; default 2.
    TInt iRetryDelayMs;   // Exponential backoff base; default 500.
    TInt iMaxRetryDelayMs;// Longer Retry-After disables auto-retry; default 10000.
    TBool iRetryIdempotent; // Also PUT/DELETE; default false. Never POST/PATCH.
    CN70CancelToken* iCancel; // Optional, borrowed.
    TBool iPromptForAccessPoint;
    };
class TN70HttpResult
    {
public:
    TN70HttpResult();
    TInt iStatus;
    TInt iRedirects;
    uint64_t iBodyBytes;
    uint64_t iWireBodyBytes;
    TInt iAttempts,iRetries,iNewConnections,iReusedConnections,iAddressAttempts;
    TInt iRetryAfterMs;   // -1 absent/invalid; may exceed auto-retry cap.
    TBuf8<4096> iFinalUrl;
    };
class MN70HttpSink
    {
public:
    // Only final response headers/body are delivered. Descriptors are borrowed
    // for the duration of the callback. Callbacks execute in the worker thread.
    virtual void HeadersL(TInt aStatus,const TDesC8& aHeaders)=0;
    virtual void DataL(const TDesC8& aData)=0;
protected:
    virtual ~MN70HttpSink() {}
    };
#endif
