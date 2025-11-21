/**
 * NTT Dimensional Projection Demo
 *
 * Demonstrates projecting a vector through the stack of 7 NTT layers,
 * tracking relationships and properties at each level.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "ntt64.h"

#define N 64

// Helper function to compute L2 norm
static double compute_l2_norm(const uint32_t *vec, size_t len, uint32_t modulus) {
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) {
        // Convert to centered representation [-q/2, q/2]
        int64_t centered = (int64_t)vec[i];
        if (centered > modulus / 2) {
            centered -= modulus;
        }
        sum += (double)centered * (double)centered;
    }
    return sqrt(sum);
}

// Helper function to compute L-infinity norm
static uint32_t compute_linf_norm(const uint32_t *vec, size_t len, uint32_t modulus) {
    uint32_t max = 0;
    for (size_t i = 0; i < len; i++) {
        // Convert to centered representation
        int64_t centered = (int64_t)vec[i];
        if (centered > modulus / 2) {
            centered -= modulus;
        }
        uint32_t abs_val = (uint32_t)llabs(centered);
        if (abs_val > max) {
            max = abs_val;
        }
    }
    return max;
}

// Helper function to count non-zero elements
static size_t count_nonzero(const uint32_t *vec, size_t len) {
    size_t count = 0;
    for (size_t i = 0; i < len; i++) {
        if (vec[i] != 0) {
            count++;
        }
    }
    return count;
}

// Helper function to compute dot product with itself (squared L2)
static uint64_t compute_squared_l2(const uint32_t *vec, size_t len, uint32_t modulus) {
    uint64_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        int64_t centered = (int64_t)vec[i];
        if (centered > modulus / 2) {
            centered -= modulus;
        }
        sum += (uint64_t)(centered * centered);
    }
    return sum;
}

// Print first and last few elements of vector
static void print_vector_summary(const char *label, const uint32_t *vec, size_t len, uint32_t modulus) {
    printf("  %s: [", label);
    size_t print_count = (len < 8) ? len : 4;

    for (size_t i = 0; i < print_count; i++) {
        printf("%u", vec[i]);
        if (i < print_count - 1) printf(", ");
    }

    if (len > 8) {
        printf(", ..., ");
        for (size_t i = len - 4; i < len; i++) {
            printf("%u", vec[i]);
            if (i < len - 1) printf(", ");
        }
    }

    printf("] (mod %u)\n", modulus);
}

// Structure to hold layer information
typedef struct {
    int layer_id;
    uint32_t modulus;
    const char *name;
    uint32_t input[N];
    uint32_t ntt_output[N];
    double input_l2_norm;
    uint32_t input_linf_norm;
    double ntt_l2_norm;
    uint32_t ntt_linf_norm;
    size_t input_nonzero;
    size_t ntt_nonzero;
} layer_info_t;

int main(int argc, char **argv) {
    printf("=== NTT Dimensional Projection Demo ===\n\n");

    // Initialize the original 64-dimensional vector
    // Using a sparse vector with a pattern to make it interesting
    uint32_t original[N];
    memset(original, 0, sizeof(original));

    // Create a structured pattern:
    // - Prime positions get Fibonacci-like values
    // - Every 8th position gets a marker value
    // This creates structure that we can track through transformations
    int primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61};
    size_t num_primes = sizeof(primes) / sizeof(primes[0]);

    uint32_t fib_a = 1, fib_b = 1;
    for (size_t i = 0; i < num_primes; i++) {
        if (primes[i] < N) {
            original[primes[i]] = fib_b % 100;  // Keep values moderate
            uint32_t next = fib_a + fib_b;
            fib_a = fib_b;
            fib_b = next;
        }
    }

    // Add markers at regular intervals
    for (int i = 0; i < N; i += 8) {
        if (original[i] == 0) {
            original[i] = 42;
        }
    }

    printf("Original Vector Configuration:\n");
    printf("  Dimension: %d\n", N);
    printf("  Pattern: Fibonacci values at prime indices + markers at 8-intervals\n");
    print_vector_summary("Values", original, N, 0);
    printf("\n");

    // Process through all 7 NTT layers
    layer_info_t layers[7];
    const char *layer_names[] = {
        "Layer 0 (q=257, tiny)",
        "Layer 1 (q=3329, small)",
        "Layer 2 (q=10753, medium)",
        "Layer 3 (q=43777, large)",
        "Layer 4 (q=64513, large+)",
        "Layer 5 (q=686593, very large)",
        "Layer 6 (q=2818573313, huge)"
    };

    for (int layer = 0; layer < 7; layer++) {
        layers[layer].layer_id = layer;
        layers[layer].modulus = ntt64_get_modulus(layer);
        layers[layer].name = layer_names[layer];

        // Reduce original vector mod this layer's modulus
        for (int i = 0; i < N; i++) {
            layers[layer].input[i] = original[i] % layers[layer].modulus;
        }

        // Compute input statistics
        layers[layer].input_l2_norm = compute_l2_norm(layers[layer].input, N, layers[layer].modulus);
        layers[layer].input_linf_norm = compute_linf_norm(layers[layer].input, N, layers[layer].modulus);
        layers[layer].input_nonzero = count_nonzero(layers[layer].input, N);

        // Apply forward NTT
        memcpy(layers[layer].ntt_output, layers[layer].input, sizeof(layers[layer].input));
        ntt64_forward(layers[layer].ntt_output, layer);

        // Compute NTT domain statistics
        layers[layer].ntt_l2_norm = compute_l2_norm(layers[layer].ntt_output, N, layers[layer].modulus);
        layers[layer].ntt_linf_norm = compute_linf_norm(layers[layer].ntt_output, N, layers[layer].modulus);
        layers[layer].ntt_nonzero = count_nonzero(layers[layer].ntt_output, N);
    }

    // Print detailed layer-by-layer analysis
    printf("=== Layer-by-Layer Projection ===\n\n");

    for (int layer = 0; layer < 7; layer++) {
        printf("─────────────────────────────────────────────────────────\n");
        printf("%s\n", layers[layer].name);
        printf("─────────────────────────────────────────────────────────\n");

        printf("COEFFICIENT DOMAIN (Input):\n");
        print_vector_summary("Values", layers[layer].input, N, layers[layer].modulus);
        printf("  L2 Norm:     %.2f\n", layers[layer].input_l2_norm);
        printf("  L∞ Norm:     %u\n", layers[layer].input_linf_norm);
        printf("  Non-zeros:   %zu / %d (%.1f%% sparse)\n",
               layers[layer].input_nonzero, N,
               100.0 * (1.0 - (double)layers[layer].input_nonzero / N));

        printf("\nNTT DOMAIN (Transformed):\n");
        print_vector_summary("Values", layers[layer].ntt_output, N, layers[layer].modulus);
        printf("  L2 Norm:     %.2f\n", layers[layer].ntt_l2_norm);
        printf("  L∞ Norm:     %u\n", layers[layer].ntt_linf_norm);
        printf("  Non-zeros:   %zu / %d (%.1f%% sparse)\n",
               layers[layer].ntt_nonzero, N,
               100.0 * (1.0 - (double)layers[layer].ntt_nonzero / N));

        printf("\nTRANSFORMATION PROPERTIES:\n");
        double norm_ratio = layers[layer].ntt_l2_norm / layers[layer].input_l2_norm;
        printf("  Norm ratio (NTT/Input): %.4f\n", norm_ratio);
        printf("  L∞ growth:              %.2fx\n",
               (double)layers[layer].ntt_linf_norm / (double)layers[layer].input_linf_norm);
        printf("  Density change:         %+.1f%%\n",
               100.0 * ((double)layers[layer].ntt_nonzero - (double)layers[layer].input_nonzero) / N);

        // Verify NTT is reversible
        uint32_t reconstructed[N];
        memcpy(reconstructed, layers[layer].ntt_output, sizeof(reconstructed));
        ntt64_inverse(reconstructed, layer);

        int mismatch = 0;
        for (int i = 0; i < N; i++) {
            if (reconstructed[i] != layers[layer].input[i]) {
                mismatch++;
            }
        }
        printf("  Reversibility:          %s\n", mismatch == 0 ? "✓ Perfect" : "✗ Failed");

        printf("\n");
    }

    // Cross-layer relationship analysis
    printf("=== Cross-Layer Relationship Analysis ===\n\n");

    printf("Modulus Progression:\n");
    for (int layer = 0; layer < 7; layer++) {
        double ratio = (layer > 0) ?
            (double)layers[layer].modulus / (double)layers[layer-1].modulus : 0.0;
        printf("  Layer %d: q=%10u", layer, layers[layer].modulus);
        if (layer > 0) {
            printf("  (%.2fx previous)", ratio);
        }
        printf("\n");
    }
    printf("\n");

    printf("L2 Norm Evolution Across Layers:\n");
    printf("  Coefficient Domain: ");
    for (int layer = 0; layer < 7; layer++) {
        printf("%8.2f", layers[layer].input_l2_norm);
        if (layer < 6) printf(" → ");
    }
    printf("\n");

    printf("  NTT Domain:         ");
    for (int layer = 0; layer < 7; layer++) {
        printf("%8.2f", layers[layer].ntt_l2_norm);
        if (layer < 6) printf(" → ");
    }
    printf("\n\n");

    printf("Sparsity Evolution:\n");
    printf("  Coefficient Domain: ");
    for (int layer = 0; layer < 7; layer++) {
        printf("%3zu", layers[layer].input_nonzero);
        if (layer < 6) printf(" → ");
    }
    printf("\n");

    printf("  NTT Domain:         ");
    for (int layer = 0; layer < 7; layer++) {
        printf("%3zu", layers[layer].ntt_nonzero);
        if (layer < 6) printf(" → ");
    }
    printf("\n\n");

    // Test chaining: transform in one layer, bring back, transform in another
    printf("=== Chain Transformation Test ===\n");
    printf("Testing: Layer 1 → inverse → Layer 3 → inverse → Layer 6\n\n");

    uint32_t chain[N];

    // Start at layer 1
    for (int i = 0; i < N; i++) {
        chain[i] = original[i] % ntt64_get_modulus(1);
    }
    printf("Step 1 - Layer 1 forward NTT:\n");
    ntt64_forward(chain, 1);
    printf("  L2 norm: %.2f, L∞ norm: %u\n",
           compute_l2_norm(chain, N, ntt64_get_modulus(1)),
           compute_linf_norm(chain, N, ntt64_get_modulus(1)));

    // Back to coefficient
    ntt64_inverse(chain, 1);
    printf("Step 2 - Layer 1 inverse NTT:\n");
    printf("  L2 norm: %.2f, L∞ norm: %u\n",
           compute_l2_norm(chain, N, ntt64_get_modulus(1)),
           compute_linf_norm(chain, N, ntt64_get_modulus(1)));

    // Move to layer 3
    for (int i = 0; i < N; i++) {
        chain[i] = chain[i] % ntt64_get_modulus(3);
    }
    printf("Step 3 - Reduce to Layer 3 modulus and forward NTT:\n");
    ntt64_forward(chain, 3);
    printf("  L2 norm: %.2f, L∞ norm: %u\n",
           compute_l2_norm(chain, N, ntt64_get_modulus(3)),
           compute_linf_norm(chain, N, ntt64_get_modulus(3)));

    // Back to coefficient
    ntt64_inverse(chain, 3);
    printf("Step 4 - Layer 3 inverse NTT:\n");
    printf("  L2 norm: %.2f, L∞ norm: %u\n",
           compute_l2_norm(chain, N, ntt64_get_modulus(3)),
           compute_linf_norm(chain, N, ntt64_get_modulus(3)));

    // Move to layer 6
    for (int i = 0; i < N; i++) {
        chain[i] = chain[i] % ntt64_get_modulus(6);
    }
    printf("Step 5 - Reduce to Layer 6 modulus and forward NTT:\n");
    ntt64_forward(chain, 6);
    printf("  L2 norm: %.2f, L∞ norm: %u\n",
           compute_l2_norm(chain, N, ntt64_get_modulus(6)),
           compute_linf_norm(chain, N, ntt64_get_modulus(6)));

    printf("\n=== Projection Complete ===\n");

    return 0;
}
