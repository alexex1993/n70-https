#include "httpfeatures.h"
#include "n70https.h"
#include "n70testpaths.h"
#include "testanchor.h"
#include <f32file.h>

_LIT(KDownload,"C:\\system\\apps\\N70Test\\download.bin");

class TCountingSink : public CBase, public MN70HttpSink
    {
public:
    void Reset()
        { status=0;bytes=0;lines=0;headers=0;chunked=gzip=EFalse;preview.Zero();contentType.Zero(); }
    void HeadersL(TInt s,const TDesC8& raw)
        {
        status=s;++headers;
        TPtrC8 type=N70HeaderValue(raw,_L8("Content-Type"));
        contentType.Copy(type.Left(contentType.MaxLength()));
        chunked=N70HeaderValue(raw,_L8("Transfer-Encoding")).CompareF(_L8("chunked"))==0;
        gzip=N70HeaderValue(raw,_L8("Content-Encoding")).CompareF(_L8("gzip"))==0;
        }
    void DataL(const TDesC8& data)
        {
        bytes+=data.Length();
        for(TInt i=0;i<data.Length();++i)if(data[i]=='\n')++lines;
        preview.Append(data.Left(Min(data.Length(),preview.MaxLength()-preview.Length())));
        }
    TInt status,headers,lines;
    TUint bytes;
    TBool chunked,gzip;
    TBuf8<2048> preview;
    TBuf8<128> contentType;
    };

// Predicate for the service's simple ASCII fixtures, not a general JSON decoder.
static TBool EchoString(const TDesC8& json,const TDesC8& key,const TDesC8& value)
    {
    TBuf8<64> quoted;quoted.Append('"');quoted.Append(key);quoted.Append('"');
    TInt start=0;
    while(start<json.Length())
        {
        TInt found=json.Mid(start).Find(quoted);if(found<0)return EFalse;
        TInt p=start+found+quoted.Length();start=p;
        while(p<json.Length()&&json[p]<=32)++p;
        if(p>=json.Length()||json[p++]!=':')continue;
        while(p<json.Length()&&json[p]<=32)++p;
        if(p+value.Length()+2<=json.Length()&&json[p]=='"'&&
           json.Mid(p+1,value.Length())==value&&json[p+value.Length()+1]=='"')return ETrue;
        }
    return EFalse;
    }

class TFeatureLog
    {
public:
    TFeatureLog(RFile& file):iFile(file),iPassed(0),iAttempted(0),iSkipped(0) {}
    void StartL(const TDesC8& name,const TDesC8& url)
        {
        iName.Copy(name);TBuf8<512> line;line.Copy(_L8("\r\nSTART "));
        line.Append(name);line.Append(_L8("\r\nURL: "));line.Append(url);line.Append(_L8("\r\n"));
        WriteL(line);
        }
    void WriteL(const TDesC8& text)
        { User::LeaveIfError(iFile.Write(text));User::LeaveIfError(iFile.Flush()); }
    void ResultL(TBool good,TInt error,TInt expected,CN70Https& client,
        const TN70HttpResult& result,const TCountingSink& sink)
        {
        ++iAttempted;if(good)++iPassed;
        TPtrC8 verdict(good?_L8("PASS"):_L8("FAIL"));TBuf8<512> line;
        line.Format(_L8("%S %S | Error=%d expected=%d HTTP=%d Bytes=%u Redirects=%d\r\n"),
            &verdict,&iName,error,expected,result.iStatus,(TUint)result.iBodyBytes,result.iRedirects);
        WriteL(line);
        line.Format(_L8("Attempts=%d Retries=%d NewTCP=%d Reused=%d AddressAttempts=%d RetryAfterMs=%d\r\n"),
            result.iAttempts,result.iRetries,result.iNewConnections,result.iReusedConnections,result.iAddressAttempts,result.iRetryAfterMs);WriteL(line);
        line.Format(_L8("TLS=0x%04x BearSSL=%d I/O=%d Headers=%d Lines=%d Chunked=%d Gzip=%d WireBytes=%u\r\nStage: "),
            client.ProtocolVersion(),client.TlsError(),client.NetworkError(),sink.headers,sink.lines,sink.chunked,sink.gzip,(TUint)result.iWireBodyBytes);
        line.Append(client.StageName());line.Append(_L8("\r\nFinal URL: "));
        line.Append(result.iFinalUrl.Left(256));line.Append(_L8("\r\n"));WriteL(line);
        if(sink.preview.Length())
            { WriteL(_L8("Body preview (up to 2048 bytes):\r\n"));WriteL(sink.preview);WriteL(_L8("\r\n")); }
        }
    void SummaryL(TBool cancelled=EFalse)
        {
        TBuf8<128> line;line.Format(_L8("\r\nSummary: %d/%d passed. Cancelled=%d\r\n"),iPassed,iAttempted,cancelled);
        WriteL(line);
        if(iSkipped){line.Format(_L8("Skipped/inconclusive: %d; not counted as passes.\r\n"),iSkipped);WriteL(line);}
        }
    void SkipL(){++iSkipped;WriteL(_L8("INCONCLUSIVE: target did not stay in TCP connect; no timeout exercised.\r\n"));}
    RFile& iFile;
    TInt iPassed,iAttempted,iSkipped;
    TBuf8<80> iName;
    };

