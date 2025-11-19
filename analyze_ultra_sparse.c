/**
 * Analysis for ULTRA-SPARSE vectors (user's actual data)
 *
 * Real-world data from user:
 * - L2 norm: ~5.13 average (range 3.6 - 6.1)
 * - Non-zeros: ~27 average (1.3% sparsity!)
 * - Dimension: 2048
 * - Values: Likely {-2,-1,0,1,2} based on DNTL scheme
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

/* Estimate position encoding bits */
float position_bits(uint32_t k, uint32_t dim) {
    float p = (float)k / dim;
    return dim * binary_entropy(p);
}

void analyze_sparse_encoding(uint32_t dim, uint32_t k_avg, uint32_t k_min, uint32_t k_max) {
    printf("=================================================================\n");
    printf("OPTIMAL ENCODING FOR ULTRA-SPARSE VECTORS\n");
    printf("=================================================================\n\n");

    printf("Your actual data:\n");
    printf("  Dimension: %u\n", dim);
    printf("  Avg non-zeros: %u (%.1f%% sparsity)\n", k_avg, 100.0 * (1.0 - (float)k_avg/dim));
    printf("  Range: %u - %u non-zeros\n", k_min, k_max);
    printf("  L2 norm: ~5.13 average\n\n");

    printf("=================================================================\n");
    printf("METHOD 1: COO (Coordinate List) Format\n");
    printf("=================================================================\n\n");

    printf("Format: [count:2] [index:2, value:1]*\n");
    printf("  - 2 bytes for count\n");
    printf("  - 2 bytes per index (for dim=2048, need 11 bits, use 16)\n");
    printf("  - 1 byte per value (for {-2,-1,0,1,2})\n\n");

    uint32_t coo_avg = 2 + k_avg * 3;
    uint32_t coo_min = 2 + k_min * 3;
    uint32_t coo_max = 2 + k_max * 3;

    printf("Estimated sizes:\n");
    printf("  Average (k=%u): %u bytes\n", k_avg, coo_avg);
    printf("  Best case (k=%u): %u bytes\n", k_min, coo_min);
    printf("  Worst case (k=%u): %u bytes\n", k_max, coo_max);
    printf("  Compression: %.1fx vs dense (2048 bytes)\n\n", (float)dim/coo_avg);

    printf("=================================================================\n");
    printf("METHOD 2: Bit-Packed Format\n");
    printf("=================================================================\n\n");

    printf("Format: [count:2] [11-bit index, 3-bit value]*\n");
    printf("  - 2 bytes for count\n");
    printf("  - 11 bits per index (exactly fits 2048)\n");
    printf("  - 3 bits per value (5 values: -2,-1,0,1,2)\n");
    printf("  - Total: 14 bits per entry\n\n");

    uint32_t packed_avg = 2 + (k_avg * 14 + 7) / 8;
    uint32_t packed_min = 2 + (k_min * 14 + 7) / 8;
    uint32_t packed_max = 2 + (k_max * 14 + 7) / 8;

    printf("Estimated sizes:\n");
    printf("  Average (k=%u): %u bytes (%.2f bytes/entry)\n", k_avg, packed_avg, (float)packed_avg/k_avg);
    printf("  Best case (k=%u): %u bytes\n", k_min, packed_min);
    printf("  Worst case (k=%u): %u bytes\n", k_max, packed_max);
    printf("  Compression: %.1fx vs dense\n\n", (float)dim/packed_avg);

    printf("=================================================================\n");
    printf("METHOD 3: Arithmetic Coding (Near-Optimal)\n");
    printf("=================================================================\n\n");

    printf("Positions (binary entropy):\n");
    float pos_bits_avg = position_bits(k_avg, dim);
    float pos_bits_min = position_bits(k_min, dim);
    float pos_bits_max = position_bits(k_max, dim);

    printf("  p = k/dim = %.6f (average)\n", (float)k_avg/dim);
    printf("  Binary entropy H(p) = %.6f bits/position\n", binary_entropy((float)k_avg/dim));
    printf("  Total position bits: %.1f (%.1f bytes)\n", pos_bits_avg, pos_bits_avg/8);

    printf("\nValues (assuming uniform {-2,-1,0,1,2}):\n");
    float val_bits_avg = k_avg * log2(5);  /* log2(5) ≈ 2.32 bits */
    printf("  log2(5) = %.2f bits/value\n", log2(5));
    printf("  Total value bits: %.1f (%.1f bytes)\n", val_bits_avg, val_bits_avg/8);

    uint32_t arith_avg = (pos_bits_avg + val_bits_avg + 16 + 7) / 8;  /* +16 for header */
    uint32_t arith_min = (pos_bits_min + k_min * log2(5) + 16 + 7) / 8;
    uint32_t arith_max = (pos_bits_max + k_max * log2(5) + 16 + 7) / 8;

    printf("\nTotal (position + values + header):\n");
    printf("  Average (k=%u): %u bytes\n", k_avg, arith_avg);
    printf("  Best case (k=%u): %u bytes\n", k_min, arith_min);
    printf("  Worst case (k=%u): %u bytes\n", k_max, arith_max);
    printf("  Compression: %.1fx vs dense\n\n", (float)dim/arith_avg);

    printf("=================================================================\n");
    printf("METHOD 4: Hybrid (Positions + Huffman Values)\n");
    printf("=================================================================\n\n");

    /* Assume skewed distribution: more ±1 than ±2 */
    printf("Assuming value distribution (typical for crypto):\n");
    printf("  ±1: 60%% (entropy ≈ 1 bit)\n");
    printf("  ±2: 40%% (entropy ≈ 1 bit)\n");
    printf("  Combined entropy: ~2 bits/value\n\n");

    float hybrid_val_bits = k_avg * 2.0;  /* Estimated Huffman */
    uint32_t hybrid_avg = (pos_bits_avg + hybrid_val_bits + 16 + 7) / 8;

    printf("  Position bits: %.1f\n", pos_bits_avg);
    printf("  Value bits (Huffman): %.1f\n", hybrid_val_bits);
    printf("  Total: %u bytes\n", hybrid_avg);
    printf("  Compression: %.1fx vs dense\n\n", (float)dim/hybrid_avg);

    printf("=================================================================\n");
    printf("RECOMMENDATION\n");
    printf("=================================================================\n\n");

    printf("For YOUR ultra-sparse data (k~%u):\n\n", k_avg);

    printf("1. SIMPLEST: COO Format → ~%u bytes\n", coo_avg);
    printf("   - Easy to implement\n");
    printf("   - Good compression (%.1fx)\n", (float)dim/coo_avg);
    printf("   - Fast encode/decode\n\n");

    printf("2. BETTER: Bit-Packed → ~%u bytes\n", packed_avg);
    printf("   - 20%% smaller than COO\n");
    printf("   - Still simple\n");
    printf("   - %.1fx compression\n\n", (float)dim/packed_avg);

    printf("3. BEST: Arithmetic Coding → ~%u bytes\n", arith_avg);
    printf("   - Near information-theoretic optimum\n");
    printf("   - %.1fx compression\n", (float)dim/arith_avg);
    printf("   - Complex implementation\n\n");

    printf("For 256-byte target: ALL METHODS EASILY MEET IT!\n");
    printf("  - COO: %u bytes (%.0f bytes under budget)\n", coo_avg, 256.0 - coo_avg);
    printf("  - Packed: %u bytes (%.0f bytes under budget)\n", packed_avg, 256.0 - packed_avg);
    printf("  - Arithmetic: %u bytes (%.0f bytes under budget)\n\n", arith_avg, 256.0 - arith_avg);

    printf("RECOMMENDED: Use bit-packed format (Method 2)\n");
    printf("  - Simple to implement (~100 lines of code)\n");
    printf("  - ~%u bytes average (well under 256-byte target)\n", packed_avg);
    printf("  - Lossless, fast, no dependencies\n");
}

