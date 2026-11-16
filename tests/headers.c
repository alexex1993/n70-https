#include "n70headers.h"
#include "n70http.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{++tests;if(!(x)){fprintf(stderr,"FAIL headers line %d\n",__LINE__);return 1;}}while(0)
int main(void)
{
    char data[4096],saved[4096];size_t len=0,old;unsigned tests=0,i;
    static const char*const blocked[]={"Host","Content-Length","Content-Type","Transfer-Encoding","Connection",
        "Accept-Encoding","TE","Trailer","Upgrade","Proxy-Authorization","Proxy-Connection","Expect","Keep-Alive"};
    CHECK(!n70_headers_set(data,&len,sizeof data,"Authorization",13,"Bearer n70-test",15));
    CHECK(n70_headers_has(data,len,"authorization",13));
    CHECK(!n70_headers_set(data,&len,sizeof data,"authorization",13,"Bearer next",11));
    CHECK(len==28&&!memcmp(data,"authorization: Bearer next\r\n",28));
    CHECK(!n70_headers_set(data,&len,sizeof data,"X-Test",6,"one",3));
    CHECK(n70_headers_has(data,len,"x-TEST",6));old=len;memcpy(saved,data,len);
    for(i=0;i<sizeof blocked/sizeof blocked[0];++i){
        CHECK(n70_headers_set(data,&len,sizeof data,blocked[i],strlen(blocked[i]),"bad",3)==N70_BAD);
        CHECK(len==old&&!memcmp(data,saved,len));
    }
    for(i=0;i<256;++i){
        char value[3]={'a',(char)i,'b'};int invalid=(i<32&&i!=9)||i>=127;
        int r=n70_headers_set(data,&len,sizeof data,"X-Value",7,value,3);
        CHECK((r==N70_BAD)==invalid);
    }
    CHECK(n70_headers_set(data,&len,sizeof data,"X\r\nInjected",11,"value",5)==N70_BAD);
    CHECK(n70_headers_set(data,&len,sizeof data,"",0,"value",5)==N70_BAD);
    CHECK(n70_headers_set(data,&len,sizeof data,"Bad Name",8,"value",5)==N70_BAD);
    old=len;memcpy(saved,data,len);
    CHECK(n70_headers_set(data,&len,len,"Authorization",13,"longer than the previous token",30)==N70_LIMIT);
    CHECK(len==old&&!memcmp(data,saved,len));
    CHECK(n70_bearer_valid("abc-._~+/09==",13));
    CHECK(!n70_bearer_valid("",0));CHECK(!n70_bearer_valid("=",1));
    CHECK(!n70_bearer_valid("abc def",7));CHECK(!n70_bearer_valid("abc\r\nX:x",8));
    CHECK(!n70_bearer_valid("a=b",3));CHECK(!n70_bearer_valid("a\0b",3));
    CHECK(n70_headers_forward(1,1));CHECK(!n70_headers_forward(1,0));CHECK(!n70_headers_forward(0,1));
    puts("PASS: header replacement, reserved fields, all byte values, injection, size transactions, Bearer grammar and sticky redirect stripping.");
    printf("%u header checks\n",tests);return 0;
}