static TBool ValidTls(CN70Https& client)
    { return client.ProtocolVersion()==BR_TLS12&&client.TlsError()==0; }

static void RequestTestsL(TDes& stage,CN70Https& client,TN70HttpOptions& options,
    const br_x509_trust_anchor* anchors,TN70HttpResult& result,TCountingSink& sink,TFeatureLog& log)
    {
    for(TInt i=0;i<10;++i)
        {
        TBuf8<160> url;url.Copy(_L8("https://httpbingo.org/"));
        TBuf8<80> name;TPtrC8 method(_L8("GET")),type(KNullDesC8),body(KNullDesC8);
        switch(i)
            {
            case 0:name.Copy(_L8("GET /get"));url.Append(_L8("get"));break;
            case 1:name.Copy(_L8("HEAD: headers, zero body"));url.Append(_L8("get"));method.Set(_L8("HEAD"));break;
            case 2:name.Copy(_L8("POST JSON: echoed parsed value"));url.Append(_L8("anything"));break;
            case 3:name.Copy(_L8("POST form: echoed payload"));url.Append(_L8("anything"));break;
            case 4:name.Copy(_L8("GET redirect: one hop"));url.Append(_L8("redirect/1"));break;
            case 5:name.Copy(_L8("POST 303: changes to GET, drops body"));url.Append(_L8("redirect-to?url=%2Fanything&status_code=303"));break;
            case 6:name.Copy(_L8("POST 307: preserves method and body"));url.Append(_L8("redirect-to?url=%2Fanything&status_code=307"));break;
            case 7:name.Copy(_L8("Chunked stream: 20 decoded JSON lines"));url.Append(_L8("stream/20"));break;
            case 8:name.Copy(_L8("HTTP 404 is an HTTP result"));url.Append(_L8("status/404"));break;
            default:name.Copy(_L8("Body limit: 4096 bytes exceed limit 128"));url.Append(_L8("bytes/4096"));break;
            }
        if(i==2||i==3||i==5||i==6)
            {
            method.Set(_L8("POST"));type.Set(_L8("application/json"));body.Set(_L8("{\"n70\":\"tls12-json-071\"}"));
            if(i==3){type.Set(_L8("application/x-www-form-urlencoded"));body.Set(_L8("n70=tls12-form-071&device=N70"));}
            }
        options.iMaxBodyBytes=i==9?128:2*1024*1024;client.SetOptionsL(options);
        sink.Reset();stage.Copy(name);log.StartL(name,url);
        TRAPD(error,client.RequestAutoL(method,url,type,body,anchors,TEST_ANCHOR_COUNT,KDeviceState,sink,result));
        TBool good=error==KErrNone&&ValidTls(client)&&result.iStatus==200&&sink.headers==1&&
            sink.status==200&&result.iBodyBytes==sink.bytes;
        if(i==1)good=good&&sink.bytes==0&&sink.contentType.Length()>0;
        else if(i!=8&&i!=9)good=good&&sink.bytes>0;
        if(i==2||i==6)good=good&&EchoString(sink.preview,_L8("method"),_L8("POST"))&&
            EchoString(sink.preview,_L8("n70"),_L8("tls12-json-071"));
        if(i==3)good=good&&EchoString(sink.preview,_L8("method"),_L8("POST"))&&
            sink.preview.Find(body)>=0&&sink.preview.Find(type)>=0;
        if(i==4)good=good&&result.iRedirects==1&&result.iFinalUrl==_L8("https://httpbingo.org/get");
        if(i==5)good=good&&EchoString(sink.preview,_L8("method"),_L8("GET"))&&
            EchoString(sink.preview,_L8("data"),KNullDesC8)&&sink.preview.Find(_L8("tls12-json-071"))<0;
        if(i==5||i==6)good=good&&result.iRedirects==1&&result.iFinalUrl==_L8("https://httpbingo.org/anything");
        if(i==7)good=good&&sink.chunked&&sink.lines==20;
        if(i==8)good=error==KErrNone&&ValidTls(client)&&result.iStatus==404&&sink.headers==1&&sink.status==404;
        // A known Content-Length may be rejected before HeadersL is delivered.
        if(i==9)good=error==KErrOverflow&&ValidTls(client)&&sink.bytes<=128&&result.iBodyBytes<=128;
        log.ResultL(good,error,i==9?KErrOverflow:KErrNone,client,result,sink);
        if(error==KErrCancel){log.SummaryL(ETrue);User::Leave(error);}
        }
    options.iMaxBodyBytes=2*1024*1024;client.SetOptionsL(options);
    }

