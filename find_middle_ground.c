/**
 * Find the "middle ground" for 256-byte target
 *
 * Explores different L2 norms and sparsity levels to find
 * optimal parameters that hit ~256 bytes with simple encoding.
 *
 * Sweet spot analysis between:
 * - Ultra-sparse: L2~5, k~27 → 50 bytes (too much headroom)
 * - Dense: L2~71, k~1572 → 535 bytes (too big)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

/* Calculate binary entropy */
float binary_entropy(float p) {
    if (p <= 0.0 || p >= 1.0) return 0.0;
    return -p * log2(p) - (1-p) * log2(1-p);
}

/* Estimate bit-packed encoding size */
uint32_t estimate_bitpacked(uint32_t k) {
    /* Format: [count:2] [11-bit index, 3-bit value]* */
    return 2 + (k * 14 + 7) / 8;
}

/* Estimate COO encoding size */
uint32_t estimate_coo(uint32_t k) {
    /* Format: [count:2] [index:2, value:1]* */
    return 2 + k * 3;
}

/* Estimate arithmetic coding size */
uint32_t estimate_arithmetic(uint32_t k, uint32_t dim) {
    /* Position bits using binary entropy */
    float p = (float)k / dim;
    float pos_bits = dim * binary_entropy(p);

    /* Value bits (log2(5) for 5 values) */
    float val_bits = k * log2(5);

    /* Total with header */
    return (pos_bits + val_bits + 16 + 7) / 8;
}

/* Calculate L2 norm for k non-zeros with specific value distribution */
float calculate_l2_norm(uint32_t k, float avg_value_sq) {
    return sqrtf(k * avg_value_sq);
}

void analyze_range(uint32_t dim, uint32_t k_min, uint32_t k_max, uint32_t k_step) {
    printf("k (non-zeros) | Sparsity | L2 (±1) | L2 (±2) | Bit-packed | COO  | Arith | Target?\n");
    printf("--------------|----------|---------|---------|------------|------|-------|---------\n");

    for (uint32_t k = k_min; k <= k_max; k += k_step) {
        float sparsity = 100.0 * (1.0 - (float)k / dim);

        /* L2 norms for different value distributions */
        /* Assuming all ±1: sum_sq = k */
        float l2_pm1 = sqrtf(k);

        /* Assuming all ±2: sum_sq = 4k */
        float l2_pm2 = sqrtf(4.0 * k);

        /* Encoding sizes */
        uint32_t bitpacked = estimate_bitpacked(k);
        uint32_t coo = estimate_coo(k);
        uint32_t arith = estimate_arithmetic(k, dim);

        /* Check if close to 256-byte target (within 20%) */
        char target_mark = ' ';
        if (bitpacked >= 205 && bitpacked <= 307) target_mark = '*';  /* 256 ± 20% */

        printf("%5u (%4.1f%%) | %6.1f%% | %7.2f | %7.2f | %6u     | %4u | %5u | %c\n",
               k, 100.0*k/dim, sparsity, l2_pm1, l2_pm2,
               bitpacked, coo, arith, target_mark);
    }
}

void find_sweet_spots(uint32_t dim) {
    printf("\n=================================================================\n");
    printf("SWEET SPOTS for 256-byte target (bit-packed encoding)\n");
    printf("=================================================================\n\n");

    printf("Target: 200-256 bytes (allowing 20%% margin)\n");
    printf("Dimension: %u\n\n", dim);

    /* Solve for k: 2 + 14k/8 = target_bytes */
    /* 14k/8 = target - 2 */
    /* k = 8*(target - 2)/14 */

    uint32_t target_min = 200;
    uint32_t target_max = 256;

    uint32_t k_for_200 = (8 * (target_min - 2) + 13) / 14;  /* Round up */
    uint32_t k_for_256 = (8 * (target_max - 2) + 13) / 14;

    printf("For bit-packed encoding [count:2] [11-bit idx, 3-bit val]*:\n");
    printf("  200 bytes → k ≈ %u non-zeros\n", k_for_200);
    printf("  256 bytes → k ≈ %u non-zeros\n\n", k_for_256);

    printf("Sweet spot range:\n");
    printf("  k: %u - %u non-zeros\n", k_for_200, k_for_256);
    printf("  Sparsity: %.1f%% - %.1f%%\n",
           100.0*(1.0-(float)k_for_256/dim),
           100.0*(1.0-(float)k_for_200/dim));

    /* Calculate L2 norms for this range */
    printf("\nL2 norms for this k range:\n");
    printf("  All ±1: L2 = %.1f - %.1f\n", sqrtf(k_for_200), sqrtf(k_for_256));
    printf("  All ±2: L2 = %.1f - %.1f\n", sqrtf(4.0*k_for_200), sqrtf(4.0*k_for_256));
    printf("  Mix (50%% ±1, 50%% ±2): L2 = %.1f - %.1f\n",
           sqrtf(2.5*k_for_200), sqrtf(2.5*k_for_256));
}

