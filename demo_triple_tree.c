/**
 * Triple NTT Convolution Tree with Algebraic Property Testing
 *
 * Creates three independent convolution trees (X, Y, Z):
 * - Each tree: 64 vectors (16384D) → ... → 1 final vector
 * - Tests algebraic properties when combining tree outputs:
 *   • Associativity: (x⊗y)⊗z = x⊗(y⊗z)
 *   • Commutativity: x⊗y = y⊗x
 *   • Permutation invariance: Different orderings
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
#define INITIAL_VECTORS 64      // Starting with 64 vectors per tree

// High-dimensional vector (16384D = 256 blocks of 64D)
typedef struct {
    uint32_t blocks[NUM_BLOCKS][N];
} highdim_vec_t;

// ============================================================================
// STATISTICS
// ============================================================================

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
// NTT CONVOLUTION
// ============================================================================

/**
 * Convolve two 64D blocks using NTT
 */
static void ntt_convolve_block(const uint32_t a[N], const uint32_t b[N],
                               uint32_t result[N], int layer) {
    uint32_t a_ntt[N], b_ntt[N];

    memcpy(a_ntt, a, N * sizeof(uint32_t));
    memcpy(b_ntt, b, N * sizeof(uint32_t));
    ntt64_forward(a_ntt, layer);
    ntt64_forward(b_ntt, layer);

    ntt64_pointwise_mul(result, a_ntt, b_ntt, layer);
    ntt64_inverse(result, layer);
}

/**
 * Convolve two high-dimensional vectors block-by-block
 */
static void convolve_highdim(const highdim_vec_t *a, const highdim_vec_t *b,
                             highdim_vec_t *result, int layer) {
    for (int block = 0; block < NUM_BLOCKS; block++) {
        ntt_convolve_block(a->blocks[block], b->blocks[block],
                          result->blocks[block], layer);
    }
}

/**
 * Check if two high-dimensional vectors are equal
 */
static int vectors_equal(const highdim_vec_t *a, const highdim_vec_t *b) {
    for (int block = 0; block < NUM_BLOCKS; block++) {
        for (int i = 0; i < N; i++) {
            if (a->blocks[block][i] != b->blocks[block][i]) {
                return 0;
            }
        }
    }
    return 1;
}

/**
 * Count differences between two vectors
 */
