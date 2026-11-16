#include "n70https.h"
#include "n70entropy.h"
#include "n70http.h"
#include "n70timer.h"
#include "n70headers.h"
#include "n70gzip.h"
#include "n70policy.h"
#include <f32file.h>
#include <string.h>

void CN70Https::SetHeaderL(const TDesC8& name,const TDesC8& value)
    {
    if(iRunning)User::Leave(KErrInUse);
    HBufC8* next=HBufC8::NewLC(N70_REQUEST_HEADERS_MAX);TPtr8 out=next->Des();
    if(iHeaders)out.Copy(*iHeaders);size_t len=out.Length();
    TInt e=n70_headers_set(reinterpret_cast<char*>(const_cast<TUint8*>(out.Ptr())),&len,out.MaxLength(),
        reinterpret_cast<const char*>(name.Ptr()),name.Length(),reinterpret_cast<const char*>(value.Ptr()),value.Length());
    if(e){out.SetLength(out.MaxLength());out.FillZ();User::Leave(e==N70_LIMIT?KErrOverflow:KErrArgument);}
    Mem::FillZ(const_cast<TUint8*>(out.Ptr())+len,out.MaxLength()-len);
    out.SetLength(len);if(iHeaders){iHeaders->Des().FillZ();delete iHeaders;}
    CleanupStack::Pop(next);iHeaders=next;
    }
void CN70Https::SetBearerTokenL(const TDesC8& token)
    {
    if(!n70_bearer_valid(reinterpret_cast<const char*>(token.Ptr()),token.Length()))User::Leave(KErrArgument);
    HBufC8* value=HBufC8::NewLC(token.Length()+7);TPtr8 out=value->Des();out.Copy(_L8("Bearer "));out.Append(token);
    TRAPD(error,SetHeaderL(_L8("Authorization"),out));out.FillZ();CleanupStack::PopAndDestroy(value);User::LeaveIfError(error);
    }
void CN70Https::ClearHeadersL()
    { if(iRunning)User::Leave(KErrInUse);if(iHeaders){iHeaders->Des().FillZ();delete iHeaders;iHeaders=NULL;} }

void CN70Https::SetOptionsL(const TN70HttpOptions& o)
    {
    if(iRunning)User::Leave(KErrInUse);
    if(o.iTotalTimeoutMs<=0||o.iTotalTimeoutMs>86400000||o.iIoTimeoutMs<=0||
       o.iIoTimeoutMs>86400000||o.iMaxRedirects<0||o.iMaxRedirects>20||
       o.iMaxAddresses<1||o.iMaxAddresses>32||o.iKeepAliveIdleMs<1||o.iKeepAliveIdleMs>300000||
       o.iMaxRetries<0||o.iMaxRetries>10||o.iRetryDelayMs<0||o.iRetryDelayMs>600000||
       o.iMaxRetryDelayMs<o.iRetryDelayMs||o.iMaxRetryDelayMs>600000)User::Leave(KErrArgument);
    if(!o.iKeepAlive||o.iKeepAliveIdleMs!=iOptions.iKeepAliveIdleMs)CloseTransport();
    iOptions=o;
    }
void CN70Https::BeginL()
    {
    if(iRunning)User::Leave(KErrInUse);
    iRunning=ETrue;iStartTick=User::TickCount();iNetError=KErrNone;iStage=EValidate;iOperationComplete=EFalse;
    iNewConnections=iReusedConnections=iAddressAttempts=0;
    CleanupStack::PushL(TCleanupItem(Finish,this));
    iDeadlineRemaining=iOptions.iTotalTimeoutMs;ArmTimer(iDeadlineTimer,iDeadlineStatus,iDeadlineRemaining);
    iDeadlineArmed=ETrue;CheckL();
    }
void CN70Https::Finish(TAny* p)
    {
    CN70Https* s=static_cast<CN70Https*>(p);
    if(s->iDeadlineArmed){s->iDeadlineTimer.Cancel();User::WaitForRequest(s->iDeadlineStatus);s->iDeadlineArmed=EFalse;}
    s->iResolver.Close();if(!s->iOperationComplete||!s->iTlsReady)s->CloseTransport();s->iRunning=EFalse;
    }
