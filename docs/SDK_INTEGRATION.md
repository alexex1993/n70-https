# N70 HTTPS 0.9.1 integration reference

## Source and SDK layouts

Repository: `library/include`, `library/src`, `library/group`, `library/compat`,
`library/certs`, `examples/N70Test`, `docs`, `tests`, `tools`, `dist`.
The ready SDK is `dist/N70HttpsSDK` or `dist/N70HttpsSDK.zip`.
An extracted SDK has `include`, `lib`, `examples`, `certs`, `docs`, `skills`.

The public headers are `n70https.h`, `n70httpclient.h`, `n70entropy.h`;
the SDK also supplies `stdint.h` and all BearSSL public headers. Consumers
need the entire SDK include directory, not just one header. Do not add a
desktop/system BearSSL or zlib library: they are built into `n70https.lib`.

## Static linking

Copy the SDK into your app as `third_party/N70HttpsSDK` (relative to app root).
Copy `lib/n70https.lib` into the **active SDK's**
`Epoc32/release/thumb/urel/n70https.lib`. With `EPOCROOT` set to a private SDK,
this means that private SDK, not the original installation. An ordinary app
with `group/App.mmp` can add:

```text
USERINCLUDE ..\third_party\N70HttpsSDK\include
USERINCLUDE ..\third_party\N70HttpsSDK\examples
SYSTEMINCLUDE ..\third_party\N70HttpsSDK\include
SYSTEMINCLUDE \epoc32\include \epoc32\include\libc
STATICLIBRARY n70https.lib
LIBRARY euser.lib esock.lib insock.lib efsrv.lib estlib.lib commdb.lib hal.lib
EPOCSTACKSIZE 32768
EPOCHEAPSIZE 65536 4194304
```

Keep your normal GUI libraries too. `EPOCSTACKSIZE` does not size an explicit
worker thread: set the worker's `RThread::Create` stack argument to **65536**.
The linked machine code goes into your `.app`; an uncompressed archive `.lib`
can be much larger than the compressed final SIS. No separate library entry
is needed in the SIS PKG. Include your app's resources and an independent UID.

Target is S60 2nd Edition FP3 / Symbian 8.1a, legacy THUMB UREL C++98.
Do not use Qt, C++11, `RBuf`, or APIs available only on newer Symbian releases.
The library is not a WINS emulator binary. Keep writable data in instances,
worker stack/heap or files: S60 2nd app DLLs reject writable globals. BearSSL's
three pointer lookup tables are made const by the repository's prepare script;
do not replace its patched dependency tree with an unpatched desktop build.

## Worker request example

[request-example.cpp](request-example.cpp) is a complete helper translation
unit for compilation inside your worker. It sends a JSON POST to a test echo
endpoint and collects up to 64 KiB of decoded body in caller-owned storage.
Replace the endpoint and roots for your service. It deliberately does not
create a UI thread, JSON parser or OAuth flow. The original GUI worker and
shutdown implementation is in repository `examples/N70Test/src/testrunner.cpp`
and `testcancel.cpp`.

Create a `CTrapCleanup` at the worker entry point; TRAP the request work and
destroy the cleanup object before returning. Have the UI create/own the cancel
token and keep it alive until worker exit. `CN70Https`, sockets and sink belong
to the worker. Stack-local anchors and response storage must outlive the call.
Use files or a correctly owned queue to deliver results to the UI. A pointer to
a buffer on the worker heap becomes invalid when that heap is destroyed.

Do not signal completion and destroy objects that a running worker still uses.
In an active object's `DoCancel`, signal the token only. `CActive::Cancel`
already waits for the active request status; waiting again can hang forever.

## Requests, results and response data

- `RequestAutoL(method,url,contentType,body,anchors,count,stateFile,sink,result)`:
  automatic per-app persistent entropy management.
- `RequestL` takes a fresh external seed instead of `stateFile` for applications
  that provide their own entropy source. Its freshness requirement still applies.
- `DownloadAutoL(url,destination,anchors,count,stateFile,result)`: final 2xx
  response, decoded streaming output, unique temporary file, flush and replace
  only after success. Prior destination survives failure.
- `GetL` / `GetAutoL` are legacy raw HTTP helpers. They do not provide the new
  decoded response, custom header and redirect features.

`MN70HttpSink::HeadersL(status,headers)` receives the final HTTP response once;
`DataL(bytes)` receives decoded fragments. Both run in the worker and their
descriptors are borrowed for the callback only. Copy data you retain, enforce
limits and never assume fragment boundaries equal lines, JSON tokens or UTF-8
character boundaries. A callback may leave to abort. Data emitted before a
later framing/gzip integrity failure is partial, not a successful document.

`N70HeaderValue(headers,name)` returns a borrowed view of the first matching
field. Handle repeated fields separately if service semantics require them.
Results include `iStatus`, `iFinalUrl`, `iRedirects`, `iBodyBytes` (decoded),
`iWireBodyBytes` (before gzip, without chunk framing), `iAttempts`, `iRetries`,
`iNewConnections`, `iReusedConnections`, `iAddressAttempts`, `iRetryAfterMs`.
`ProtocolVersion()==0x0303` means TLS 1.2.

