/**
 * test_sparse_optimal.c
 *
 * Test the optimal sparse encoder for k=145, 70% ±2 / 30% ±1
 */

#include "sparse_optimal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Generate test vector with target distribution */
void generate_test_vector(int8_t *vector, size_t dim, uint16_t k, unsigned int seed) {
    memset(vector, 0, dim * sizeof(int8_t));
    srand(seed);

    /* Generate k random positions */
    uint16_t placed = 0;
    while (placed < k) {
        size_t pos = rand() % dim;
        if (vector[pos] == 0) {
            /* 70% ±2, 30% ±1 */
            float r = (float)rand() / RAND_MAX;
            if (r < 0.35) {
                vector[pos] = -2;
            } else if (r < 0.70) {
                vector[pos] = 2;
            } else if (r < 0.85) {
                vector[pos] = -1;
            } else {
                vector[pos] = 1;
            }
            placed++;
        }
    }
}

/* Verify vectors match */
int verify_match(const int8_t *v1, const int8_t *v2, size_t dim) {
    int mismatches = 0;
    for (size_t i = 0; i < dim; i++) {
        if (v1[i] != v2[i]) {
            if (mismatches < 5) {
                printf("  Mismatch at %zu: expected %d, got %d\n", i, v1[i], v2[i]);
            }
            mismatches++;
        }
    }
    return mismatches;
}

void test_configuration(const char *name, size_t dim, uint16_t k, unsigned int seed) {
    printf("\n=== %s ===\n", name);

    int8_t *original = malloc(dim * sizeof(int8_t));
    int8_t *decoded = malloc(dim * sizeof(int8_t));
    if (!original || !decoded) {
        printf("Memory allocation failed\n");
        free(original);
        free(decoded);
        return;
    }

    /* Generate test vector */
    generate_test_vector(original, dim, k, seed);

    /* Get statistics */
    uint16_t actual_k;
    float l2_norm, entropy;
    sparse_stats(original, dim, &actual_k, &l2_norm, &entropy);

    printf("Original vector:\n");
    printf("  Dimension: %zu\n", dim);
    printf("  Non-zeros: %u (%.1f%% sparse)\n", actual_k, 100.0 * (1.0 - (float)actual_k/dim));
    printf("  L2 norm: %.2f\n", l2_norm);
    printf("  Entropy: %.2f bits/value\n", entropy);
    printf("  Security score: %.2f\n", entropy * l2_norm);

    /* Encode */
    clock_t start = clock();
    sparse_encoded_t *encoded = sparse_encode(original, dim);
    clock_t end = clock();
    double encode_time = (double)(end - start) / CLOCKS_PER_SEC * 1000000.0;

    if (!encoded) {
        printf("✗ Encoding failed\n");
        free(original);
        free(decoded);
        return;
    }

    printf("\nEncoded:\n");
    printf("  Size: %zu bytes (%.2fx compression)\n", encoded->size, (double)dim / encoded->size);
    printf("  Encode time: %.1f µs\n", encode_time);
    printf("  Bytes/entry: %.2f\n", (double)encoded->size / actual_k);

    /* Check 256-byte target */
    if (encoded->size <= 256) {
        printf("  ✓ MEETS 256-byte target (%zu bytes under)\n", 256 - encoded->size);
    } else {
        printf("  ✗ Exceeds 256-byte target by %zu bytes\n", encoded->size - 256);
    }

    /* Decode */
    start = clock();
    int decode_result = sparse_decode(encoded, decoded, dim);
    end = clock();
    double decode_time = (double)(end - start) / CLOCKS_PER_SEC * 1000000.0;

    if (decode_result != 0) {
        printf("\n✗ Decoding failed\n");
        sparse_free(encoded);
        free(original);
        free(decoded);
        return;
    }

    printf("  Decode time: %.1f µs\n", decode_time);

    /* Verify */
    int mismatches = verify_match(original, decoded, dim);
    if (mismatches == 0) {
        printf("\n✓ Perfect reconstruction!\n");
    } else {
        printf("\n✗ %d mismatches found\n", mismatches);
    }

    sparse_free(encoded);
    free(original);
    free(decoded);
}

int main() {
    printf("=========================================================\n");
    printf("Optimal Sparse Vector Encoder Test\n");
    printf("=========================================================\n");
    printf("Target: k=145, 70%% ±2 / 30%% ±1, <256 bytes\n");
    printf("Format: [count:16] [pos:11, val:2]*\n");
    printf("=========================================================\n");

    /* Test 1: Exact target configuration */
    test_configuration("Target: k=145, 70/30 distribution", 2048, 145, 12345);

    /* Test 2: Slightly different k */
    test_configuration("Alternative: k=135", 2048, 135, 23456);

    /* Test 3: k=155 (from analysis) */
    test_configuration("Alternative: k=155", 2048, 155, 34567);

    /* Test 4: Ultra-sparse (user's original data) */
    printf("\n=== Bonus: Ultra-sparse (k=27) ===\n");
    int8_t *sparse_vec = calloc(2048, sizeof(int8_t));
    generate_test_vector(sparse_vec, 2048, 27, 99999);

    sparse_encoded_t *encoded = sparse_encode(sparse_vec, 2048);
    if (encoded) {
        printf("k=27 ultra-sparse: %zu bytes (%.1fx compression)\n",
               encoded->size, 2048.0 / encoded->size);
        sparse_free(encoded);
    }
    free(sparse_vec);

    /* Random trials */
    printf("\n=== Random Stress Test (20 trials, k=145) ===\n");
    size_t total_size = 0;
    int successes = 0;
    int meets_target = 0;

    for (int trial = 0; trial < 20; trial++) {
        int8_t *vec = malloc(2048 * sizeof(int8_t));
        generate_test_vector(vec, 2048, 145, 10000 + trial * 1111);

        sparse_encoded_t *enc = sparse_encode(vec, 2048);
        if (enc) {
            int8_t *dec = malloc(2048 * sizeof(int8_t));
            if (dec && sparse_decode(enc, dec, 2048) == 0) {
                if (verify_match(vec, dec, 2048) == 0) {
                    char mark = enc->size <= 256 ? '*' : ' ';
                    printf("  Trial %2d: %3zu bytes (%.2fx) %c\n",
                           trial + 1, enc->size, 2048.0 / enc->size, mark);
                    total_size += enc->size;
                    successes++;
                    if (enc->size <= 256) meets_target++;
                }
            }
            free(dec);
            sparse_free(enc);
        }
        free(vec);
    }

    if (successes > 0) {
        printf("\nAverage: %.1f bytes (%.2fx compression)\n",
               (double)total_size / successes, 2048.0 / (total_size / (double)successes));
        printf("Success rate: %d/20\n", successes);
        printf("Meets 256-byte target: %d/%d (%.0f%%)\n",
               meets_target, successes, 100.0 * meets_target / successes);
    }

    printf("\n=========================================================\n");
    printf("SUMMARY\n");
    printf("=========================================================\n");
    printf("Optimal configuration: k=145, 70%% ±2 / 30%% ±1\n");
    printf("Expected size: ~238 bytes\n");
    printf("L2 norm: ~21.2\n");
    printf("Entropy: ~1.88 bits/value\n");
    printf("Security score: ~39.89\n");
    printf("\nThis maximizes security while staying well under 256 bytes!\n");
    printf("=========================================================\n");

    return 0;
}