void CN70Https::GetL(const TDesC8& host,const TDesC8& path,
    const br_x509_trust_anchor* roots,size_t count,const TDesC8& seed,
    TDes8& response,TBool prompt,TBool headersOnly)
    {
    BeginL();RawGetL(host,path,roots,count,seed,response,prompt,headersOnly);
    CheckL();CleanupStack::PopAndDestroy();
    }
void CN70Https::WriteTlsL(const TDesC8& data)
    {
    CheckL();
    if(br_sslio_write_all(&iIo,data.Ptr(),data.Length())<0||br_sslio_flush(&iIo)<0)
        User::Leave(iNetError?iNetError:KErrGeneral);
    CheckL();if(ProtocolVersion()!=BR_TLS12)User::Leave(KErrNotSupported);
    }
TInt CN70Https::ReadTlsL(TDes8& data)
    {
    CheckL();data.Zero();
    TInt n=br_sslio_read(&iIo,const_cast<TUint8*>(data.Ptr()),data.MaxLength());CheckL();
    if(n<0){
        if(iNetError)User::Leave(iNetError);
        if(br_ssl_engine_current_state(&iClient.eng)==BR_SSL_CLOSED&&!TlsError())return 0;
        User::Leave(KErrCorrupt);
    }
    if(!n)User::Leave(KErrCorrupt);data.SetLength(n);return n;
    }
struct TRequestSecrets { TBuf8<32> seed;br_hmac_drbg_context rng; };
static void ClearRequest(TAny* p)
    {
    TRequestSecrets* s=static_cast<TRequestSecrets*>(p);s->seed.FillZ();
    volatile TUint8* b=reinterpret_cast<volatile TUint8*>(&s->rng);
    for(TUint i=0;i<sizeof s->rng;++i)b[i]=0;
    }
static void ClearSeed(TAny* p) { static_cast<TDes8*>(p)->FillZ(); }
void CN70Https::RequestAutoL(const TDesC8& method,const TDesC8& url,
    const TDesC8& type,const TDesC8& body,const br_x509_trust_anchor* roots,
    size_t count,const TDesC& state,MN70HttpSink& sink,TN70HttpResult& result)
    {
    result=TN70HttpResult();BeginL();iStage=EEntropy;
    TBuf8<32> seed;CleanupStack::PushL(TCleanupItem(ClearSeed,static_cast<TDes8*>(&seed)));
    TN70Entropy::NextAutomaticSeedL(state,seed);CheckL();
    RequestInternalL(method,url,type,body,roots,count,seed,sink,result);
    CleanupStack::PopAndDestroy();CleanupStack::PopAndDestroy();
    }
void CN70Https::RequestL(const TDesC8& method,const TDesC8& url,
    const TDesC8& type,const TDesC8& body,const br_x509_trust_anchor* roots,
    size_t count,const TDesC8& seed,MN70HttpSink& sink,TN70HttpResult& result)
    {
    result=TN70HttpResult();BeginL();RequestInternalL(method,url,type,body,roots,count,seed,sink,result);
    CleanupStack::PopAndDestroy();
    }
struct TBodySink { MN70HttpSink* sink;CN70Https* client;TInt (*control)(TAny*);n70_gzip* gzip;TInt error;uint64_t bytes,limit;TBool compressed; };
static int Output(void* ctx,const unsigned char* data,size_t len)
    {
    TBodySink* s=static_cast<TBodySink*>(ctx);
    s->error=s->control(s->client);if(s->error)return 1;
    if(len>s->limit-s->bytes){s->error=KErrOverflow;return 1;}
    TRAP(s->error,s->sink->DataL(TPtrC8(data,len)));
    if(s->error)return 1;s->bytes+=len;return 0;
    }
