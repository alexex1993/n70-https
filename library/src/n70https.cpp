#include "n70https.h"
#include "n70entropy.h"
#include <commdbconnpref.h>
#include <hal.h>
#include <hal_data.h>

CN70Https::CN70Https() : iNetError(KErrNone), iStage(EIdle), iConnectionStarted(EFalse), iStartTick(0), iTickUs(0), iRunning(EFalse) {}
CN70Https* CN70Https::NewL()
    {
    CN70Https* self = new(ELeave) CN70Https;
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
    }
void CN70Https::ConstructL()
    {
    User::LeaveIfError(iServer.Connect());
    User::LeaveIfError(iTimer.CreateLocal());
    User::LeaveIfError(iDeadlineTimer.CreateLocal());
    User::LeaveIfError(iIoTimer.CreateLocal());
    User::LeaveIfError(iIdleTimer.CreateLocal());
    User::LeaveIfError(HAL::Get(HALData::ESystemTickPeriod,iTickUs));
    if (iTickUs<=0) User::Leave(KErrNotSupported);
    }
CN70Https::~CN70Https()
    {
    CloseTransport();iIdleTimer.Close();iConnection.Close();
    iDeadlineTimer.Close();iIoTimer.Close();
    if(iHeaders){iHeaders->Des().FillZ();delete iHeaders;}
    iTimer.Close(); iServer.Close();
    }
TInt CN70Https::TlsError() const
    { return (iStage==EIdle || iStage==EEntropy || iStage==EValidate) ? 0 :
        br_ssl_engine_last_error(&iClient.eng); }
TInt CN70Https::NetworkError() const { return iNetError; }
TUint CN70Https::ProtocolVersion() const
    { return (iStage==EIdle || iStage==EEntropy || iStage==EValidate) ? 0 :
        br_ssl_engine_get_version(&iClient.eng); }
TPtrC8 CN70Https::StageName() const
    {
    switch (iStage)
        {
        case EEntropy: return _L8("entropy-state");
        case EValidate: return _L8("validate");
        case ETlsInit: return _L8("tls-init");
        case EConnectionOpen: return _L8("connection-open");
        case EConnectionStart: return _L8("access-point-start");
        case EResolverOpen: return _L8("dns-open");
        case EDnsLookup: return _L8("dns-lookup");
        case ESocketOpen: return _L8("socket-open");
        case ETcpConnect: return _L8("tcp-connect");
        case ETlsHandshake: return _L8("tls-handshake/http-send");
        case EHttpRead: return _L8("http-read");
        case ERetryWait: return _L8("retry-backoff");
        case EComplete: return _L8("complete");
        default: return _L8("idle");
        }
    }

static void WipeTlsSeed(TAny* aSeed)
    { static_cast<TDes8*>(aSeed)->FillZ(); }
void CN70Https::GetAutoL(const TDesC8& aHost,const TDesC8& aPath,
    const br_x509_trust_anchor* aAnchors,size_t aCount,const TDesC& aStateFile,
    TDes8& aResponse,TBool aPromptForAccessPoint,TBool aHeadersOnly)
    {
    BeginL();
    iStage=EEntropy; iNetError=KErrNone; aResponse.Zero();
    TBuf8<32> seed;
    CleanupStack::PushL(TCleanupItem(WipeTlsSeed,static_cast<TDes8*>(&seed)));
    TN70Entropy::NextAutomaticSeedL(aStateFile,seed);
    CheckL();
    RawGetL(aHost,aPath,aAnchors,aCount,seed,aResponse,aPromptForAccessPoint,aHeadersOnly);
    CheckL();
    CleanupStack::PopAndDestroy();
    CleanupStack::PopAndDestroy(); // operation
    }
