/**
 * Analyze encoding options for sparse s_mangled (79 nz, norm=304)
 * Theoretical analysis without calling encoders
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

void generate_sparse_s_mangled(int8_t *vector, size_t dim, unsigned int seed) {
    memset(vector, 0, dim * sizeof(int8_t));
    srand(seed);

    uint16_t target_nz = 79;
    uint16_t placed = 0;

    while (placed < target_nz) {
        size_t pos = rand() % dim;
        if (vector[pos] == 0) {
            float r = (float)rand() / RAND_MAX;
            int val;

            if (r < 0.2) val = 25 + rand() % 10;
            else if (r < 0.4) val = 35 + rand() % 10;
            else if (r < 0.6) val = 20 + rand() % 10;
            else if (r < 0.8) val = 30 + rand() % 10;
            else val = 40 + rand() % 10;

            if (rand() % 2) val = -val;
            if (val > 127) val = 127;
            if (val < -128) val = -128;

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
    printf("Sparse s_mangled Analysis (79 nz, norm=304)\n");
    printf("=========================================================\n\n");

    int8_t *vector = malloc(2048 * sizeof(int8_t));
    if (!vector) return 1;

    generate_sparse_s_mangled(vector, 2048, 123);

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
    printf("  Non-zeros: %u (sparsity: %.1f%%)\n", nz, sparsity);
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
    printf("\n");

    /* Calculate theoretical encoding sizes */
    printf("=========================================================\n");
    printf("ENCODING SIZE ESTIMATES\n");
    printf("=========================================================\n\n");

    /* Method 1: Naive COO (pos:16, val:8) */
    size_t naive_coo = 2 + nz * 3;  /* count:16 + (pos:16, val:8) * nz */
    printf("1. Naive COO (16-bit pos, 8-bit value):\n");
    printf("   Size: %zu bytes\n", naive_coo);
    printf("   Format: [count:16] [pos:16, val:8]×%u\n", nz);
    printf("\n");

    /* Method 2: Compressed positions (binary entropy) */
    float p = (float)nz / 2048.0;
    float pos_entropy = binary_entropy(p);
    float pos_bits = 2048 * pos_entropy;
    float val_bits = nz * entropy;
    size_t optimal_sparse = (size_t)((16 + pos_bits + val_bits + 7) / 8);

    printf("2. Optimal sparse (entropy-coded positions + Huffman values):\n");
    printf("   Position encoding: %.1f bits (binary entropy)\n", pos_bits);
    printf("   Value encoding: %.1f bits (Huffman)\n", val_bits);
    printf("   Total: ~%zu bytes\n", optimal_sparse);
    printf("\n");

    /* Method 3: Rice-coded gaps */
    float avg_gap = 2048.0 / nz;
    uint8_t r = (uint8_t)log2(avg_gap);
    float rice_bits_per_gap = r + 1 + 1.0/avg_gap;
    float gap_bits = nz * rice_bits_per_gap;
    size_t rice_sparse = (size_t)((16 + 3 + 11 + gap_bits + val_bits + 7) / 8);

    printf("3. Rice gaps + Huffman values:\n");
    printf("   Average gap: %.1f\n", avg_gap);
    printf("   Rice parameter: %u\n", r);
    printf("   Gap bits: %.1f (%.1f bits/gap)\n", gap_bits, rice_bits_per_gap);
    printf("   Value bits: %.1f\n", val_bits);
    printf("   Total: ~%zu bytes\n", rice_sparse);
    printf("\n");

    /* Method 4: Bit-vector positions */
    size_t bitvec_size = (size_t)((2048 + val_bits + 7) / 8);
    printf("4. Bit-vector positions + Huffman values:\n");
    printf("   Position bits: 2048 (one bit per position)\n");
    printf("   Value bits: %.1f\n", val_bits);
    printf("   Total: ~%zu bytes\n", bitvec_size);
    printf("\n");

    printf("=========================================================\n");
    printf("RECOMMENDATION\n");
    printf("=========================================================\n\n");

    printf("For k=%u, norm=%.0f, range [%d, %d]:\n\n", nz, norm, min_val, max_val);

    size_t best = rice_sparse;
    if (optimal_sparse < best) best = optimal_sparse;
    if (naive_coo < best) best = naive_coo;

    printf("BEST: ");
    if (best == rice_sparse) {
        printf("Rice gaps + Huffman values (~%zu bytes)\n", rice_sparse);
    } else if (best == optimal_sparse) {
        printf("Optimal entropy coding (~%zu bytes)\n", optimal_sparse);
    } else {
        printf("Naive COO (~%zu bytes)\n", naive_coo);
    }

    printf("\n");
    printf("Implementation options:\n");
    printf("  1. Use sparse_adaptive.h/c (if it handles large value ranges)\n");
    printf("  2. Implement simple COO with variable-length encoding:\n");
    printf("     - Rice/Elias-gamma for positions\n");
    printf("     - Variable-byte or Huffman for values\n");
    printf("  3. Naive COO: %zu bytes (simple, no compression)\n\n", naive_coo);

    printf("Expected final size: %zu-%zu bytes\n", best, naive_coo);
    printf("Compression: %.1fx vs naive 2048-byte storage\n", 2048.0 / best);

    printf("=========================================================\n");

    free(vector);
    return 0;
}
