/* Host regression check for the exact trust-anchor header used on the N70.
 * Usage: verify_anchors host anchor-count expected-error cert.der ...
 * This tests X.509 validation, not Symbian sockets or the ARM binary. */
#include <stdio.h>
#include <stdlib.h>
#include <bearssl.h>
#include "testanchor.h"

int main(int argc, char **argv)
{
    br_x509_trust_anchor anchors[TEST_ANCHOR_COUNT];
    br_x509_minimal_context ctx;
    int i, count, expected;
    unsigned result;
    if (argc < 5) return 2;
    count = atoi(argv[2]); expected = atoi(argv[3]);
    if (count < 1 || count > TEST_ANCHOR_COUNT) return 2;
    InitTestAnchors(anchors);
    br_x509_minimal_init_full(&ctx, anchors, count);
    ctx.vtable->start_chain(&ctx.vtable, argv[1]);
    for (i = 4; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        unsigned char *data;
        long size;
        if (!f) return 2;
        if (fseek(f, 0, SEEK_END) || (size = ftell(f)) <= 0 || size > 1048576) {
            fclose(f); return 2;
        }
        rewind(f);
        data = malloc((size_t)size);
        if (!data) { fclose(f); return 2; }
        if (fread(data, 1, (size_t)size, f) != (size_t)size) {
            free(data); fclose(f); return 2;
        }
        fclose(f);
        ctx.vtable->start_cert(&ctx.vtable, (uint32_t)size);
        ctx.vtable->append(&ctx.vtable, data, (size_t)size);
        ctx.vtable->end_cert(&ctx.vtable);
        free(data);
    }
    result = ctx.vtable->end_chain(&ctx.vtable);
    printf("host=%s anchors=%d result=%u expected=%d: %s\n",
        argv[1], count, result, expected, result == (unsigned)expected ? "PASS" : "FAIL");
    return result == (unsigned)expected ? 0 : 1;
}