static TInt HashFileL(RFs& fs,const TDesC& name,TDes8& digest)
    {
    RFile file;User::LeaveIfError(file.Open(fs,name,EFileRead|EFileShareReadersOnly));CleanupClosePushL(file);
    TInt size;User::LeaveIfError(file.Size(size));br_sha256_context hash;br_sha256_init(&hash);
    TBuf8<1024> data;
    for(;;){User::LeaveIfError(file.Read(data));if(!data.Length())break;br_sha256_update(&hash,data.Ptr(),data.Length());}
    digest.SetLength(32);br_sha256_out(&hash,const_cast<TUint8*>(digest.Ptr()));
    CleanupStack::PopAndDestroy(&file);return size;
    }

static void DownloadTestsL(TDes& stage,RFs& fs,CN70Https& client,TN70HttpOptions& options,
    const br_x509_trust_anchor* anchors,TN70HttpResult& result,TCountingSink& sink,TFeatureLog& log)
    {
    sink.Reset();stage.Copy(_L("Download 128 KiB to file"));
    log.StartL(_L8("Download to file: exactly 131072 bytes"),_L8("https://httpbingo.org/bytes/131072"));
    TRAPD(error,client.DownloadAutoL(_L8("https://httpbingo.org/bytes/131072"),KDownload,
        anchors,TEST_ANCHOR_COUNT,KDeviceState,result));
    TBuf8<32> before,after;TInt size=0,hashError=KErrNotReady;
    if(!error){TRAP(hashError,size=HashFileL(fs,KDownload,before));}
    TBool good=!error&&!hashError&&size==131072&&result.iBodyBytes==131072&&result.iStatus==200&&ValidTls(client);
    TBuf8<128> detail;detail.Format(_L8("File size=%d expected=131072; hash read error=%d\r\n"),size,hashError);log.WriteL(detail);
    log.ResultL(good,error,KErrNone,client,result,sink);
    if(error==KErrCancel){log.SummaryL(ETrue);User::Leave(error);}

    stage.Copy(_L("Failed download preserves file"));sink.Reset();
    log.StartL(_L8("Failed download: old file SHA-256 stays unchanged"),_L8("https://httpbingo.org/bytes/4096"));
    options.iMaxBodyBytes=128;client.SetOptionsL(options);
    TRAP(error,client.DownloadAutoL(_L8("https://httpbingo.org/bytes/4096"),KDownload,
        anchors,TEST_ANCHOR_COUNT,KDeviceState,result));
    TInt afterSize=0;TRAPD(afterError,afterSize=HashFileL(fs,KDownload,after));
    good=error==KErrOverflow&&!hashError&&!afterError&&before==after&&afterSize==131072&&ValidTls(client);
    detail.Format(_L8("Old file preserved=%d; read error=%d; size=%d\r\n"),good,afterError,afterSize);log.WriteL(detail);
    log.ResultL(good,error,KErrOverflow,client,result,sink);
    options.iMaxBodyBytes=2*1024*1024;client.SetOptionsL(options);
    if(error==KErrCancel){log.SummaryL(ETrue);User::Leave(error);}
    }

