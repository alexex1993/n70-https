#include "n70entropy.h"
#include "n70entropy_core.h"
#include <f32file.h>
#include <e32math.h>
extern "C" {
#include "bearssl.h"
}

struct TSeedSecrets
    {
    TBuf8<32> iOld;
    TBuf8<N70_ENTROPY_RECORD_SIZE> iRecord;
    unsigned char iDerived[64];
    };
static void ClearSecrets(TAny* aData)
    {
    TSeedSecrets* s=static_cast<TSeedSecrets*>(aData);
    s->iOld.FillZ();
    s->iRecord.FillZ();
    volatile unsigned char* p=s->iDerived;
    for (TInt i=0; i<64; ++i) p[i]=0;
    }
static void ReadSecretL(RFs& aFs, const TDesC& aName, TDes8& aSecret, TInt aSize=32)
    {
    RFile file;
    User::LeaveIfError(file.Open(aFs,aName,EFileRead|EFileShareExclusive));
    CleanupClosePushL(file);
    TInt size=0; User::LeaveIfError(file.Size(size));
    if (size!=aSize) User::Leave(KErrCorrupt);
    User::LeaveIfError(file.Read(aSecret,aSize));
    if (aSecret.Length()!=aSize) User::Leave(KErrCorrupt);
    CleanupStack::PopAndDestroy(&file);
    }
void TN70Entropy::NextSeedL(const TDesC& aStateFile,
    const TDesC& aProvisioningFile, TDes8& aSeed)
    { NextInternalL(aStateFile,&aProvisioningFile,aSeed); }
void TN70Entropy::NextDeviceSeedL(const TDesC& aStateFile, TDes8& aSeed)
    { NextInternalL(aStateFile,NULL,aSeed); }
void TN70Entropy::NextInternalL(const TDesC& aStateFile,
    const TDesC* aProvisioningFile, TDes8& aSeed)
    {
    aSeed.Zero();
    if (aSeed.MaxLength()<32 || aStateFile.Length()<4 ||
        aStateFile[1]!=':' || aStateFile[2]!='\\' ||
        aStateFile.Length()>KMaxFileName-5 ||
        (aProvisioningFile && (aProvisioningFile->Length()<4 ||
        (*aProvisioningFile)[1]!=':' || (*aProvisioningFile)[2]!='\\' ||
        aStateFile.CompareF(*aProvisioningFile)==0))) User::Leave(KErrArgument);
    TFileName lockName(aStateFile); lockName.Append(_L(".lock"));
    TFileName nextName(aStateFile); nextName.Append(_L(".next"));
    if (aProvisioningFile && (nextName.CompareF(*aProvisioningFile)==0 ||
        lockName.CompareF(*aProvisioningFile)==0)) User::Leave(KErrArgument);
    RFs fs; User::LeaveIfError(fs.Connect()); CleanupClosePushL(fs);
    TParse parsed; User::LeaveIfError(parsed.Set(aStateFile,NULL,NULL));
    TInt error=fs.MkDirAll(parsed.DriveAndPath());
    if (error!=KErrAlreadyExists) User::LeaveIfError(error);
    // The lock file is never renamed: an exclusive open serializes processes
    // as well as threads. A competing request fails with KErrInUse.
    RFile lock;
    error=lock.Open(fs,lockName,EFileRead|EFileWrite|EFileShareExclusive);
    if (error==KErrNotFound)
        error=lock.Create(fs,lockName,EFileRead|EFileWrite|EFileShareExclusive);
    User::LeaveIfError(error); CleanupClosePushL(lock);
    TSeedSecrets secrets;
    CleanupStack::PushL(TCleanupItem(ClearSecrets,&secrets));
    TEntry entry;
    error=fs.Entry(aStateFile,entry);
    if (error==KErrNotFound)
        {
        if (!aProvisioningFile) User::Leave(KErrNotReady);
        ReadSecretL(fs,*aProvisioningFile,secrets.iOld);
        }
    else
        {
        User::LeaveIfError(error);
        // Corrupt or inaccessible state must not silently reinitialize.
        ReadSecretL(fs,aStateFile,secrets.iRecord,N70_ENTROPY_RECORD_SIZE);
        secrets.iOld.SetLength(32);
        if (!n70_entropy_decode(secrets.iRecord.Ptr(),secrets.iRecord.Length(),
            const_cast<TUint8*>(secrets.iOld.Ptr()))) User::Leave(KErrCorrupt);
        }
    // Personalization/uniqueness, not a claimed source of secret entropy.
    TTime now; now.UniversalTime();
    TBuf8<96> context;
    TDateTime date=now.DateTime();
    context.Format(_L8("%d/%d/%d %d:%d:%d.%d %u"),date.Year(),
        date.Month(),date.Day(),date.Hour(),date.Minute(),date.Second(),
        date.MicroSecond(),User::TickCount());
    n70_entropy_derive(secrets.iOld.Ptr(),context.Ptr(),context.Length(),secrets.iDerived);
    secrets.iRecord.SetLength(N70_ENTROPY_RECORD_SIZE);
    n70_entropy_encode(secrets.iDerived,const_cast<TUint8*>(secrets.iRecord.Ptr()));
    RFile next;
    User::LeaveIfError(next.Replace(fs,nextName,EFileWrite|EFileShareExclusive));
    CleanupClosePushL(next);
    User::LeaveIfError(next.Write(secrets.iRecord));
    User::LeaveIfError(next.Flush());
    CleanupStack::PopAndDestroy(&next);
    // Commit before exposing TLS bytes. If this fails, no seed is returned.
    User::LeaveIfError(fs.Replace(nextName,aStateFile));
    // Discard initial material after the first commit, or after an upgrade
    // which provisioned a fresh file alongside an existing state.
    if (aProvisioningFile)
        {
        error=fs.Delete(*aProvisioningFile);
        if (error!=KErrNotFound) User::LeaveIfError(error);
        }
    aSeed.Copy(TPtrC8(secrets.iDerived+32,32));
    CleanupStack::PopAndDestroy(); // secrets
    CleanupStack::PopAndDestroy(&lock);
    CleanupStack::PopAndDestroy(&fs);
    }

