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
static int check_site(const char *host, const unsigned char seed[32])
{
    struct addrinfo hints, *list = NULL, *p;
    struct transport t;
    DWORD timeout = 10000;
    br_ssl_client_context *client = calloc(1, sizeof *client);
    br_x509_minimal_context x509;
    br_x509_trust_anchor anchors[TEST_ANCHOR_COUNT];
    br_sslio_context io;
    unsigned char *buffer = malloc(BR_SSL_BUFSIZE_BIDI);
    char request[512], response[16385];
    size_t used = 0;
    int result = 1, n;
    t.socket = INVALID_SOCKET; t.error = 0;
    if (!client || !buffer) goto done;
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
        "GET / HTTP/1.0\r\nHost: %s\r\nConnection: close\r\nAccept-Encoding: identity\r\n\r\n", host);
    if (br_sslio_write_all(&io, request, (size_t)n) < 0 || br_sslio_flush(&io) < 0) goto done;
    response[0] = 0;
    while (used < sizeof response - 1) {
        n = br_sslio_read(&io, response + used, sizeof response - 1 - used);
        if (n <= 0) goto done;
        used += n; response[used] = 0;
        if (strstr(response, "\r\n\r\n")) {
            if (!memcmp(response, "HTTP/", 5) &&
                br_ssl_engine_get_version(&client->eng) == BR_TLS12 &&
                !br_ssl_engine_last_error(&client->eng)) result = 0;
            break;
        }
    }
done:
    response[used] = 0;
    response[strcspn(response, "\r\n")] = 0;
    printf("%s | %s | TLS=0x%04x BearSSL=%d IO=%d | %.100s\n", host,
        result ? "FAIL" : "PASS", client ? br_ssl_engine_get_version(&client->eng) : 0,
        client ? br_ssl_engine_last_error(&client->eng) : -1, t.error, response);
    fflush(stdout);
    if (t.socket != INVALID_SOCKET) closesocket(t.socket);
    if (list) freeaddrinfo(list);
    if (client) { SecureZeroMemory(client, sizeof *client); free(client); }
    if (buffer) { SecureZeroMemory(buffer, BR_SSL_BUFSIZE_BIDI); free(buffer); }
    return result;
}
int main(void)
{
    WSADATA wsa;
    unsigned char secret[32], material[64];
    int i, failures = 0;
    if (WSAStartup(MAKEWORD(2, 2), &wsa)) return 2;
    if (BCryptGenRandom(NULL, secret, sizeof secret, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 2;
    for (i = 0; i < N70_TEST_SITE_COUNT; i++) {
        n70_entropy_derive(secret, NULL, 0, material);
        memcpy(secret, material, 32);
        failures += check_site(KN70TestSites[i], material + 32);
    }
    SecureZeroMemory(secret, sizeof secret); SecureZeroMemory(material, sizeof material);
    WSACleanup();
    printf("Summary: %d/%d passed (host BearSSL, not N70)\n", N70_TEST_SITE_COUNT - failures, N70_TEST_SITE_COUNT);
    return failures ? 1 : 0;
}
