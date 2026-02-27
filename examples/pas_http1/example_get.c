/*
    example_get.c - GET request with pas_http1.h
    From repo root: gcc -o examples/pas_http1/example_get examples/pas_http1/example_get.c -I.
    On Windows add -lws2_32 when linking (or use the pragma in the header).
*/

#define PAS_HTTP1_IMPLEMENTATION
#include "pas_http1.h"
#include <stdio.h>

int main(void)
{
    char buf[4096];
    pas_http_response_t res;
    int status;
    int r;

    r = pas_http_get("http://example.com/", buf, sizeof(buf), 10000, &res, &status);
    if (r != 0 || status != PAS_HTTP_OK) {
        (void)fprintf(stderr, "pas_http_get failed: %d (status %d)\n", r, status);
        return 1;
    }
    (void)printf("Status: %d\n", res.status_code);
    (void)printf("Body length: %zu\n", res.body_len);
    if (res.body_len > 0 && res.body)
        (void)printf("%.*s\n", (int)res.body_len, res.body);
    return 0;
}