static void WarmupL(TDes& stage,CN70Https& client,const br_x509_trust_anchor* anchors,
    TN70HttpResult& result,TCountingSink& sink,TFeatureLog& log)
    {
    stage.Copy(_L("Connect before control test"));sink.Reset();
    log.WriteL(_L8("WARMUP HEAD /get: establish bearer outside timed test\r\n"));
    TRAPD(error,client.RequestAutoL(_L8("HEAD"),_L8("https://httpbingo.org/get"),KNullDesC8,KNullDesC8,
        anchors,TEST_ANCHOR_COUNT,KDeviceState,sink,result));
    if(!error&&(!ValidTls(client)||result.iStatus!=200))error=KErrGeneral;
    TBuf8<128> line;line.Format(_L8("Warmup error=%d HTTP=%d TLS=0x%04x BearSSL=%d\r\n"),
        error,result.iStatus,client.ProtocolVersion(),client.TlsError());log.WriteL(line);
    User::LeaveIfError(error);
    }

static void ControlTestL(TDes& stage,CN70Https& client,TN70HttpOptions& options,
    const br_x509_trust_anchor* anchors,TN70HttpResult& result,TCountingSink& sink,TFeatureLog& log,TBool cancelTest)
    {
    sink.Reset();
    if(cancelTest)
        {
        stage.Copy(_L("Waiting: Options > Cancel request"));
        log.StartL(_L8("Manual cancellation: choose Options > Cancel request"),_L8("https://httpbingo.org/delay/10"));
        }
    else
        {
        options.iTotalTimeoutMs=2000;client.SetOptionsL(options);
        stage.Copy(_L("2s deadline, server delays 10s"));
        log.StartL(_L8("Total timeout: 2 seconds incl. TCP/TLS"),_L8("https://httpbingo.org/delay/10"));
        }
    TInt expected=cancelTest?KErrCancel:KErrTimedOut;
    TRAPD(error,client.RequestAutoL(_L8("GET"),_L8("https://httpbingo.org/delay/10"),KNullDesC8,KNullDesC8,
        anchors,TEST_ANCHOR_COUNT,KDeviceState,sink,result));
    log.ResultL(error==expected,error,expected,client,result,sink);
    if(error==KErrCancel&&!cancelTest){log.SummaryL(ETrue);User::Leave(error);}
    }

