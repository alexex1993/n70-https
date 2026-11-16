#include "n70https.h"
#include "n70connect.h"
#include "n70trust.h"
#include <commdbconnpref.h>
#include <nifvar.h>

void CN70Https::CloseTransport()
    {
    if(iIdleArmed){iIdleTimer.Cancel();User::WaitForRequest(iIdleStatus);iIdleArmed=EFalse;}
    iTlsReady=EFalse;iSocket.Close();iResolver.Close();
    }
void CN70Https::ResetConnection()
    { CloseTransport();iConnection.Close();iConnectionStarted=EFalse; }
void CN70Https::DisconnectL()
    { if(iRunning)User::Leave(KErrInUse);ResetConnection(); }
static TBool DeadBearer(RConnection& connection)
    {
    TNifProgress progress;
    if(connection.Progress(progress)!=KErrNone)return ETrue;
    return progress.iError!=KErrNone||progress.iStage==KConnectionUninitialised||
        progress.iStage==KConnectionFailure||progress.iStage==KConnectionClosed||progress.iStage==KLinkLayerClosed;
    }
void CN70Https::EnsureConnectionL(TBool prompt)
    {
    CheckL();if(iConnectionStarted&&DeadBearer(iConnection))ResetConnection();
    if(iConnectionStarted)return;
    iConnection.Close();iStage=EConnectionOpen;User::LeaveIfError(iConnection.Open(iServer));
    iStage=EConnectionStart;TRequestStatus status;TCommDbConnPref pref;
    if(prompt){pref.SetDirection(ECommDbConnectionDirectionOutgoing);
        pref.SetDialogPreference(ECommDbDialogPrefPrompt);iConnection.Start(pref,status);}
    else iConnection.Start(status);
    User::LeaveIfError(Wait(status,2));iConnectionStarted=ETrue;
    }
struct TN70Network
    {
    CN70Https* client;TPtrC8 host;TUint port;TBool prompt;TInt lastError;CN70Https::TStage lastStage;
    TN70Network(CN70Https*c,const TDesC8&h,TUint p,TBool q):client(c),host(h),port(p),prompt(q),lastError(0),lastStage(CN70Https::EDnsLookup){}
    };
TInt CN70Https::NetStart(TAny* ctx)
    {TN70Network*n=static_cast<TN70Network*>(ctx);TRAPD(e,n->client->EnsureConnectionL(n->prompt));return e;}
TInt CN70Https::NetResolve(TAny* ctx,TInt first,uint32_t* ip)
    {
    TN70Network*n=static_cast<TN70Network*>(ctx);CN70Https*c=n->client;
    TRequestStatus status;TNameEntry entry;TBuf<254> host;
    if(first){
        n->lastError=0;
        c->iResolver.Close();c->iStage=EResolverOpen;
        TInt e=c->iResolver.Open(c->iServer,KAfInet,KProtocolInetUdp,c->iConnection);if(e)return e;
        host.Copy(n->host);c->iStage=EDnsLookup;c->iResolver.GetByName(host,entry,status);
    }else{c->iStage=EDnsLookup;c->iResolver.Next(entry,status);}
    TInt e=c->Wait(status,1);
    if(e){if(!first&&n->lastError&&(e==KErrEof||e==KErrNotFound))c->iStage=n->lastStage;return e;}
    if(entry().iAddr.Family()!=KAfInet)return KErrNotSupported;
    *ip=TInetAddr::Cast(entry().iAddr).Address();return KErrNone;
    }
TInt CN70Https::NetConnect(TAny* ctx,uint32_t ip)
    {
    TN70Network*n=static_cast<TN70Network*>(ctx);CN70Https*c=n->client;
    c->iSocket.Close();c->iStage=ESocketOpen;
    TInt e=c->iSocket.Open(c->iServer,KAfInet,KSockStream,KProtocolInetTcp,c->iConnection);
    if(e){n->lastError=e;n->lastStage=c->iStage;return e;}
    TInetAddr address(ip,n->port);TRequestStatus status;c->iStage=ETcpConnect;++c->iAddressAttempts;
    c->iSocket.Connect(address,status);e=c->Wait(status,3);if(e)c->iSocket.Close();
    n->lastError=e;n->lastStage=c->iStage;return e;
    }
void CN70Https::NetReset(TAny* ctx){static_cast<TN70Network*>(ctx)->client->ResetConnection();}
TInt CN70Https::NetControl(TAny*ctx)
    {TN70Network*n=static_cast<TN70Network*>(ctx);return n->client->ControlError();}
void CN70Https::ConnectSocketL(const TDesC8& host,TUint port,TBool prompt)
    {
    TN70Network n(this,host,port,prompt);n70_connect_ops ops;
    ops.context=&n;ops.start=NetStart;ops.resolve=NetResolve;ops.connect=NetConnect;ops.reset=NetReset;ops.control=NetControl;
    TInt e=n70_connect_run(&ops,iOptions.iMaxAddresses);iResolver.Close();User::LeaveIfError(e);++iNewConnections;
    }
TBool CN70Https::ReuseConnection(const TDesC8& host,TUint port,const br_x509_trust_anchor* roots,size_t count)
    {
    if(!iOptions.iKeepAlive||!iTlsReady||!iIdleArmed||iIdleStatus!=KRequestPending||
       host.CompareF(iPeerHost)!=0||port!=iPeerPort||!iConnectionStarted||DeadBearer(iConnection))return EFalse;
    TBuf8<32> digest;digest.SetLength(32);
    if(!n70_trust_digest(roots,count,const_cast<TUint8*>(digest.Ptr()))||digest!=iPeerTrust)return EFalse;
    if(br_ssl_engine_current_state(&iClient.eng)&BR_SSL_CLOSED)return EFalse;
    size_t pending=0;br_ssl_engine_recvapp_buf(&iClient.eng,&pending);if(pending)return EFalse;
    iIdleTimer.Cancel();User::WaitForRequest(iIdleStatus);iIdleArmed=EFalse;iTlsReady=EFalse;
    iStage=ETlsHandshake;iNetError=KErrNone;++iReusedConnections;return ETrue;
    }
void CN70Https::SaveConnectionL(const TDesC8& host,TUint port,const br_x509_trust_anchor* roots,size_t count)
    {
    if(!iOptions.iKeepAlive){CloseTransport();return;}
    size_t pending=0;br_ssl_engine_recvapp_buf(&iClient.eng,&pending);
    if(pending||(br_ssl_engine_current_state(&iClient.eng)&BR_SSL_CLOSED)){CloseTransport();return;}
    iPeerTrust.SetLength(32);
    if(!n70_trust_digest(roots,count,const_cast<TUint8*>(iPeerTrust.Ptr())))User::Leave(KErrArgument);
    iPeerHost.Copy(host);iPeerPort=port;iTlsReady=ETrue;
    iIdleTimer.After(iIdleStatus,iOptions.iKeepAliveIdleMs*1000);iIdleArmed=ETrue;
    }
