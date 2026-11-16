/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __N70TESTAPPLICATION_H__
#define __N70TESTAPPLICATION_H__

// INCLUDES
#include <aknapp.h>


// CLASS DECLARATION

/**
* CN70TestApplication application class.
* Provides factory to create concrete document object.
* An instance of CN70TestApplication is the application part of the
* AVKON application framework for the N70Test example application.
*/
class CN70TestApplication : public CAknApplication
    {
    public: // Functions from base classes

        /**
        * From CApaApplication, AppDllUid.
        * @return Application's UID (KUidN70TestApp).
        */
        TUid AppDllUid() const;

    protected: // Functions from base classes

        /**
        * From CApaApplication, CreateDocumentL.
        * Creates CN70TestDocument document object. The returned
        * pointer in not owned by the CN70TestApplication object.
        * @return A pointer to the created document object.
        */
        CApaDocument* CreateDocumentL();
    };

#endif // __N70TESTAPPLICATION_H__

// End of File