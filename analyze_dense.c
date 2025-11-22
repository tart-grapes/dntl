/**
 * analyze_dense.c
 *
 * Analyze why dense batch encoding works for s but not w/w2
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static void generate_signature_like(uint16_t *vector, uint16_t dimension,
                                     uint16_t modulus, float small_ratio,
                                     unsigned int seed) {
    srand(seed);
    for (uint16_t i = 0; i < dimension; i++) {
        float r = (float)rand() / RAND_MAX;
        if (r < small_ratio) {
            vector[i] = rand() % 10;
        } else {
            vector[i] = 10 + (rand() % (modulus - 10));
        }
    }
}

static void analyze(const char *name, uint16_t dimension, uint16_t modulus,
                    float small_ratio, uint16_t num_vectors) {
    printf("\n=== %s: %d coeffs, mod %d, %.1f%% small ===\n",
           name, dimension, modulus, small_ratio * 100);

    /* Generate vectors */
    uint16_t **vectors = malloc(num_vectors * sizeof(uint16_t*));
    for (uint16_t v = 0; v < num_vectors; v++) {
        vectors[v] = malloc(dimension * sizeof(uint16_t));
        generate_signature_like(vectors[v], dimension, modulus, small_ratio, 1000 + v);
    }

    /* Count unique values */
    uint32_t *freqs = calloc(modulus, sizeof(uint32_t));
    for (uint16_t v = 0; v < num_vectors; v++) {
        for (uint16_t i = 0; i < dimension; i++) {
            freqs[vectors[v][i]]++;
        }
    }

    int n_unique = 0;
    for (uint16_t i = 0; i < modulus; i++) {
        if (freqs[i] > 0) n_unique++;
    }

    /* Calculate overhead */
    int bits_per_value = 0;
    uint16_t temp = modulus - 1;
    while (temp > 0) {
        bits_per_value++;
        temp >>= 1;
    }

    int alphabet_bits = n_unique * bits_per_value;
    int freq_table_bits = n_unique * 10;  /* 10-bit frequencies */
    int overhead_bits = 8 * 8 + alphabet_bits + freq_table_bits + 32;  /* header + alphabet + freqs + state */

    printf("Unique values: %d / %d (%.1f%%)\n", n_unique, modulus,
           100.0 * n_unique / modulus);
    printf("Overhead breakdown:\n");
    printf("  Header:    8 bytes\n");
    printf("  Alphabet:  %d values × %d bits = %d bytes\n",
           n_unique, bits_per_value, (alphabet_bits + 7) / 8);
    printf("  Freq table: %d values × 10 bits = %d bytes\n",
           n_unique, (freq_table_bits + 7) / 8);
    printf("  State:     4 bytes\n");
    printf("  Total overhead: %d bytes\n", (overhead_bits + 7) / 8);
    printf("  Overhead per vector: %.1f bytes\n\n",
           (double)(overhead_bits + 7) / 8 / num_vectors);

    /* Baseline size */
    size_t baseline_per_vec = (dimension * bits_per_value + 7) / 8;
    printf("Baseline: %zu bytes/vector\n", baseline_per_vec);
    printf("Overhead is %.1f%% of baseline\n",
           100.0 * ((overhead_bits + 7) / 8) / (baseline_per_vec * num_vectors));

    free(freqs);
    for (uint16_t v = 0; v < num_vectors; v++) {
        free(vectors[v]);
    }
    free(vectors);
}

int main() {
    printf("=== Dense Batch Overhead Analysis ===\n");

    const uint16_t num_vectors = 10;

    analyze("s", 128, 1031, 0.828, num_vectors);
    analyze("w", 64, 521, 0.531, num_vectors);
    analyze("w2", 32, 257, 0.70, num_vectors);

    printf("\n=== Conclusion ===\n");
    printf("Problem: With random large values, we get many unique values\n");
    printf("Solution: Need value clustering or delta encoding\n");

    return 0;
}
