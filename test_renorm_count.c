#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "sparse_phase2.h"

int main() {
    // Test with the first trial seed that was failing
    srand(1000);
    
    int8_t vector[2048] = {0};
    int count = 0;
    
    // Generate ~97 non-zero values
    for (int i = 0; i < 2048 && count < 97; i++) {
        if (rand() % 100 < 5) {  // ~5% density → ~100 non-zeros
            vector[i] = (rand() % 60) - 30;  // Range [-30, 30]
            if (vector[i] != 0) count++;
        }
    }
    
    printf("Generated %d non-zero values\n", count);
    
    // Encode
    sparse_phase2_t *encoded = sparse_phase2_encode(vector, 2048);
    if (!encoded) {
        printf("Encoding FAILED\n");
        return 1;
    }
    printf("Encoding succeeded: %zu bytes\n", encoded->size);
    
    // Decode
    int8_t decoded[2048];
    if (sparse_phase2_decode(encoded, decoded, 2048) < 0) {
        printf("Decoding FAILED\n");
        free(encoded->data);
        free(encoded);
        return 1;
    }
    printf("Decoding succeeded\n");
    
    // Verify
    int errors = 0;
    for (int i = 0; i < 2048; i++) {
        if (vector[i] != decoded[i]) {
            if (errors < 5) {
                printf("ERROR: [%d] = %d (expected %d)\n", i, decoded[i], vector[i]);
            }
            errors++;
        }
    }
    
    if (errors == 0) {
        printf("✓ All values correct!\n");
    } else {
        printf("✗ %d errors found\n", errors);
    }
    
    free(encoded->data);
    free(encoded);
    return errors > 0 ? 1 : 0;
}
