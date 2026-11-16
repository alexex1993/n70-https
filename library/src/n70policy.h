#ifndef N70_POLICY_H
#define N70_POLICY_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int n70_method_valid(const char*);
int n70_method_has_body(const char*);
int n70_method_retryable(const char*,int allow_idempotent);
int n70_retry_status(int);
int n70_transport_error(int);
int n70_bearer_error(int);
/* Milliseconds, saturating at INT32_MAX; -1 for invalid/missing header. */
int n70_retry_after(const char*,size_t,uint64_t now,int current_year);
uint64_t n70_epoch_seconds(int,int,int,int,int,int);
/* -1 means do not retry: never shorten a server's Retry-After to the cap. */
int n70_retry_delay(int base_ms,int max_ms,int retry_index,int retry_after_ms,unsigned jitter);
int n70_retry_allowed(const char*,int,int delivered,int error,int control_error);
#ifdef __cplusplus
}
#endif
#endif
