/* Copyright (c) 2004, Nokia. All rights reserved */


// INCLUDE FILES
#include "N70TestDocument.h"
#include "N70TestApplication.h"

// ============================ MEMBER FUNCTIONS ===============================

// UID for the application;
// this should correspond to the uid defined in the mmp file
const TUid KUidN70TestApp = { 0x70007121 };

// -----------------------------------------------------------------------------
// CN70TestApplication::CreateDocumentL()
// Creates CApaDocument object
// -----------------------------------------------------------------------------
//
CApaDocument* CN70TestApplication::CreateDocumentL()
    {
    // Create an N70Test document, and return a pointer to it
    return (static_cast<CApaDocument*>
                    ( CN70TestDocument::NewL( *this ) ) );
    }

// -----------------------------------------------------------------------------
// CN70TestApplication::AppDllUid()
// Returns application UID
// -----------------------------------------------------------------------------
//
TUid CN70TestApplication::AppDllUid() const
    {
    // Return the UID for the N70Test application
    return KUidN70TestApp;
    }

// End of File