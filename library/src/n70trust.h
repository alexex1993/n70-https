#ifndef N70_TRUST_H
#define N70_TRUST_H
#include <bearssl.h>
#ifdef __cplusplus
extern "C" {
#endif
int n70_trust_digest(const br_x509_trust_anchor*,size_t,unsigned char digest[32]);
#ifdef __cplusplus
}
#endif
#endif