void CN70Https::GetWithDeviceEntropyL(const TDesC8& aHost,const TDesC8& aPath,
    const br_x509_trust_anchor* aAnchors,size_t aCount,const TDesC& aStateFile,
    TDes8& aResponse,TBool aPromptForAccessPoint,TBool aHeadersOnly)
    {
    BeginL();
    iStage=EEntropy; iNetError=KErrNone; aResponse.Zero();
    TBuf8<32> seed;
    CleanupStack::PushL(TCleanupItem(WipeTlsSeed,static_cast<TDes8*>(&seed)));
    TN70Entropy::NextDeviceSeedL(aStateFile,seed);
    CheckL();
    RawGetL(aHost,aPath,aAnchors,aCount,seed,aResponse,aPromptForAccessPoint,aHeadersOnly);
    CheckL();
    CleanupStack::PopAndDestroy();
    CleanupStack::PopAndDestroy(); // operation
    }
void CN70Https::GetWithStoredEntropyL(const TDesC8& aHost, const TDesC8& aPath,
    const br_x509_trust_anchor* aAnchors, size_t aCount,
    const TDesC& aStateFile, const TDesC& aProvisioningFile,
    TDes8& aResponse, TBool aPromptForAccessPoint, TBool aHeadersOnly)
    {
    BeginL();
    iStage=EEntropy;
    iNetError=KErrNone;
    aResponse.Zero();
    TBuf8<32> seed;
    CleanupStack::PushL(TCleanupItem(WipeTlsSeed,static_cast<TDes8*>(&seed)));
    TN70Entropy::NextSeedL(aStateFile,aProvisioningFile,seed);
    CheckL();
    RawGetL(aHost,aPath,aAnchors,aCount,seed,aResponse,aPromptForAccessPoint,aHeadersOnly);
    CheckL();
    CleanupStack::PopAndDestroy();
    CleanupStack::PopAndDestroy(); // operation
    }

int CN70Https::Send(void* aContext, const unsigned char* aData, size_t aLen)
    {
    CN70Https* self = static_cast<CN70Https*>(aContext);
    TPtrC8 data(aData, aLen);
    TRequestStatus status;
    self->iSocket.Write(data, status);
    self->iNetError = self->Wait(status, EFalse);
    return self->iNetError ? -1 : (int)aLen;
    }
int CN70Https::Receive(void* aContext, unsigned char* aData, size_t aLen)
    {
    CN70Https* self = static_cast<CN70Https*>(aContext);
    TPtr8 data(aData, 0, aLen);
    TSockXfrLength length;
    TRequestStatus status;
    self->iSocket.RecvOneOrMore(data, 0, status, length);
    self->iNetError = self->Wait(status, EFalse);
    if(!self->iNetError&&!data.Length())self->iNetError=KErrEof;
    return self->iNetError ? -1 : data.Length();
    }
