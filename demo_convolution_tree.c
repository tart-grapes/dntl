/**
 * NTT-Based Hierarchical Convolution Tree
 *
 * Demonstrates convolution projection through an inverted binary tree:
 * - Start: 64 vectors × 16384D each
 * - Level 1: Convolve pairs → 32 results
 * - Level 2: Convolve pairs → 16 results
 * - Level 3: Convolve pairs → 8 results
 * - Level 4: Convolve pairs → 4 results
 * - Level 5: Convolve pairs → 2 results
 * - Level 6: Convolve pairs → 1 final result
 *
 * Each convolution is done via NTT for efficiency.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "ntt64.h"

#define N 64                    // NTT dimension
#define NUM_BLOCKS 256          // Blocks per vector: 256 × 64 = 16384D
#define TOTAL_DIM (NUM_BLOCKS * N)
#define INITIAL_VECTORS 64      // Starting with 64 vectors

// High-dimensional vector (16384D = 256 blocks of 64D)
typedef struct {
    uint32_t blocks[NUM_BLOCKS][N];
} highdim_vec_t;

// ============================================================================
// STATISTICS
// ============================================================================

static double compute_l2_norm(const uint32_t *vec, size_t len, uint32_t mod) {
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) {
        int64_t c = (int64_t)vec[i];
        if (c > mod/2) c -= mod;
        sum += (double)c * c;
    }
    return sqrt(sum);
}

static double compute_highdim_l2(const highdim_vec_t *vec, uint32_t mod) {
    double sum = 0.0;
    for (int b = 0; b < NUM_BLOCKS; b++) {
        for (int i = 0; i < N; i++) {
            int64_t c = (int64_t)vec->blocks[b][i];
            if (c > mod/2) c -= mod;
            sum += (double)c * c;
        }
    }
    return sqrt(sum);
}

static size_t count_nonzero(const uint32_t *vec, size_t len) {
    size_t cnt = 0;
    for (size_t i = 0; i < len; i++) if (vec[i] != 0) cnt++;
    return cnt;
}

static size_t count_highdim_nonzero(const highdim_vec_t *vec) {
    size_t cnt = 0;
    for (int b = 0; b < NUM_BLOCKS; b++) {
        for (int i = 0; i < N; i++) {
            if (vec->blocks[b][i] != 0) cnt++;
        }
    }
    return cnt;
}

// ============================================================================
// NTT CONVOLUTION (per 64D block)
// ============================================================================

/**
 * Convolve two 64D blocks using NTT
 * result = a ⊗ b (circular convolution)
 */
static void ntt_convolve_block(const uint32_t a[N], const uint32_t b[N],
                               uint32_t result[N], int layer) {
    uint32_t a_ntt[N], b_ntt[N];

    // Forward NTT on both inputs
    memcpy(a_ntt, a, N * sizeof(uint32_t));
    memcpy(b_ntt, b, N * sizeof(uint32_t));
    ntt64_forward(a_ntt, layer);
    ntt64_forward(b_ntt, layer);

    // Pointwise multiplication in NTT domain
    ntt64_pointwise_mul(result, a_ntt, b_ntt, layer);

    // Inverse NTT to get convolution result
    ntt64_inverse(result, layer);
}

/**
 * Convolve two high-dimensional vectors block-by-block
 * Each block is convolved independently using NTT
 */
