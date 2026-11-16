/* Copyright (c) 2004, Nokia. All rights reserved */


#ifndef __N70TEST_PAN__
#define __N70TEST_PAN__

/** N70Test application panic codes */
enum TN70TestPanics
    {
    EN70TestUi = 1
    // add further panics here
    };

inline void Panic(TN70TestPanics aReason)
    {
    _LIT(applicationName,"N70Test");
    User::Panic(applicationName, aReason);
    }

#endif // __N70TEST_PAN__
