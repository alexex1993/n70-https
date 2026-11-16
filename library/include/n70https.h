#ifndef N70HTTPS_H
#define N70HTTPS_H
#include <e32base.h>
#include <es_sock.h>
#include <in_sock.h>
#include "n70httpclient.h"
extern "C" {
#include <bearssl.h>
}
struct TN70Exchange;

// Blocking API: call from a worker thread (64 KiB stack for Request APIs).
// Objects belong to that thread; only the cancel token is shared with the UI.
// Anchors must remain valid until GetL returns. FreshSeed must contain at
// least 32 cryptographically random bytes, never reused across connections.
class CN70Https : public CBase
    {
public:
    enum TStage
        {
        EIdle, EEntropy, EValidate, ETlsInit, EConnectionOpen, EConnectionStart,
        EResolverOpen, EDnsLookup, ESocketOpen, ETcpConnect,
        ETlsHandshake, EHttpRead, ERetryWait, EComplete
        };
    static CN70Https* NewL();
    ~CN70Https();
    void SetOptionsL(const TN70HttpOptions& aOptions);
    // Copied, case-insensitive replacement. Managed framing fields are reserved.
    // Custom headers are stripped permanently after a cross-origin redirect.
    void SetHeaderL(const TDesC8& aName,const TDesC8& aValue);
    void SetBearerTokenL(const TDesC8& aToken);
    void ClearHeadersL();
    void DisconnectL(); // Release own handles; never stops other apps' bearer.
    // GET, HEAD, POST, PUT, PATCH, DELETE. Decodes chunked and gzip.
    void RequestAutoL(const TDesC8& aMethod,const TDesC8& aUrl,
        const TDesC8& aContentType,const TDesC8& aBody,
        const br_x509_trust_anchor* aAnchors,size_t aCount,
        const TDesC& aStateFile,MN70HttpSink& aSink,TN70HttpResult& aResult);
    void RequestL(const TDesC8& aMethod,const TDesC8& aUrl,
        const TDesC8& aContentType,const TDesC8& aBody,
        const br_x509_trust_anchor* aAnchors,size_t aCount,
        const TDesC8& aFreshSeed,MN70HttpSink& aSink,TN70HttpResult& aResult);
    // Atomic publication: a unique temporary file is renamed only after a
    // complete 2xx response and Flush. Existing destination survives failure.
    void DownloadAutoL(const TDesC8& aUrl,const TDesC& aDestination,
        const br_x509_trust_anchor* aAnchors,size_t aCount,
        const TDesC& aStateFile,TN70HttpResult& aResult);
    // Unattended, experimental entropy fallback; NOT guaranteed cryptographic
    // randomness on N70. No keyboard setup or shared installer secret.
    void GetAutoL(const TDesC8& aHost,const TDesC8& aPath,
        const br_x509_trust_anchor* aAnchors,size_t aCount,
        const TDesC& aStateFile,TDes8& aResponse,
        TBool aPromptForAccessPoint=EFalse,TBool aHeadersOnly=EFalse);
    void GetWithDeviceEntropyL(const TDesC8& aHost, const TDesC8& aPath,
        const br_x509_trust_anchor* aAnchors, size_t aCount,
        const TDesC& aStateFile, TDes8& aResponse,
        TBool aPromptForAccessPoint=EFalse, TBool aHeadersOnly=EFalse);
    // Automatic persistent seed management. State is application-specific;
    // provisioning file is needed only before the first successful rotation.
    void GetWithStoredEntropyL(const TDesC8& aHost, const TDesC8& aPath,
        const br_x509_trust_anchor* aAnchors, size_t aCount,
        const TDesC& aStateFile, const TDesC& aProvisioningFile,
        TDes8& aResponse, TBool aPromptForAccessPoint=EFalse,
        TBool aHeadersOnly=EFalse);
    // Returns the raw HTTP response, including headers. Leaves KErrOverflow
    // rather than silently truncating a response. Only port 443, DNS names,
    // ASCII origin-form paths. No redirects, decompression or chunk decoding.
    void GetL(const TDesC8& aHost, const TDesC8& aPath,
        const br_x509_trust_anchor* aAnchors, size_t aCount,
        const TDesC8& aFreshSeed, TDes8& aResponse,
        TBool aPromptForAccessPoint=EFalse, TBool aHeadersOnly=EFalse);
    TInt TlsError() const;
    TPtrC8 StageName() const;
    TInt NetworkError() const;
    TUint ProtocolVersion() const;
private:
    CN70Https();
    void ConstructL();
    TInt Wait(TRequestStatus& aStatus, TInt aTarget);
    void BeginL();
    static void Finish(TAny* aSelf);
    TInt ControlError();
    static TInt ControlCallback(TAny* aSelf);
    void CheckL();
    void OpenTlsL(const TDesC8& aHost,TUint aPort,const br_x509_trust_anchor* aAnchors,
        size_t aCount,const TDesC8& aSeed,TBool aPrompt);
    void EnsureConnectionL(TBool aPrompt);
    void ConnectSocketL(const TDesC8&,TUint,TBool);
    void CloseTransport();
    void ResetConnection();
    TBool ReuseConnection(const TDesC8&,TUint,const br_x509_trust_anchor*,size_t);
    void SaveConnectionL(const TDesC8&,TUint,const br_x509_trust_anchor*,size_t);
    void DelayL(TInt aMilliseconds);
    static TInt NetStart(TAny*);
    static TInt NetResolve(TAny*,TInt,uint32_t*);
    static TInt NetConnect(TAny*,uint32_t);
    static void NetReset(TAny*);
    static TInt NetControl(TAny*);
    void RawGetL(const TDesC8& aHost,const TDesC8& aPath,
        const br_x509_trust_anchor*,size_t,const TDesC8&,TDes8&,TBool,TBool);
    void WriteTlsL(const TDesC8& aData);
    TInt ReadTlsL(TDes8& aData);
    void RequestInternalL(const TDesC8&,const TDesC8&,const TDesC8&,const TDesC8&,
        const br_x509_trust_anchor*,size_t,const TDesC8&,MN70HttpSink&,TN70HttpResult&);
    void ExchangeL(const TDesC8&,const TDesC8&,const TDesC8&,const TDesC8&,
        const br_x509_trust_anchor*,size_t,const TDesC8&,MN70HttpSink&,TN70HttpResult&,TN70Exchange&);
    static int Send(void* aContext, const unsigned char* aData, size_t aLen);
    static int Receive(void* aContext, unsigned char* aData, size_t aLen);
    RSocketServ iServer;
    RConnection iConnection;
    RSocket iSocket;
    RHostResolver iResolver;
    RTimer iTimer;
    RTimer iDeadlineTimer,iIoTimer;
    TRequestStatus iDeadlineStatus;
    TInt iDeadlineRemaining;
    TBool iDeadlineArmed;
    HBufC8* iHeaders;
    RTimer iIdleTimer;
    TRequestStatus iIdleStatus;
    TBool iIdleArmed,iTlsReady,iOperationComplete;
    TBuf8<253> iPeerHost;
    TUint iPeerPort;
    TBuf8<32> iPeerTrust;
    TInt iNewConnections,iReusedConnections,iAddressAttempts;
    br_ssl_client_context iClient;
    br_x509_minimal_context iX509;
    br_sslio_context iIo;
    unsigned char iBuffer[BR_SSL_BUFSIZE_BIDI];
    TInt iNetError;
    TStage iStage;
    TBool iConnectionStarted;
    TN70HttpOptions iOptions;
    TUint32 iStartTick;
    TInt iTickUs;
    TBool iRunning;
    };
#endif
