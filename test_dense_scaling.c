/**
 * test_dense_scaling.c
 *
 * Test how dense batch encoding scales with batch size
 */

#include "dense_batch.h"
#include <stdio.h>
#include <stdlib.h>

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

static void test_batch_size(int N) {
    const uint16_t dimension = 128;
    const uint16_t modulus = 1031;
    const float small_ratio = 0.828;

    /* Generate vectors */
    uint16_t **vectors = malloc(N * sizeof(uint16_t*));
    for (int v = 0; v < N; v++) {
        vectors[v] = malloc(dimension * sizeof(uint16_t));
        generate_signature_like(vectors[v], dimension, modulus, small_ratio, 1000 + v);
    }

    /* Baseline */
    size_t baseline = 176 * N;

    /* Batch encode */
    dense_batch_t *batch = dense_batch_encode((const uint16_t**)vectors,
                                               N, dimension, modulus);
    if (!batch) {
        fprintf(stderr, "Encoding failed\n");
        return;
    }

    double per_vec = (double)batch->size / N;
    int saved = (int)baseline - (int)batch->size;
    double percent = 100.0 * saved / baseline;

    printf("N=%3d: Baseline=%5zu  Batch=%5zu (%.1f/vec)  ",
           N, baseline, batch->size, per_vec);

    if (saved > 0) {
        printf("Saved %4d bytes (%.1f%%)\n", saved, percent);
    } else {
        printf("Overhead %4d bytes (%.1f%%)\n", -saved, -percent);
    }

    dense_batch_free(batch);
    for (int v = 0; v < N; v++) {
        free(vectors[v]);
    }
    free(vectors);
}

int main() {
    printf("=== Dense Batch Scaling (s component: 128 coeffs mod 1031) ===\n\n");

    int sizes[] = {1, 2, 5, 10, 20, 50, 100};
    for (int i = 0; i < 7; i++) {
        test_batch_size(sizes[i]);
    }

    printf("\nConclusion:\n");
    printf("- Small batches (N<20): Overhead dominates\n");
    printf("- Large batches (N≥50): Starts to pay off\n");
    printf("- Problem: Too many unique values even with concentrated distribution\n");

    return 0;
}
