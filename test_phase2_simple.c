/**
 * Minimal test to debug Phase 2 decoder
 */

#include "sparse_phase2.h"
#include <stdio.h>
#include <string.h>

int main() {
    int8_t vector[2048] = {0};
    int8_t decoded[2048];

    /* Simple test: 3 elements with 3 unique values */
    vector[10] = 5;
    vector[100] = -3;
    vector[500] = 7;

    printf("Original vector: [10]=5, [100]=-3, [500]=7\n\n");

    /* Encode */
    sparse_phase2_t *enc = sparse_phase2_encode(vector, 2048);
    if (!enc) {
        printf("ERROR: Encoding failed\n");
        return 1;
    }
    printf("✓ Encoding succeeded: %zu bytes\n\n", enc->size);

    /* Print encoded data */
    printf("Encoded data (hex):\n");
    for (size_t i = 0; i < enc->size && i < 32; i++) {
        printf("%02x ", enc->data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n\n");

    /* Decode */
    memset(decoded, 0, sizeof(decoded));
    int result = sparse_phase2_decode(enc, decoded, 2048);

    if (result < 0) {
        printf("✗ Decoding FAILED\n");
        sparse_phase2_free(enc);
        return 1;
    }

    printf("✓ Decoding succeeded\n\n");

    /* Verify */
    int errors = 0;
    if (decoded[10] != 5) { printf("ERROR: [10] = %d (expected 5)\n", decoded[10]); errors++; }
    if (decoded[100] != -3) { printf("ERROR: [100] = %d (expected -3)\n", decoded[100]); errors++; }
    if (decoded[500] != 7) { printf("ERROR: [500] = %d (expected 7)\n", decoded[500]); errors++; }

    /* Check for spurious non-zeros */
    for (int i = 0; i < 2048; i++) {
        if (i != 10 && i != 100 && i != 500 && decoded[i] != 0) {
            printf("ERROR: [%d] = %d (expected 0)\n", i, decoded[i]);
            errors++;
            if (errors >= 10) {
                printf("... (more errors)\n");
                break;
            }
        }
    }

    if (errors == 0) {
        printf("✓ All values correct!\n");
    } else {
        printf("✗ %d errors found\n", errors);
    }

    sparse_phase2_free(enc);
    return errors > 0 ? 1 : 0;
}