static int Deliver(void* ctx,const unsigned char* data,size_t len)
    {
    TBodySink* s=static_cast<TBodySink*>(ctx);
    if(!s->compressed)return Output(ctx,data,len);
    TInt e=n70_gzip_feed(s->gzip,data,len,Output,ctx);
    if(e<0&&e!=N70_SINK)s->error=e==N70_LIMIT?KErrOverflow:(e==N70_MEMORY?KErrNoMemory:KErrCorrupt);
    return e<0?1:0;
    }
static void* GzipAlloc(void*,unsigned n,unsigned size)
    {if(size&&n>0x7fffffffU/size)return NULL;void* p=User::Alloc(n*size);if(p)Mem::FillZ(p,n*size);return p;}
static void GzipFree(void*,void* p){User::Free(p);}
static void CloseGzip(TAny* p){n70_gzip_close(static_cast<n70_gzip*>(p));User::Free(p);}
static TInt ParseError(TInt error)
{ return error==N70_LIMIT?KErrOverflow:(error==N70_ENCODING?KErrNotSupported:KErrCorrupt); }
static TBool IsRedirect(TInt status)
{ return status==301||status==302||status==303||status==307||status==308; }
struct TN70Exchange : public CBase
    {
    TBool sendHeaders,delivered;
    TInt action,retryBudget,retryDelay;
    TBuf8<N70_URL_MAX+1> location;
    };
static const char* MethodText(const TDesC8& method,TDes8& storage)
    {storage.Copy(method);storage.Append(TChar(0));return reinterpret_cast<const char*>(storage.Ptr());}
static TInt RetryAfter(const TDesC8& headers)
    {
    TPtrC8 value=N70HeaderValue(headers,_L8("Retry-After"));if(!value.Length())return -1;
    TTime now;now.UniversalTime();TDateTime d=now.DateTime();
    uint64_t seconds=n70_epoch_seconds(d.Year(),d.Month()+1,d.Day()+1,d.Hour(),d.Minute(),d.Second());
    return n70_retry_after(reinterpret_cast<const char*>(value.Ptr()),value.Length(),seconds,d.Year());
    }
