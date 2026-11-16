/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __N70TESTAPPUI_H__
#define __N70TESTAPPUI_H__

// INCLUDES
#include <aknappui.h>


// FORWARD DECLARATIONS
class CN70TestAppView;
class CTestRunner;


// CLASS DECLARATION
/**
* CN70TestAppUi application UI class.
* Interacts with the user through the UI and request message processing
* from the handler class
*/
class CN70TestAppUi : public CAknAppUi
    {
    public: // Constructors and destructor

        /**
        * ConstructL.
        * 2nd phase constructor.
        */
        void ConstructL();

        /**
        * CN70TestAppUi.
        * C++ default constructor. This needs to be public due to
        * the way the framework constructs the AppUi
        */
        CN70TestAppUi();

        /**
        * ~CN70TestAppUi.
        * Virtual Destructor.
        */
        virtual ~CN70TestAppUi();

    public:  // Functions from base classes

        /**
        * From CEikAppUi, HandleCommandL.
        * Takes care of command handling.
        * @param aCommand Command to be handled.
        */
        void HandleCommandL( TInt aCommand );

    private: // Data

        /**
        * The application view
        * Owned by CN70TestAppUi
        */
        CN70TestAppView* iAppView;
        CTestRunner* iRunner;
    };

#endif // __N70TESTAPPUI_H__

// End of File