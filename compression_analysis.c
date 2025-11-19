/**
 * Theoretical compression limits for k=145, 70% ±2 / 30% ±1
 *
 * Current bit-packed: 238 bytes
 * Can we do better?
 */

#include <stdio.h>
#include <math.h>
#include <stdint.h>

/* Binary entropy function */
float binary_entropy(float p) {
    if (p <= 0.0 || p >= 1.0) return 0.0;
    return -p * log2(p) - (1-p) * log2(1-p);
}

/* Calculate entropy for value distribution */
float value_entropy(float p_neg2, float p_neg1, float p_pos1, float p_pos2) {
    float entropy = 0.0;
    if (p_neg2 > 0) entropy -= p_neg2 * log2(p_neg2);
    if (p_neg1 > 0) entropy -= p_neg1 * log2(p_neg1);
    if (p_pos1 > 0) entropy -= p_pos1 * log2(p_pos1);
    if (p_pos2 > 0) entropy -= p_pos2 * log2(p_pos2);
    return entropy;
}

/* Log-binomial coefficient */
float log_binomial(uint32_t n, uint32_t k) {
    /* log2(C(n,k)) using Stirling approximation */
    if (k == 0 || k == n) return 0.0;

    /* More accurate for large n */
    float result = 0.0;
    for (uint32_t i = 0; i < k; i++) {
        result += log2((float)(n - i) / (i + 1));
    }
    return result;
}