void CN70Https::RequestInternalL(const TDesC8& methodIn,const TDesC8& url,
    const TDesC8& type,const TDesC8& bodyIn,const br_x509_trust_anchor* roots,
    size_t count,const TDesC8& seed,MN70HttpSink& sink,TN70HttpResult& result)
    {
    iStage=EValidate;result=TN70HttpResult();TBuf8<8> methodString;
    if(methodIn.Length()>6||methodIn.Locate(TChar(0))!=KErrNotFound)User::Leave(KErrArgument);
    const char* originalMethod=MethodText(methodIn,methodString);
    if(!roots||!count||count>256||seed.Length()<32||url.Length()>N70_URL_MAX||
       url.Locate(TChar(0))!=KErrNotFound||type.Length()>256||
       !n70_method_valid(originalMethod)||(!n70_method_has_body(originalMethod)&&bodyIn.Length()))User::Leave(KErrArgument);
    for(TInt j=0;j<type.Length();++j)if(type[j]<32||type[j]>=127)User::Leave(KErrArgument);
    TBuf8<N70_URL_MAX+1> initial;initial.Copy(url);initial.Append(TChar(0));
    n70_url current;if(!n70_url_parse(reinterpret_cast<const char*>(initial.Ptr()),&current))User::Leave(KErrArgument);
    TBuf8<6> method;method.Copy(methodIn);TPtrC8 body(bodyIn);
    TRequestSecrets secrets;CleanupStack::PushL(TCleanupItem(ClearRequest,&secrets));
    br_hmac_drbg_init(&secrets.rng,&br_sha256_vtable,seed.Ptr(),seed.Length());
    TN70Exchange* exchange=new(ELeave) TN70Exchange;CleanupStack::PushL(exchange);exchange->sendHeaders=ETrue;
    TInt hop=0;
    for(;;){
        CheckL();iNetError=0;exchange->action=0;exchange->delivered=EFalse;exchange->location.Zero();
        exchange->retryBudget=iOptions.iMaxRetries-result.iRetries;
        char formatted[N70_URL_MAX+1];if(!n70_url_format(&current,formatted,sizeof formatted))User::Leave(KErrOverflow);
        result.iFinalUrl.Copy(TPtrC8(reinterpret_cast<const TUint8*>(formatted)));
        result.iStatus=0;result.iBodyBytes=result.iWireBodyBytes=0;result.iRetryAfterMs=-1;result.iRedirects=hop;++result.iAttempts;
        secrets.seed.SetLength(32);br_hmac_drbg_generate(&secrets.rng,const_cast<TUint8*>(secrets.seed.Ptr()),32);
        TRAPD(error,ExchangeL(method,result.iFinalUrl,type,body,roots,count,secrets.seed,sink,result,*exchange));
        result.iNewConnections=iNewConnections;result.iReusedConnections=iReusedConnections;result.iAddressAttempts=iAddressAttempts;
        const char* verb=MethodText(method,methodString);
        if(error){
            TInt control=ControlError(),transport=iNetError?iNetError:error;
            if(TlsError()&&TlsError()!=BR_ERR_IO)transport=KErrCorrupt;
            if(!iNetError&&TlsError()==BR_ERR_IO)transport=KErrEof;
            TBool again=exchange->retryBudget>0&&n70_retry_allowed(verb,iOptions.iRetryIdempotent,exchange->delivered,transport,control);
            CloseTransport();
            if(n70_bearer_error(transport))ResetConnection();
            if(!again)User::Leave(control?control:error);
            exchange->retryDelay=n70_retry_delay(iOptions.iRetryDelayMs,iOptions.iMaxRetryDelayMs,result.iRetries,-1,User::FastCounter());
            exchange->action=2;
        }
        if(exchange->action==2){DelayL(exchange->retryDelay);++result.iRetries;continue;}
        if(exchange->action==1){
            if(hop>=iOptions.iMaxRedirects)User::Leave(KErrOverflow);
            n70_url target;TBuf8<N70_URL_MAX+1> location;location.Copy(exchange->location);location.Append(TChar(0));
            if(!n70_url_resolve(&current,reinterpret_cast<const char*>(location.Ptr()),&target))User::Leave(KErrNotSupported);
            if(result.iStatus==303||((result.iStatus==301||result.iStatus==302)&&method==_L8("POST"))){
                if(method!=_L8("HEAD"))method.Copy(_L8("GET"));body.Set(KNullDesC8);
            }
            if(method!=_L8("GET")&&method!=_L8("HEAD")&&!n70_same_origin(&current,&target))User::Leave(KErrAccessDenied);
            exchange->sendHeaders=n70_headers_forward(exchange->sendHeaders,n70_same_origin(&current,&target));
            current=target;++hop;if(exchange->retryDelay>0)DelayL(exchange->retryDelay);continue;
        }
        break;
    }
    CheckL();iOperationComplete=ETrue;iStage=EComplete;
    CleanupStack::PopAndDestroy(exchange);CleanupStack::PopAndDestroy();
    }