TRAP Symbian leaves separately from HTTP errors. A returned HTTP 401, 404 or
503 is not necessarily a transport failure. On a leave, record the Symbian
error and client diagnostics before destroying the client. Never log tokens,
seed material or state bytes. Examples of stages: `tcp-connect`,
`tls-handshake/http-send`, `http-read`; use the actual `StageName()` string.

## Options and service behavior

Set a `TN70HttpOptions` before requests. Default total budget is 120000 ms and
individual I/O budget 30000 ms. Total budget spans connection setup, redirects,
retries and backoff. Cancellation is polled in waiting paths at roughly 100 ms;
blocking file operations, computation and callbacks are cooperative, not hard
real-time. Use short bounded callbacks and distinguish elapsed time from stage.

Defaults: decoded/wire body caps 8 MiB each, redirects 5, DNS IPv4 answers 8
(allowed 1..32), keep-alive enabled with 15 s idle, retries 2 extra attempts,
backoff base 500 ms/cap 10 s. Tune for a phone's RAM and the service.

GET/HEAD can retry selected temporary network failures and HTTP
408/429/502/503/504 before final headers are delivered. PUT/DELETE require
`iRetryIdempotent=ETrue`. POST/PATCH are never automatically repeated, even if
an `Idempotency-Key` is present. If the response was lost after sending a POST,
the operation may already have executed; reconcile via the service protocol.
`Retry-After` seconds and HTTP dates are parsed. A server delay above the
configured cap returns the response rather than retrying too soon. App-level
login refresh, pagination and rate-limit scheduling are your responsibility.

One idle client can reuse one same-origin TLS connection, with matching root
contents, until its idle deadline. Keep it in the worker for a sequence of
requests. `DisconnectL()` releases this client's handles only. No pool,
pipelining or TLS session resumption is provided.

## Headers and URLs

`SetHeaderL(name,value)` copies/replaces case-insensitively. Name maximum 128
bytes, value 2048 bytes, combined header storage 4096 bytes. CR/LF/NUL and
invalid syntax are rejected. Framing and negotiated fields (`Host`,
`Content-Length`, `Content-Type`, `Transfer-Encoding`, `Connection`,
`Accept-Encoding`, etc.) are managed by the library. Pass the content type as
the request argument; gzip is requested and decoded automatically. `Accept`
and `User-Agent` can be set. `SetBearerTokenL` adds the `Bearer ` prefix itself.
`ClearHeadersL` removes custom headers. On cross-origin redirect all custom
headers are permanently stripped, including if the chain later returns to the
original origin. Do not restore credentials blindly. HTTP downgrade and unsafe
method changes across origins are restricted; see the installed API reference.

URLs are absolute `https://`, ASCII/percent-encoded, DNS or IPv4, optional port;
no userinfo or IPv6. Current limits: 4096-byte URL, 3072-byte request target.
The app must escape query values and encode JSON/text as UTF-8.

## Roots, time and per-device state

`examples/testanchor.h` exposes `TEST_ANCHOR_COUNT` and
`InitTestAnchors(br_x509_trust_anchor*)`. Its eight demo roots cover the tested
services, not every CA on the Internet. Repository `library/certs` contains
the DER sources pinned by `tools/prepare.ps1`. To change roots, validate CA
sources/fingerprints and extend the generator or supply your own equivalent
const arrays plus runtime initializer. No ready arbitrary-root CLI is promised.
Roots must have an appropriate lifetime and CA flags. Phone time must be
correct for certificate validity. The OS/browser certificate store is separate.

Use an absolute writable path distinct for each app, e.g.
`C:\system\apps\MyApp\rng-device-v1.bin`. The library creates parent folders
and initializes missing state without keyboard input. Keep valid state across
upgrades; do not install an existing state file, shared bootstrap or device
dump. Corrupt/inaccessible state must fail, not silently be reset. Do not roll
back old state as normal recovery. Automatic initial entropy uses unassessed
device observations: HMAC-DRBG conditioning does not prove their entropy.

## Supported scope and evidence

TLS 1.2 only, SNI, certificate/hostname validation and HTTP/1.1 over IPv4.
Fixed/chunked/close-delimited response bodies and gzip are supported. HTTP/2,
TLS 1.3, IPv6, WebSocket, Brotli/deflate response encoding, proxy support,
cookie persistence, JSON parsing and OAuth authorization flows are absent.
Use a working Internet access point rather than a WAP-only gateway.

The archived N70 0.9.1 run passed 43 feature checks; its separate site batch was
9/10 with an unresolved letsencrypt.org TLS/EOF failure. Host tests cover
portable core/policy and modeled Symbian waits, not all radio/firmware behavior.
Build and test the actual consuming app on the phone before claiming it works.
