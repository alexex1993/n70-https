#ifndef N70ENTROPY_H
#define N70ENTROPY_H
#include <e32base.h>

// Persistent HMAC-DRBG seeding for systems without a public CSPRNG API.
// InitializeFromTimingL is an experimental input source, not an audited RNG.
// Legacy NextSeedL requires a unique secret provisioning seed; the common
// test SIS does not use or ship such a file.
// State is rotated and flushed before seed is returned. Never restore/copy
// state between devices, or distribute a common provisioning secret.
class TN70Entropy
    {
public:
    // Unattended experimental initialization. No guaranteed entropy strength.
    // Keeps valid existing state; never resets corrupt state automatically.
    static void InitializeAutomaticL(const TDesC& aStateFile);
    static void NextAutomaticSeedL(const TDesC& aStateFile,TDes8& aSeed);
    static TBool IsInitializedL(const TDesC& aStateFile);
    // Experimental human input timing, not an assessed CSPRNG source.
    // 128 records (fast counter, ticks, ASCII digit, microseconds), LE uint32.
    // Refuses existing state, even if corrupt; no shared bootstrap fallback.
    static void InitializeFromTimingL(const TDesC& aStateFile, const TDesC8& aSamples);
    static void NextDeviceSeedL(const TDesC& aStateFile, TDes8& aSeed);
    static void NextSeedL(const TDesC& aStateFile,
        const TDesC& aProvisioningFile, TDes8& aSeed);
private:
    static void InstallSeedL(const TDesC& aStateFile,const TDesC8& aSeed);
    static void NextInternalL(const TDesC& aStateFile,
        const TDesC* aProvisioningFile, TDes8& aSeed);
    };
#endif
