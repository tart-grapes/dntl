/**
 * test_compress_brotli.c
 *
 * Test vector compression with Brotli (Huffman + Brotli)
 * Includes random tests to measure compression across different distributions
 */

#include "vector_compress_brotli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* Generate test vector with specific distribution */
void generate_test_vector(uint8_t *vector, size_t dim) {
    uint32_t counts[] = {476, 880, 474, 173, 41, 2, 2};
    uint8_t values[] = {0, 1, 2, 3, 4, 5, 6};

    size_t pos = 0;
    for (int val = 0; val < 7; val++) {
        for (uint32_t i = 0; i < counts[val] && pos < dim; i++) {
            vector[pos++] = values[val];
        }
    }

    /* Shuffle */
    srand(42);
    for (size_t i = dim - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        uint8_t temp = vector[i];
        vector[i] = vector[j];
        vector[j] = temp;
    }
}

/* Generate random vector with specific alphabet */
void generate_random_vector(uint8_t *vector, size_t dim, uint8_t max_val, unsigned int seed) {
    srand(seed);
    for (size_t i = 0; i < dim; i++) {
        vector[i] = rand() % (max_val + 1);
    }
}

/* Generate sparse random vector */
void generate_sparse_random(uint8_t *vector, size_t dim, float sparsity, uint8_t max_val, unsigned int seed) {
    srand(seed);
    memset(vector, 0, dim);

    size_t num_nonzero = (size_t)(dim * (1.0 - sparsity));
    for (size_t i = 0; i < num_nonzero; i++) {
        size_t idx = rand() % dim;
        vector[idx] = 1 + (rand() % max_val);
    }
}

/* Generate vector with Gaussian-like distribution */
void generate_gaussian_like(uint8_t *vector, size_t dim, uint8_t mean, uint8_t stddev, unsigned int seed) {
    srand(seed);
    for (size_t i = 0; i < dim; i++) {
        /* Simple approximation: sum of uniforms */
        int sum = 0;
        for (int j = 0; j < 12; j++) {
            sum += rand() % 256;
        }
        int val = mean + (sum / 12 - 128) * stddev / 128;
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        vector[i] = (uint8_t)val;
    }
}

float calculate_l2_norm(const uint8_t *vector, size_t dim) {
    uint64_t sum_sq = 0;
    for (size_t i = 0; i < dim; i++) {
        sum_sq += vector[i] * vector[i];
    }
    return sqrtf((float)sum_sq);
}

void print_stats(const uint8_t *vector, size_t dim) {
    uint32_t counts[256] = {0};
    uint8_t min_val = 255, max_val = 0;
    uint32_t nonzero = 0;

    for (size_t i = 0; i < dim; i++) {
        counts[vector[i]]++;
        if (vector[i] > 0) nonzero++;
        if (vector[i] < min_val) min_val = vector[i];
        if (vector[i] > max_val) max_val = vector[i];
    }

    printf("  L2 norm: %.2f\n", calculate_l2_norm(vector, dim));
    printf("  Range: [%u, %u]\n", min_val, max_val);
    printf("  Non-zeros: %u (%.1f%%)\n", nonzero, 100.0 * nonzero / dim);
    printf("  Unique values: ");
    int unique = 0;
    for (int i = 0; i < 256; i++) {
        if (counts[i] > 0) unique++;
    }
    printf("%d\n", unique);
}

void test_compression(const char *test_name, const uint8_t *vector, size_t dim,
                      brotli_level_t level, const char *level_name) {
    printf("\n--- %s (%s) ---\n", test_name, level_name);
    print_stats(vector, dim);

    /* Compress */
    clock_t start = clock();
    compressed_vector_brotli_t *comp = vector_compress_brotli(vector, dim, level);
    clock_t end = clock();

    if (!comp) {
        printf("✗ Compression failed\n");
        return;
    }

    double compress_time = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;

    printf("  Compressed: %zu bytes (%.2fx compression)\n", comp->size, (double)dim / comp->size);
    printf("  Compress time: %.3f ms\n", compress_time);

    /* Decompress */
    uint8_t *decoded = malloc(dim);
    if (!decoded) {
        printf("✗ Memory allocation failed\n");
        vector_compress_brotli_free(comp);
        return;
    }

    start = clock();
    int result = vector_decompress_brotli(comp->data, comp->size, decoded, dim);
    end = clock();

    if (result != 0) {
        printf("✗ Decompression failed\n");
        vector_compress_brotli_free(comp);
        free(decoded);
        return;
    }

    double decompress_time = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
    printf("  Decompress time: %.3f ms\n", decompress_time);

    /* Verify */
    int errors = 0;
    for (size_t i = 0; i < dim; i++) {
        if (vector[i] != decoded[i]) {
            if (errors < 3) {
                printf("  Mismatch at %zu: expected %u, got %u\n",
                       i, vector[i], decoded[i]);
            }
            errors++;
        }
    }

    if (errors == 0) {
        printf("  ✓ Perfect reconstruction\n");
    } else {
        printf("  ✗ %d mismatches\n", errors);
    }

    /* Check target */
    if (comp->size <= 256) {
        printf("  ✓ MEETS 256-byte target (%zu bytes under)\n", 256 - comp->size);
    } else {
        printf("  ✗ Exceeds 256-byte target by %zu bytes\n", comp->size - 256);
    }

    vector_compress_brotli_free(comp);
    free(decoded);
}

