/**
 * Test ultimate encoder
 */

#include "sparse_ultimate.h"
#include "sparse_optimal_large.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void generate_s_mangled_97nz(int8_t *vector, unsigned int seed) {
    memset(vector, 0, 2048);
    srand(seed);

    uint16_t placed = 0;
    while (placed < 97) {
        size_t pos = rand() % 2048;
        if (vector[pos] == 0) {
            float r = (float)rand() / RAND_MAX;
            int val;
            if (r < 0.25) val = 20 + rand() % 15;
            else if (r < 0.50) val = 25 + rand() % 15;
            else if (r < 0.75) val = 30 + rand() % 13;
            else val = 15 + rand() % 15;
            if (rand() % 2) val = -val;
            if (val > 42) val = 42;
            if (val < -43) val = -43;
            vector[pos] = val;
            placed++;
        }
    }
}

void compute_stats(const int8_t *vector, uint16_t *nz, float *norm) {
    uint64_t sum_sq = 0;
    *nz = 0;
    for (size_t i = 0; i < 2048; i++) {
        if (vector[i] != 0) {
            (*nz)++;
            sum_sq += (int64_t)vector[i] * vector[i];
        }
    }
    *norm = sqrtf((float)sum_sq);
}

int verify_decode(const int8_t *orig, const int8_t *decoded) {
    for (size_t i = 0; i < 2048; i++) {
        if (orig[i] != decoded[i]) return 0;
    }
    return 1;
}

int main() {
    printf("=========================================================\n");
    printf("Ultimate Sparse Encoder Test\n");
    printf("=========================================================\n");
    printf("Target: ~148 bytes (vs 209 bytes baseline)\n");
    printf("Optimizations: Delta alphabet + ANS + adaptive Rice\n");
    printf("=========================================================\n\n");

    int8_t *vector = malloc(2048);
    int8_t *decoded = malloc(2048);

    generate_s_mangled_97nz(vector, 456);

    uint16_t nz;
    float norm;
    compute_stats(vector, &nz, &norm);

    printf("Test vector: %u nz, norm=%.1f\n\n", nz, norm);

    /* Baseline: optimal_large */
    printf("=== Baseline (sparse_optimal_large) ===\n");
    sparse_optimal_large_t *baseline = sparse_optimal_large_encode(vector, 2048);
    if (baseline) {
        printf("Size: %zu bytes\n", baseline->size);
        memset(decoded, 0, 2048);
        if (sparse_optimal_large_decode(baseline, decoded, 2048) == 0) {
            printf("Decode: %s\n", verify_decode(vector, decoded) ? "✓" : "✗");
        }
        sparse_optimal_large_free(baseline);
    }

    /* Ultimate encoder */
    printf("\n=== Ultimate Encoder ===\n");
    sparse_ultimate_t *ultimate = sparse_ultimate_encode(vector, 2048);
    if (ultimate) {
        printf("Size: %zu bytes\n", ultimate->size);

        if (baseline) {
            int saved = (int)baseline->size - (int)ultimate->size;
            float pct = 100.0 * saved / baseline->size;
            printf("vs baseline: %d bytes saved (%.1f%% reduction)\n", saved, pct);
        }

        printf("vs 1792 bytes: %.1fx better\n", 1792.0 / ultimate->size);

        memset(decoded, 0, 2048);
        int result = sparse_ultimate_decode(ultimate, decoded, 2048);
        if (result == 0) {
            if (verify_decode(vector, decoded)) {
                printf("Decode: ✓ Perfect\n");
            } else {
                printf("Decode: ✗ Mismatch\n");
                /* Show first few mismatches */
                int shown = 0;
                for (size_t i = 0; i < 2048 && shown < 5; i++) {
                    if (vector[i] != decoded[i]) {
                        printf("  [%zu]: expected %d, got %d\n", i, vector[i], decoded[i]);
                        shown++;
                    }
                }
            }
        } else {
            printf("Decode: ✗ Failed\n");
        }

        sparse_ultimate_free(ultimate);
    } else {
        printf("✗ Encoding failed\n");
    }

    /* Multiple trials */
    printf("\n=== Random Trials (20 vectors) ===\n");
    size_t total_baseline = 0;
    size_t total_ultimate = 0;
    int successes = 0;

    for (int i = 0; i < 20; i++) {
        generate_s_mangled_97nz(vector, 1000 + i * 123);

        sparse_optimal_large_t *base = sparse_optimal_large_encode(vector, 2048);
        sparse_ultimate_t *ult = sparse_ultimate_encode(vector, 2048);

        if (base && ult) {
            memset(decoded, 0, 2048);
            int ok = (sparse_ultimate_decode(ult, decoded, 2048) == 0 &&
                     verify_decode(vector, decoded));

            printf("  Trial %2d: Base=%3zu B, Ultimate=%3zu B (-%d B) %s\n",
                   i + 1, base->size, ult->size,
                   (int)base->size - (int)ult->size,
                   ok ? "✓" : "✗");

            total_baseline += base->size;
            total_ultimate += ult->size;
            if (ok) successes++;
        }

        sparse_optimal_large_free(base);
        sparse_ultimate_free(ult);
    }

    printf("\nAverage:\n");
    printf("  Baseline: %.1f bytes\n", (float)total_baseline / 20);
    printf("  Ultimate: %.1f bytes\n", (float)total_ultimate / 20);
    printf("  Savings: %.1f bytes (%.1f%% reduction)\n",
           (float)(total_baseline - total_ultimate) / 20,
           100.0 * (total_baseline - total_ultimate) / (float)total_baseline);
    printf("  Success rate: %d/20\n", successes);

    printf("\n=========================================================\n");
    printf("SUMMARY\n");
    printf("=========================================================\n");
    if ((float)total_ultimate / 20 < 175) {
        printf("✓ Target achieved! Average < 175 bytes\n");
    } else {
        printf("⚠ Still optimizing, current average: %.1f bytes\n",
               (float)total_ultimate / 20);
    }
    printf("=========================================================\n");

    free(vector);
    free(decoded);

    return 0;
}
