#include "n70http.h"
#include "n70gzip.h"
#include "n70headers.h"
/* Real host-side TLS 1.2 interoperability checks. This uses the same BearSSL,
 * roots, site list and seed-derivation core; it does not test Symbian sockets. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <bearssl.h>
#include "testanchor.h"
#include "n70testsites.h"
#include "n70entropy_core.h"
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

struct transport { SOCKET socket; int error; };
static int send_data(void *opaque, const unsigned char *data, size_t len)
{
    struct transport *t = opaque;
    int n = send(t->socket, (const char *)data, (int)len, 0);
    if (n <= 0) { t->error = WSAGetLastError(); return -1; }
    return n;
}
static int recv_data(void *opaque, unsigned char *data, size_t len)
{
    struct transport *t = opaque;
    int n = recv(t->socket, (char *)data, (int)len, 0);
    if (n <= 0) { t->error = WSAGetLastError(); return -1; }
    return n;
}
struct http_output {uint64_t bytes;char preview[2049];size_t used;n70_gzip*gzip;int compressed;};
static int http_body(void*p,const unsigned char*d,size_t n) {
    struct http_output*o=p;size_t take=n;
    o->bytes+=n;if(take>2048-o->used)take=2048-o->used;
    memcpy(o->preview+o->used,d,take);o->used+=take;o->preview[o->used]=0;return 0;
}
static void* gz_alloc(void*ctx,unsigned n,unsigned size){(void)ctx;return calloc(n,size);}
static void gz_free(void*ctx,void*p){(void)ctx;free(p);}
static int wire_body(void*p,const unsigned char*d,size_t n)
{struct http_output*o=p;return o->compressed?n70_gzip_feed(o->gzip,d,n,http_body,p)<0:http_body(p,d,n);}
static int check_http(const char *host,const char* path,const char* method,const char* payload,int expected,uint64_t expected_bytes,const unsigned char seed[32],const char*extra,const char*needle,int want_gzip)
{
    struct addrinfo hints, *list = NULL, *p;
    struct transport t;
    DWORD timeout = 10000;
    br_ssl_client_context *client = calloc(1, sizeof *client);
    br_x509_minimal_context x509;
    br_x509_trust_anchor anchors[TEST_ANCHOR_COUNT];
    br_sslio_context io;
    unsigned char *buffer = malloc(BR_SSL_BUFSIZE_BIDI);
    char request[1024], response[16385];
    size_t used = 0;
    n70_http* parser=malloc(sizeof(n70_http)); struct http_output output; int complete=0;
    memset(&output,0,sizeof output);output.gzip=calloc(1,sizeof(n70_gzip));if(parser)n70_http_init(parser,1048576,!strcmp(method,"HEAD"));
    int result = 1, n;
    t.socket = INVALID_SOCKET; t.error = 0;
    if (!client || !buffer || !parser || !output.gzip) goto done;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, "443", &hints, &list)) goto done;
    for (p = list; p; p = p->ai_next) {
        u_long nonblocking = 1;
        fd_set writable;
        struct timeval wait = { 8, 0 };
        int err = 0, errlen = sizeof err;
        t.socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (t.socket == INVALID_SOCKET) continue;
        ioctlsocket(t.socket, FIONBIO, &nonblocking);
        n = connect(t.socket, p->ai_addr, (int)p->ai_addrlen);
        FD_ZERO(&writable); FD_SET(t.socket, &writable);
        if (!n || (WSAGetLastError() == WSAEWOULDBLOCK &&
            select(0, NULL, &writable, NULL, &wait) > 0 &&
            !getsockopt(t.socket, SOL_SOCKET, SO_ERROR, (char *)&err, &errlen) && !err)) {
            nonblocking = 0; ioctlsocket(t.socket, FIONBIO, &nonblocking);
            break;
        }
        closesocket(t.socket); t.socket = INVALID_SOCKET;
    }
    if (t.socket == INVALID_SOCKET) goto done;
    setsockopt(t.socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout);
    setsockopt(t.socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof timeout);
    InitTestAnchors(anchors);
    br_ssl_client_init_full(client, &x509, anchors, TEST_ANCHOR_COUNT);
    br_ssl_engine_set_versions(&client->eng, BR_TLS12, BR_TLS12);
    br_ssl_engine_set_buffer(&client->eng, buffer, BR_SSL_BUFSIZE_BIDI, 1);
    br_ssl_engine_inject_entropy(&client->eng, seed, 32);
    if (!br_ssl_client_reset(client, host, 0)) goto done;
    br_sslio_init(&io, &client->eng, recv_data, &t, send_data, &t);
    n = snprintf(request, sizeof request,
        "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: N70Https/0.8\r\nAccept: */*\r\nConnection: close\r\nAccept-Encoding: gzip, identity\r\n%sContent-Type: application/json\r\nContent-Length: %u\r\n\r\n%s", method,path,host,extra,(unsigned)strlen(payload),payload);
    if (br_sslio_write_all(&io, request, (size_t)n) < 0 || br_sslio_flush(&io) < 0) goto done;
    response[0]=0;
    while(!complete) {
        unsigned char data[2048]; size_t offset=0;
        n=br_sslio_read(&io,data,sizeof data);
        if(n<0) {
            if(br_ssl_engine_current_state(&client->eng)==BR_SSL_CLOSED && !br_ssl_engine_last_error(&client->eng) && n70_http_eof(parser)==N70_DONE)complete=1;
            break;
        }
        if(!n)break;
        while(offset<(size_t)n) {
            size_t consumed=0;int event=n70_http_feed(parser,data+offset,n-offset,&consumed,wire_body,&output);
            offset+=consumed;
            if(event<0)goto done;
            if(event==N70_HEADERS) {
                size_t first=strcspn(parser->headers,"\r\n");if(first>100)first=100;
                memcpy(response,parser->headers,first);response[first]=0;used=first;
                if(parser->gzip&&strcmp(method,"HEAD")&&parser->status!=204&&parser->status!=304){
                    if(n70_gzip_init(output.gzip,1048576,gz_alloc,gz_free,NULL))goto done;output.compressed=1;
                }
            }
            if(event==N70_DONE){complete=1;break;}
            if(!consumed&&event!=N70_HEADERS)goto done;
        }
        {size_t ignored;if(n70_http_feed(parser,NULL,0,&ignored,wire_body,&output)==N70_DONE)complete=1;}
    }
    if(complete && parser->status==expected && br_ssl_engine_get_version(&client->eng)==BR_TLS12 &&
        !br_ssl_engine_last_error(&client->eng) && (!expected_bytes||output.bytes==expected_bytes) &&
        (strcmp(method,"POST")||strstr(output.preview,"tls12"))&&
        (!needle||strstr(output.preview,needle))&&(!want_gzip||output.compressed)&&
        (!output.compressed||n70_gzip_eof(output.gzip)==N70_DONE))result=0;
