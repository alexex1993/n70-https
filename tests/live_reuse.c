/* Host-only live interoperability: real BearSSL, shared HTTP/gzip/reuse policy.
 * The Symbian socket/cache driver is exercised by the new SIS device menu. */
#define main previous_http_test_main
#include "live_http.c"
#undef main
#include "n70policy.h"
struct persistent {
    struct transport t;br_ssl_client_context ssl;br_x509_minimal_context x509;
    br_x509_trust_anchor roots[TEST_ANCHOR_COUNT];br_sslio_context io;
    unsigned char buffer[BR_SSL_BUFSIZE_BIDI];int requests;
};
static int open_persistent(struct persistent*p,const unsigned char seed[32])
{
    struct addrinfo hints,*list=NULL,*a;DWORD timeout=15000;
    memset(&hints,0,sizeof hints);hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;p->t.socket=INVALID_SOCKET;
    if(getaddrinfo("httpbingo.org","443",&hints,&list))return 1;
    for(a=list;a;a=a->ai_next){
        u_long nonblock=1;fd_set writable;struct timeval wait={8,0};int e=0,size=sizeof e,r;
        p->t.socket=socket(a->ai_family,a->ai_socktype,a->ai_protocol);if(p->t.socket==INVALID_SOCKET)continue;
        ioctlsocket(p->t.socket,FIONBIO,&nonblock);r=connect(p->t.socket,a->ai_addr,(int)a->ai_addrlen);
        FD_ZERO(&writable);FD_SET(p->t.socket,&writable);
        if(!r||(WSAGetLastError()==WSAEWOULDBLOCK&&select(0,NULL,&writable,NULL,&wait)>0&&
            !getsockopt(p->t.socket,SOL_SOCKET,SO_ERROR,(char*)&e,&size)&&!e)){
            nonblock=0;ioctlsocket(p->t.socket,FIONBIO,&nonblock);break;
        }closesocket(p->t.socket);p->t.socket=INVALID_SOCKET;
    }freeaddrinfo(list);if(p->t.socket==INVALID_SOCKET)return 1;
    setsockopt(p->t.socket,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof timeout);
    setsockopt(p->t.socket,SOL_SOCKET,SO_SNDTIMEO,(const char*)&timeout,sizeof timeout);
    InitTestAnchors(p->roots);br_ssl_client_init_full(&p->ssl,&p->x509,p->roots,TEST_ANCHOR_COUNT);
    br_ssl_engine_set_versions(&p->ssl.eng,BR_TLS12,BR_TLS12);br_ssl_engine_add_flags(&p->ssl.eng,BR_OPT_NO_RENEGOTIATION);
    br_ssl_engine_set_buffer(&p->ssl.eng,p->buffer,sizeof p->buffer,1);br_ssl_engine_inject_entropy(&p->ssl.eng,seed,32);
    if(!br_ssl_client_reset(&p->ssl,"httpbingo.org",0))return 1;
    br_sslio_init(&p->io,&p->ssl.eng,recv_data,&p->t,send_data,&p->t);return 0;
}
static int persistent_request(struct persistent*c,const char*method,const char*path)
{
    char request[1024],echo[48];const char*payload=n70_method_has_body(method)?"{\"n70\":\"tls12-090\"}":"";
    n70_http*p=malloc(sizeof *p);struct http_output out;int result=1,done=0,clean=1,n;
    memset(&out,0,sizeof out);out.gzip=calloc(1,sizeof *out.gzip);if(!p||!out.gzip)goto end;
    n70_http_init(p,1048576,0);
    n=snprintf(request,sizeof request,"%s %s HTTP/1.1\r\nHost: httpbingo.org\r\nUser-Agent: N70Https/0.9\r\nAccept-Encoding: gzip, identity\r\nConnection: keep-alive\r\nContent-Type: application/json\r\nContent-Length: %u\r\n\r\n%s",method,path,(unsigned)strlen(payload),payload);
    if(br_sslio_write_all(&c->io,request,n)<0||br_sslio_flush(&c->io)<0)goto end;
    while(!done){unsigned char data[2048];size_t pos=0;n=br_sslio_read(&c->io,data,sizeof data);if(n<=0)goto end;
        while(pos<(size_t)n){size_t used=0;int e=n70_http_feed(p,data+pos,n-pos,&used,wire_body,&out);pos+=used;
            if(e<0)goto end;
            if(e==N70_HEADERS&&p->gzip){if(n70_gzip_init(out.gzip,1048576,gz_alloc,gz_free,NULL))goto end;out.compressed=1;}
            if(e==N70_DONE){if(pos!=(size_t)n)clean=0;done=1;break;}
            if(!used&&e!=N70_HEADERS)goto end;
        }
        {size_t ignored;if(n70_http_feed(p,NULL,0,&ignored,wire_body,&out)==N70_DONE)done=1;}
    }
    snprintf(echo,sizeof echo,"\"method\": \"%s\"",method);
    if(p->status==200&&n70_http_reusable(p,clean)&&(!out.compressed||n70_gzip_eof(out.gzip)==N70_DONE)&&
       (!payload[0]||(strstr(out.preview,echo)&&strstr(out.preview,"tls12-090")))&&br_ssl_engine_get_version(&c->ssl.eng)==BR_TLS12)result=0;
end:
    printf("%s %s | %s | single-connection request=%d TLS=0x%04x HTTP=%d gzip=%d\n",method,path,result?"FAIL":"PASS",++c->requests,br_ssl_engine_get_version(&c->ssl.eng),p?p->status:0,out.compressed);
    if(out.gzip){n70_gzip_close(out.gzip);free(out.gzip);}free(p);return result;
}
int main(void)
{
    WSADATA wsa;unsigned char seed[32];int i,failures=0;struct persistent*p=calloc(1,sizeof *p);
    static const char*const methods[]={"GET","GET","PUT","PATCH","DELETE"};
    static const char*const paths[]={"/get","/gzip","/anything","/anything","/anything"};
    if(!p||WSAStartup(MAKEWORD(2,2),&wsa))return 2;
    if(BCryptGenRandom(NULL,seed,32,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return 2;
    if(open_persistent(p,seed)){puts("FAIL: open persistent TLS");return 1;}
    for(i=0;i<5;++i){failures+=persistent_request(p,methods[i],paths[i]);if(failures)break;}
    printf("Summary: %d/5 passed on one TCP/TLS connection (host, not N70)\n",p->requests-failures);
    closesocket(p->t.socket);SecureZeroMemory(p,sizeof *p);free(p);WSACleanup();return failures?1:0;
}