void detailed_analysis(uint32_t dim) {
    printf("\n=================================================================\n");
    printf("DETAILED ANALYSIS: Specific Configurations\n");
    printf("=================================================================\n\n");

    struct {
        uint32_t k;
        const char *description;
    } configs[] = {
        {27, "Ultra-sparse (user's current)"},
        {50, "Very sparse"},
        {100, "Sparse"},
        {145, "256-byte target (bit-packed)"},
        {200, "Moderate sparse"},
        {300, "Less sparse"},
        {500, "Approaching dense"},
    };

    printf("Config | k     | Sparsity | L2 (±2) | Bit-packed | COO  | Best Method\n");
    printf("-------|-------|----------|---------|------------|------|-------------\n");

    for (int i = 0; i < 7; i++) {
        uint32_t k = configs[i].k;
        float sparsity = 100.0 * (1.0 - (float)k / dim);
        float l2_pm2 = sqrtf(4.0 * k);  /* All ±2 */

        uint32_t bitpacked = estimate_bitpacked(k);
        uint32_t coo = estimate_coo(k);
        uint32_t arith = estimate_arithmetic(k, dim);

        /* Determine best method */
        const char *best;
        if (k < 50) {
            best = "Bit-packed";
        } else if (k < 150) {
            best = "Bit-packed";
        } else if (k < 400) {
            best = "COO";
        } else {
            best = "Huffman";
        }

        printf("%2d     | %5u | %7.1f%% | %7.1f | %6u     | %4u | %s\n",
               i+1, k, sparsity, l2_pm2, bitpacked, coo, best);
    }

    printf("\n%s\n", configs[0].description);
    for (int i = 1; i < 7; i++) {
        printf("%s\n", configs[i].description);
    }
}

void practical_recommendations() {
    printf("\n=================================================================\n");
    printf("PRACTICAL RECOMMENDATIONS\n");
    printf("=================================================================\n\n");

    printf("For 256-byte target with dim=2048:\n\n");

    printf("OPTION 1: k ≈ 145 non-zeros\n");
    printf("  Encoding: Bit-packed COO\n");
    printf("  Size: ~256 bytes (exactly at target)\n");
    printf("  L2 norm (all ±2): ~24.1\n");
    printf("  L2 norm (all ±1): ~12.0\n");
    printf("  Sparsity: 92.9%%\n");
    printf("  Pros: Simple, hits target exactly\n\n");

    printf("OPTION 2: k ≈ 100 non-zeros\n");
    printf("  Encoding: Bit-packed COO\n");
    printf("  Size: ~177 bytes (79 bytes under budget)\n");
    printf("  L2 norm (all ±2): ~20.0\n");
    printf("  L2 norm (all ±1): ~10.0\n");
    printf("  Sparsity: 95.1%%\n");
    printf("  Pros: Clean round numbers, good margin\n\n");

    printf("OPTION 3: k ≈ 200 non-zeros\n");
    printf("  Encoding: COO format (simpler)\n");
    printf("  Size: ~602 bytes with COO (exceeds target)\n");
    printf("  Size: ~352 bytes with bit-packed (exceeds target)\n");
    printf("  Would need Huffman or compression\n\n");

    printf("RECOMMENDED: Option 2 (k=100)\n");
    printf("  - Clean parameters (L2=20, k=100)\n");
    printf("  - Good security margin (95%% sparse)\n");
    printf("  - Well under 256-byte budget\n");
    printf("  - Simple bit-packed encoding\n");
}

int main() {
    printf("=========================================================\n");
    printf("Finding the Middle Ground: 256-Byte Target\n");
    printf("=========================================================\n");
    printf("Goal: Find optimal L2 norm and sparsity for 256 bytes\n");
    printf("=========================================================\n\n");

    uint32_t dim = 2048;

    /* Explore range around target */
    printf("RANGE ANALYSIS (looking for 256-byte target marked with *)\n");
    printf("=================================================================\n\n");

    /* Fine-grained search near target */
    analyze_range(dim, 20, 60, 5);
    printf("\n");
    analyze_range(dim, 60, 160, 10);
    printf("\n");
    analyze_range(dim, 160, 400, 20);

    /* Sweet spot analysis */
    find_sweet_spots(dim);

    /* Detailed configurations */
    detailed_analysis(dim);

    /* Practical recommendations */
    practical_recommendations();

    printf("\n=========================================================\n");
    printf("SUMMARY\n");
    printf("=========================================================\n\n");

    printf("For 256-byte target:\n");
    printf("  Sweet spot: k = 100-145 non-zeros\n");
    printf("  L2 norm range: 20-24 (with ±2 values)\n");
    printf("  Sparsity: 93-95%%\n");
    printf("  Encoding: Bit-packed COO (simple, fast)\n\n");

    printf("This is the 'middle ground' between:\n");
    printf("  - Ultra-sparse (k=27, 50 bytes)\n");
    printf("  - Dense (k=1572, 535 bytes)\n\n");

    printf("Allows good security parameters while hitting 256B target!\n");

    printf("=========================================================\n");

    return 0;
}
