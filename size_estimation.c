/**
 * Size estimation for sparse vector encoding
 *
 * Parameters:
 * - Dimension: 2048
 * - L2 norm: 45 (sum of squares = 2025)
 * - Value range: {-2, -1, 0, 1, 2}
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

/* Calculate theoretical minimum size for different sparsity patterns */

void estimate_coo_size(uint16_t k) {
    /* COO Format: [count:2] [index:2, value:1]* */
    size_t size = 2 + k * 3;
    printf("  COO (k=%4u): %5zu bytes\n", k, size);
}

void estimate_packed_size(uint16_t k) {
    /* Packed Format: [count:2] + bit-packed (11-bit index + 3-bit value) */
    /* 14 bits per entry = 1.75 bytes per entry */
    size_t bits = 16 + k * 14;  /* 16-bit count + 14 bits per entry */
    size_t size = (bits + 7) / 8;  /* Round up to bytes */
    printf("  Packed (k=%4u): %5zu bytes (%.2f bytes/entry)\n", k, size, (float)size/k);
}

void estimate_rle_size(uint16_t k, float clustering) {
    /* RLE Format: [count:2] [first:2] + variable delta encoding */
    /* Assuming average delta < 255 (1 byte) with given clustering */
    size_t size = 4 + k * 2;  /* Conservative: 2 bytes per entry (1 delta + 1 value) */

    if (clustering > 0.1) {
        /* Sparse distribution - larger deltas */
        size += k * 0.5;  /* ~50% need 2-byte deltas */
    }

    printf("  RLE (k=%4u, cluster=%.2f): %5zu bytes\n", k, clustering, size);
}

void analyze_distribution(uint16_t k, int8_t avg_val_abs) {
    printf("\nDistribution Analysis (k=%u non-zeros):\n", k);

    float sum_sq = 2025.0;  /* L2 norm squared */

    /* Estimate sparsity */
    float sparsity = 1.0 - (float)k / 2048.0;
    printf("  Sparsity: %.2f%%\n", sparsity * 100);

    /* Estimate average value magnitude */
    float rms = sqrt(sum_sq / k);
    printf("  RMS value: %.2f\n", rms);

    /* Estimate index clustering */
    float avg_delta = 2048.0 / k;
    float clustering_ratio = avg_delta / 2048.0;
    printf("  Avg index delta: %.1f\n", avg_delta);
    printf("  Clustering ratio: %.3f\n", clustering_ratio);

    printf("\nEncoded Sizes:\n");
    estimate_coo_size(k);
    estimate_packed_size(k);
    estimate_rle_size(k, clustering_ratio);

    /* Compare to dense */
    size_t dense_int8 = 2048;
    size_t dense_float = 2048 * 4;
    size_t best_size = 2 + k * 14 / 8;  /* Packed format */

    printf("\nComparison:\n");
    printf("  Dense int8:  %zu bytes\n", dense_int8);
    printf("  Dense float: %zu bytes\n", dense_float);
    printf("  Best sparse: %zu bytes\n", best_size);
    printf("  Compression: %.1fx vs int8, %.1fx vs float\n",
           (float)dense_int8/best_size, (float)dense_float/best_size);
}

int main() {
    printf("===============================================\n");
    printf("Sparse Vector Encoding Size Estimation\n");
    printf("===============================================\n");
    printf("Parameters:\n");
    printf("  Dimension: 2048\n");
    printf("  L2 norm: 45 (sum of squares = 2025)\n");
    printf("  Value range: {-2, -1, 0, 1, 2}\n");
    printf("===============================================\n\n");

    /* Scenario 1: Very sparse (mostly ±1) */
    printf("SCENARIO 1: High sparsity (~500 non-zeros)\n");
    printf("Example: 400×(±1), 50×(±2)\n");
    printf("  Sum of squares: 400 + 200 = 600... Need more ±2\n");
    printf("  Better: 225×(±2), 225×(±1)\n");
    printf("  Sum of squares: 900 + 225 = 1125... Still need more\n");
    printf("  Actual: ~506 values of (±1), 0 values of (±2)\n");
    printf("  Sum of squares: 506*1 = 506 (too low)\n");
    printf("  With 506×(±1) + 253×(±2): 506 + 1012 = 1518\n");
    printf("  With 225×(±2) + 1125×(±1): 900 + 1125 = 2025 ✓\n");
    analyze_distribution(1350, 1);

    printf("\n===============================================\n\n");

    /* Scenario 2: Moderate sparsity (mix of values) */
    printf("SCENARIO 2: Moderate sparsity (~700 non-zeros)\n");
    printf("Example: ~506 values, mixed ±1 and ±2\n");
    printf("  Let: a = count of ±2, b = count of ±1\n");
    printf("  4a + b = 2025\n");
    printf("  a + b ≈ 506-700\n");
    printf("  Solution: a ≈ 506, b = 0 (all ±2)\n");
    printf("  Or: a = 253, b = 1013\n");
    analyze_distribution(700, 1);

    printf("\n===============================================\n\n");

    /* Scenario 3: Actual optimal (mathematical) */
    printf("SCENARIO 3: Mathematically optimal\n");
    printf("Solving: 4a + b = 2025 for minimum k = a + b\n");
    printf("  Maximize a (use ±2), minimize b (use ±1)\n");
    printf("  Max a = 506, then b = 1\n");
    printf("  Check: 4*506 + 1 = 2025 ✓\n");
    printf("  Optimal k = 507 non-zeros\n");
    analyze_distribution(507, 2);

    printf("\n===============================================\n\n");

    /* Scenario 4: Different distribution */
    printf("SCENARIO 4: Equal mix\n");
    printf("  Let a = b\n");
    printf("  4a + a = 2025\n");
    printf("  5a = 2025\n");
    printf("  a = 405\n");
    printf("  k = 810 non-zeros (405×(±2) + 405×(±1))\n");
    analyze_distribution(810, 1);

    printf("\n===============================================\n");
    printf("RECOMMENDATION\n");
    printf("===============================================\n");
    printf("\nFor L2=45, dim=2048, values∈{-2,-1,0,1,2}:\n\n");

    printf("Minimum k = 507 (506×(±2) + 1×(±1))\n");
    size_t min_size = 2 + (507 * 14 + 7) / 8;
    printf("  → Packed format: %zu bytes (%.1fx compression)\n\n",
           min_size, 2048.0/min_size);

    printf("Typical k ≈ 700-1000\n");
    size_t typ_size = 2 + (850 * 14 + 7) / 8;
    printf("  → Packed format: ~%zu bytes (%.1fx compression)\n\n",
           typ_size, 2048.0/typ_size);

    printf("COO format (simpler, 3 bytes/entry):\n");
    printf("  → k=507:  %zu bytes\n", 2 + 507*3);
    printf("  → k=850:  %zu bytes\n\n", 2 + 850*3);

    printf("Packed format (optimal, 1.75 bytes/entry):\n");
    printf("  → k=507:  %zu bytes ← BEST\n", min_size);
    printf("  → k=850:  %zu bytes\n\n", typ_size);

    printf("===============================================\n");

    return 0;
}
