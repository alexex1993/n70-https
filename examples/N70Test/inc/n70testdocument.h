/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __N70TESTDOCUMENT_H__
#define __N70TESTDOCUMENT_H__

// INCLUDES
#include <akndoc.h>

// FORWARD DECLARATIONS
class CN70TestAppUi;
class CEikApplication;


// CLASS DECLARATION

/**
* CN70TestDocument application class.
* An instance of class CN70TestDocument is the Document part of the
* AVKON application framework for the N70Test example application.
*/
class CN70TestDocument : public CAknDocument
    {
    public: // Constructors and destructor

        /**
        * NewL.
        * Two-phased constructor.
        * Construct a CN70TestDocument for the AVKON application aApp
        * using two phase construction, and return a pointer
        * to the created object.
        * @param aApp Application creating this document.
        * @return A pointer to the created instance of CN70TestDocument.
        */
        static CN70TestDocument* NewL( CEikApplication& aApp );

        /**
        * NewLC.
        * Two-phased constructor.
        * Construct a CN70TestDocument for the AVKON application aApp
        * using two phase construction, and return a pointer
        * to the created object.
        * @param aApp Application creating this document.
        * @return A pointer to the created instance of CN70TestDocument.
        */
        static CN70TestDocument* NewLC( CEikApplication& aApp );

        /**
        * ~CN70TestDocument
        * Virtual Destructor.
        */
        virtual ~CN70TestDocument();

    public: // Functions from base classes

        /**
        * CreateAppUiL
        * From CEikDocument, CreateAppUiL.
        * Create a CN70TestAppUi object and return a pointer to it.
        * The object returned is owned by the Uikon framework.
        * @return Pointer to created instance of AppUi.
        */
        CEikAppUi* CreateAppUiL();

    private: // Constructors

        /**
        * ConstructL
        * 2nd phase constructor.
        */
        void ConstructL();

        /**
        * CN70TestDocument.
        * C++ default constructor.
        * @param aApp Application creating this document.
        */
        CN70TestDocument( CEikApplication& aApp );

    };

#endif // __N70TESTDOCUMENT_H__

// End of File