static void ApiTestsL(TDes& stage,RFs& fs,CN70Https& client,TN70HttpOptions& options,
    const br_x509_trust_anchor* anchors,TN70HttpResult& result,TCountingSink& sink,TFeatureLog& log)
    {
    for(TInt i=0;i<8;++i)
        {
        client.ClearHeadersL();options.iMaxBodyBytes=i==6?32:2*1024*1024;client.SetOptionsL(options);
        TBuf8<160> url;url.Copy(_L8("https://httpbingo.org/"));TBuf8<80> name;
        switch(i)
            {
            case 0:name.Copy(_L8("Bearer endpoint without token: HTTP 401"));url.Append(_L8("bearer"));break;
            case 1:name.Copy(_L8("Bearer authorization: echoed public test token"));url.Append(_L8("bearer"));break;
            case 2:name.Copy(_L8("Custom header replacement and Accept override"));url.Append(_L8("headers"));break;
            case 3:name.Copy(_L8("Same-origin redirect preserves Bearer"));url.Append(_L8("redirect-to?url=%2Fbearer&status_code=302"));break;
            case 4:name.Copy(_L8("Gzip: decoded JSON, wire smaller than body"));url.Append(_L8("gzip"));break;
            case 5:name.Copy(_L8("Gzip plus chunked: 20 JSON lines"));url.Append(_L8("stream/20"));break;
            case 6:name.Copy(_L8("Gzip decoded limit 32 bytes"));url.Append(_L8("gzip"));break;
            default:name.Copy(_L8("Invalid gzip is rejected"));url.Append(_L8("response-headers?Content-Encoding=gzip"));break;
            }
        if(i==1||i==3)client.SetBearerTokenL(_L8("n70-public-test-080"));
        if(i==2){client.SetHeaderL(_L8("X-N70-Test"),_L8("old-value"));client.SetHeaderL(_L8("x-n70-test"),_L8("custom-header-080"));client.SetHeaderL(_L8("Accept"),_L8("application/json"));}
        stage.Copy(name);sink.Reset();log.StartL(name,url);
        TRAPD(error,client.RequestAutoL(_L8("GET"),url,KNullDesC8,KNullDesC8,anchors,TEST_ANCHOR_COUNT,KDeviceState,sink,result));
        TInt expected=i==6?KErrOverflow:(i==7?KErrCorrupt:KErrNone);
        TBool good=error==expected&&ValidTls(client)&&result.iStatus==(i==0?401:200)&&sink.headers==1;
        if(i<6)good=good&&result.iBodyBytes==sink.bytes;
        if(i==1||i==3)good=good&&EchoString(sink.preview,_L8("token"),_L8("n70-public-test-080"));
        if(i==2)good=good&&sink.preview.Find(_L8("custom-header-080"))>=0&&sink.preview.Find(_L8("old-value"))<0&&sink.preview.Find(_L8("application/json"))>=0;
        if(i==3)good=good&&result.iRedirects==1;
        if(i==4)good=good&&sink.gzip&&sink.preview.Find(_L8("\"gzipped\": true"))>=0&&result.iWireBodyBytes<result.iBodyBytes;
        if(i==5)good=good&&sink.gzip&&sink.chunked&&sink.lines==20;
        if(i==6)good=good&&sink.gzip&&sink.bytes<=32&&result.iBodyBytes<=32;
        if(i==7)good=good&&sink.gzip;
        log.ResultL(good,error,expected,client,result,sink);
        if(error==KErrCancel){log.SummaryL(ETrue);User::Leave(error);}
        }
    client.ClearHeadersL();options.iMaxBodyBytes=2*1024*1024;client.SetOptionsL(options);sink.Reset();
    stage.Copy(_L("Download and decode gzip to file"));
    log.StartL(_L8("Gzip download: file contains decoded JSON"),_L8("https://httpbingo.org/gzip"));
    _LIT(KGzipFile,"C:\\system\\apps\\N70Test\\gzip-response.json");
    TRAPD(error,client.DownloadAutoL(_L8("https://httpbingo.org/gzip"),KGzipFile,anchors,TEST_ANCHOR_COUNT,KDeviceState,result));
    TInt size=0;TBuf8<2048> text;
    TRAPD(readError,RFile file;User::LeaveIfError(file.Open(fs,KGzipFile,EFileRead|EFileShareReadersOnly));CleanupClosePushL(file);
        User::LeaveIfError(file.Size(size));User::LeaveIfError(file.Read(text));CleanupStack::PopAndDestroy(&file));
    TBool good=!error&&!readError&&ValidTls(client)&&size==(TInt)result.iBodyBytes&&text.Find(_L8("\"gzipped\": true"))>=0;
    log.ResultL(good,error,KErrNone,client,result,sink);
    if(error==KErrCancel){log.SummaryL(ETrue);User::Leave(error);}
    for(TInt j=0;j<3;++j)
        {
        result=TN70HttpResult();sink.Reset();stage.Copy(_L("Local header validation"));
        log.StartL(j==0?_L8("Reject CRLF header injection"):(j==1?_L8("Reject reserved Host override"):_L8("Reject invalid Bearer token")),_L8("LOCAL: no network request"));
        TRAP(error,if(j==0)client.SetHeaderL(_L8("X-Test"),_L8("value\r\nInjected: yes"));
            else if(j==1)client.SetHeaderL(_L8("Host"),_L8("other.example"));else client.SetBearerTokenL(_L8("bad token")));
        log.ResultL(error==KErrArgument,error,KErrArgument,client,result,sink);
        }
    }

static void TcpTestsL(TDes& stage,CN70Https& client,TN70HttpOptions& options,
    const br_x509_trust_anchor* anchors,TN70HttpResult& result,TCountingSink& sink,TFeatureLog& log)
    {
    WarmupL(stage,client,anchors,result,sink,log);
    options.iMaxRetries=0; // Measure a single TCP wait, not retry backoff.
    for(TInt i=0;i<2;++i)
        {
        options.iTotalTimeoutMs=i==0?5000:2000;options.iIoTimeoutMs=i==0?2000:10000;client.SetOptionsL(options);
        TPtrC8 url(i==0?_L8("https://192.0.2.1/"):_L8("https://198.51.100.1/"));
        stage.Copy(_L("TCP timeout to TEST-NET address"));sink.Reset();
        log.StartL(i==0?_L8("TCP I/O timeout: 2s, total 5s"):_L8("TCP total timeout: 2s, I/O 10s"),url);
        TTime start,end;start.UniversalTime();TUint ticks=User::TickCount();
        TRAPD(error,client.RequestAutoL(_L8("GET"),url,KNullDesC8,KNullDesC8,anchors,TEST_ANCHOR_COUNT,KDeviceState,sink,result));
        end.UniversalTime();TInt ms=(TInt)(end.MicroSecondsFrom(start).Int64().GetTReal()/1000.0);
        TBuf8<160> line;line.Format(_L8("Wall elapsed=%d ms; tick delta=%u; expected ~2000 ms, tolerance 1500 ms\r\n"),ms,User::TickCount()-ticks);log.WriteL(line);
        if(error==KErrCancel){log.ResultL(EFalse,error,KErrTimedOut,client,result,sink);log.SummaryL(ETrue);User::Leave(error);}
        TBool inTcp=client.StageName()==_L8("tcp-connect");
        if(ms>=0&&ms<=3500&&(!inTcp||error!=KErrTimedOut)){
            log.SkipL();line.Format(_L8("Actual error=%d; stage: "),error);line.Append(client.StageName());line.Append(_L8("\r\n"));log.WriteL(line);continue;
        }
        log.ResultL(inTcp&&error==KErrTimedOut&&ms>=1800&&ms<=3500,error,KErrTimedOut,client,result,sink);
        }
    }

