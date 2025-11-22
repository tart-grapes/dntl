/**
 * test_dense_batch.c
 *
 * Test dense batch encoding with signature-like data
 * - s: 128 coeffs mod 1031, 82.8% small values
 * - w: 64 coeffs mod 521, 53.1% small values
 * - w2: 32 coeffs mod 257, mostly small
 */

#include "dense_batch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Generate vector with concentrated value distribution */
static void generate_signature_like(uint16_t *vector, uint16_t dimension,
                                     uint16_t modulus, float small_ratio,
                                     unsigned int seed) {
    srand(seed);

    for (uint16_t i = 0; i < dimension; i++) {
        float r = (float)rand() / RAND_MAX;

        if (r < small_ratio) {
            /* Small value (0-9) */
            vector[i] = rand() % 10;
        } else {
            /* Larger value distributed across modulus */
            vector[i] = 10 + (rand() % (modulus - 10));
        }
    }
}

static void test_component(const char *name, uint16_t dimension, uint16_t modulus,
                           float small_ratio, uint16_t num_vectors) {
    printf("\n=== %s: %d coeffs, mod %d, %.1f%% small ===\n",
           name, dimension, modulus, small_ratio * 100);

    /* Generate vectors */
    uint16_t **vectors = malloc(num_vectors * sizeof(uint16_t*));
    for (uint16_t v = 0; v < num_vectors; v++) {
        vectors[v] = malloc(dimension * sizeof(uint16_t));
        generate_signature_like(vectors[v], dimension, modulus, small_ratio, 1000 + v);
    }

    /* Calculate baseline (packed) size */
    int bits_per_coeff = 0;
    uint16_t temp = modulus - 1;
    while (temp > 0) {
        bits_per_coeff++;
        temp >>= 1;
    }
    size_t baseline_size = (dimension * bits_per_coeff + 7) / 8;

    printf("Baseline (packed): %zu bytes per vector\n", baseline_size);
    printf("Baseline total:    %zu bytes for %d vectors\n\n",
           baseline_size * num_vectors, num_vectors);

    /* Encode batch */
    dense_batch_t *batch = dense_batch_encode((const uint16_t**)vectors,
                                               num_vectors, dimension, modulus);
    if (!batch) {
        fprintf(stderr, "Batch encoding failed\n");
        return;
    }

    printf("Batch encoding:\n");
    printf("  Total size: %zu bytes\n", batch->size);
    printf("  Per vector: %.1f bytes\n", (double)batch->size / num_vectors);
    printf("  Bits/coeff: %.2f\n\n", (double)batch->size * 8 / (num_vectors * dimension));

    /* Calculate savings */
    int saved = (int)(baseline_size * num_vectors) - (int)batch->size;
    double percent = 100.0 * saved / (baseline_size * num_vectors);

    if (saved > 0) {
        printf("Savings: %d bytes (%.1f%% reduction)\n", saved, percent);
        printf("Per-vector savings: %.1f bytes\n", (double)saved / num_vectors);
    } else {
        printf("Overhead: %d bytes (%.1f%% increase)\n", -saved, -percent);
    }

    /* Verify decode */
    uint16_t **decoded = malloc(num_vectors * sizeof(uint16_t*));
    for (uint16_t v = 0; v < num_vectors; v++) {
        decoded[v] = malloc(dimension * sizeof(uint16_t));
    }

    if (dense_batch_decode(batch, decoded, num_vectors, dimension) < 0) {
        fprintf(stderr, "Batch decoding failed\n");
        return;
    }

    /* Verify correctness */
    int errors = 0;
    for (uint16_t v = 0; v < num_vectors; v++) {
        for (uint16_t i = 0; i < dimension; i++) {
            if (vectors[v][i] != decoded[v][i]) {
                if (errors < 5) {
                    fprintf(stderr, "Mismatch: vec %d pos %d: %d != %d\n",
                            v, i, vectors[v][i], decoded[v][i]);
                }
                errors++;
            }
        }
    }

    if (errors == 0) {
        printf("✓ All %d vectors decoded correctly\n", num_vectors);
    } else {
        printf("✗ %d mismatches found\n", errors);
    }

    /* Cleanup */
    for (uint16_t v = 0; v < num_vectors; v++) {
        free(vectors[v]);
        free(decoded[v]);
    }
    free(vectors);
    free(decoded);
    dense_batch_free(batch);
}

int main() {
    printf("=== Dense Batch Encoding: Signature Components ===\n");

    /* Test with batch size 10 */
    const uint16_t num_vectors = 10;

    /* Component s: 128 coeffs mod 1031, 82.8% small */
    test_component("s", 128, 1031, 0.828, num_vectors);

    /* Component w: 64 coeffs mod 521, 53.1% small */
    test_component("w", 64, 521, 0.531, num_vectors);

    /* Component w2: 32 coeffs mod 257, 70% small (estimate) */
    test_component("w2", 32, 257, 0.70, num_vectors);

    /* Summary */
    printf("\n=== Summary ===\n");
    printf("Batch encoding exploits value concentration:\n");
    printf("- Frequent small values get fewer bits\n");
    printf("- Rare large values get more bits\n");
    printf("- Shared frequency table amortizes overhead\n");
    printf("\nExpected savings increase with:\n");
    printf("- Larger batch sizes (N)\n");
    printf("- Higher concentration of small values\n");

    return 0;
}