static void convolve_highdim(const highdim_vec_t *a, const highdim_vec_t *b,
                             highdim_vec_t *result, int layer) {
    for (int block = 0; block < NUM_BLOCKS; block++) {
        ntt_convolve_block(a->blocks[block], b->blocks[block],
                          result->blocks[block], layer);
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

/**
 * Initialize a high-dimensional vector with a pattern
 */
static void init_highdim_vector(highdim_vec_t *vec, uint32_t seed, uint32_t mod) {
    // Simple PRNG for reproducibility
    uint32_t state = seed;

    for (int block = 0; block < NUM_BLOCKS; block++) {
        for (int i = 0; i < N; i++) {
            // Linear congruential generator
            state = (state * 1103515245u + 12345u) & 0x7FFFFFFFu;

            // Generate small values (0-10) with some sparsity
            uint32_t val = state % 20;
            if (val < 10) {
                vec->blocks[block][i] = val % mod;
            } else {
                vec->blocks[block][i] = 0;
            }
        }
    }
}

// ============================================================================
// CONVOLUTION TREE
// ============================================================================

typedef struct {
    int level;              // Tree level (0 = top, 6 = bottom)
    int num_vectors;        // Number of vectors at this level
    highdim_vec_t *vectors; // Array of vectors
    double avg_l2_norm;     // Average L2 norm across vectors
    size_t avg_nonzero;     // Average non-zeros per vector
} tree_level_t;

static void compute_level_stats(tree_level_t *level, uint32_t mod) {
    double sum_l2 = 0.0;
    size_t sum_nz = 0;

    for (int i = 0; i < level->num_vectors; i++) {
        sum_l2 += compute_highdim_l2(&level->vectors[i], mod);
        sum_nz += count_highdim_nonzero(&level->vectors[i]);
    }

    level->avg_l2_norm = sum_l2 / level->num_vectors;
    level->avg_nonzero = sum_nz / level->num_vectors;
}

static void print_level_info(const tree_level_t *level) {
    printf("  Level %d: %2d vectors | Avg L2: %12.2f | Avg density: %5zu / %d (%.1f%%)\n",
           level->level, level->num_vectors,
           level->avg_l2_norm,
           level->avg_nonzero, TOTAL_DIM,
           100.0 * level->avg_nonzero / TOTAL_DIM);
}

// ============================================================================
// MAIN DEMONSTRATION
// ============================================================================

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║     NTT HIERARCHICAL CONVOLUTION TREE                     ║\n");
    printf("║     Inverted Binary Tree: 64 → 32 → 16 → 8 → 4 → 2 → 1  ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    // Choose NTT layer (we'll use layer 3: q=43777, 16-bit modulus)
    int ntt_layer = 3;
    uint32_t modulus = ntt64_get_modulus(ntt_layer);

    printf("Configuration:\n");
    printf("  Vector dimension:     %d (%d blocks × %d)\n", TOTAL_DIM, NUM_BLOCKS, N);
    printf("  Initial vectors:      %d\n", INITIAL_VECTORS);
    printf("  NTT layer:            %d (modulus = %u)\n", ntt_layer, modulus);
    printf("  Convolution method:   Block-wise NTT (circular convolution)\n\n");

    // Allocate tree levels
    // Level 0: 64 vectors, Level 1: 32, Level 2: 16, etc.
    tree_level_t levels[7];
    int num_vecs[] = {64, 32, 16, 8, 4, 2, 1};

    for (int l = 0; l < 7; l++) {
        levels[l].level = l;
        levels[l].num_vectors = num_vecs[l];
        levels[l].vectors = malloc(num_vecs[l] * sizeof(highdim_vec_t));
        if (!levels[l].vectors) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }
    }

    // ========================================================================
    // LEVEL 0: Initialize 64 input vectors
    // ========================================================================
    printf("┌───────────────────────────────────────────────────────────┐\n");
    printf("│ LEVEL 0: INITIALIZATION (64 vectors)                     │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    for (int i = 0; i < 64; i++) {
        init_highdim_vector(&levels[0].vectors[i], i * 12347, modulus);
    }

    compute_level_stats(&levels[0], modulus);
    print_level_info(&levels[0]);

    printf("\n  Sample vector 0, block 0: [%u, %u, %u, %u, ...]\n",
           levels[0].vectors[0].blocks[0][0],
           levels[0].vectors[0].blocks[0][1],
           levels[0].vectors[0].blocks[0][2],
           levels[0].vectors[0].blocks[0][3]);

    // ========================================================================
    // LEVELS 1-6: Successive pairwise convolutions
    // ========================================================================
    printf("\n┌───────────────────────────────────────────────────────────┐\n");
    printf("│ CONVOLUTION TREE PROJECTION                               │\n");
    printf("└───────────────────────────────────────────────────────────┘\n\n");

    for (int level = 0; level < 6; level++) {
        int src_count = levels[level].num_vectors;
        int dst_count = levels[level + 1].num_vectors;

        printf("Level %d → Level %d: Convolving %d pairs...\n",
               level, level + 1, dst_count);

        // Convolve pairs: (0,1)→0, (2,3)→1, (4,5)→2, etc.
        for (int pair = 0; pair < dst_count; pair++) {
            int idx_a = pair * 2;
            int idx_b = pair * 2 + 1;

            convolve_highdim(&levels[level].vectors[idx_a],
                           &levels[level].vectors[idx_b],
                           &levels[level + 1].vectors[pair],
                           ntt_layer);
        }

        compute_level_stats(&levels[level + 1], modulus);
        print_level_info(&levels[level + 1]);
        printf("\n");
    }

    // ========================================================================
    // DETAILED ANALYSIS
    // ========================================================================
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                    TREE ANALYSIS                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    printf("Level-by-Level Statistics:\n");
    printf("──────────────────────────────────────────────────────────\n");
    printf("Level | Vectors | Avg L2 Norm  | Avg Density | Norm Growth\n");
    printf("──────────────────────────────────────────────────────────\n");

    for (int l = 0; l < 7; l++) {
        double growth = (l > 0) ?
            levels[l].avg_l2_norm / levels[l-1].avg_l2_norm : 1.0;

        printf("  %d   |   %2d    | %12.2f | %5zu / %5d | %.3fx\n",
               l, levels[l].num_vectors,
               levels[l].avg_l2_norm,
               levels[l].avg_nonzero, TOTAL_DIM,
               growth);
    }

    printf("\n");
    printf("Key Observations:\n");
    printf("  • Total convolutions:     %d (one per tree edge)\n",
           64 + 32 + 16 + 8 + 4 + 2 - 6);
    printf("  • Dimension preserved:    %d (no dimensional reduction)\n", TOTAL_DIM);
    printf("  • Norm growth:            %.2fx (level 0 → level 6)\n",
           levels[6].avg_l2_norm / levels[0].avg_l2_norm);
    printf("  • Structural reduction:   64 vectors → 1 vector\n");

    // ========================================================================
    // INSPECT FINAL RESULT
    // ========================================================================
    printf("\n┌───────────────────────────────────────────────────────────┐\n");
    printf("│ FINAL RESULT (Level 6, single vector)                    │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    highdim_vec_t *final = &levels[6].vectors[0];
    double final_l2 = compute_highdim_l2(final, modulus);
    size_t final_nz = count_highdim_nonzero(final);

    printf("  Dimension:        %d\n", TOTAL_DIM);
    printf("  L2 norm:          %.2f\n", final_l2);
    printf("  Density:          %zu / %d (%.1f%%)\n",
           final_nz, TOTAL_DIM, 100.0 * final_nz / TOTAL_DIM);
    printf("  Modulus:          %u\n", modulus);

    printf("\n  First block (64D): [");
    for (int i = 0; i < 10; i++) {
        printf("%u", final->blocks[0][i]);
        if (i < 9) printf(", ");
    }
    printf(", ...]\n");

    printf("\n  Last block (64D): [..., ");
    for (int i = N-6; i < N; i++) {
        printf("%u", final->blocks[NUM_BLOCKS-1][i]);
        if (i < N-1) printf(", ");
    }
    printf("]\n");

    // ========================================================================
    // PROJECTION VERIFICATION
    // ========================================================================
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║              PROJECTION CORRECTNESS CHECK                 ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    printf("Testing convolution properties:\n\n");

    // Test 1: Verify single convolution
    printf("Test 1: Single block convolution verification\n");
    uint32_t test_a[N], test_b[N], result_direct[N];
    memcpy(test_a, levels[0].vectors[0].blocks[0], N * sizeof(uint32_t));
    memcpy(test_b, levels[0].vectors[1].blocks[0], N * sizeof(uint32_t));

    ntt_convolve_block(test_a, test_b, result_direct, ntt_layer);

    // Compare with what's in level 1, vector 0, block 0
    int mismatch = 0;
    for (int i = 0; i < N; i++) {
        if (result_direct[i] != levels[1].vectors[0].blocks[0][i]) {
            mismatch++;
        }
    }
    printf("  Expected result matches level 1: %s (%d mismatches)\n",
           mismatch == 0 ? "✓ PASS" : "✗ FAIL", mismatch);

    // Test 2: Commutativity (a ⊗ b = b ⊗ a for circular convolution)
    uint32_t result_ab[N], result_ba[N];
    ntt_convolve_block(test_a, test_b, result_ab, ntt_layer);
    ntt_convolve_block(test_b, test_a, result_ba, ntt_layer);

    int comm_errors = 0;
    for (int i = 0; i < N; i++) {
        if (result_ab[i] != result_ba[i]) {
            comm_errors++;
        }
    }
    printf("\n  Commutativity (a⊗b = b⊗a): %s (%d errors)\n",
           comm_errors == 0 ? "✓ PASS" : "✗ FAIL", comm_errors);

    // Test 3: Identity element (convolve with [1,0,0,...,0] should preserve)
    uint32_t identity[N] = {0};
    identity[0] = 1;
    uint32_t result_id[N];
    ntt_convolve_block(test_a, identity, result_id, ntt_layer);

    int id_errors = 0;
    for (int i = 0; i < N; i++) {
        if (result_id[i] != test_a[i]) {
            id_errors++;
        }
    }
    printf("  Identity element: %s (%d errors)\n",
           id_errors == 0 ? "✓ PASS" : "✗ FAIL", id_errors);

    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║            ★ CONVOLUTION TREE COMPLETE ★                 ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    // Cleanup
    for (int l = 0; l < 7; l++) {
        free(levels[l].vectors);
    }

    return 0;
}
