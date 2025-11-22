/**
 * test_batch.c
 *
 * Test batch encoding vs individual encoding
 * Shows overhead amortization benefit
 */

#include "sparse_batch.h"
#include "sparse_phase2.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void generate_test_vector(int8_t *vector, size_t dimension, unsigned int seed) {
    srand(seed);

    for (size_t i = 0; i < dimension; i++) {
        vector[i] = 0;
    }

    int target_nz = 97;
    for (int i = 0; i < target_nz; i++) {
        size_t pos = rand() % dimension;

        float r = (float)rand() / RAND_MAX;
        int val;
        if (r < 0.3f) {
            val = (rand() % 7) - 3;
        } else {
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
    const uint16_t num_vectors = 10;
    const size_t dimension = 2048;

    /* Generate test vectors */
    int8_t **vectors = malloc(num_vectors * sizeof(int8_t*));
    for (uint16_t v = 0; v < num_vectors; v++) {
        vectors[v] = malloc(dimension * sizeof(int8_t));
        generate_test_vector(vectors[v], dimension, 1000 + v);
    }

    printf("=== Batch Encoding Test ===\n\n");
    printf("Vectors: %d × %zu dimensions\n\n", num_vectors, dimension);

    /* Method 1: Individual encoding (Phase 2) */
    size_t individual_total = 0;
    sparse_phase2_t **individual_encoded = malloc(num_vectors * sizeof(sparse_phase2_t*));

    for (uint16_t v = 0; v < num_vectors; v++) {
        individual_encoded[v] = sparse_phase2_encode(vectors[v], dimension);
        if (!individual_encoded[v]) {
            fprintf(stderr, "Individual encoding failed for vector %d\n", v);
            return 1;
        }
        individual_total += individual_encoded[v]->size;
    }

    printf("Method 1: Individual Phase 2 encoding\n");
    printf("  Total size: %zu bytes\n", individual_total);
    printf("  Average per vector: %.1f bytes\n\n", (double)individual_total / num_vectors);

    /* Method 2: Batch encoding */
    sparse_batch_t *batch = sparse_batch_encode((const int8_t**)vectors, num_vectors, dimension);
    if (!batch) {
        fprintf(stderr, "Batch encoding failed\n");
        return 1;
    }

    printf("Method 2: Batch encoding\n");
    printf("  Total size: %zu bytes\n", batch->size);
    printf("  Average per vector: %.1f bytes\n\n", (double)batch->size / num_vectors);

    /* Savings */
    int saved = (int)individual_total - (int)batch->size;
    double percent = 100.0 * saved / individual_total;
    printf("Savings: %d bytes (%.1f%% reduction)\n\n", saved, percent);

    /* Verify decode */
    int8_t **decoded = malloc(num_vectors * sizeof(int8_t*));
    for (uint16_t v = 0; v < num_vectors; v++) {
        decoded[v] = malloc(dimension * sizeof(int8_t));
    }

    if (sparse_batch_decode(batch, decoded, num_vectors, dimension) < 0) {
        fprintf(stderr, "Batch decoding failed\n");
        return 1;
    }

    /* Verify correctness */
    int errors = 0;
    for (uint16_t v = 0; v < num_vectors; v++) {
        for (size_t i = 0; i < dimension; i++) {
            if (vectors[v][i] != decoded[v][i]) {
                if (errors < 5) {
                    fprintf(stderr, "Mismatch: vector %d pos %zu: %d != %d\n",
                            v, i, vectors[v][i], decoded[v][i]);
                }
                errors++;
            }
        }
    }

    if (errors == 0) {
        printf("✓ All %d vectors decoded correctly\n\n", num_vectors);
    } else {
        printf("✗ %d mismatches found\n\n", errors);
        return 1;
    }

    /* Breakdown */
    printf("=== Size Breakdown ===\n\n");
    printf("Individual encoding overhead per vector:\n");
    printf("  Frequency table: ~30 bytes\n");
    printf("  Final state:     4 bytes\n");
    printf("  Total overhead:  ~34 bytes × %d = ~%d bytes\n\n",
           num_vectors, 34 * num_vectors);

    printf("Batch encoding overhead (shared):\n");
    printf("  Frequency table: ~30 bytes\n");
    printf("  Final state:     4 bytes\n");
    printf("  Total overhead:  ~34 bytes (paid once)\n\n");

    printf("Overhead saved: ~%d bytes\n", 34 * (num_vectors - 1));

    /* Cleanup */
    for (uint16_t v = 0; v < num_vectors; v++) {
        free(vectors[v]);
        free(decoded[v]);
        sparse_phase2_free(individual_encoded[v]);
    }
    free(vectors);
    free(decoded);
    free(individual_encoded);
    sparse_batch_free(batch);

    return 0;
}
