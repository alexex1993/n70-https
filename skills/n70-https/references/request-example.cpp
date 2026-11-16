#include "n70https.h"
#include "testanchor.h"

class TBoundedJsonSink : public MN70HttpSink
    {
public:
    explicit TBoundedJsonSink(TDes8& aBody) : iBody(aBody),iStatus(0) {}
    void HeadersL(TInt aStatus,const TDesC8&) { iStatus=aStatus; iBody.Zero(); }
    void DataL(const TDesC8& aData)
        {
        if (aData.Length()>iBody.MaxLength()-iBody.Length()) User::Leave(KErrOverflow);
        iBody.Append(aData);
        }
    TInt Status() const { return iStatus; }
private:
    TDes8& iBody;
    TInt iStatus;
    };

// Run inside the application's worker thread (64 KiB stack, CTrapCleanup).
// aBody is caller-owned and must remain alive. Never return a worker-heap
// descriptor to a UI that will read it after that worker has exited.
void PostJsonL(TDes8& aBody,CN70CancelToken& aCancel,TN70HttpResult& aResult)
    {
    CN70Https* client=CN70Https::NewL();
    CleanupStack::PushL(client);
    TN70HttpOptions options;
    options.iTotalTimeoutMs=60000;
    options.iIoTimeoutMs=15000;
    options.iMaxBodyBytes=65536;
    options.iMaxWireBytes=131072;
    options.iCancel=&aCancel;
    client->SetOptionsL(options);
    client->SetHeaderL(_L8("Accept"),_L8("application/json"));
    // For a real authenticated service: client->SetBearerTokenL(actualToken);
    br_x509_trust_anchor anchors[TEST_ANCHOR_COUNT];
    InitTestAnchors(anchors);
    TBoundedJsonSink sink(aBody);
    aBody.Zero();
    client->RequestAutoL(_L8("POST"),_L8("https://httpbingo.org/post"),
        _L8("application/json"),_L8("{\"message\":\"Hello from N70\"}"),
        anchors,TEST_ANCHOR_COUNT,_L("C:\\system\\apps\\MyApp\\rng-device-v1.bin"),
        sink,aResult);
    // Inspect aResult.iStatus for 2xx/401/429/etc.; then parse aBody as UTF-8.
    // A leave indicates transport/framing/decoding failure; discard partial body.
    CleanupStack::PopAndDestroy(client);
    }