void CN70Https::RawGetL(const TDesC8& aHost, const TDesC8& aPath,
    const br_x509_trust_anchor* aAnchors, size_t aCount,
    const TDesC8& aFreshSeed, TDes8& aResponse, TBool aPromptForAccessPoint,
    TBool aHeadersOnly)
    {
    iStage=EValidate;
    iNetError=KErrNone;
    if (!aAnchors || !aCount || aFreshSeed.Length() < 32 ||
        !aHost.Length() || aHost.Length() > 253 ||
        !aPath.Length() || aPath[0] != '/' || aPath.Length() > 2048)
        User::Leave(KErrArgument);
    TInt n;
    for (n=0; n<aHost.Length(); ++n)
        {
        TUint8 c=aHost[n];
        if (!((c>='a' && c<='z') || (c>='A' && c<='Z') ||
            (c>='0' && c<='9') || c=='.' || c=='-')) User::Leave(KErrArgument);
        }
    for (n=0; n<aPath.Length(); ++n)
        if (aPath[n] <= 32 || aPath[n] >= 127) User::Leave(KErrArgument);
    aResponse.Zero(); iNetError=KErrNone;
    CloseTransport();
    OpenTlsL(aHost,443,aAnchors,aCount,aFreshSeed,aPromptForAccessPoint);
    HBufC8* request=HBufC8::NewLC(aPath.Length()+aHost.Length()+128);
    TPtr8 req=request->Des();
    req.Append(_L8("GET ")); req.Append(aPath);
    req.Append(_L8(" HTTP/1.0\r\nHost: ")); req.Append(aHost);
    req.Append(_L8("\r\nConnection: close\r\nAccept-Encoding: identity\r\n\r\n"));
    iStage=ETlsHandshake;
    int result=br_sslio_write_all(&iIo, req.Ptr(), req.Length());
    CleanupStack::PopAndDestroy(request);
    if (result < 0 || br_sslio_flush(&iIo) < 0)
        User::Leave(iNetError ? iNetError : KErrGeneral);
    if (ProtocolVersion()!=BR_TLS12) User::Leave(KErrNotSupported);
    unsigned char chunk[1024];
    iStage=EHttpRead;
    for (;;)
        {
        int got=br_sslio_read(&iIo, chunk, sizeof chunk);
        if (got < 0)
            {
            if (br_ssl_engine_current_state(&iClient.eng)==BR_SSL_CLOSED && !TlsError()) break;
            User::Leave(iNetError ? iNetError : KErrCorrupt);
            }
        TInt room=aResponse.MaxLength()-aResponse.Length();
        if (got>room && !aHeadersOnly) User::Leave(KErrOverflow);
        aResponse.Append(chunk,got>room ? room : got);
        if (aHeadersOnly)
            {
            TInt end=aResponse.Find(_L8("\r\n\r\n"));
            if (end!=KErrNotFound) { aResponse.SetLength(end+4); break; }
            if (got>=room) User::Leave(KErrOverflow);
            }
        }
    if (aHeadersOnly && aResponse.Find(_L8("\r\n\r\n"))==KErrNotFound)
        User::Leave(KErrCorrupt);
    CloseTransport();
    iOperationComplete=ETrue;
    iStage=EComplete;
    }

void CN70Https::OpenTlsL(const TDesC8& aHost,TUint aPort,const br_x509_trust_anchor* aAnchors,
    size_t aCount,const TDesC8& aSeed,TBool aPrompt)
    {
    CheckL();CloseTransport();
    iStage=ETlsInit;
    br_ssl_client_init_full(&iClient, &iX509, aAnchors, aCount);
    br_ssl_engine_set_versions(&iClient.eng, BR_TLS12, BR_TLS12);
    // Cached TLS must not renegotiate using anchors borrowed from an old call.
    br_ssl_engine_add_flags(&iClient.eng,BR_OPT_NO_RENEGOTIATION);
    br_ssl_engine_set_buffer(&iClient.eng, iBuffer, sizeof iBuffer, 1);
    br_ssl_engine_inject_entropy(&iClient.eng, aSeed.Ptr(), aSeed.Length());
    // BearSSL day zero is 0000-01-01; Symbian TTime uses the same epoch.
    TTime now; now.UniversalTime();
    TDateTime date=now.DateTime();
    TTime midnight(TDateTime(date.Year(), date.Month(), date.Day(),0,0,0,0));
    TTime epoch(TInt64(0));
    TTimeIntervalDays days=midnight.DaysFrom(epoch);
    TTimeIntervalSeconds seconds;
    User::LeaveIfError(now.SecondsFrom(midnight, seconds));
    br_x509_minimal_set_time(&iX509, days.Int(), seconds.Int());
    TBuf8<254> host; host.Copy(aHost); host.Append(TChar(0));
    if (!br_ssl_client_reset(&iClient, (const char*)host.Ptr(), 0))
        User::Leave(KErrGeneral);
    ConnectSocketL(aHost,aPort,aPrompt);
    br_sslio_init(&iIo, &iClient.eng, Receive, this, Send, this);
    }