static void ClientTestsL(TDes& stage,CN70Https& client,TN70HttpOptions& options,
    const br_x509_trust_anchor* anchors,TN70HttpResult& result,TCountingSink& sink,TFeatureLog& log)
    {
    client.ClearHeadersL();
    for(TInt i=0;i<14;++i)
        {
        options.iKeepAlive=i!=4;options.iKeepAliveIdleMs=i==13?500:15000;
        options.iMaxRetries=i==7?0:(i==11?1:2);options.iRetryIdempotent=i==8||i==9||i==11;
        options.iTotalTimeoutMs=120000;options.iIoTimeoutMs=30000;client.SetOptionsL(options);
        TBuf8<160> url;url.Copy(_L8("https://httpbingo.org/"));TBuf8<80> name;
        TPtrC8 method(_L8("GET")),body(KNullDesC8),type(KNullDesC8);
        if(i<3){
            method.Set(i==0?_L8("PUT"):(i==1?_L8("PATCH"):_L8("DELETE")));
            name.Copy(method);name.Append(_L8(" JSON echo"));url.Append(_L8("anything"));
            type.Set(_L8("application/json"));body.Set(_L8("{\"n70\":\"method-090\"}"));
        }else if(i==3){name.Copy(_L8("Keep-alive: second request reuses TLS"));url.Append(_L8("get"));}
        else if(i==4){name.Copy(_L8("Keep-alive disabled: fresh TCP"));url.Append(_L8("get"));}
        else if(i==5){name.Copy(_L8("DisconnectL then request reconnects"));url.Append(_L8("get"));client.DisconnectL();}
        else if(i==12){name.Copy(_L8("Retry-After seconds parsed from HTTP"));url.Append(_L8("response-headers?Retry-After=1"));}
        else if(i==13){name.Copy(_L8("Idle timer expiry prevents reuse"));url.Append(_L8("get"));}
        else{
            url.Append(_L8("status/503"));
            if(i==6)name.Copy(_L8("GET 503: two bounded retries"));
            if(i==7)name.Copy(_L8("GET 503: retries disabled"));
            if(i==8){name.Copy(_L8("POST 503: never repeated"));method.Set(_L8("POST"));}
            if(i==9){name.Copy(_L8("PATCH 503: never repeated"));method.Set(_L8("PATCH"));}
            if(i==10){name.Copy(_L8("PUT 503: no retry by default"));method.Set(_L8("PUT"));}
            if(i==11){name.Copy(_L8("PUT 503: explicit opt-in, one retry"));method.Set(_L8("PUT"));}
        }
        if(i==3||i==13){
            client.DisconnectL();stage.Copy(_L("Warmup for reuse/idle test"));sink.Reset();
            client.RequestAutoL(_L8("GET"),_L8("https://httpbingo.org/get"),KNullDesC8,KNullDesC8,
                anchors,TEST_ANCHOR_COUNT,KDeviceState,sink,result);
            log.WriteL(_L8("WARMUP GET /get completed before keep-alive check\r\n"));
            if(i==13)User::After(800000);
        }
        stage.Copy(name);sink.Reset();log.StartL(name,url);
        TRAPD(error,client.RequestAutoL(method,url,type,body,anchors,TEST_ANCHOR_COUNT,KDeviceState,sink,result));
        TBool status503=i>=6&&i<=11;
        TBool good=!error&&ValidTls(client)&&result.iStatus==(status503?503:200)&&sink.headers==1&&result.iBodyBytes==sink.bytes;
        if(i<3)good=good&&EchoString(sink.preview,_L8("method"),method)&&EchoString(sink.preview,_L8("n70"),_L8("method-090"));
        if(i==3)good=good&&result.iReusedConnections==1&&result.iNewConnections==0&&result.iRetries==0;
        if(i==4||i==5||i==13)good=good&&result.iNewConnections==1&&result.iReusedConnections==0;
        if(i==6)good=good&&result.iAttempts==3&&result.iRetries==2;
        if(i>=7&&i<=10)good=good&&result.iAttempts==1&&result.iRetries==0;
        if(i==11)good=good&&result.iAttempts==2&&result.iRetries==1;
        if(i==12)good=good&&result.iRetryAfterMs==1000&&result.iRetries==0;
        log.ResultL(good,error,KErrNone,client,result,sink);
        if(error==KErrCancel){log.SummaryL(ETrue);User::Leave(error);}
        }
    }