static size_t count_differences(const highdim_vec_t *a, const highdim_vec_t *b) {
    size_t diff = 0;
    for (int block = 0; block < NUM_BLOCKS; block++) {
        for (int i = 0; i < N; i++) {
            if (a->blocks[block][i] != b->blocks[block][i]) {
                diff++;
            }
        }
    }
    return diff;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

static void init_highdim_vector(highdim_vec_t *vec, uint32_t seed, uint32_t mod) {
    uint32_t state = seed;

    for (int block = 0; block < NUM_BLOCKS; block++) {
        for (int i = 0; i < N; i++) {
            state = (state * 1103515245u + 12345u) & 0x7FFFFFFFu;
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
    highdim_vec_t *vectors;
    int num_vectors;
} tree_level_t;

/**
 * Run a complete convolution tree from 64 vectors down to 1
 */
static void run_convolution_tree(tree_level_t levels[7], int ntt_layer, const char *tree_name) {
    printf("  Running %s tree:\n", tree_name);

    for (int level = 0; level < 6; level++) {
        int dst_count = levels[level + 1].num_vectors;

        for (int pair = 0; pair < dst_count; pair++) {
            int idx_a = pair * 2;
            int idx_b = pair * 2 + 1;

            convolve_highdim(&levels[level].vectors[idx_a],
                           &levels[level].vectors[idx_b],
                           &levels[level + 1].vectors[pair],
                           ntt_layer);
        }

        printf("    Level %d → %d: %2d vectors\n",
               level, level + 1, levels[level + 1].num_vectors);
    }
}

// ============================================================================
// MAIN DEMONSTRATION
// ============================================================================

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║           TRIPLE CONVOLUTION TREE ANALYSIS                ║\n");
    printf("║   Testing Associativity & Commutativity Properties       ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    int ntt_layer = 3;
    uint32_t modulus = ntt64_get_modulus(ntt_layer);

    printf("Configuration:\n");
    printf("  Vector dimension:     %d (%d blocks × %d)\n", TOTAL_DIM, NUM_BLOCKS, N);
    printf("  Initial vectors:      %d per tree\n", INITIAL_VECTORS);
    printf("  Number of trees:      3 (X, Y, Z)\n");
    printf("  NTT layer:            %d (modulus = %u)\n", ntt_layer, modulus);
    printf("  Tree depth:           6 levels (64→32→16→8→4→2→1)\n\n");

    // ========================================================================
    // ALLOCATE THREE TREES
    // ========================================================================
    tree_level_t x_tree[7], y_tree[7], z_tree[7];
    int num_vecs[] = {64, 32, 16, 8, 4, 2, 1};

    for (int l = 0; l < 7; l++) {
        x_tree[l].num_vectors = num_vecs[l];
        y_tree[l].num_vectors = num_vecs[l];
        z_tree[l].num_vectors = num_vecs[l];

        x_tree[l].vectors = malloc(num_vecs[l] * sizeof(highdim_vec_t));
        y_tree[l].vectors = malloc(num_vecs[l] * sizeof(highdim_vec_t));
        z_tree[l].vectors = malloc(num_vecs[l] * sizeof(highdim_vec_t));

        if (!x_tree[l].vectors || !y_tree[l].vectors || !z_tree[l].vectors) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }
    }

    // ========================================================================
    // INITIALIZE THREE TREES WITH DIFFERENT SEEDS
    // ========================================================================
    printf("┌───────────────────────────────────────────────────────────┐\n");
    printf("│ INITIALIZATION: Three Independent Trees                  │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    printf("  Initializing X tree (64 vectors, seed=1000)...\n");
    for (int i = 0; i < 64; i++) {
        init_highdim_vector(&x_tree[0].vectors[i], 1000 + i * 123, modulus);
    }

    printf("  Initializing Y tree (64 vectors, seed=2000)...\n");
    for (int i = 0; i < 64; i++) {
        init_highdim_vector(&y_tree[0].vectors[i], 2000 + i * 456, modulus);
    }

    printf("  Initializing Z tree (64 vectors, seed=3000)...\n");
    for (int i = 0; i < 64; i++) {
        init_highdim_vector(&z_tree[0].vectors[i], 3000 + i * 789, modulus);
    }

    printf("  ✓ All trees initialized\n\n");

    // ========================================================================
    // RUN THREE TREES TO COMPLETION
    // ========================================================================
    printf("┌───────────────────────────────────────────────────────────┐\n");
    printf("│ TREE EXECUTION: 64 → 32 → 16 → 8 → 4 → 2 → 1            │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    run_convolution_tree(x_tree, ntt_layer, "X");
    run_convolution_tree(y_tree, ntt_layer, "Y");
    run_convolution_tree(z_tree, ntt_layer, "Z");

    printf("  ✓ All trees completed\n\n");

    // Extract final results
    highdim_vec_t *x_final = &x_tree[6].vectors[0];
    highdim_vec_t *y_final = &y_tree[6].vectors[0];
    highdim_vec_t *z_final = &z_tree[6].vectors[0];

    printf("Final Results:\n");
    printf("  X final: L2=%.2f, density=%zu/%d\n",
           compute_highdim_l2(x_final, modulus),
           count_highdim_nonzero(x_final), TOTAL_DIM);
    printf("  Y final: L2=%.2f, density=%zu/%d\n",
           compute_highdim_l2(y_final, modulus),
           count_highdim_nonzero(y_final), TOTAL_DIM);
    printf("  Z final: L2=%.2f, density=%zu/%d\n\n",
           compute_highdim_l2(z_final, modulus),
           count_highdim_nonzero(z_final), TOTAL_DIM);

    // ========================================================================
    // TEST COMMUTATIVITY: x⊗y = y⊗x
    // ========================================================================
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                  COMMUTATIVITY TEST                       ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    highdim_vec_t xy, yx;

    printf("Computing x⊗y...\n");
    convolve_highdim(x_final, y_final, &xy, ntt_layer);
    printf("  L2 norm: %.2f\n", compute_highdim_l2(&xy, modulus));

    printf("\nComputing y⊗x...\n");
    convolve_highdim(y_final, x_final, &yx, ntt_layer);
    printf("  L2 norm: %.2f\n", compute_highdim_l2(&yx, modulus));

    int comm_equal = vectors_equal(&xy, &yx);
    size_t comm_diff = count_differences(&xy, &yx);

    printf("\nCommutativity (x⊗y = y⊗x): %s\n",
           comm_equal ? "✓ PASS" : "✗ FAIL");
    if (!comm_equal) {
        printf("  Differences: %zu / %d elements (%.2f%%)\n",
               comm_diff, TOTAL_DIM, 100.0 * comm_diff / TOTAL_DIM);
    }

    // ========================================================================
    // TEST ASSOCIATIVITY: (x⊗y)⊗z = x⊗(y⊗z)
    // ========================================================================
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                  ASSOCIATIVITY TEST                       ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    highdim_vec_t xy_z, x_yz, yz;

    printf("Computing (x⊗y)⊗z:\n");
    printf("  Step 1: x⊗y (already computed)\n");
    printf("  Step 2: (x⊗y)⊗z...\n");
    convolve_highdim(&xy, z_final, &xy_z, ntt_layer);
    printf("    L2 norm: %.2f\n", compute_highdim_l2(&xy_z, modulus));

    printf("\nComputing x⊗(y⊗z):\n");
    printf("  Step 1: y⊗z...\n");
    convolve_highdim(y_final, z_final, &yz, ntt_layer);
    printf("    L2 norm: %.2f\n", compute_highdim_l2(&yz, modulus));
    printf("  Step 2: x⊗(y⊗z)...\n");
    convolve_highdim(x_final, &yz, &x_yz, ntt_layer);
    printf("    L2 norm: %.2f\n", compute_highdim_l2(&x_yz, modulus));

    int assoc_equal = vectors_equal(&xy_z, &x_yz);
    size_t assoc_diff = count_differences(&xy_z, &x_yz);

    printf("\nAssociativity ((x⊗y)⊗z = x⊗(y⊗z)): %s\n",
           assoc_equal ? "✓ PASS" : "✗ FAIL");
    if (!assoc_equal) {
        printf("  Differences: %zu / %d elements (%.2f%%)\n",
               assoc_diff, TOTAL_DIM, 100.0 * assoc_diff / TOTAL_DIM);
    }

    // ========================================================================
    // TEST ALL 6 PERMUTATIONS
    // ========================================================================
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║              PERMUTATION INVARIANCE TEST                  ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    printf("Testing all 6 permutations of (x, y, z):\n\n");

    highdim_vec_t perm_results[6];
    const char *perm_names[] = {
        "(x⊗y)⊗z",
        "(x⊗z)⊗y",
        "(y⊗x)⊗z",
        "(y⊗z)⊗x",
        "(z⊗x)⊗y",
        "(z⊗y)⊗x"
    };

    // Already computed: (x⊗y)⊗z
    memcpy(&perm_results[0], &xy_z, sizeof(highdim_vec_t));

    // Compute (x⊗z)⊗y
    highdim_vec_t xz;
    convolve_highdim(x_final, z_final, &xz, ntt_layer);
    convolve_highdim(&xz, y_final, &perm_results[1], ntt_layer);

    // (y⊗x)⊗z = (x⊗y)⊗z (already have yx)
    convolve_highdim(&yx, z_final, &perm_results[2], ntt_layer);

    // (y⊗z)⊗x (already have yz)
    convolve_highdim(&yz, x_final, &perm_results[3], ntt_layer);

    // (z⊗x)⊗y
    highdim_vec_t zx;
    convolve_highdim(z_final, x_final, &zx, ntt_layer);
    convolve_highdim(&zx, y_final, &perm_results[4], ntt_layer);

    // (z⊗y)⊗x
    highdim_vec_t zy;
    convolve_highdim(z_final, y_final, &zy, ntt_layer);
    convolve_highdim(&zy, x_final, &perm_results[5], ntt_layer);

    // Print all results
    for (int i = 0; i < 6; i++) {
        printf("  %s: L2=%.2f\n",
               perm_names[i],
               compute_highdim_l2(&perm_results[i], modulus));
    }

    // Check if all permutations give same result
    printf("\nCross-checking all permutations:\n");
    int all_equal = 1;
    for (int i = 1; i < 6; i++) {
        int eq = vectors_equal(&perm_results[0], &perm_results[i]);
        size_t diff = count_differences(&perm_results[0], &perm_results[i]);

        printf("  %s vs %s: %s",
               perm_names[0], perm_names[i],
               eq ? "✓ EQUAL" : "✗ DIFFER");

        if (!eq) {
            printf(" (%zu diffs)", diff);
            all_equal = 0;
        }
        printf("\n");
    }

    printf("\n%s: All permutations %s\n",
           all_equal ? "✓ PASS" : "✗ FAIL",
           all_equal ? "produce identical results" : "differ");

    // ========================================================================
    // SUMMARY
    // ========================================================================
    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                      SUMMARY                              ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    printf("Algebraic Properties:\n");
    printf("  ✓ Commutativity:           %s\n", comm_equal ? "HOLDS" : "FAILS");
    printf("  ✓ Associativity:           %s\n", assoc_equal ? "HOLDS" : "FAILS");
    printf("  ✓ Permutation invariance:  %s\n\n", all_equal ? "HOLDS" : "FAILS");

    printf("Tree Operations:\n");
    printf("  • Trees processed:         3 (X, Y, Z)\n");
    printf("  • Convolutions per tree:   63 (sum of 32+16+8+4+2+1)\n");
    printf("  • Total convolutions:      189 + 11 (cross-tree)\n");
    printf("  • Final dimension:         %d\n\n", TOTAL_DIM);

    if (comm_equal && assoc_equal && all_equal) {
        printf("✓ NTT convolution forms a COMMUTATIVE, ASSOCIATIVE algebra\n");
        printf("✓ Results are INDEPENDENT of evaluation order\n");
        printf("✓ All algebraic properties VERIFIED\n");
    } else {
        printf("⚠ Some algebraic properties do not hold\n");
    }

    printf("\n");

    // Cleanup
    for (int l = 0; l < 7; l++) {
        free(x_tree[l].vectors);
        free(y_tree[l].vectors);
        free(z_tree[l].vectors);
    }

    return 0;
}
