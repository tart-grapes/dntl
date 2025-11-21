/**
 * analyze_phase2_size.c
 *
 * Analyze where bytes are being used in Phase 2 encoding
 */

#include "sparse_phase2.h"
#include "sparse_delta.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void generate_test_vector(int8_t *vector, size_t dimension, unsigned int seed) {
    srand(seed);

    /* Generate ~97 non-zeros with s-mangled distribution */
    for (size_t i = 0; i < dimension; i++) {
        vector[i] = 0;
    }

    int target_nz = 97;
    for (int i = 0; i < target_nz; i++) {
        size_t pos = rand() % dimension;

        /* s-mangled: bimodal with center spike */
        float r = (float)rand() / RAND_MAX;
        int val;
        if (r < 0.3f) {
            /* Spike near zero */
            val = (rand() % 7) - 3;
        } else {
            /* Spread distribution */
            float u = (float)rand() / RAND_MAX;
            float theta = 2.0f * 3.14159f * u;
            float mag = sqrtf(-2.0f * logf((float)rand() / RAND_MAX + 0.0001f)) * 8.0f;
            val = (int)round(mag * cosf(theta));
        }

        if (val < -128) val = -128;
        if (val > 127) val = 127;
        vector[pos] = val;
    }
}

int main() {
    int8_t vector[2048];
    generate_test_vector(vector, 2048, 1000);

    /* Encode with both phases */
    sparse_delta_t *phase1 = sparse_delta_encode(vector, 2048);
    sparse_phase2_t *phase2 = sparse_phase2_encode(vector, 2048);

    if (!phase1 || !phase2) {
        fprintf(stderr, "Encoding failed\n");
        return 1;
    }

    /* Count unique values */
    int value_counts[256] = {0};
    int n_unique = 0;
    for (size_t i = 0; i < 2048; i++) {
        if (vector[i] != 0) {
            if (value_counts[(uint8_t)(vector[i] + 128)]++ == 0) {
                n_unique++;
            }
        }
    }

    printf("=== Size Breakdown ===\n\n");
    printf("Phase 1 (Huffman): %zu bytes\n", phase1->size);
    printf("Phase 2 (rANS):    %zu bytes\n", phase2->size);
    printf("Difference:        %+zd bytes\n\n", (ssize_t)phase2->size - (ssize_t)phase1->size);

    printf("Components:\n");
    printf("  Header (count, min, max): 4 bytes (both)\n");
    printf("  Alphabet bitfield:        ~%d bits = %d bytes\n",
           60, (60 + 7) / 8);

    printf("\n  Phase 1: Huffman code lengths: ~%d bits = %d bytes\n",
           n_unique * 4, (n_unique * 4 + 7) / 8);
    printf("  Phase 2: Frequency table:     %d bits = %d bytes\n",
           n_unique * 12, (n_unique * 12 + 7) / 8);
    printf("  Frequency overhead: +%d bytes\n",
           (n_unique * 12 + 7) / 8 - (n_unique * 4 + 7) / 8);

    printf("\n  Rice parameter: 3 bits (both)\n");
    printf("  Position encoding: ~same for both\n");

    printf("\n  Value encoding:\n");
    printf("    Phase 1 (Huffman): Variable length codes\n");
    printf("    Phase 2 (rANS):    Stream + 4-byte state\n");

    /* Estimate rANS overhead */
    printf("\n  Estimated rANS overhead:\n");
    printf("    Frequency table: +%d bytes\n", (n_unique * 12 - n_unique * 4) / 8);
    printf("    Final state:     +4 bytes\n");
    printf("    Renorm bytes:    Extra bytes from renormalization\n");

    printf("\n  Unique values: %d\n", n_unique);
    printf("  Non-zero count: %d\n", phase2->count);

    sparse_delta_free(phase1);
    sparse_phase2_free(phase2);

    return 0;
}