done:
    response[used] = 0;
    response[strcspn(response, "\r\n")] = 0;
    printf("%s %s | %s | TLS=0x%04x BearSSL=%d IO=%d | %.100s", method,path,
        result ? "FAIL" : "PASS", client ? br_ssl_engine_get_version(&client->eng) : 0,
        client ? br_ssl_engine_last_error(&client->eng) : -1, t.error, response);
    printf(" | decoded=%llu wire=%llu chunked=%d gzip=%d\n",(unsigned long long)output.bytes,
        (unsigned long long)(parser?parser->body_bytes:0),parser?parser->chunked:0,output.compressed);
    fflush(stdout);
    if (t.socket != INVALID_SOCKET) closesocket(t.socket);
    if (list) freeaddrinfo(list);
    if (client) { SecureZeroMemory(client, sizeof *client); free(client); }
    if (buffer) { SecureZeroMemory(buffer, BR_SSL_BUFSIZE_BIDI); free(buffer); }
    if(output.gzip){n70_gzip_close(output.gzip);free(output.gzip);}free(parser);return result;
}
int main(void)
{
    WSADATA wsa;
    unsigned char secret[32], material[64];
    int i, failures = 0;
    if (WSAStartup(MAKEWORD(2, 2), &wsa)) return 2;
    if (BCryptGenRandom(NULL, secret, sizeof secret, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 2;
    for(i=0;i<10;i++) {
        static const char*const paths[]={"/get","/post","/stream/20","/bytes/131072","/redirect/1","/get","/bearer","/headers","/gzip","/gzip"};
        const char* method=i==1?"POST":(i==5?"HEAD":"GET");
        char extra[4096];size_t extra_len=0;const char*needle=NULL;
        if(i==6||i==9){const char*v="Bearer n70-public-test-080";
            if(n70_headers_set(extra,&extra_len,sizeof extra,"Authorization",13,v,strlen(v)))return 3;needle="n70-public-test-080";}
        if(i==7){if(n70_headers_set(extra,&extra_len,sizeof extra,"X-N70-Test",10,"custom-header-080",17))return 3;needle="custom-header-080";}
        if(i==8)needle="\"gzipped\": true";extra[extra_len]=0;
        n70_entropy_derive(secret,NULL,0,material);memcpy(secret,material,32);
        failures+=check_http("httpbingo.org",paths[i],method,i==1?"{\"n70\":\"tls12\"}":"",i==4?302:200,i==3?131072:0,material+32,extra,needle,i>=8);
    }
    SecureZeroMemory(secret, sizeof secret); SecureZeroMemory(material, sizeof material);
    WSACleanup();
    printf("Summary: %d/%d passed (host BearSSL, not N70)\n", 10 - failures, 10);
    return failures ? 1 : 0;
}
