/* Copyright (c) 2004, Nokia. All rights reserved */


// INCLUDE FILES
#include "N70TestAppUi.h"
#include "N70TestDocument.h"

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CN70TestDocument::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CN70TestDocument* CN70TestDocument::NewL( CEikApplication&
                                                          aApp )
    {
    CN70TestDocument* self = NewLC( aApp );
    CleanupStack::Pop( self );
    return self;
    }

// -----------------------------------------------------------------------------
// CN70TestDocument::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CN70TestDocument* CN70TestDocument::NewLC( CEikApplication&
                                                           aApp )
    {
    CN70TestDocument* self =
        new ( ELeave ) CN70TestDocument( aApp );

    CleanupStack::PushL( self );
    self->ConstructL();
    return self;
    }

// -----------------------------------------------------------------------------
// CN70TestDocument::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CN70TestDocument::ConstructL()
    {
    // No implementation required
    }

// -----------------------------------------------------------------------------
// CN70TestDocument::CN70TestDocument()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CN70TestDocument::CN70TestDocument( CEikApplication& aApp )
    : CAknDocument( aApp )
    {
    // No implementation required
    }

// ---------------------------------------------------------------------------
// CN70TestDocument::~CN70TestDocument()
// Destructor.
// ---------------------------------------------------------------------------
//
CN70TestDocument::~CN70TestDocument()
    {
    // No implementation required
    }

// ---------------------------------------------------------------------------
// CN70TestDocument::CreateAppUiL()
// Constructs CreateAppUi.
// ---------------------------------------------------------------------------
//
CEikAppUi* CN70TestDocument::CreateAppUiL()
    {
    // Create the application user interface, and return a pointer to it;
    // the framework takes ownership of this object
    return ( static_cast <CEikAppUi*> ( new ( ELeave )
                                        CN70TestAppUi ) );
    }

// End of File