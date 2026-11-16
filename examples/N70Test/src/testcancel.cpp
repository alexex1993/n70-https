#include "testrunner.h"
void CTestRunner::DoCancel()
    {
    // CActive::Cancel performs the single wait for Logon after DoCancel.
    // Waiting here as well would consume the same completion twice.
    CancelWork();
    }
