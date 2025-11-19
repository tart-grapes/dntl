/**
 * test_compress.c
 *
 * Test complete vector compression pipeline (Huffman + zstd)
 */

#include "vector_compress.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* Generate test vector with realistic distribution */
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

float calculate_l2_norm(const uint8_t *vector, size_t dim) {
    uint64_t sum_sq = 0;
    for (size_t i = 0; i < dim; i++) {
        sum_sq += vector[i] * vector[i];
    }
    return sqrtf((float)sum_sq);
}

void print_distribution(const uint8_t *vector, size_t dim) {
    uint32_t counts[256] = {0};
    uint8_t max_val = 0;

    for (size_t i = 0; i < dim; i++) {
        counts[vector[i]]++;
        if (vector[i] > max_val) max_val = vector[i];
    }

    printf("Value distribution:\n");
    for (uint8_t i = 0; i <= max_val; i++) {
        if (counts[i] > 0) {
            printf("  %u: %5u (%5.1f%%)\n", i, counts[i], 100.0 * counts[i] / dim);
        }
    }
}

void test_compression_level(const uint8_t *original, size_t dim,
                            compress_level_t level, const char *level_name) {
    printf("\n--- %s ---\n", level_name);

    /* Compress */
    clock_t start = clock();
    compressed_vector_t *comp = vector_compress(original, dim, level);
    clock_t end = clock();

    if (!comp) {
        fprintf(stderr, "Compression failed\n");
        return;
    }

    double compress_time = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;

    printf("Compressed size: %zu bytes\n", comp->size);
    printf("Compression ratio: %.2fx vs 2048 bytes\n", (double)dim / comp->size);
    printf("Compress time: %.3f ms\n", compress_time);

    /* Decompress */
    uint8_t *decoded = malloc(dim);
    if (!decoded) {
        fprintf(stderr, "Memory allocation failed\n");
        vector_compress_free(comp);
        return;
    }

    start = clock();
    int result = vector_decompress(comp->data, comp->size, decoded, dim);
    end = clock();

    if (result != 0) {
        fprintf(stderr, "Decompression failed\n");
        vector_compress_free(comp);
        free(decoded);
        return;
    }

    double decompress_time = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
    printf("Decompress time: %.3f ms\n", decompress_time);

    /* Verify */
    int errors = 0;
    for (size_t i = 0; i < dim; i++) {
        if (original[i] != decoded[i]) {
            if (errors < 5) {
                printf("Mismatch at %zu: expected %u, got %u\n",
                       i, original[i], decoded[i]);
            }
            errors++;
        }
    }

    if (errors == 0) {
        printf("✓ Perfect reconstruction!\n");
    } else {
        printf("✗ %d mismatches\n", errors);
    }

    /* Check if it meets 256-byte target */
    if (comp->size <= 256) {
        printf("✓ MEETS 256-byte target (%.1f%% under budget)\n",
               100.0 * (256 - comp->size) / 256);
    } else {
        printf("✗ Exceeds 256-byte target by %zu bytes\n", comp->size - 256);
    }

    vector_compress_free(comp);
    free(decoded);
}

int main() {
    printf("=========================================================\n");
    printf("Vector Compression Test (Huffman + zstd)\n");
    printf("=========================================================\n\n");

    size_t dim = 2048;
    uint8_t *original = malloc(dim);

    if (!original) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    /* Generate test vector */
    printf("Generating test vector (dim=%zu)...\n", dim);
    generate_test_vector(original, dim);

    float l2_norm = calculate_l2_norm(original, dim);
    printf("L2 norm: %.2f\n\n", l2_norm);

    print_distribution(original, dim);

    /* Test different compression levels */
    test_compression_level(original, dim, COMPRESS_LEVEL_FAST, "FAST (level 1)");
    test_compression_level(original, dim, COMPRESS_LEVEL_DEFAULT, "DEFAULT (level 3)");
    test_compression_level(original, dim, COMPRESS_LEVEL_BEST, "BEST (level 19)");
    test_compression_level(original, dim, COMPRESS_LEVEL_MAX, "MAX (level 22)");

    printf("\n=========================================================\n");
    printf("SUMMARY\n");
    printf("=========================================================\n");
    printf("Input: %zu bytes\n", dim);
    printf("Best compression: Use level 19 or 22 for ~200-250 bytes\n");
    printf("Fast compression: Use level 1-3 for ~300-400 bytes\n");
    printf("\nFor 256-byte target: Level 19+ recommended\n");
    printf("=========================================================\n");

    free(original);

    return 0;
}
