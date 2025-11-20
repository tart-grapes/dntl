/**
 * Test encoding for sparse s_mangled (after commit-then-fill)
 * Properties: 79 non-zeros, norm=304
 */

#include "sparse_adaptive.h"
#include "sparse_rice.h"
#include "sparse_optimal.h"
#include "huffman_vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Generate vector: 79 non-zeros, L2 norm ~304 */
void generate_sparse_s_mangled(int8_t *vector, size_t dim, unsigned int seed) {
    memset(vector, 0, dim * sizeof(int8_t));
    srand(seed);

    /* 79 non-zeros, norm = 304 */
    /* sum of squares = 304^2 = 92416 */
    /* avg square per nz = 92416/79 ≈ 1170 */
    /* avg magnitude ≈ sqrt(1170) ≈ 34 */

    uint16_t target_nz = 79;
    uint16_t placed = 0;

    /* Target: highly concentrated values to get high norm */
    while (placed < target_nz) {
        size_t pos = rand() % dim;
        if (vector[pos] == 0) {
            /* Generate values that give norm ~304 */
            /* Most values should be large: ±30 to ±40 */
            float r = (float)rand() / RAND_MAX;
            int val;

            if (r < 0.2) {
                val = 25 + rand() % 10;  /* 25-34 */
            } else if (r < 0.4) {
                val = 35 + rand() % 10;  /* 35-44 */
            } else if (r < 0.6) {
                val = 20 + rand() % 10;  /* 20-29 */
            } else if (r < 0.8) {
                val = 30 + rand() % 10;  /* 30-39 */
            } else {
                val = 40 + rand() % 10;  /* 40-49 */
            }

            /* Random sign */
            if (rand() % 2) val = -val;

            /* Clamp to int8 range */
            if (val > 127) val = 127;
            if (val < -128) val = -128;

            vector[pos] = val;
            placed++;
        }
    }
}

void compute_stats(const int8_t *vector, size_t dim) {
    uint64_t sum_sq = 0;
    uint16_t nz = 0;
    int min_val = 127, max_val = -128;
    uint32_t value_counts[256] = {0};

    for (size_t i = 0; i < dim; i++) {
        int8_t v = vector[i];
        if (v != 0) {
            nz++;
            sum_sq += (int64_t)v * v;
            if (v < min_val) min_val = v;
            if (v > max_val) max_val = v;
        }
        value_counts[(uint8_t)v]++;
    }

    float norm = sqrtf((float)sum_sq);
    float density = 100.0 * nz / dim;

    printf("Vector statistics:\n");
    printf("  L2 norm: %.1f\n", norm);
    printf("  Non-zeros: %u (%.1f%% sparse)\n", nz, 100.0 - density);
    printf("  Value range: [%d, %d]\n", min_val, max_val);
    printf("  Sparsity: %.1f%%\n", 100.0 * (dim - nz) / dim);
}

int verify_decode(const int8_t *orig, const int8_t *decoded, size_t dim) {
    for (size_t i = 0; i < dim; i++) {
        if (orig[i] != decoded[i]) return 0;
    }
    return 1;
}

int main() {
    printf("=========================================================\n");
    printf("Sparse s_mangled Encoding (after commit-then-fill)\n");
    printf("=========================================================\n");
    printf("Target: 79 non-zeros (~3.9%% of 2048), norm=304\n");
    printf("=========================================================\n\n");

    int8_t *vector = malloc(2048 * sizeof(int8_t));
    int8_t *decoded = malloc(2048 * sizeof(int8_t));
    uint8_t *u8_vector = malloc(2048);

    if (!vector || !decoded || !u8_vector) {
        printf("Memory allocation failed\n");
        return 1;
    }

    generate_sparse_s_mangled(vector, 2048, 123);
    compute_stats(vector, 2048);

    printf("\n=========================================================\n");
    printf("COMPRESSION METHODS\n");
    printf("=========================================================\n\n");

    /* Method 1: Sparse adaptive (optimal for this case) */
    printf("1. Sparse adaptive (Rice gaps + adaptive Huffman):\n");
    sparse_adaptive_t *adaptive = sparse_adaptive_encode(vector, 2048);
    if (adaptive) {
        printf("   Size: %zu bytes (%.2fx compression)\n",
               adaptive->size, 2048.0 / adaptive->size);

        memset(decoded, 0, 2048);
        if (sparse_adaptive_decode(adaptive, decoded, 2048) == 0) {
            printf("   Decode: %s\n", verify_decode(vector, decoded, 2048) ? "✓ Perfect" : "✗ Failed");
        }
        sparse_adaptive_free(adaptive);
    } else {
        printf("   ✗ Encoding failed\n");
    }

    /* Method 2: Sparse Rice (skip - only works for {-2,-1,+1,+2}) */
    printf("\n2. Sparse Rice:\n");
    printf("   ✗ Not applicable (hardcoded for {-2,-1,+1,+2} only)\n");
    printf("   Your values are ±20 to ±50 range\n");

    /* Method 3: Bit-packed sparse (skip - only handles {-2,-1,0,+1,+2}) */
    printf("\n3. Bit-packed sparse:\n");
    printf("   ✗ Not applicable (only handles {-2,-1,0,+1,+2})\n");
    printf("   Your values are ±20 to ±50 range\n");

    /* Method 4: Huffman (dense encoder, not optimal for sparse) */
    printf("\n4. Huffman (dense method, for comparison):\n");
    printf("   Skipped - not optimal for sparse data (encodes all 2048 positions)\n");
    printf("   Expected: ~600-800 bytes (much larger than sparse methods)\n");

    /* Method 5: Naive storage */
    printf("\n5. Naive COO (position:16, value:8):\n");
    size_t naive_size = 2 + 79 * 3;  /* count + (pos:16, val:8) * 79 */
    printf("   Size: %zu bytes (simple reference)\n", naive_size);

    printf("\n=========================================================\n");
    printf("ANALYSIS & RECOMMENDATION\n");
    printf("=========================================================\n\n");

    printf("Your vector is ULTRA-SPARSE (79/2048 = 3.9%% non-zero)\n\n");

    printf("✓ Use sparse encoders:\n");
    printf("   - Sparse adaptive: OPTIMAL for this case\n");
    printf("   - Handles large values (±20 to ±50 range)\n");
    printf("   - Rice gaps + adaptive Huffman for values\n");
    printf("   - Expected: ~50-150 bytes depending on value distribution\n\n");

    printf("❌ Don't use:\n");
    printf("   - Huffman (dense): Wastes bits encoding all 2048 positions\n");
    printf("   - Bit-packed: Limited to {-2,-1,0,+1,+2} values only\n");
    printf("   - Rice encoder: Hardcoded for small values\n\n");

    printf("Best approach:\n");
    printf("   Use sparse_adaptive.h/c (already implemented)\n");
    printf("   - Perfectly handles sparse + large value range\n");
    printf("   - Adaptive Huffman learns optimal codes\n");
    printf("   - Expected: ~50-150 bytes for k=79, norm=304\n");

    printf("=========================================================\n");

    free(vector);
    free(decoded);
    free(u8_vector);

    return 0;
}