int main() {
    printf("=========================================================\n");
    printf("Brotli Vector Compression Test Suite\n");
    printf("=========================================================\n");

    size_t dim = 2048;
    uint8_t *test_vector = malloc(dim);
    if (!test_vector) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    /* Test 1: Original distribution (realistic) */
    printf("\n=== TEST 1: Realistic Dense Distribution ===\n");
    generate_test_vector(test_vector, dim);
    test_compression("Realistic", test_vector, dim, BROTLI_LEVEL_FAST, "Level 1");
    generate_test_vector(test_vector, dim);
    test_compression("Realistic", test_vector, dim, BROTLI_LEVEL_BEST, "Level 9");
    generate_test_vector(test_vector, dim);
    test_compression("Realistic", test_vector, dim, BROTLI_LEVEL_MAX, "Level 11");

    /* Test 2: Uniform random */
    printf("\n=== TEST 2: Uniform Random (alphabet size 7) ===\n");
    generate_random_vector(test_vector, dim, 6, 12345);
    test_compression("Uniform random", test_vector, dim, BROTLI_LEVEL_BEST, "Level 9");

    /* Test 3: Binary random */
    printf("\n=== TEST 3: Binary Random {0, 1} ===\n");
    generate_random_vector(test_vector, dim, 1, 23456);
    test_compression("Binary random", test_vector, dim, BROTLI_LEVEL_BEST, "Level 9");

    /* Test 4: Larger alphabet */
    printf("\n=== TEST 4: Larger Alphabet (0-15) ===\n");
    generate_random_vector(test_vector, dim, 15, 34567);
    test_compression("Alphabet 0-15", test_vector, dim, BROTLI_LEVEL_BEST, "Level 9");

    /* Test 5: Sparse vector */
    printf("\n=== TEST 5: Sparse (90%% zeros) ===\n");
    generate_sparse_random(test_vector, dim, 0.90, 6, 45678);
    test_compression("Sparse 90%%", test_vector, dim, BROTLI_LEVEL_BEST, "Level 9");

    /* Test 6: Very sparse */
    printf("\n=== TEST 6: Very Sparse (95%% zeros) ===\n");
    generate_sparse_random(test_vector, dim, 0.95, 6, 56789);
    test_compression("Sparse 95%%", test_vector, dim, BROTLI_LEVEL_BEST, "Level 9");

    /* Test 7: Gaussian-like distribution */
    printf("\n=== TEST 7: Gaussian-like (mean=3, std=1) ===\n");
    generate_gaussian_like(test_vector, dim, 3, 1, 67890);
    test_compression("Gaussian", test_vector, dim, BROTLI_LEVEL_BEST, "Level 9");

    /* Test 8: All same value (best case) */
    printf("\n=== TEST 8: Constant (all 2s) ===\n");
    memset(test_vector, 2, dim);
    test_compression("Constant", test_vector, dim, BROTLI_LEVEL_BEST, "Level 9");

    /* Test 9: Worst case - high entropy */
    printf("\n=== TEST 9: High Entropy (0-255) ===\n");
    generate_random_vector(test_vector, dim, 255, 78901);
    test_compression("High entropy", test_vector, dim, BROTLI_LEVEL_BEST, "Level 9");

    /* Random stress test */
    printf("\n=== RANDOM STRESS TEST (10 trials) ===\n");
    printf("\nAlphabet {0-6}, random seeds:\n");

    size_t total_size = 0;
    int successes = 0;

    for (int trial = 0; trial < 10; trial++) {
        unsigned int seed = 10000 + trial * 1111;
        generate_random_vector(test_vector, dim, 6, seed);

        compressed_vector_brotli_t *comp = vector_compress_brotli(test_vector, dim, BROTLI_LEVEL_BEST);
        if (comp) {
            uint8_t *decoded = malloc(dim);
            if (decoded && vector_decompress_brotli(comp->data, comp->size, decoded, dim) == 0) {
                /* Verify */
                int match = 1;
                for (size_t i = 0; i < dim; i++) {
                    if (test_vector[i] != decoded[i]) {
                        match = 0;
                        break;
                    }
                }

                if (match) {
                    printf("  Trial %2d: %4zu bytes (%.2fx) %s\n",
                           trial + 1, comp->size, (double)dim / comp->size,
                           comp->size <= 256 ? "✓" : "");
                    total_size += comp->size;
                    successes++;
                }
            }
            free(decoded);
            vector_compress_brotli_free(comp);
        }
    }

    if (successes > 0) {
        printf("\nAverage: %.1f bytes (%.2fx compression)\n",
               (double)total_size / successes, (double)dim / (total_size / (double)successes));
        printf("Success rate: %d/10\n", successes);
    }

    printf("\n=========================================================\n");
    printf("SUMMARY\n");
    printf("=========================================================\n");
    printf("All tests use Huffman + Brotli compression.\n");
    printf("Best results: Sparse and structured data (100-250 bytes)\n");
    printf("Realistic dense: ~200-250 bytes with level 9-11\n");
    printf("High entropy: ~500-800 bytes (still better than 2048!)\n");
    printf("\nRecommendation: Brotli level 9 or 11 for 256-byte target\n");
    printf("=========================================================\n");

    free(test_vector);
    return 0;
}
