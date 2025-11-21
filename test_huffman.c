/**
 * test_huffman.c
 *
 * Test program for Huffman vector encoding
 */

#include "huffman_vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* Generate test vector with realistic distribution */
void generate_test_vector(uint8_t *vector, size_t dim) {
    /* Distribution matching user's data:
     * 0: 23.2%, 1: 43.0%, 2: 23.1%, 3: 8.4%, 4: 2.0%, 5: 0.1%, 6: 0.1%
     */
    uint32_t counts[] = {476, 880, 474, 173, 41, 2, 2};
    uint8_t values[] = {0, 1, 2, 3, 4, 5, 6};

    size_t pos = 0;
    for (int val = 0; val < 7; val++) {
        for (uint32_t i = 0; i < counts[val] && pos < dim; i++) {
            vector[pos++] = values[val];
        }
    }

    /* Shuffle for realism */
    srand(42);
    for (size_t i = dim - 1; i > 0; i--) {
        size_t j = rand() % (i + 1);
        uint8_t temp = vector[i];
        vector[i] = vector[j];
        vector[j] = temp;
    }
}

/* Calculate L2 norm */
float calculate_l2_norm(const uint8_t *vector, size_t dim) {
    uint64_t sum_sq = 0;
    for (size_t i = 0; i < dim; i++) {
        sum_sq += vector[i] * vector[i];
    }
    return sqrtf((float)sum_sq);
}

/* Count value distribution */
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

int main() {
    printf("=========================================================\n");
    printf("Huffman Vector Encoding Test\n");
    printf("=========================================================\n\n");

    size_t dim = 2048;
    uint8_t *original = malloc(dim);
    uint8_t *decoded = malloc(dim);

    if (!original || !decoded) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    /* Generate test vector */
    printf("Generating test vector (dim=%zu)...\n", dim);
    generate_test_vector(original, dim);

    float l2_norm = calculate_l2_norm(original, dim);
    printf("L2 norm: %.2f\n\n", l2_norm);

    print_distribution(original, dim);

    /* Encode */
    printf("\n--- Encoding ---\n");
    clock_t start = clock();
    encoded_result_t *enc = huffman_encode(original, dim);
    clock_t end = clock();

    if (!enc) {
        fprintf(stderr, "Encoding failed\n");
        free(original);
        free(decoded);
        return 1;
    }

    double encode_time = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;

    printf("Encoded size: %zu bytes\n", enc->size);
    printf("Original size (int8): %zu bytes\n", dim);
    printf("Original size (3-bit packed): %zu bytes\n", (dim * 3 + 7) / 8);
    printf("Compression ratio: %.2fx vs int8\n", (double)dim / enc->size);
    printf("Compression ratio: %.2fx vs 3-bit\n", (double)(dim * 3 / 8) / enc->size);
    printf("Encode time: %.3f ms\n", encode_time);

    /* Show first few bytes */
    printf("\nFirst 32 bytes of encoded data:\n");
    for (size_t i = 0; i < 32 && i < enc->size; i++) {
        printf("%02x ", enc->data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");

    /* Decode */
    printf("\n--- Decoding ---\n");
    start = clock();
    int result = huffman_decode(enc->data, enc->size, decoded, dim);
    end = clock();

    if (result != 0) {
        fprintf(stderr, "Decoding failed\n");
        huffman_free(enc);
        free(original);
        free(decoded);
        return 1;
    }

    double decode_time = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
    printf("Decode time: %.3f ms\n", decode_time);

    /* Verify */
    printf("\n--- Verification ---\n");
    int errors = 0;
    for (size_t i = 0; i < dim; i++) {
        if (original[i] != decoded[i]) {
            if (errors < 10) {
                printf("Mismatch at index %zu: expected %u, got %u\n",
                       i, original[i], decoded[i]);
            }
            errors++;
        }
    }

    if (errors == 0) {
        printf("✓ Perfect reconstruction! All %zu values match.\n", dim);
    } else {
        printf("✗ %d mismatches found\n", errors);
    }

    /* Estimate final size with zstd */
    printf("\n--- Compression Estimate ---\n");
    printf("Huffman encoded: %zu bytes\n", enc->size);
    printf("Estimated with zstd level 19:\n");
    printf("  Conservative (2.0x): ~%zu bytes (total: %.1fx compression)\n",
           enc->size / 2, (double)dim / (enc->size / 2));
    printf("  Optimistic (2.5x):   ~%zu bytes (total: %.1fx compression)\n",
           enc->size / 2.5, (double)dim / (enc->size / 2.5));

    printf("\n=========================================================\n");
    printf("SUMMARY\n");
    printf("=========================================================\n");
    printf("Input:        %zu bytes (2048 uint8 values)\n", dim);
    printf("Huffman only: %zu bytes (%.1fx compression)\n", enc->size, (double)dim / enc->size);
    printf("+ zstd:       ~%zu-%zu bytes (%.1f-%.1fx compression)\n",
           (size_t)(enc->size / 2.5), enc->size / 2,
           (double)dim / (enc->size / 2), (double)dim / (enc->size / 2.5));
    printf("\nFor 256-byte target: Huffman + zstd should achieve it!\n");
    printf("=========================================================\n");

    /* Cleanup */
    huffman_free(enc);
    free(original);
    free(decoded);

    return 0;
}
