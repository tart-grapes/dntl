/**
 * High-Dimensional NTT Projection Demo
 *
 * Demonstrates projecting a 16384-dimensional vector (represented as 256 blocks
 * of 64-dimensional NTT vectors) through the stack of NTT layers.
 * Tracks aggregate statistics and relationships across the entire high-dimensional space.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "ntt64.h"

#define N 64
#define NUM_BLOCKS 256  // 256 blocks × 64 dims = 16384 dimensions
#define TOTAL_DIM (N * NUM_BLOCKS)

// Global structure to hold high-dimensional vector as blocks
typedef struct {
    uint32_t blocks[NUM_BLOCKS][N];
} high_dim_vector_t;

// Statistics for the entire high-dimensional vector
typedef struct {
    double global_l2_norm;
    uint64_t global_max_coeff;
    uint64_t global_min_coeff;
    size_t total_nonzero;
    double avg_block_l2_norm;
    double stddev_block_l2_norm;
} vector_stats_t;

// Compute statistics for a high-dimensional vector
static void compute_vector_stats(const high_dim_vector_t *vec, uint32_t modulus, vector_stats_t *stats) {
    double global_sum_sq = 0.0;
    stats->global_max_coeff = 0;
    stats->global_min_coeff = modulus;
    stats->total_nonzero = 0;

    double block_norms[NUM_BLOCKS];
    double sum_block_norms = 0.0;

    for (size_t block = 0; block < NUM_BLOCKS; block++) {
        double block_sum_sq = 0.0;

        for (size_t i = 0; i < N; i++) {
            uint32_t val = vec->blocks[block][i];

            // Convert to centered representation
            int64_t centered = (int64_t)val;
            if (centered > modulus / 2) {
                centered -= modulus;
            }

            uint64_t abs_val = (uint64_t)llabs(centered);
            if (abs_val > stats->global_max_coeff) {
                stats->global_max_coeff = abs_val;
            }
            if (abs_val < stats->global_min_coeff && abs_val > 0) {
                stats->global_min_coeff = abs_val;
            }

            if (val != 0) {
                stats->total_nonzero++;
            }

            double contrib = (double)centered * (double)centered;
            block_sum_sq += contrib;
            global_sum_sq += contrib;
        }

        block_norms[block] = sqrt(block_sum_sq);
        sum_block_norms += block_norms[block];
    }

    stats->global_l2_norm = sqrt(global_sum_sq);
    stats->avg_block_l2_norm = sum_block_norms / NUM_BLOCKS;

    // Compute standard deviation of block norms
    double var_sum = 0.0;
    for (size_t block = 0; block < NUM_BLOCKS; block++) {
        double diff = block_norms[block] - stats->avg_block_l2_norm;
        var_sum += diff * diff;
    }
    stats->stddev_block_l2_norm = sqrt(var_sum / NUM_BLOCKS);
}

// Initialize a high-dimensional vector with structured pattern
static void init_high_dim_vector(high_dim_vector_t *vec) {
    // Create a fractal-like pattern with different structure at different scales

    for (size_t block = 0; block < NUM_BLOCKS; block++) {
        for (size_t i = 0; i < N; i++) {
            uint32_t val = 0;

            // Pattern 1: Block-level structure (low frequency)
            if (block % 16 == 0 && i == 0) {
                val = 100;  // Markers at every 16th block
            }

            // Pattern 2: Prime positions within blocks
            if ((i == 2 || i == 3 || i == 5 || i == 7 || i == 11 || i == 13) &&
                (block % 4 == 0)) {
                val = ((block / 4) % 50) + 1;  // Small values at primes
            }

            // Pattern 3: Diagonal structure across blocks
            if (i == (block % N)) {
                val += 10;
            }

            // Pattern 4: Power-of-2 positions
            if ((i & (i - 1)) == 0 && i > 0) {  // i is power of 2
                val += (block % 20) + 1;
            }

            vec->blocks[block][i] = val;
        }
    }
}

// Apply NTT to all blocks of a high-dimensional vector
static void apply_ntt_to_all_blocks(high_dim_vector_t *vec, int layer) {
    for (size_t block = 0; block < NUM_BLOCKS; block++) {
        ntt64_forward(vec->blocks[block], layer);
    }
}

// Apply inverse NTT to all blocks
static void apply_intt_to_all_blocks(high_dim_vector_t *vec, int layer) {
    for (size_t block = 0; block < NUM_BLOCKS; block++) {
        ntt64_inverse(vec->blocks[block], layer);
    }
}

// Reduce all blocks modulo a new modulus
static void reduce_modulus(high_dim_vector_t *vec, uint32_t new_modulus) {
    for (size_t block = 0; block < NUM_BLOCKS; block++) {
        for (size_t i = 0; i < N; i++) {
            vec->blocks[block][i] %= new_modulus;
        }
    }
}

// Copy high-dimensional vector
static void copy_high_dim_vector(high_dim_vector_t *dest, const high_dim_vector_t *src) {
    memcpy(dest, src, sizeof(high_dim_vector_t));
}

int main(int argc, char **argv) {
    printf("=== High-Dimensional (16384D) NTT Projection Demo ===\n\n");

    // Initialize the high-dimensional vector
    high_dim_vector_t original;
    printf("Initializing %d-dimensional vector (%d blocks × %d dimensions)...\n",
           TOTAL_DIM, NUM_BLOCKS, N);

    clock_t start_init = clock();
    init_high_dim_vector(&original);
    clock_t end_init = clock();

    double init_time = (double)(end_init - start_init) / CLOCKS_PER_SEC;
    printf("Initialization complete in %.3f seconds\n\n", init_time);

    // Compute statistics for original vector
    vector_stats_t orig_stats;
    compute_vector_stats(&original, UINT32_MAX, &orig_stats);

    printf("Original Vector Statistics:\n");
    printf("  Total dimensions:        %d\n", TOTAL_DIM);
    printf("  Global L2 norm:          %.2f\n", orig_stats.global_l2_norm);
    printf("  Total non-zeros:         %zu / %d (%.2f%% density)\n",
           orig_stats.total_nonzero, TOTAL_DIM,
           100.0 * orig_stats.total_nonzero / TOTAL_DIM);
    printf("  Max coefficient:         %lu\n", orig_stats.global_max_coeff);
    printf("  Avg block L2 norm:       %.2f ± %.2f\n",
           orig_stats.avg_block_l2_norm, orig_stats.stddev_block_l2_norm);
    printf("\n");

    // Process through all 7 layers
    printf("=== Projecting Through NTT Stack ===\n\n");

    for (int layer = 0; layer < 7; layer++) {
        uint32_t modulus = ntt64_get_modulus(layer);

        printf("───────────────────────────────────────────────────────────\n");
        printf("LAYER %d: modulus = %u\n", layer, modulus);
        printf("───────────────────────────────────────────────────────────\n");

        // Prepare vector for this layer
        high_dim_vector_t layer_vec;
        copy_high_dim_vector(&layer_vec, &original);
        reduce_modulus(&layer_vec, modulus);

        // Compute coefficient domain stats
        vector_stats_t coeff_stats;
        compute_vector_stats(&layer_vec, modulus, &coeff_stats);

        printf("COEFFICIENT DOMAIN:\n");
        printf("  Global L2 norm:          %.2f\n", coeff_stats.global_l2_norm);
        printf("  Global L∞ norm:          %lu\n", coeff_stats.global_max_coeff);
        printf("  Total non-zeros:         %zu / %d (%.2f%% density)\n",
               coeff_stats.total_nonzero, TOTAL_DIM,
               100.0 * coeff_stats.total_nonzero / TOTAL_DIM);
        printf("  Avg block L2 norm:       %.2f ± %.2f\n",
               coeff_stats.avg_block_l2_norm, coeff_stats.stddev_block_l2_norm);

        // Apply forward NTT to all blocks
        printf("\n  Applying forward NTT to all %d blocks...", NUM_BLOCKS);
        fflush(stdout);
        clock_t start_ntt = clock();
        apply_ntt_to_all_blocks(&layer_vec, layer);
        clock_t end_ntt = clock();
        double ntt_time = (double)(end_ntt - start_ntt) / CLOCKS_PER_SEC;
        printf(" done in %.3f seconds\n", ntt_time);
        printf("  Throughput: %.2f blocks/sec, %.2f million dims/sec\n\n",
               NUM_BLOCKS / ntt_time, (TOTAL_DIM / 1e6) / ntt_time);

        // Compute NTT domain stats
        vector_stats_t ntt_stats;
        compute_vector_stats(&layer_vec, modulus, &ntt_stats);

        printf("NTT DOMAIN:\n");
        printf("  Global L2 norm:          %.2f\n", ntt_stats.global_l2_norm);
        printf("  Global L∞ norm:          %lu\n", ntt_stats.global_max_coeff);
        printf("  Total non-zeros:         %zu / %d (%.2f%% density)\n",
               ntt_stats.total_nonzero, TOTAL_DIM,
               100.0 * ntt_stats.total_nonzero / TOTAL_DIM);
        printf("  Avg block L2 norm:       %.2f ± %.2f\n",
               ntt_stats.avg_block_l2_norm, ntt_stats.stddev_block_l2_norm);

        printf("\nTRANSFORMATION ANALYSIS:\n");
        printf("  L2 norm ratio:           %.4f\n",
               ntt_stats.global_l2_norm / coeff_stats.global_l2_norm);
        printf("  L∞ norm ratio:           %.4f\n",
               (double)ntt_stats.global_max_coeff / (double)coeff_stats.global_max_coeff);
        printf("  Density change:          %+.2f%%\n",
               100.0 * ((double)ntt_stats.total_nonzero - (double)coeff_stats.total_nonzero) / TOTAL_DIM);
        printf("  Block norm variance:     %.2f → %.2f (%.2fx)\n",
               coeff_stats.stddev_block_l2_norm, ntt_stats.stddev_block_l2_norm,
               ntt_stats.stddev_block_l2_norm / (coeff_stats.stddev_block_l2_norm + 1e-10));

        // Test reversibility on a sample of blocks
        printf("\n  Testing reversibility on sample blocks...");
        fflush(stdout);
        apply_intt_to_all_blocks(&layer_vec, layer);

        int total_errors = 0;
        for (size_t block = 0; block < NUM_BLOCKS; block++) {
            for (size_t i = 0; i < N; i++) {
                uint32_t expected = original.blocks[block][i] % modulus;
                if (layer_vec.blocks[block][i] != expected) {
                    total_errors++;
                }
            }
        }

        if (total_errors == 0) {
            printf(" ✓ Perfect (0 errors)\n");
        } else {
            printf(" ✗ Failed (%d errors)\n", total_errors);
        }

        printf("\n");
    }

    // Cross-layer comparison
    printf("=== Cross-Layer Analysis ===\n\n");

    printf("Norm Growth Across Layers:\n");
    printf("Layer | Modulus       | Coeff L2   | NTT L2       | Ratio\n");
    printf("------|---------------|------------|--------------|-------\n");

    for (int layer = 0; layer < 7; layer++) {
        uint32_t modulus = ntt64_get_modulus(layer);

        high_dim_vector_t layer_vec;
        copy_high_dim_vector(&layer_vec, &original);
        reduce_modulus(&layer_vec, modulus);

        vector_stats_t coeff_stats, ntt_stats;
        compute_vector_stats(&layer_vec, modulus, &coeff_stats);

        apply_ntt_to_all_blocks(&layer_vec, layer);
        compute_vector_stats(&layer_vec, modulus, &ntt_stats);

        printf("  %d   | %11u | %10.2f | %12.2f | %.2f\n",
               layer, modulus,
               coeff_stats.global_l2_norm,
               ntt_stats.global_l2_norm,
               ntt_stats.global_l2_norm / coeff_stats.global_l2_norm);
    }

    printf("\n");

    // Demonstrate projection cascade
    printf("=== Projection Cascade: Layer 0 → 2 → 5 ===\n\n");

    high_dim_vector_t cascade;
    copy_high_dim_vector(&cascade, &original);

    // Layer 0
    printf("Step 1: Reduce to Layer 0 (q=%u) and forward NTT\n", ntt64_get_modulus(0));
    reduce_modulus(&cascade, ntt64_get_modulus(0));
    vector_stats_t s1;
    compute_vector_stats(&cascade, ntt64_get_modulus(0), &s1);
    apply_ntt_to_all_blocks(&cascade, 0);
    vector_stats_t s1_ntt;
    compute_vector_stats(&cascade, ntt64_get_modulus(0), &s1_ntt);
    printf("  Coeff: L2=%.2f, L∞=%lu\n", s1.global_l2_norm, s1.global_max_coeff);
    printf("  NTT:   L2=%.2f, L∞=%lu\n\n", s1_ntt.global_l2_norm, s1_ntt.global_max_coeff);

    // Back to coeff, then Layer 2
    apply_intt_to_all_blocks(&cascade, 0);
    printf("Step 2: Inverse NTT, reduce to Layer 2 (q=%u) and forward NTT\n", ntt64_get_modulus(2));
    reduce_modulus(&cascade, ntt64_get_modulus(2));
    vector_stats_t s2;
    compute_vector_stats(&cascade, ntt64_get_modulus(2), &s2);
    apply_ntt_to_all_blocks(&cascade, 2);
    vector_stats_t s2_ntt;
    compute_vector_stats(&cascade, ntt64_get_modulus(2), &s2_ntt);
    printf("  Coeff: L2=%.2f, L∞=%lu\n", s2.global_l2_norm, s2.global_max_coeff);
    printf("  NTT:   L2=%.2f, L∞=%lu\n\n", s2_ntt.global_l2_norm, s2_ntt.global_max_coeff);

    // Back to coeff, then Layer 5
    apply_intt_to_all_blocks(&cascade, 2);
    printf("Step 3: Inverse NTT, reduce to Layer 5 (q=%u) and forward NTT\n", ntt64_get_modulus(5));
    reduce_modulus(&cascade, ntt64_get_modulus(5));
    vector_stats_t s3;
    compute_vector_stats(&cascade, ntt64_get_modulus(5), &s3);
    apply_ntt_to_all_blocks(&cascade, 5);
    vector_stats_t s3_ntt;
    compute_vector_stats(&cascade, ntt64_get_modulus(5), &s3_ntt);
    printf("  Coeff: L2=%.2f, L∞=%lu\n", s3.global_l2_norm, s3.global_max_coeff);
    printf("  NTT:   L2=%.2f, L∞=%lu\n\n", s3_ntt.global_l2_norm, s3_ntt.global_max_coeff);

    printf("Cascade Summary:\n");
    printf("  Layer 0 NTT magnification: %.2fx\n",
           s1_ntt.global_l2_norm / s1.global_l2_norm);
    printf("  Layer 2 NTT magnification: %.2fx\n",
           s2_ntt.global_l2_norm / s2.global_l2_norm);
    printf("  Layer 5 NTT magnification: %.2fx\n",
           s3_ntt.global_l2_norm / s3.global_l2_norm);

    printf("\n=== Projection Complete ===\n");

    return 0;
}