TBool TN70Entropy::IsInitializedL(const TDesC& aStateFile)
    {
    RFs fs; User::LeaveIfError(fs.Connect()); CleanupClosePushL(fs);
    TEntry entry; TInt error=fs.Entry(aStateFile,entry);
    if (error==KErrNotFound || error==KErrPathNotFound)
        { CleanupStack::PopAndDestroy(&fs); return EFalse; }
    User::LeaveIfError(error);
    TSeedSecrets secrets; CleanupStack::PushL(TCleanupItem(ClearSecrets,&secrets));
    ReadSecretL(fs,aStateFile,secrets.iRecord,N70_ENTROPY_RECORD_SIZE);
    secrets.iOld.SetLength(32);
    if (!n70_entropy_decode(secrets.iRecord.Ptr(),secrets.iRecord.Length(),
        const_cast<TUint8*>(secrets.iOld.Ptr()))) User::Leave(KErrCorrupt);
    CleanupStack::PopAndDestroy(); CleanupStack::PopAndDestroy(&fs);
    return ETrue;
    }
void TN70Entropy::InitializeFromTimingL(const TDesC& aStateFile,const TDesC8& aSamples)
    {
    TSeedSecrets secrets; CleanupStack::PushL(TCleanupItem(ClearSecrets,&secrets));
    secrets.iOld.SetLength(32);
    if (!n70_entropy_from_timing(aSamples.Ptr(),aSamples.Length(),
        const_cast<TUint8*>(secrets.iOld.Ptr()))) User::Leave(KErrArgument);
    InstallSeedL(aStateFile,secrets.iOld);
    CleanupStack::PopAndDestroy();
    }