void CN70Https::ExchangeL(const TDesC8& method,const TDesC8& url,const TDesC8& type,
    const TDesC8& body,const br_x509_trust_anchor* roots,size_t count,const TDesC8& seed,
    MN70HttpSink& sink,TN70HttpResult& result,TN70Exchange& exchange)
    {
    TBuf8<N70_URL_MAX+1> initial;initial.Copy(url);initial.Append(TChar(0));n70_url current;
    if(!n70_url_parse(reinterpret_cast<const char*>(initial.Ptr()),&current))User::Leave(KErrArgument);
    TPtrC8 host(reinterpret_cast<const TUint8*>(current.host));
    if(!ReuseConnection(host,current.port,roots,count))OpenTlsL(host,current.port,roots,count,seed,iOptions.iPromptForAccessPoint);
    iTlsReady=EFalse;iNetError=0;
    HBufC8* request=HBufC8::NewLC(4096+(iHeaders?iHeaders->Length():0));TPtr8 req=request->Des();
    req.Append(method);req.Append(_L8(" "));req.Append(TPtrC8(reinterpret_cast<const TUint8*>(current.target)));
    req.Append(_L8(" HTTP/1.1\r\nHost: "));req.Append(host);
    if(current.port!=443){req.Append(TChar(':'));req.AppendNum(current.port);}req.Append(_L8("\r\n"));
    const char* extra=iHeaders?reinterpret_cast<const char*>(iHeaders->Ptr()):"";
    size_t extraLen=exchange.sendHeaders&&iHeaders?iHeaders->Length():0;
    if(!n70_headers_has(extra,extraLen,"User-Agent",10))req.Append(_L8("User-Agent: N70Https/0.9\r\n"));
    if(!n70_headers_has(extra,extraLen,"Accept",6))req.Append(_L8("Accept: */*\r\n"));
    req.Append(iOptions.iKeepAlive?_L8("Connection: keep-alive\r\n"):_L8("Connection: close\r\n"));
    req.Append(_L8("Accept-Encoding: gzip, identity\r\n"));if(extraLen)req.Append(*iHeaders);
    if(method!=_L8("GET")&&method!=_L8("HEAD")){
        req.Append(_L8("Content-Length: "));req.AppendNum(body.Length());req.Append(_L8("\r\n"));
        if(type.Length()){req.Append(_L8("Content-Type: "));req.Append(type);req.Append(_L8("\r\n"));}
    }
    req.Append(_L8("\r\n"));iStage=ETlsHandshake;
    TRAPD(writeError,WriteTlsL(req));req.FillZ();CleanupStack::PopAndDestroy(request);User::LeaveIfError(writeError);
    for(TInt offset=0;offset<body.Length();offset+=1024)WriteTlsL(body.Mid(offset,Min(1024,body.Length()-offset)));
    n70_http* parser=static_cast<n70_http*>(User::AllocLC(sizeof(n70_http)));
    n70_gzip* gzip=static_cast<n70_gzip*>(User::AllocL(sizeof(n70_gzip)));Mem::FillZ(gzip,sizeof *gzip);
    CleanupStack::PushL(TCleanupItem(CloseGzip,gzip));
    TBodySink delivery;delivery.sink=&sink;delivery.client=this;delivery.control=ControlCallback;
    delivery.gzip=gzip;delivery.error=0;delivery.limit=iOptions.iMaxBodyBytes;delivery.bytes=0;delivery.compressed=EFalse;
    n70_http_init(parser,iOptions.iMaxWireBytes,method==_L8("HEAD"));iStage=EHttpRead;TBool clean=ETrue;
    for(;;){
        TBuf8<2048> data;
        if(!ReadTlsL(data)){if(n70_http_eof(parser)!=N70_DONE)User::Leave(KErrCorrupt);break;}
        size_t offset=0;
        while(offset<(size_t)data.Length()){
            CheckL();size_t used=0;TInt event=n70_http_feed(parser,data.Ptr()+offset,data.Length()-offset,&used,Deliver,&delivery);
            offset+=used;result.iBodyBytes=delivery.bytes;result.iWireBodyBytes=parser->body_bytes;
            if(event<0)User::Leave(event==N70_SINK?delivery.error:ParseError(event));
            if(event==N70_HEADERS){
                result.iStatus=parser->status;
                TPtrC8 headers(reinterpret_cast<const TUint8*>(parser->headers),parser->hlen);result.iRetryAfterMs=RetryAfter(headers);
                if(IsRedirect(parser->status)&&parser->location[0]&&iOptions.iMaxRedirects&&result.iRetryAfterMs<=iOptions.iMaxRetryDelayMs){
                    exchange.location.Copy(TPtrC8(reinterpret_cast<const TUint8*>(parser->location)));exchange.action=1;
                    exchange.retryDelay=Max(0,result.iRetryAfterMs);
                    CloseTransport();CleanupStack::PopAndDestroy();CleanupStack::PopAndDestroy();return;
                }
                TBuf8<8> text;const char* verb=MethodText(method,text);
                TInt delay=n70_retry_delay(iOptions.iRetryDelayMs,iOptions.iMaxRetryDelayMs,result.iRetries,result.iRetryAfterMs,User::FastCounter());
                if(exchange.retryBudget>0&&n70_method_retryable(verb,iOptions.iRetryIdempotent)&&n70_retry_status(parser->status)&&delay>=0){
                    exchange.action=2;exchange.retryDelay=delay;
                    CloseTransport();CleanupStack::PopAndDestroy();CleanupStack::PopAndDestroy();return;
                }
                if(parser->gzip&&method!=_L8("HEAD")&&parser->status!=204&&parser->status!=304){
                    TInt e=n70_gzip_init(gzip,iOptions.iMaxBodyBytes,GzipAlloc,GzipFree,NULL);
                    if(e)User::Leave(e==N70_MEMORY?KErrNoMemory:KErrCorrupt);delivery.compressed=ETrue;
                }
                exchange.delivered=ETrue;sink.HeadersL(parser->status,headers);CheckL();
            }
            if(event==N70_DONE){if(offset<(size_t)data.Length())clean=EFalse;break;}
            if(!used&&event!=N70_HEADERS)User::Leave(KErrCorrupt);
        }
        size_t ignored=0;if(n70_http_feed(parser,NULL,0,&ignored,Deliver,&delivery)==N70_DONE)break;
    }
    if(delivery.compressed&&n70_gzip_eof(gzip)!=N70_DONE)User::Leave(KErrCorrupt);
    if(n70_http_reusable(parser,clean))SaveConnectionL(host,current.port,roots,count);else CloseTransport();
    iStage=EComplete;CleanupStack::PopAndDestroy();CleanupStack::PopAndDestroy();
    }
