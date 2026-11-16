/* Copyright (c) 2004, Nokia. All rights reserved */


// INCLUDE FILES
#include <coemain.h>
#include "N70TestAppView.h"





// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CN70TestAppView::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CN70TestAppView* CN70TestAppView::NewL( const TRect& aRect )
    {
    CN70TestAppView* self = CN70TestAppView::NewLC( aRect );
    CleanupStack::Pop( self );
    return self;
    }

// -----------------------------------------------------------------------------
// CN70TestAppView::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CN70TestAppView* CN70TestAppView::NewLC( const TRect& aRect )
    {
    CN70TestAppView* self = new ( ELeave ) CN70TestAppView;
    CleanupStack::PushL( self );
    self->ConstructL( aRect );
    return self;
    }

// -----------------------------------------------------------------------------
// CN70TestAppView::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CN70TestAppView::ConstructL( const TRect& aRect )
    {
    // Create a window for this application view
    CreateWindowL();

    // Set the windows size
    SetRect( aRect );

    // Activate the window, which makes it ready to be drawn
    ActivateL();
    }

// -----------------------------------------------------------------------------
// CN70TestAppView::CN70TestAppView()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CN70TestAppView::CN70TestAppView()
    {
    // No implementation required
    }


// -----------------------------------------------------------------------------
// CN70TestAppView::~CN70TestAppView()
// Destructor.
// -----------------------------------------------------------------------------
//
CN70TestAppView::~CN70TestAppView()
    {
    // No secret input is stored in the view.
    }

// -----------------------------------------------------------------------------
// CN70TestAppView::Draw()
// Draws the display.
// -----------------------------------------------------------------------------
//
void CN70TestAppView::Draw( const TRect& /*aRect*/ ) const
    {
    // Get the standard graphics context
    CWindowGc& gc = SystemGc();

    // Gets the control's extent
    TRect rect = Rect();

    // Clears the screen
    gc.Clear( rect );
    const CFont* font=iCoeEnv->NormalFont();
    gc.UseFont(font);
    gc.SetPenColor(KRgbBlack);
    TInt y=font->AscentInPixels()+6;
    TInt step=font->HeightInPixels()+5;
    gc.DrawText(_L("N70 HTTPS 0.9.1 / TLS 1.2"),TPoint(4,y)); y+=step;
    gc.DrawText(_L("Options > Test HTTPS"),TPoint(4,y)); y+=step;
    gc.DrawText(_L("Test client 0.9: 14 checks"),TPoint(4,y)); y+=step;
    gc.DrawText(_L("Methods / reuse / retries"),TPoint(4,y)); y+=step;
    gc.DrawText(_L("Log: client-tests.txt"),TPoint(4,y)); y+=step;
    gc.DrawText(_L("API + gzip / TCP tests"),TPoint(4,y));
    gc.DiscardFont();
    }

// End of File