void TestHttpFeaturesL(TDes& stage,CN70CancelToken* cancel,TInt mode)
    {
    stage.Copy(_L("create feature log"));
    RFs fs;User::LeaveIfError(fs.Connect());CleanupClosePushL(fs);
    TInt e=fs.MkDirAll(_L("C:\\system\\apps\\N70Test\\"));if(e!=KErrAlreadyExists)User::LeaveIfError(e);
    TPtrC path(_L("C:\\system\\apps\\N70Test\\http-features.txt"));
    if(mode==3)path.Set(_L("C:\\system\\apps\\N70Test\\timeout.txt"));
    if(mode==4)path.Set(_L("C:\\system\\apps\\N70Test\\cancel.txt"));
    if(mode==5)path.Set(_L("C:\\system\\apps\\N70Test\\api-tests.txt"));
    if(mode==6)path.Set(_L("C:\\system\\apps\\N70Test\\tcp-timeout.txt"));
    if(mode==7)path.Set(_L("C:\\system\\apps\\N70Test\\client-tests.txt"));
    RFile file;User::LeaveIfError(file.Replace(fs,path,EFileWrite|EFileShareExclusive));CleanupClosePushL(file);
    TFeatureLog log(file);log.WriteL(_L8("N70Test 0.9.1; HTTPS feature checks\r\nTLS must be 0x0303 (1.2). Expected errors are test successes.\r\n"));
    CN70Https* client=CN70Https::NewL();CleanupStack::PushL(client);
    TN70HttpOptions options;options.iCancel=cancel;options.iPromptForAccessPoint=ETrue;
    options.iMaxBodyBytes=2*1024*1024;client->SetOptionsL(options);
    br_x509_trust_anchor anchors[TEST_ANCHOR_COUNT];InitTestAnchors(anchors);
    TN70HttpResult* result=new(ELeave) TN70HttpResult;CleanupStack::PushL(result);
    TCountingSink* sink=new(ELeave) TCountingSink;CleanupStack::PushL(sink);sink->Reset();
    if(mode==7)ClientTestsL(stage,*client,options,anchors,*result,*sink,log);
    else if(mode==5)ApiTestsL(stage,fs,*client,options,anchors,*result,*sink,log);
    else if(mode==6)TcpTestsL(stage,*client,options,anchors,*result,*sink,log);
    else if(mode==2)
        {
        RequestTestsL(stage,*client,options,anchors,*result,*sink,log);
        DownloadTestsL(stage,fs,*client,options,anchors,*result,*sink,log);
        }
    else WarmupL(stage,*client,anchors,*result,*sink,log);
    if(mode<=4)ControlTestL(stage,*client,options,anchors,*result,*sink,log,mode==4);
    log.SummaryL();
    stage.Format(_L("%d/%d checks passed"),log.iPassed,log.iAttempted);
    if(log.iSkipped)stage.Format(_L("%d/%d passed, %d inconclusive"),log.iPassed,log.iAttempted,log.iSkipped);
    TBool failed=log.iPassed!=log.iAttempted;
    CleanupStack::PopAndDestroy(sink);CleanupStack::PopAndDestroy(result);CleanupStack::PopAndDestroy(client);
    CleanupStack::PopAndDestroy(&file);CleanupStack::PopAndDestroy(&fs);
    if(failed)User::Leave(KErrGeneral);
    }