void TN70Entropy::InstallSeedL(const TDesC& aStateFile,const TDesC8& aSeed)
    {
    if (aSeed.Length()!=32 || aStateFile.Length()<4 || aStateFile.Length()>KMaxFileName-5 ||
        aStateFile[1]!=':' || aStateFile[2]!='\\') User::Leave(KErrArgument);
    TSeedSecrets secrets; CleanupStack::PushL(TCleanupItem(ClearSecrets,&secrets));
    secrets.iOld.Copy(aSeed);
    RFs fs; User::LeaveIfError(fs.Connect()); CleanupClosePushL(fs);
    TParse parsed; User::LeaveIfError(parsed.Set(aStateFile,NULL,NULL));
    TInt error=fs.MkDirAll(parsed.DriveAndPath());
    if (error!=KErrAlreadyExists) User::LeaveIfError(error);
    TFileName lockName(aStateFile); lockName.Append(_L(".lock"));
    TFileName nextName(aStateFile); nextName.Append(_L(".next"));
    RFile lock;
    error=lock.Open(fs,lockName,EFileRead|EFileWrite|EFileShareExclusive);
    if (error==KErrNotFound) error=lock.Create(fs,lockName,EFileRead|EFileWrite|EFileShareExclusive);
    User::LeaveIfError(error); CleanupClosePushL(lock);
    TEntry entry; error=fs.Entry(aStateFile,entry);
    if (error==KErrNone) User::Leave(KErrAlreadyExists);
    if (error!=KErrNotFound) User::LeaveIfError(error);
    secrets.iRecord.SetLength(N70_ENTROPY_RECORD_SIZE);
    n70_entropy_encode(secrets.iOld.Ptr(),const_cast<TUint8*>(secrets.iRecord.Ptr()));
    RFile next; User::LeaveIfError(next.Replace(fs,nextName,EFileWrite|EFileShareExclusive));
    CleanupClosePushL(next);
    User::LeaveIfError(next.Write(secrets.iRecord)); User::LeaveIfError(next.Flush());
    CleanupStack::PopAndDestroy(&next);
    User::LeaveIfError(fs.Replace(nextName,aStateFile));
    CleanupStack::PopAndDestroy(&lock); CleanupStack::PopAndDestroy(&fs);
    CleanupStack::PopAndDestroy();
    }

void TN70Entropy::InitializeAutomaticL(const TDesC& aStateFile)
    {
    // Intentionally best-effort: these observations have no assessed
    // min-entropy on N70. Hashing them cannot manufacture secret entropy.
    if (IsInitializedL(aStateFile)) return;
    TSeedSecrets secrets; CleanupStack::PushL(TCleanupItem(ClearSecrets,&secrets));
    br_sha256_context hash; br_sha256_init(&hash);
    static const char domain[]="N70 automatic experimental seed v1";
    br_sha256_update(&hash,domain,sizeof domain-1);
    for (TInt i=0; i<64; ++i)
        {
        TTime now; now.UniversalTime(); TDateTime date=now.DateTime();
        TUint32 data[8];
        data[0]=Math::Random(); data[1]=User::FastCounter(); data[2]=User::TickCount();
        data[3]=date.Year(); data[4]=date.Month()*32+date.Day();
        data[5]=date.Hour()*3600+date.Minute()*60+date.Second();
        data[6]=date.MicroSecond(); data[7]=i;
        br_sha256_update(&hash,data,sizeof data);
        TInt delay=1000+(data[0]&1023);
        Mem::FillZ(data,sizeof data);
        User::After(delay);
        TUint32 after=User::FastCounter();
        br_sha256_update(&hash,&after,sizeof after); after=0;
        }
    secrets.iOld.SetLength(32);
    br_sha256_out(&hash,const_cast<TUint8*>(secrets.iOld.Ptr()));
    volatile TUint8* wipe=reinterpret_cast<volatile TUint8*>(&hash);
    for (TUint j=0; j<sizeof hash; ++j) wipe[j]=0;
    TRAPD(error,InstallSeedL(aStateFile,secrets.iOld));
    CleanupStack::PopAndDestroy();
    // A competing initializer may have installed its state while collecting.
    if (error==KErrAlreadyExists)
        { if (!IsInitializedL(aStateFile)) User::Leave(KErrNotReady); }
    else User::LeaveIfError(error);
    }
void TN70Entropy::NextAutomaticSeedL(const TDesC& aStateFile,TDes8& aSeed)
    {
    aSeed.Zero();
    InitializeAutomaticL(aStateFile);
    NextDeviceSeedL(aStateFile,aSeed);
    }