class TFileSink : public MN70HttpSink
    {
public:
    TFileSink(RFile& f):file(f){}
    void HeadersL(TInt status,const TDesC8&) { if(status<200||status>=300)User::Leave(KErrGeneral); }
    void DataL(const TDesC8& data) { User::LeaveIfError(file.Write(data)); }
    RFile& file;
    };
struct TTempFile { RFs* fs;RFile* file;TFileName name; };
static void RemoveTemp(TAny* p){TTempFile*t=static_cast<TTempFile*>(p);t->file->Close();t->fs->Delete(t->name);}
void CN70Https::DownloadAutoL(const TDesC8& url,const TDesC& destination,
    const br_x509_trust_anchor* roots,size_t count,const TDesC& state,TN70HttpResult& result)
    {
    if(destination.Length()<4||destination[1]!=':'||destination[2]!='\\'||destination.CompareF(state)==0)User::Leave(KErrArgument);
    RFs fs;User::LeaveIfError(fs.Connect());CleanupClosePushL(fs);
    TParse path;User::LeaveIfError(path.Set(destination,NULL,NULL));
    TInt error=fs.MkDirAll(path.DriveAndPath());if(error!=KErrAlreadyExists)User::LeaveIfError(error);
    TTempFile temp;temp.fs=&fs;RFile file;temp.file=&file;
    User::LeaveIfError(file.Temp(fs,path.DriveAndPath(),temp.name,EFileWrite|EFileShareExclusive));
    CleanupStack::PushL(TCleanupItem(RemoveTemp,&temp));CleanupClosePushL(file);
    TFileSink sink(file);
    RequestAutoL(_L8("GET"),url,KNullDesC8,KNullDesC8,roots,count,state,sink,result);
    CheckL();User::LeaveIfError(file.Flush());CleanupStack::PopAndDestroy(&file);
    CheckL();User::LeaveIfError(fs.Replace(temp.name,destination));
    CleanupStack::PopAndDestroy();CleanupStack::PopAndDestroy(&fs);
    }
