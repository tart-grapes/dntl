/**
 * Analyze encoding for s_mangled: 97 nz, norm=297, range [-43,42]
 * Currently: 7 bits/coeff = 1792 bytes
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

void generate_s_mangled_97nz(int8_t *vector, size_t dim, unsigned int seed) {
    memset(vector, 0, dim * sizeof(int8_t));
    srand(seed);

    /* Target: 97 non-zeros, L2 norm ~297, range [-43, 42] */
    /* sum of squares = 297^2 = 88209 */
    /* avg square per nz = 88209/97 ≈ 909 */
    /* avg magnitude ≈ sqrt(909) ≈ 30 */

    uint16_t target_nz = 97;
    uint16_t placed = 0;

    while (placed < target_nz) {
        size_t pos = rand() % dim;
        if (vector[pos] == 0) {
            /* Generate values that give norm ~297 */
            float r = (float)rand() / RAND_MAX;
            int val;

            if (r < 0.25) val = 20 + rand() % 15;      /* 20-34 */
            else if (r < 0.50) val = 25 + rand() % 15;  /* 25-39 */
            else if (r < 0.75) val = 30 + rand() % 13;  /* 30-42 */
            else val = 15 + rand() % 15;                /* 15-29 */

            if (rand() % 2) val = -val;
            if (val > 42) val = 42;
            if (val < -43) val = -43;

            vector[pos] = val;
            placed++;
        }
    }
}

float binary_entropy(float p) {
    if (p <= 0.0 || p >= 1.0) return 0.0;
    return -p * log2(p) - (1-p) * log2(1-p);
}

