#include "testrunner.h"
/* Copyright (c) 2004, Nokia. All rights reserved */

// INCLUDE FILES
#include <avkon.hrh>
#include <aknnotewrappers.h>
#include <stringloader.h>
#include <N70Test.rsg>

#include "N70Test.pan"
#include "N70TestAppUi.h"
#include "N70TestAppView.h"
#include "N70Test.hrh"

// ============================ MEMBER FUNCTIONS ===============================


// -----------------------------------------------------------------------------
// CN70TestAppUi::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CN70TestAppUi::ConstructL()
    {
    // Initialise app UI with standard value.
    BaseConstructL();
    iRunner=CTestRunner::NewL();

    // Create view object
    iAppView = CN70TestAppView::NewL( ClientRect() );
    AddToStackL(iAppView);
    }

// -----------------------------------------------------------------------------
// CN70TestAppUi::CN70TestAppUi()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CN70TestAppUi::CN70TestAppUi()
    {
    // No implementation required
    }

// -----------------------------------------------------------------------------
// CN70TestAppUi::~CN70TestAppUi()
// Destructor.
// -----------------------------------------------------------------------------
//
CN70TestAppUi::~CN70TestAppUi()
    {
    delete iRunner;
    if ( iAppView )
        {
        RemoveFromStack(iAppView);
        delete iAppView;
        iAppView = NULL;
        }
    }

// -----------------------------------------------------------------------------
// CN70TestAppUi::HandleCommandL()
// Takes care of command handling.
// -----------------------------------------------------------------------------
//
void CN70TestAppUi::HandleCommandL( TInt aCommand )
    {
    switch( aCommand )
        {
        case EEikCmdExit:
        case EAknSoftkeyExit:
            Exit();
            break;

        case EN70TestCommand1:
            {
            iRunner->StartL();
            }
            break;

        case EN70TestTenSites:
            iRunner->StartL(1);
            break;
        case EN70TestHttpFeatures:
            iRunner->StartL(2);
            break;
        case EN70TestApi:
            iRunner->StartL(5);
            break;
        case EN70TestClient:
            iRunner->StartL(7);
            break;
        case EN70TestTcp:
            iRunner->StartL(6);
            break;
        case EN70TestTimeout:
            iRunner->StartL(3);
            break;
        case EN70TestCancellation:
            if(!iRunner->IsActive())
                {
                (new(ELeave) CAknInformationNote)->ExecuteLD(_L("While running: Options > Cancel request"));
                iRunner->StartL(4);
                }
            break;
        case EN70TestCancel:
            iRunner->CancelWork();
            break;

        default:
            Panic( EN70TestUi );
            break;
        }
    }

// End of File
