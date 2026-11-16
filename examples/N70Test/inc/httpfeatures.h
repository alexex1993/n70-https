#ifndef N70_FEATURES_H
#define N70_FEATURES_H
#include <e32base.h>
class CN70CancelToken;
// 2=features,3=timeout,4=cancel,5=API/gzip,6=TCP,7=methods/reuse/retry/reconnect.
void TestHttpFeaturesL(TDes& aStage,CN70CancelToken* aCancel,TInt aMode);
#endif
