/**
 * test_dense_range.c
 *
 * Test range-based encoding for single dense vectors
 */

#include "dense_range.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void generate_signature_like(uint16_t *vector, uint16_t dimension,
                                     uint16_t modulus, float small_ratio,
                                     unsigned int seed) {
    srand(seed);
    for (uint16_t i = 0; i < dimension; i++) {
        float r = (float)rand() / RAND_MAX;
        if (r < small_ratio) {
            vector[i] = rand() % 10;
        } else {
            vector[i] = 10 + (rand() % (modulus - 10));
        }
    }
}

static void test_component(const char *name, uint16_t dimension, uint16_t modulus,
                           float small_ratio) {
    printf("\n=== %s: %d coeffs, mod %d, %.1f%% small ===\n",
           name, dimension, modulus, small_ratio * 100);

    /* Generate vector */
    uint16_t *vector = malloc(dimension * sizeof(uint16_t));
    generate_signature_like(vector, dimension, modulus, small_ratio, 1000);

    /* Calculate baseline (packed) size */
    int bits_per_coeff = 0;
    uint16_t temp = modulus - 1;
    while (temp > 0) {
        bits_per_coeff++;
        temp >>= 1;
    }
    size_t baseline_size = (dimension * bits_per_coeff + 7) / 8;

    printf("Baseline (packed): %zu bytes (%.2f bits/coeff)\n",
           baseline_size, (double)bits_per_coeff);

    /* Encode with range encoding */
    dense_range_t *encoded = dense_range_encode(vector, dimension, modulus);
    if (!encoded) {
        fprintf(stderr, "Range encoding failed\n");
        return;
    }

    printf("Range encoding:    %zu bytes (%.2f bits/coeff)\n",
           encoded->size, (double)encoded->size * 8 / dimension);

    /* Calculate savings */
    int saved = (int)baseline_size - (int)encoded->size;
    double percent = 100.0 * saved / baseline_size;

    if (saved > 0) {
        printf("Savings: %d bytes (%.1f%% reduction)\n", saved, percent);
    } else {
        printf("Overhead: %d bytes (%.1f%% increase)\n", -saved, -percent);
    }

    /* Verify decode */
    uint16_t *decoded = malloc(dimension * sizeof(uint16_t));
    if (dense_range_decode(encoded, decoded, dimension) < 0) {
        fprintf(stderr, "Range decoding failed\n");
        return;
    }

    /* Verify correctness */
    int errors = 0;
    for (uint16_t i = 0; i < dimension; i++) {
        if (vector[i] != decoded[i]) {
            if (errors < 5) {
                fprintf(stderr, "Mismatch at pos %d: %d != %d\n",
                        i, vector[i], decoded[i]);
            }
            errors++;
        }
    }

    if (errors == 0) {
        printf("✓ Decoded correctly\n");
    } else {
        printf("✗ %d mismatches found\n", errors);
    }

    free(vector);
    free(decoded);
    dense_range_free(encoded);
}

int main() {
    printf("=== Range-Based Encoding: Single Vectors ===\n");

    /* Test signature components */
    test_component("s", 128, 1031, 0.828);
    test_component("w", 64, 521, 0.531);
    test_component("w2", 32, 257, 0.70);

    printf("\n=== Summary ===\n");
    printf("Range encoding benefits:\n");
    printf("- No frequency table overhead\n");
    printf("- Small values (0-15) use 4-6 bits\n");
    printf("- Works well for single vectors\n");
    printf("- Savings scale with small value concentration\n");

    return 0;
}
