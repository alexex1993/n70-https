/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __N70TESTAPPVIEW_H__
#define __N70TESTAPPVIEW_H__

// INCLUDES
#include <coecntrl.h>

// CLASS DECLARATION
class CN70TestAppView : public CCoeControl
    {
    public: // New methods

        /**
        * NewL.
        * Two-phased constructor.
        * Create a CN70TestAppView object, which will draw itself to aRect.
        * @param aRect The rectangle this view will be drawn to.
        * @return a pointer to the created instance of CN70TestAppView.
        */
        static CN70TestAppView* NewL( const TRect& aRect );

        /**
        * NewLC.
        * Two-phased constructor.
        * Create a CN70TestAppView object, which will draw itself
        * to aRect.
        * @param aRect Rectangle this view will be drawn to.
        * @return A pointer to the created instance of CN70TestAppView.
        */
        static CN70TestAppView* NewLC( const TRect& aRect );

        /**
        * ~CN70TestAppView
        * Virtual Destructor.
        */
        virtual ~CN70TestAppView();

    public:  // Functions from base classes

        /**
        * From CCoeControl, Draw
        * Draw this CN70TestAppView to the screen.
        * @param aRect the rectangle of this view that needs updating
        */
        void Draw( const TRect& aRect ) const;

    private: // Constructors

        /**
        * ConstructL
        * 2nd phase constructor.
        * Perform the second phase construction of a
        * CN70TestAppView object.
        * @param aRect The rectangle this view will be drawn to.
        */
        void ConstructL(const TRect& aRect);

        /**
        * CN70TestAppView.
        * C++ default constructor.
        */
        CN70TestAppView();

    };

#endif // __N70TESTAPPVIEW_H__

// End of File