int main() {
    printf("=========================================================\n");
    printf("Theoretical Compression Limits Analysis\n");
    printf("=========================================================\n\n");

    uint32_t dim = 2048;
    uint32_t k = 145;

    /* Value distribution: 70% ±2, 30% ±1 */
    float p_neg2 = 0.35;
    float p_pos2 = 0.35;
    float p_neg1 = 0.15;
    float p_pos1 = 0.15;

    printf("Configuration:\n");
    printf("  Dimension: %u\n", dim);
    printf("  Non-zeros (k): %u (%.1f%% density)\n", k, 100.0 * k / dim);
    printf("  Distribution: 70%% ±2, 30%% ±1\n\n");

    printf("=================================================================\n");
    printf("METHOD 1: Current Bit-Packed (Fixed-Width)\n");
    printf("=================================================================\n\n");

    uint32_t current_pos_bits = k * 11;
    uint32_t current_val_bits = k * 2;
    uint32_t current_header = 16;
    uint32_t current_total = current_header + current_pos_bits + current_val_bits;
    uint32_t current_bytes = (current_total + 7) / 8;

    printf("Format: [count:16] [pos:11, val:2]*\n");
    printf("  Header: %u bits\n", current_header);
    printf("  Positions: %u × 11 = %u bits\n", k, current_pos_bits);
    printf("  Values: %u × 2 = %u bits\n", k, current_val_bits);
    printf("  Total: %u bits = %u bytes\n\n", current_total, current_bytes);

    printf("=================================================================\n");
    printf("METHOD 2: Arithmetic Coding (Information-Theoretic Optimum)\n");
    printf("=================================================================\n\n");

    /* Position encoding using binary entropy */
    float p = (float)k / dim;
    float pos_entropy = binary_entropy(p);
    float pos_bits_opt = dim * pos_entropy;

    printf("Position encoding (binary entropy):\n");
    printf("  Density p = k/dim = %.6f\n", p);
    printf("  Binary entropy H(%.4f) = %.6f bits/position\n", p, pos_entropy);
    printf("  Total bits: %u × %.4f = %.1f bits (%.1f bytes)\n\n",
           dim, pos_entropy, pos_bits_opt, pos_bits_opt / 8);

    /* Value encoding using entropy */
    float val_entropy = value_entropy(p_neg2, p_neg1, p_pos1, p_pos2);
    float val_bits_opt = k * val_entropy;

    printf("Value encoding (entropy):\n");
    printf("  Distribution: -2:%.0f%%, -1:%.0f%%, +1:%.0f%%, +2:%.0f%%\n",
           p_neg2*100, p_neg1*100, p_pos1*100, p_pos2*100);
    printf("  Entropy H(X) = %.4f bits/value\n", val_entropy);
    printf("  Total bits: %u × %.4f = %.1f bits (%.1f bytes)\n\n",
           k, val_entropy, val_bits_opt, val_bits_opt / 8);

    uint32_t optimal_bits = (uint32_t)(pos_bits_opt + val_bits_opt + current_header + 0.5);
    uint32_t optimal_bytes = (optimal_bits + 7) / 8;

    printf("Total (arithmetic coding):\n");
    printf("  Theoretical minimum: %.1f + %.1f + 16 = %.1f bits\n",
           pos_bits_opt, val_bits_opt, pos_bits_opt + val_bits_opt + 16);
    printf("  Bytes: %u bytes\n\n", optimal_bytes);

    printf("=================================================================\n");
    printf("METHOD 3: Combinatorial Number System (Positions Only)\n");
    printf("=================================================================\n\n");

    /* Combinatorial encoding: choose k positions from dim */
    float comb_bits = log_binomial(dim, k);

    printf("Positions (combinatorial):\n");
    printf("  Need to encode C(%u, %u) combinations\n", dim, k);
    printf("  Bits needed: log2(C(%u, %u)) ≈ %.1f bits (%.1f bytes)\n",
           dim, k, comb_bits, comb_bits / 8);
    printf("  Values: %.1f bits (same as before)\n", val_bits_opt);
    printf("  Total: %.1f bits = %.1f bytes\n\n",
           comb_bits + val_bits_opt + 16, (comb_bits + val_bits_opt + 16) / 8);

    printf("=================================================================\n");
    printf("METHOD 4: Gap Encoding (Delta + Rice Coding)\n");
    printf("=================================================================\n\n");

    /* Average gap between positions */
    float avg_gap = (float)dim / k;

    /* Rice parameter r = floor(log2(average gap)) */
    uint32_t r = (uint32_t)log2(avg_gap);
    float rice_bits_per_gap = r + 1 + 1.0/avg_gap;  /* Approximate */
    float gap_bits = k * rice_bits_per_gap;

    printf("Gap encoding:\n");
    printf("  Average gap: %.1f\n", avg_gap);
    printf("  Rice parameter r: %u\n", r);
    printf("  Bits per gap: ~%.2f\n", rice_bits_per_gap);
    printf("  Total gap bits: %.1f bits (%.1f bytes)\n", gap_bits, gap_bits / 8);
    printf("  Values: %.1f bits\n", val_bits_opt);
    printf("  Total: %.1f bits = %.1f bytes\n\n",
           gap_bits + val_bits_opt + 16, (gap_bits + val_bits_opt + 16) / 8);

    printf("=================================================================\n");
    printf("METHOD 5: Bit Vector + Packed Values\n");
    printf("=================================================================\n\n");

    uint32_t bitvec_bits = dim;  /* One bit per position */
    uint32_t bitvec_val_bits = (uint32_t)val_bits_opt;
    uint32_t bitvec_total = bitvec_bits + bitvec_val_bits;
    uint32_t bitvec_bytes = (bitvec_total + 7) / 8;

    printf("Format: [2048-bit position vector] [entropy-coded values]\n");
    printf("  Position bits: %u (= %u bytes)\n", bitvec_bits, bitvec_bits / 8);
    printf("  Value bits: %.1f (= %.1f bytes)\n", val_bits_opt, val_bits_opt / 8);
    printf("  Total: %u bits = %u bytes\n\n", bitvec_total, bitvec_bytes);

    printf("=================================================================\n");
    printf("COMPARISON SUMMARY\n");
    printf("=================================================================\n\n");

    printf("Method                          | Bytes | vs Current | Complexity\n");
    printf("--------------------------------|-------|------------|-----------\n");
    printf("Current (bit-packed)            | %3u   | baseline   | Very Low\n", current_bytes);
    printf("Gap + Rice coding               | ~%3.0f  | -%.0f%%      | Low\n",
           (gap_bits + val_bits_opt + 16) / 8,
           100.0 * (1.0 - (gap_bits + val_bits_opt + 16) / 8 / current_bytes));
    printf("Bit-vector + values             | %3u   | -%d%%       | Low\n",
           bitvec_bytes, (int)(100.0 * (1.0 - (float)bitvec_bytes / current_bytes)));
    printf("**Arithmetic coding**           | **%3u** | **-%.0f%%**    | Medium\n",
           optimal_bytes,
           100.0 * (1.0 - (float)optimal_bytes / current_bytes));
    printf("Combinatorial (theoretical)     | ~%3.0f  | -%.0f%%      | High\n\n",
           (comb_bits + val_bits_opt + 16) / 8,
           100.0 * (1.0 - (comb_bits + val_bits_opt + 16) / 8 / current_bytes));

    printf("=================================================================\n");
    printf("RECOMMENDATION\n");
    printf("=================================================================\n\n");

    uint32_t savings = current_bytes - optimal_bytes;

    printf("Current bit-packed encoding wastes ~%u bytes!\n\n", savings);

    printf("BEST: Arithmetic/ANS coding\n");
    printf("  Size: %u bytes (%.1fx better than current)\n",
           optimal_bytes, (float)current_bytes / optimal_bytes);
    printf("  Savings: %u bytes (%.1f%% reduction)\n",
           savings, 100.0 * savings / current_bytes);
    printf("  Implementation: Medium complexity (can use existing libraries)\n\n");

    printf("ALTERNATIVE: Gap encoding with Rice codes\n");
    printf("  Size: ~%.0f bytes\n", (gap_bits + val_bits_opt + 16) / 8);
    printf("  Savings: ~%.0f bytes\n", current_bytes - (gap_bits + val_bits_opt + 16) / 8);
    printf("  Implementation: Low complexity (~200 lines)\n\n");

    if (optimal_bytes <= 256) {
        printf("Both methods EASILY meet the 256-byte target!\n");
        printf("Arithmetic coding gets you closest to theoretical optimum.\n");
    }

    printf("=================================================================\n");

    return 0;
}
