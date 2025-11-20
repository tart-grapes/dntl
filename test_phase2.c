/**
 * test_phase2.c
 *
 * Test Phase 2: Delta alphabet + rANS
 * Expected: ~162 bytes for 97 nz vectors (vs 167 Phase 1 baseline)
 * Savings: 5 bytes additional (3% reduction from Phase 1)
 */

#include "sparse_phase2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define DIMENSION 2048

/* Generate s_mangled-like vector with 97 nz, range [-43,42] */
void generate_s_mangled_97nz(int8_t *vector, unsigned int seed) {
    srand(seed);
    memset(vector, 0, DIMENSION * sizeof(int8_t));

    /* s_mangled has 97 nz with range [-43, 42] */
    /* Gaussian-like distribution centered around 0 */

    uint16_t k = 97;
    uint16_t positions[97];

    /* Select random positions */
    for (uint16_t i = 0; i < k; i++) {
        uint16_t pos;
        int retry = 0;
        do {
            pos = rand() % DIMENSION;
            retry = 0;
            for (uint16_t j = 0; j < i; j++) {
                if (positions[j] == pos) {
                    retry = 1;
                    break;
                }
            }
        } while (retry);
        positions[i] = pos;
    }

    /* Generate values with Gaussian-like distribution */
    for (uint16_t i = 0; i < k; i++) {
        float u1 = (rand() + 1.0f) / (RAND_MAX + 2.0f);
        float u2 = (rand() + 1.0f) / (RAND_MAX + 2.0f);
        float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * M_PI * u2);

        /* Scale to wider range for s_mangled-like distribution */
        int value = (int)round(z * 12.0);

        /* Clamp to [-43, 42] */
        if (value < -43) value = -43;
        if (value > 42) value = 42;

        vector[positions[i]] = (int8_t)value;
    }
}

/* Verify vector equality */
int verify_vectors(const int8_t *a, const int8_t *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) {
            printf("Mismatch at position %zu: expected %d, got %d\n",
                   i, a[i], b[i]);
            return 0;
        }
    }
    return 1;
}

/* Calculate L2 norm */
double calculate_l2_norm(const int8_t *vector, size_t n) {
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        sum += (double)vector[i] * vector[i];
    }
    return sqrt(sum);
}

/* Analyze vector properties */
void analyze_vector(const int8_t *vector, size_t n) {
    int8_t min_val = 127, max_val = -128;
    uint16_t count = 0;

    uint32_t freqs[256] = {0};

    for (size_t i = 0; i < n; i++) {
        if (vector[i] != 0) {
            count++;
            if (vector[i] < min_val) min_val = vector[i];
            if (vector[i] > max_val) max_val = vector[i];
            freqs[(uint8_t)(vector[i] + 128)]++;
        }
    }

    int n_unique = 0;
    for (int v = -128; v <= 127; v++) {
        if (freqs[(uint8_t)(v + 128)] > 0) {
            n_unique++;
        }
    }

    double l2_norm = calculate_l2_norm(vector, n);

    printf("Vector stats:\n");
    printf("  Non-zeros: %u / %zu (%.1f%% sparse)\n",
           count, n, 100.0 * (1.0 - (double)count / n));
    printf("  Range: [%d, %d] (span=%d)\n", min_val, max_val, max_val - min_val + 1);
    printf("  Unique values: %d\n", n_unique);
    printf("  L2 norm: %.1f\n", l2_norm);
}

int main(void) {
    printf("=== Phase 2: Delta Alphabet + rANS ===\n\n");

    int8_t *vector = malloc(DIMENSION * sizeof(int8_t));
    int8_t *decoded = malloc(DIMENSION * sizeof(int8_t));

    if (!vector || !decoded) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    /* Test on 20 random instances */
    size_t total_size = 0;
    int success_count = 0;

    for (int i = 0; i < 20; i++) {
        generate_s_mangled_97nz(vector, 1000 + i * 123);

        if (i == 0) {
            printf("Example vector (seed 1000):\n");
            analyze_vector(vector, DIMENSION);
            printf("\n");
        }

        /* Encode with Phase 2 (delta + rANS) */
        sparse_phase2_t *enc = sparse_phase2_encode(vector, DIMENSION);

        if (!enc) {
            printf("Trial %2d: ✗ Encoding failed\n", i + 1);
            continue;
        }

        /* Decode */
        int decode_result = sparse_phase2_decode(enc, decoded, DIMENSION);

        if (decode_result < 0) {
            printf("Trial %2d: ✗ Decoding failed\n", i + 1);
            sparse_phase2_free(enc);
            continue;
        }

        /* Verify */
        int match = verify_vectors(vector, decoded, DIMENSION);

        if (match) {
            printf("Trial %2d: ✓ %3zu bytes\n", i + 1, enc->size);
            total_size += enc->size;
            success_count++;
        } else {
            printf("Trial %2d: ✗ Verification failed (%zu bytes)\n",
                   i + 1, enc->size);
        }

        sparse_phase2_free(enc);
    }

    printf("\n=== Results ===\n");
    printf("Success rate: %d/20\n", success_count);

    if (success_count > 0) {
        double avg_size = (double)total_size / success_count;
        printf("Average size: %.1f bytes\n", avg_size);

        /* Compare to baselines */
        double baseline_original = 209.0;  /* sparse_optimal_large baseline */
        double baseline_phase1 = 167.5;     /* Phase 1 baseline */
        double savings_from_original = baseline_original - avg_size;
        double savings_from_phase1 = baseline_phase1 - avg_size;
        double improvement_from_original = 100.0 * savings_from_original / baseline_original;
        double improvement_from_phase1 = 100.0 * savings_from_phase1 / baseline_phase1;

        printf("\nComparison:\n");
        printf("  Original (sparse_optimal_large): %.0f bytes\n", baseline_original);
        printf("  Phase 1 (delta alphabet):        %.1f bytes\n", baseline_phase1);
        printf("  Phase 2 (delta + rANS):          %.1f bytes\n", avg_size);
        printf("\n");
        printf("  Savings from original:           %.1f bytes (%.1f%% reduction)\n",
               savings_from_original, improvement_from_original);
        printf("  Savings from Phase 1:            %.1f bytes (%.1f%% reduction)\n",
               savings_from_phase1, improvement_from_phase1);

        /* Expected: ~162 bytes */
        printf("\n");
        if (avg_size < 165.0) {
            printf("✓ Target achieved! (expected ~162 bytes)\n");
        } else {
            printf("⚠ Above target (expected ~162 bytes, got %.1f bytes)\n", avg_size);
        }
    }

    free(vector);
    free(decoded);

    return (success_count == 20) ? 0 : 1;
}
