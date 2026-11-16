#define main wait_tests_not_executed_here
#include "wait.cpp"
#undef main
#include "cancel_shim/testrunner.h"
static int cancel_tests()
{
    sim::now=0;sim::signals=0;sim::events.clear();checks=0;
    {CTestRunner c;c.Start();c.Cancel();CHECK(!c.active&&c.calls==1);CHECK(sim::now==10);CHECK(!sim::signals&&sim::events.empty());}
    {CTestRunner c;c.Start();sim::advance();c.Cancel();CHECK(!c.active&&c.calls==1);CHECK(!sim::signals&&sim::events.empty());}
    {CTestRunner c;c.Cancel();CHECK(!c.calls&&!sim::signals&&sim::events.empty());}
    {CTestRunner c;TRequestStatus other;c.Start();sim::schedule(other,sim::now+5,0);c.Cancel();
     CHECK(!c.active&&sim::signals==1);User::WaitForRequest(other);CHECK(!sim::signals&&sim::events.empty());}
    std::printf("PASS: %u checks of actual CTestRunner::DoCancel with framework-owned wait, immediate logon, inactive cancel and unrelated UI completions.\n",checks);return 0;
}
int main()
{
    try{return cancel_tests();}catch(const std::exception&e){std::fprintf(stderr,"FAIL cancel: %s\n",e.what());}return 1;
}