int main() {
    printf("=========================================================\n");
    printf("s_mangled Encoding Analysis\n");
    printf("=========================================================\n");
    printf("Current: 7 bits/coeff = 1792 bytes\n");
    printf("Target: 97 nz, norm=297, range [-43, 42]\n");
    printf("=========================================================\n\n");

    int8_t *vector = malloc(2048 * sizeof(int8_t));
    if (!vector) return 1;

    generate_s_mangled_97nz(vector, 2048, 456);

    /* Compute statistics */
    uint64_t sum_sq = 0;
    uint16_t nz = 0;
    int min_val = 127, max_val = -128;
    uint32_t value_counts[256] = {0};
    uint32_t unique_values = 0;

    for (size_t i = 0; i < 2048; i++) {
        int8_t v = vector[i];
        if (v != 0) nz++;
        sum_sq += (int64_t)v * v;
        if (v < min_val) min_val = v;
        if (v > max_val) max_val = v;
        if (value_counts[(uint8_t)v] == 0 && v != 0) unique_values++;
        value_counts[(uint8_t)v]++;
    }

    float norm = sqrtf((float)sum_sq);
    float sparsity = 100.0 * (2048 - nz) / 2048.0;

    printf("Vector Statistics:\n");
    printf("  L2 norm: %.1f\n", norm);
    printf("  Non-zeros: %u (%.1f%% sparse)\n", nz, sparsity);
    printf("  Value range: [%d, %d]\n", min_val, max_val);
    printf("  Unique non-zero values: %u\n", unique_values);

    /* Calculate value entropy */
    float entropy = 0.0;
    for (int v = min_val; v <= max_val; v++) {
        if (value_counts[(uint8_t)v] > 0 && v != 0) {
            float p = (float)value_counts[(uint8_t)v] / nz;
            entropy -= p * log2(p);
        }
    }

    printf("  Value entropy: %.2f bits/value\n", entropy);

    /* Check bits needed for value range */
    int value_range = max_val - min_val + 1;
    int bits_per_value = (int)ceil(log2(value_range));
    printf("  Fixed bits/value: %d (for range %d)\n", bits_per_value, value_range);
    printf("\n");

    /* Calculate theoretical encoding sizes */
    printf("=========================================================\n");
    printf("ENCODING SIZE COMPARISON\n");
    printf("=========================================================\n\n");

    /* Current method */
    size_t current_size = 1792;
    printf("0. CURRENT (7 bits/coeff for all 2048):\n");
    printf("   Size: %zu bytes\n", current_size);
    printf("   Wastes bits on 1951 zeros!\n");
    printf("\n");

    /* Method 1: Naive COO */
    size_t naive_coo = 2 + nz * 3;
    printf("1. Naive COO (pos:16, val:8):\n");
    printf("   Size: %zu bytes\n", naive_coo);
    printf("   Savings: %zu bytes (%.1fx better)\n",
           current_size - naive_coo, (float)current_size / naive_coo);
    printf("   Implementation: TRIVIAL (~20 lines)\n");
    printf("\n");

    /* Method 2: Compressed COO (Rice gaps + fixed-bit values) */
    float avg_gap = 2048.0 / nz;
    uint8_t r = (uint8_t)log2(avg_gap);
    float rice_bits_per_gap = r + 1 + 1.0/avg_gap;
    float gap_bits = nz * rice_bits_per_gap;
    float val_bits_fixed = nz * bits_per_value;
    size_t rice_fixed = (size_t)((16 + 3 + 11 + gap_bits + val_bits_fixed + 7) / 8);

    printf("2. Rice gaps + fixed-bit values:\n");
    printf("   Gap bits: %.1f (Rice r=%u)\n", gap_bits, r);
    printf("   Value bits: %.1f (%d bits each)\n", val_bits_fixed, bits_per_value);
    printf("   Size: %zu bytes\n", rice_fixed);
    printf("   Savings: %zu bytes (%.1fx better)\n",
           current_size - rice_fixed, (float)current_size / rice_fixed);
    printf("   Implementation: EASY (~100 lines)\n");
    printf("\n");

    /* Method 3: Rice gaps + Huffman values */
    float val_bits_huffman = nz * entropy;
    size_t rice_huffman = (size_t)((16 + 3 + 11 + gap_bits + val_bits_huffman + 32 + 7) / 8);

    printf("3. Rice gaps + Huffman values:\n");
    printf("   Gap bits: %.1f\n", gap_bits);
    printf("   Value bits: %.1f (Huffman)\n", val_bits_huffman);
    printf("   Size: %zu bytes\n", rice_huffman);
    printf("   Savings: %zu bytes (%.1fx better)\n",
           current_size - rice_huffman, (float)current_size / rice_huffman);
    printf("   Implementation: MEDIUM (sparse_adaptive.c needs fix)\n");
    printf("\n");

    /* Method 4: Bit-vector positions */
    size_t bitvec_size = (size_t)((2048 + val_bits_huffman + 32 + 7) / 8);
    printf("4. Bit-vector + Huffman values:\n");
    printf("   Size: %zu bytes\n", bitvec_size);
    printf("   Savings: %zu bytes (%.1fx better)\n",
           current_size - bitvec_size, (float)current_size / bitvec_size);
    printf("   Not optimal for sparse\n");
    printf("\n");

    printf("=========================================================\n");
    printf("RECOMMENDATION\n");
    printf("=========================================================\n\n");

    printf("Current encoding (1792 bytes) is VERY wasteful!\n");
    printf("  - Uses 7 bits/coeff for ALL 2048 coefficients\n");
    printf("  - Wastes 1951 × 7 bits = 13657 bits on zeros!\n\n");

    printf("✓ BEST: Naive COO = %zu bytes (%.1fx improvement)\n",
           naive_coo, (float)current_size / naive_coo);
    printf("  - Format: [count:16] [pos:16, val:8]×%u\n", nz);
    printf("  - TRIVIAL to implement (~20 lines of code)\n");
    printf("  - Saves %zu bytes immediately\n\n", current_size - naive_coo);

    printf("✓ BETTER: Rice + fixed bits = %zu bytes (%.1fx improvement)\n",
           rice_fixed, (float)current_size / rice_fixed);
    printf("  - Compress position gaps with Rice coding\n");
    printf("  - Use %d bits per value (optimal for range)\n", bits_per_value);
    printf("  - Easy to implement (~100 lines)\n");
    printf("  - Saves %zu bytes\n\n", current_size - rice_fixed);

    printf("✓ OPTIMAL: Rice + Huffman = %zu bytes (%.1fx improvement)\n",
           rice_huffman, (float)current_size / rice_huffman);
    printf("  - Best compression for this data\n");
    printf("  - Requires fixing sparse_adaptive.c (alphabet size limit)\n");
    printf("  - Saves %zu bytes\n\n", current_size - rice_huffman);

    printf("=========================================================\n");
    printf("QUICK WIN: Implement Naive COO\n");
    printf("=========================================================\n\n");

    printf("Pseudo-code:\n");
    printf("  // Encode\n");
    printf("  write_uint16(count);\n");
    printf("  for each non-zero at position i with value v:\n");
    printf("    write_uint16(i);  // position\n");
    printf("    write_int8(v);    // value\n");
    printf("\n");
    printf("  // Decode\n");
    printf("  count = read_uint16();\n");
    printf("  for i in 0..count:\n");
    printf("    pos = read_uint16();\n");
    printf("    val = read_int8();\n");
    printf("    vector[pos] = val;\n");
    printf("\n");
    printf("Saves %zu bytes with ~20 lines of code!\n", current_size - naive_coo);

    printf("=========================================================\n");

    free(vector);
    return 0;
}