void compare_with_compression() {
    printf("\n=================================================================\n");
    printf("COMPARISON: Sparse Encoding vs General Compression\n");
    printf("=================================================================\n\n");

    uint32_t dim = 2048;
    uint32_t k = 27;

    /* Sparse encoding */
    uint32_t sparse_packed = 2 + (k * 14 + 7) / 8;

    /* Dense representation + Brotli */
    /* Ultra-sparse data compresses very well with Brotli */
    uint32_t dense_brotli_est = 150;  /* Conservative estimate */

    printf("Method                          | Size    | Notes\n");
    printf("--------------------------------|---------|---------------------------\n");
    printf("Dense int8                      | 2048 B  | Baseline\n");
    printf("Dense + Brotli level 11         | ~150 B  | General compression\n");
    printf("**Sparse bit-packed**           | **%2u B** | **Optimal for sparse**\n", sparse_packed);
    printf("Sparse COO                      |  %3u B  | Simple, good\n", 2 + k*3);
    printf("Sparse arithmetic               |  ~%2u B  | Near-optimal\n",
           (uint32_t)((position_bits(k, dim) + k * log2(5) + 16 + 7) / 8));

    printf("\nKEY INSIGHT:\n");
    printf("  For ultra-sparse data (1.3%% non-zero), sparse encoding\n");
    printf("  is MUCH simpler and nearly as good as general compression.\n");
    printf("  No need for complex Brotli/zstd - just encode the positions!\n");
}

int main() {
    printf("=========================================================\n");
    printf("Ultra-Sparse Vector Encoding Analysis\n");
    printf("=========================================================\n");
    printf("Based on YOUR actual data from DNTL scheme\n");
    printf("=========================================================\n\n");

    uint32_t dim = 2048;
    uint32_t k_avg = 27;   /* Average from your stats */
    uint32_t k_min = 13;   /* Min from your stats */
    uint32_t k_max = 37;   /* Max from your stats */

    analyze_sparse_encoding(dim, k_avg, k_min, k_max);
    compare_with_compression();

    printf("\n=========================================================\n");
    printf("FINAL RECOMMENDATION\n");
    printf("=========================================================\n\n");

    printf("For your ultra-sparse vectors (L2~5, k~27):\n\n");

    printf("USE: Bit-packed COO format\n");
    printf("  Format: [count:2] [11-bit index, 3-bit value]*\n");
    printf("  Size: ~50 bytes average\n");
    printf("  Compression: ~41x vs dense\n");
    printf("  Implementation: ~100 lines of C code\n\n");

    printf("This is:\n");
    printf("  ✓ Simple to implement\n");
    printf("  ✓ Fast (microseconds)\n");
    printf("  ✓ Lossless\n");
    printf("  ✓ No dependencies\n");
    printf("  ✓ WAY under 256-byte target\n\n");

    printf("I can implement this for you if you'd like!\n");

    printf("=========================================================\n");

    return 0;
}
