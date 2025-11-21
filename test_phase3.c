#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "sparse_phase3.h"

int main() {
    int success_count = 0;
    int total_size = 0;
    
    printf("=== Phase 3: Delta Alphabet + Huffman + Adaptive Rice ===\n\n");
    
    // Generate and test 20 random sparse vectors
    for (int seed = 1000; seed < 1020; seed++) {
        srand(seed);
        
        int8_t vector[2048] = {0};
        int count = 0;
        
        // Generate ~97 non-zero values
        for (int i = 0; i < 2048 && count < 97; i++) {
            if (rand() % 100 < 5) {
                vector[i] = (rand() % 60) - 30;
                if (vector[i] != 0) count++;
            }
        }
        
        // Encode
        sparse_phase3_t *encoded = sparse_phase3_encode(vector, 2048);
        if (!encoded) {
            printf("Trial %2d: ✗ Encoding failed\n", seed - 999);
            continue;
        }
        
        // Decode
        int8_t decoded[2048];
        if (sparse_phase3_decode(encoded, decoded, 2048) < 0) {
            printf("Trial %2d: ✗ Decoding failed (%zu bytes)\n", seed - 999, encoded->size);
            sparse_phase3_free(encoded);
            continue;
        }
        
        // Verify
        int errors = 0;
        for (int i = 0; i < 2048; i++) {
            if (vector[i] != decoded[i]) {
                errors++;
            }
        }
        
        if (errors == 0) {
            printf("Trial %2d: ✓ %zu bytes\n", seed - 999, encoded->size);
            success_count++;
            total_size += encoded->size;
        } else {
            printf("Trial %2d: ✗ %d errors (%zu bytes)\n", seed - 999, errors, encoded->size);
        }
        
        sparse_phase3_free(encoded);
    }
    
    printf("\n=== Results ===\n");
    printf("Success rate: %d/20\n", success_count);
    if (success_count > 0) {
        printf("Average size: %.1f bytes\n", (double)total_size / success_count);
        printf("Comparison:\n");
        printf("  Baseline (sparse_optimal): 209 bytes\n");
        printf("  Phase 1 (delta alphabet):  167.5 bytes\n");
        printf("  Phase 3 (adaptive Rice):   %.1f bytes\n", (double)total_size / success_count);
    }
    
    return success_count == 20 ? 0 : 1;
}
