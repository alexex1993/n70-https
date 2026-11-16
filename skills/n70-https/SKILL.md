---
name: n70-https
description: "Integrate the n70https TLS 1.2 and HTTP/1.1 static library into C++ applications for Nokia N70, N72 or N90 running S60 2nd Edition FP3 / Symbian OS 8.1a. Use for HTTPS APIs, JSON GET/POST, Bearer headers, gzip, streaming downloads, cancellation, retries, keep-alive, MMP linking, or diagnosing this library. Complements s60-n70-dev for the legacy SDK and SIS toolchain."
---

# N70 HTTPS application integration

## Locate and inspect

1. Use `s60-n70-dev` too when available for SDK setup, Symbian scaffolding and packaging.
2. Run this skill's `scripts/locate-library.ps1` with an optional `-Root` supplied by the user. It checks `N70_HTTPS_ROOT`, workspace/skill ancestors and an optional local installation hint. Accept either the source repository or an extracted `N70HttpsSDK`.
3. Read [integration.md](references/integration.md) before writing application code. Read the located library's public `n70https.h`, `n70httpclient.h` and `docs/HTTP_API.md` for the exact available version. This skill describes 0.9.1; headers take precedence if the installed version differs.
4. Prefer `dist/N70HttpsSDK` or its ZIP for consuming apps. Do not copy private implementation sources into an app or depend on paths inside `build/sdk/Epoc32/BUILD`.

## Implement

- Use `CN70Https::RequestAutoL` for GET/HEAD/POST/PUT/PATCH/DELETE and `DownloadAutoL` for atomic file downloads. Implement `MN70HttpSink` for decoded response data. Avoid the legacy raw `GetAutoL` API for new service integrations.
- All client creation, requests, callbacks and destruction run in the same worker thread with **at least 65536 bytes of stack**, its own `CTrapCleanup`, and a suitable heap. Follow the repository's `examples/N70Test` worker lifecycle. Keep networking off the UI thread.
- Link `n70https.lib` using **STATICLIBRARY**. Add the public/dependency includes and Symbian libraries shown in the integration reference. Build THUMB UREL with the S60 2nd FP3 toolchain. BearSSL/zlib are already in the static library; do not install an extra TLS DLL on the phone.
- Select CA roots covering the service. Initialize writable `br_x509_trust_anchor` structures in the worker from const data; no writable global objects in an S60 app DLL. Keep anchors alive for the call. The demo roots are examples, not universal service coverage. Never turn a downloaded leaf/intermediate into a trusted root to bypass validation.
- Pass a distinct absolute per-app state path, e.g. `C:\system\apps\MyApp\rng-device-v1.bin`. Automatic initialization needs no keyboard interaction or per-phone SIS. Never embed/copy a shared seed or another phone's state. Preserve valid state across upgrades. Automatic initial entropy is experimental; do not claim assessed cryptographic strength.
- Use `SetBearerTokenL(token)` without the `Bearer ` prefix and `SetHeaderL` for additional fields. Set content type via the request argument. Do not manually add framing/connection/encoding fields. Never log credentials or complete sensitive bodies.
- Handle both Symbian leaves and HTTP status. HTTP 401/403/429/5xx can be complete successful transports; refresh/login/rate-limit behavior belongs to the app. Process final decoded bytes only after the request has completed successfully; a late gzip integrity error can follow earlier callbacks.
- Keep callbacks bounded, use UTF-8 bytes for JSON, and parse JSON in the app. No implicit JSON parser, cookie jar or OAuth flow is provided.

## Preserve lifecycle guarantees

- Configure total/I/O timeouts, body/wire limits, redirects and retries for the service. Defaults: 120 s total, 30 s I/O, 8 MiB each body limit, five redirects, two extra GET/HEAD attempts. PUT/DELETE retries require explicit opt-in; POST/PATCH are never automatically replayed. Do not add a generic POST retry wrapper.
- Custom headers are permanently stripped after a cross-origin redirect. Do not defeat that by automatically forwarding credentials to a different origin. HTTPS downgrade is rejected.
- Reuse one idle `CN70Https` in the same worker for same-origin keep-alive; no parallel use of that object. Call `DisconnectL` when idle if needed. There is no connection pool or TLS session resumption.
- UI cancellation only signals a borrowed `CN70CancelToken` that outlives the worker. Reset between requests. Do not use `RThread::Kill` for normal shutdown or pass worker-heap buffers to the UI after thread exit. Publish results through files or a correctly owned cross-thread channel.
- In `CActive::DoCancel`, signal the worker only: `CActive::Cancel` owns the wait on its request status. Never add a second wait on an already consumed completion. The library's socket wait helpers own their timer/socket completion cleanup.

## Build and verify

1. If rebuilding the library, run the repository's `build.cmd`; it prepares the private SDK, builds a baseline SDK example, the library and N70Test, then packages `dist/N70HttpsSDK.zip`. For custom SDK paths run `tools/prepare.ps1` with parameters and then `build/generated/build.cmd` directly.
2. Rebuild consuming applications after changing library headers/binary. Verify the expected `.app` was freshly produced even if a legacy tool returns exit 0. Package the app's own UID/resources in its SIS; do not reuse the N70Test UID for a new app.
3. Host regression commands: `tools/build-host-bearssl.ps1`, then `tools/test.ps1` (add `-Live` only for network checks). Logs: `tests/results/host/`; compiler files: `build/host/`; Symbian build logs: `build/logs/`.
4. Test the app on a real N70: request/response and auth, gzip, total/I/O deadline, cancel and exit during a request, disconnected network, HTTP errors, malformed/oversize response and repeated requests. PC tests and a successful SIS build do not establish phone behavior.
5. On failure record the Symbian error, `TlsError()`, `NetworkError()`, `StageName()`, negotiated `ProtocolVersion()` and request counters. Use `docs/PHONE_TESTS.md` and latest `N70_RESULTS_*.md` to separate known limitations from regressions. Treat files in `Trash/` as historical evidence, never active dependencies.

## Scope

TLS 1.2 with SNI and X.509 checks, HTTP/1.1 over IPv4, chunked and gzip decoding. No TLS 1.3, HTTP/2, WebSocket, IPv6, Brotli/deflate response decoding, proxy support or system/browser TLS upgrade. Correct phone time and a working Internet access point are required. Latest archived phone evidence is 43/43 feature checks and 9/10 separate sites; do not advertise universal HTTPS compatibility.
