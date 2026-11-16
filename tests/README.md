# Host and device tests

Run commands from the repository root. Build tools: Visual Studio C++ with
AddressSanitizer; native BearSSL is needed even without network tests (trust
fingerprinting links against it).

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/build-host-bearssl.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test.ps1
# Optional real HTTPS requests from the PC:
powershell -NoProfile -ExecutionPolicy Bypass -File tools/test.ps1 -Live
```

`tools/test-http08.ps1` and `tools/test-http09.ps1` remain compatibility aliases.
Compiler products and generated commands go into `build/host/`, logs into
`tests/results/host/host-http09-results.txt`. The native upstream BearSSL build
retains its own `vendor/bearssl-0.6/build/` convention. If dependencies or the
generated trust-root header are missing, run `tools/prepare.ps1` first.

## Maintained regression suite

| Test | Coverage |
| --- | --- |
| `gzip.c` | Fragmentation, dynamic/fixed/stored streams, concatenated members, FHCRC, CRC/ISIZE errors, truncation, expansion caps and allocation/sink failures |
| `headers.c` | Replacement, reserved fields, byte validation, CRLF/NUL rejection, transactional limits, Bearer and cross-origin stripping |
| `wait.cpp` | Actual `library/src/n70wait.cpp` against a request-semaphore model; deadlines, cancellation, simultaneous completions, cleanup balance and retry delays |
| `cancel.cpp` | Actual `examples/N70Test/src/testcancel.cpp`; framework-owned CActive cancellation wait |
| `http_core.c` | Response framing and persistence, fragmentation, truncation, limits, sink failure, redirects and URL resolution |
| `policy.c` | Methods, retry safety, delivered-response/cancel guards, Retry-After seconds and three date forms, overflow, backoff and caps |
| `connect.c` | Real portable connection driver with scripted transport: alternate IPv4, deduplication, bounded answers, one restart, cancellation and deadline |
| `trust.c` | Root contents determine reuse, independently of pointer identity |
| `live_http.c` (`-Live`) | Ten real TLS 1.2 requests covering auth/custom fields, JSON POST, gzip/chunked and large body |
| `live_reuse.c` (`-Live`) | Five GET/gzip/PUT/PATCH/DELETE requests on one TCP/TLS connection |

The eight local groups use MSVC AddressSanitizer. The wait model tracks consumed
request-semaphore signals; a repeated wait on a consumed completion does not
magically complete just because the status value is ready. This detects the
0.9.0 hang regressions; see [the analysis](../docs/N70_RESULTS_0.9.0.md).
These are portable/model tests, not execution of the phone's radio firmware.

## Device evidence

`device/n70-0.9.1/` is the original complete folder supplied by the user,
previously `test_from_n70/`. It contains raw logs, the installed app/resources,
downloads and device state. Files were moved without altering their contents.
Device state is private test evidence, excluded from Git/SDK/SIS; never copy it
into another app or installer. Do not run or package this directory as source.

The latest N70 run passed 43/43 checks in six feature suites; the separate
ten-site run was 9/10. The unresolved letsencrypt.org EOF is recorded rather
than counted as success. Read [the 0.9.1 report](../docs/N70_RESULTS_0.9.1.md)
and [device test instructions](../docs/PHONE_TESTS.md). Reorganized/rebuilt
artifacts do not by themselves constitute a new phone test run.

## Additional research harnesses

Earlier focused harnesses are retained: `verify_anchors.c`, `live_tls12.c`,
`entropy_vector.c`, `entropy_record.c`, `timing_samples.c`, `deadline.c`.
They can be compiled from a Visual Studio developer command prompt. For example:

```bat
cl /nologo /O2 /Ivendor\bearssl-0.6\inc /Iexamples\N70Test\inc tests\verify_anchors.c /Fobuild\host\ /Febuild\host\verify_anchors.exe /link vendor\bearssl-0.6\build\bearssls.lib
cl /nologo /O2 /Ivendor\bearssl-0.6\inc /Ilibrary\src tests\entropy_vector.c library\src\n70entropy_core.c /Fobuild\host\ /Febuild\host\entropy_vector.exe /link vendor\bearssl-0.6\build\bearssls.lib
powershell -NoProfile -ExecutionPolicy Bypass -File tests\check-entropy-vector.ps1
```

`verify_anchors.exe` needs a hostname, anchor count, expected verifier error and
DER chain files in leaf-to-root order; obtain fresh chains rather than trusting
historical captures. Roots come from the generated demo header, not from server
certificates. Live chain/CDN changes can invalidate old expected outcomes.
`results/host/host-tls12-results.txt` preserves the earlier TLS site run; it is
dated evidence, not a result of each new build.

The entropy-vector/codec and timing-input checks cover conditioning,
serialization and validation. They do not measure or establish the entropy of
automatic per-device initialization. The current app uses automatic setup;
the historical keyboard/provisioning steps are not integration requirements.
