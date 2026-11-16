Public self-signed CA roots used by N70Test (DER X.509).
Exported from the Windows LocalMachine Root store; prepare.ps1 checks
the SHA-1 certificate thumbprints against the explicit allowlist below.
No server leaf certificates or unverified intermediate CAs are trusted.

| File | SHA-1 thumbprint |
|---|---|
| isrg-root-x1.der | CABD2A79A1076A31F21D253635CB039D4329A5E8 |
| digicert-global-root-g2.der | DF3C24F9BFD666761B268073FE06D1CC8D4F82A4 |
| sslcom-tls-ecc-root-2022.der | 9F5FD91A546DF50C71F0EE7ABD1749988473E239 |
| globalsign-root-ca.der | B1BC968BD4F49D622AA89A81F2150152A41D829C |
| globalsign-root-r3.der | D69B561148F01C77C54578C10926DF5B856976AD |
| usertrust-ecc.der | D1CBCA5DB2D52A7F693B674DE5F05A1D0C957DF0 |
| usertrust-rsa.der | 2B8F1B57330DBBA2D07A6C51F70EE90DDAB9AD8E |
| gts-root-r4.der | 2A1D6027D94AB10A1C4D915CCD33A0CB3E2D54CB |

SSL.com publishes the ECC root identity and fingerprint in its
[CP/CPS](https://legal.ssl.com/documents/SSLcom-CP-CPS-v1.23.pdf), section 1.3.1,
and certificates in its [CA repository](https://www.ssl.com/repository/).

On 2026-11-15, example.com's TLS 1.2 chain observed from this PC used
Cloudflare TLS Issuing ECC CA 3 → SSL.com TLS Transit ECC CA R2 →
SSL.com TLS ECC Root CA 2022 (cross-signed by AAA Certificate Services).
The directly trusted self-signed SSL.com root has the same subject and key
as the cross-signed root. Chains may vary by server location and time.
