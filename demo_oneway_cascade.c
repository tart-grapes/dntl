/**
 * One-Way Dimensional Cascade
 *
 * Demonstrates multi-level one-way projections from highest to lowest dimensions:
 * - Level 0: 16384D (conceptual: 64 blocks × 256D each)
 * - Level 1: 256D secret (small coefficients in {-3,...,+3})
 * - Level 2: 64D public tag (via LWR projection)
 * - Level 3: Individual modulus reductions through NTT layers
 *
 * Tracks the cascade of information loss and dimensional compression.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "rs_params.h"
#include "rs_mats.h"
#include "rs_lwr.h"
#include "ntt64.h"

// ============================================================================
// STATISTICS HELPERS
// ============================================================================

static double compute_l2_int32(const int32_t *vec, size_t len) {
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) {
        sum += (double)vec[i] * (double)vec[i];
    }
    return sqrt(sum);
}

static double compute_l2_uint16(const uint16_t *vec, size_t len, uint16_t mod) {
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) {
        int32_t c = (int32_t)vec[i];
        if (c > mod/2) c -= mod;
        sum += (double)c * c;
    }
    return sqrt(sum);
}

static double compute_l2_uint32(const uint32_t *vec, size_t len, uint32_t mod) {
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) {
        int64_t c = (int64_t)vec[i];
        if (c > mod/2) c -= mod;
        sum += (double)c * c;
    }
    return sqrt(sum);
}

static size_t count_nonzero_int32(const int32_t *vec, size_t len) {
    size_t cnt = 0;
    for (size_t i = 0; i < len; i++) if (vec[i] != 0) cnt++;
    return cnt;
}

static size_t count_nonzero_uint16(const uint16_t *vec, size_t len) {
    size_t cnt = 0;
    for (size_t i = 0; i < len; i++) if (vec[i] != 0) cnt++;
    return cnt;
}

static size_t count_nonzero_uint32(const uint32_t *vec, size_t len) {
    size_t cnt = 0;
    for (size_t i = 0; i < len; i++) if (vec[i] != 0) cnt++;
    return cnt;
}

static double compute_entropy_int32(const int32_t *vec, size_t len) {
    int hist[7] = {0};
    for (size_t i = 0; i < len; i++) {
        int idx = vec[i] + 3;
        if (idx >= 0 && idx < 7) hist[idx]++;
    }
    double ent = 0.0;
    for (int i = 0; i < 7; i++) {
        if (hist[i] > 0) {
            double p = (double)hist[i] / len;
            ent -= p * log2(p);
        }
    }
    return ent;
}

// ============================================================================
// HIGH DIMENSIONAL STRUCTURE (16384D)
// ============================================================================

#define NUM_BLOCKS 64
#define TOTAL_DIM (NUM_BLOCKS * RS_SECRET_DIM)  // 64 × 256 = 16384

typedef struct {
    int32_t blocks[NUM_BLOCKS][RS_SECRET_DIM];
} high_dim_vec_t;

static void init_high_dim_vector(high_dim_vec_t *vec, uint8_t base_seed[32]) {
    // Create 16384D vector as 64 blocks of 256D vectors
    // Each block gets a different seed derived from base_seed
    for (int block = 0; block < NUM_BLOCKS; block++) {
        uint8_t block_seed[32];
        memcpy(block_seed, base_seed, 32);
        block_seed[0] ^= (uint8_t)block;
        block_seed[1] ^= (uint8_t)(block >> 8);

        rs_generate_secret(vec->blocks[block], block_seed);
    }
}

static double compute_high_dim_l2(const high_dim_vec_t *vec) {
    double sum = 0.0;
    for (int b = 0; b < NUM_BLOCKS; b++) {
        for (int i = 0; i < RS_SECRET_DIM; i++) {
            double v = (double)vec->blocks[b][i];
            sum += v * v;
        }
    }
    return sqrt(sum);
}

static size_t count_high_dim_nonzero(const high_dim_vec_t *vec) {
    size_t cnt = 0;
    for (int b = 0; b < NUM_BLOCKS; b++) {
        for (int i = 0; i < RS_SECRET_DIM; i++) {
            if (vec->blocks[b][i] != 0) cnt++;
        }
    }
    return cnt;
}

// ============================================================================
// MAIN DEMONSTRATION
// ============================================================================

int main(void) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║      ONE-WAY DIMENSIONAL CASCADE DEMONSTRATION            ║\n");
    printf("║   Tracking Non-Reversible Projection Through Layers      ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    // Initialize parameters
    uint8_t seeds[6][32];
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 32; j++) {
            seeds[i][j] = (uint8_t)(i * 37 + j * 13);
        }
    }

    rs_params_t params;
    rs_params_init(&params, seeds[0], seeds[1], seeds[2], seeds[3], seeds[4], seeds[5]);

    // ========================================================================
    // LEVEL 0: 16384-DIMENSIONAL SPACE (Highest Level)
    // ========================================================================
    printf("┌───────────────────────────────────────────────────────────┐\n");
    printf("│ LEVEL 0: HIGHEST DIMENSION - 16384D                      │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    high_dim_vec_t level0;
    init_high_dim_vector(&level0, seeds[0]);

    double l0_l2 = compute_high_dim_l2(&level0);
    size_t l0_nz = count_high_dim_nonzero(&level0);
    double l0_entropy = compute_entropy_int32(level0.blocks[0], RS_SECRET_DIM); // Sample from first block

    printf("  Structure:    %d blocks × %d dims = %d total dimensions\n",
           NUM_BLOCKS, RS_SECRET_DIM, TOTAL_DIM);
    printf("  Value range:  {-3, -2, -1, 0, +1, +2, +3}\n");
    printf("  L2 norm:      %.2f\n", l0_l2);
    printf("  Density:      %zu / %d (%.1f%%)\n",
           l0_nz, TOTAL_DIM, 100.0 * l0_nz / TOTAL_DIM);
    printf("  Entropy:      %.2f bits/element (sampled)\n", l0_entropy);
    printf("  Total info:   ~%.0f bits\n", l0_entropy * TOTAL_DIM);

    printf("\n  ⚠  This represents the FULL high-dimensional secret space\n");
    printf("  ⚠  Must be protected and never revealed\n\n");

    // ========================================================================
    // PROJECTION 0→1: Aggregation to single 256D secret
    // ========================================================================
    printf("┌───────────────────────────────────────────────────────────┐\n");
    printf("│ PROJECTION 0→1: Aggregation/Combination                  │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    printf("  Method:       Combine blocks via addition (mod small bound)\n");
    printf("  Direction:    16384D → 256D  (%.1fx compression)\n",
           (double)TOTAL_DIM / RS_SECRET_DIM);
    printf("  Properties:   Lossy - loses block-level structure\n");
    printf("  Information:  %.0f bits → ~%.0f bits\n",
           l0_entropy * TOTAL_DIM, l0_entropy * RS_SECRET_DIM);

    // Combine blocks by addition (simulating aggregation)
    int32_t level1[RS_SECRET_DIM] = {0};
    for (int b = 0; b < NUM_BLOCKS; b++) {
        for (int i = 0; i < RS_SECRET_DIM; i++) {
            level1[i] += level0.blocks[b][i];
        }
    }

    // Keep in small range by taking mod 7 - 3
    for (int i = 0; i < RS_SECRET_DIM; i++) {
        level1[i] = ((level1[i] % 7) + 7) % 7;
        if (level1[i] > 3) level1[i] -= 7;
    }

    printf("\n  ✓ Projection complete\n");
    printf("  ⚠ Block structure LOST - cannot recover individual blocks\n\n");

    // ========================================================================
    // LEVEL 1: 256-DIMENSIONAL SECRET
    // ========================================================================
    printf("┌───────────────────────────────────────────────────────────┐\n");
    printf("│ LEVEL 1: SECRET SPACE - 256D                             │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    double l1_l2 = compute_l2_int32(level1, RS_SECRET_DIM);
    size_t l1_nz = count_nonzero_int32(level1, RS_SECRET_DIM);
    double l1_entropy = compute_entropy_int32(level1, RS_SECRET_DIM);

    printf("  Dimensions:   %d\n", RS_SECRET_DIM);
    printf("  Value range:  {-3, -2, -1, 0, +1, +2, +3}\n");
    printf("  L2 norm:      %.2f\n", l1_l2);
    printf("  L∞ norm:      3 (by construction)\n");
    printf("  Density:      %zu / %d (%.1f%%)\n",
           l1_nz, RS_SECRET_DIM, 100.0 * l1_nz / RS_SECRET_DIM);
    printf("  Entropy:      %.2f bits/element\n", l1_entropy);
    printf("  Total info:   ~%.0f bits\n", l1_entropy * RS_SECRET_DIM);
    printf("\n  Sample:       [%d, %d, %d, %d, %d, ..., %d, %d, %d]\n",
           level1[0], level1[1], level1[2], level1[3], level1[4],
           level1[RS_SECRET_DIM-3], level1[RS_SECRET_DIM-2], level1[RS_SECRET_DIM-1]);

    printf("\n  ⚠  This is the primary secret in the cryptosystem\n");
    printf("  ⚠  Must derive public key WITHOUT revealing this\n\n");

    // ========================================================================
    // PROJECTION 1→2: LWR One-Way Function (THE KEY STEP)
    // ========================================================================
    printf("┌───────────────────────────────────────────────────────────┐\n");
    printf("│ PROJECTION 1→2: LWR ONE-WAY FUNCTION ★                   │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    printf("  Method:       t = ⌊(B · s) / 2^%d⌋ mod %u\n", RS_LWR_SHIFT, RS_P_SMALL);
    printf("  Matrix B:     %d × %d over Z_{2^32}\n", RS_PUBLIC_DIM, RS_SECRET_DIM);
    printf("  Direction:    256D → 64D  (%.1fx compression)\n",
           (double)RS_SECRET_DIM / RS_PUBLIC_DIM);
    printf("  Truncation:   Lower %d bits discarded per element\n", RS_LWR_SHIFT);
    printf("\n  Computing B · s...\n");

    // Generate B matrix rows
    rs_row_t *B_rows = malloc(RS_PUBLIC_DIM * sizeof(rs_row_t));
    if (!B_rows) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    for (int i = 0; i < RS_PUBLIC_DIM; i++) {
        rs_derive_B_row(&params, i, RS_FLAVOR_LWR, &B_rows[i]);
    }

    // Compute LWR tag
    uint16_t level2[RS_PUBLIC_DIM];
    rs_lwr_tag(B_rows, level1, level2);

    printf("  ✓ Matrix multiplication complete\n");
    printf("  ✓ Truncation applied (rounding)\n");
    printf("  ✓ Modular reduction complete\n\n");

    printf("  INFORMATION LOSS ANALYSIS:\n");
    printf("    Dimension loss:     %d dims lost (%.1f%%)\n",
           RS_SECRET_DIM - RS_PUBLIC_DIM,
           100.0 * (RS_SECRET_DIM - RS_PUBLIC_DIM) / RS_SECRET_DIM);
    printf("    Truncation loss:    %d bits × %d elements = %d bits\n",
           RS_LWR_SHIFT, RS_PUBLIC_DIM, RS_LWR_SHIFT * RS_PUBLIC_DIM);
    printf("    Total bits lost:    ~%d bits\n",
           (int)(l1_entropy * RS_SECRET_DIM - log2(RS_P_SMALL) * RS_PUBLIC_DIM));

    printf("\n  ⚠  ONE-WAY: Cannot recover secret from public tag\n");
    printf("  ⚠  Security based on LWR hardness assumption\n\n");

    free(B_rows);

    // ========================================================================
    // LEVEL 2: 64-DIMENSIONAL PUBLIC SPACE
    // ========================================================================
    printf("┌───────────────────────────────────────────────────────────┐\n");
    printf("│ LEVEL 2: PUBLIC SPACE - 64D                              │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    double l2_l2 = compute_l2_uint16(level2, RS_PUBLIC_DIM, RS_P_SMALL);
    size_t l2_nz = count_nonzero_uint16(level2, RS_PUBLIC_DIM);

    printf("  Dimensions:   %d\n", RS_PUBLIC_DIM);
    printf("  Modulus:      %u (~%.1f bits)\n", RS_P_SMALL, log2(RS_P_SMALL));
    printf("  L2 norm:      %.2f\n", l2_l2);
    printf("  Density:      %zu / %d (%.1f%%)\n",
           l2_nz, RS_PUBLIC_DIM, 100.0 * l2_nz / RS_PUBLIC_DIM);
    printf("  Max info:     %.0f bits\n", log2(RS_P_SMALL) * RS_PUBLIC_DIM);

    printf("\n  Sample:       [%u, %u, %u, %u, %u, ..., %u, %u, %u]\n",
           level2[0], level2[1], level2[2], level2[3], level2[4],
           level2[RS_PUBLIC_DIM-3], level2[RS_PUBLIC_DIM-2], level2[RS_PUBLIC_DIM-1]);

    printf("\n  ✓ SAFE TO PUBLISH - does not reveal secret\n");
    printf("  ✓ Used for verification and signature schemes\n\n");

    // ========================================================================
    // LEVEL 3: NTT Domain Projections (Multiple Layers)
    // ========================================================================
    printf("┌───────────────────────────────────────────────────────────┐\n");
    printf("│ LEVEL 3: NTT DOMAIN PROJECTIONS                          │\n");
    printf("└───────────────────────────────────────────────────────────┘\n");

    printf("  The 64D public vector can be projected through 7 NTT layers\n");
    printf("  Each layer uses a different modulus for ring operations\n\n");

    printf("  Layer | Modulus       | L2 Norm (coeff) | L2 Norm (NTT)\n");
    printf("  ─────────────────────────────────────────────────────────\n");

    for (int layer = 0; layer < 7; layer++) {
        uint32_t modulus = ntt64_get_modulus(layer);

        // Reduce public tag to this layer's modulus
        uint32_t layer_vec[RS_PUBLIC_DIM];
        for (int i = 0; i < RS_PUBLIC_DIM; i++) {
            layer_vec[i] = level2[i] % modulus;
        }

        double coeff_l2 = compute_l2_uint32(layer_vec, RS_PUBLIC_DIM, modulus);

        // Apply NTT
        ntt64_forward(layer_vec, layer);
        double ntt_l2 = compute_l2_uint32(layer_vec, RS_PUBLIC_DIM, modulus);

        printf("   %d    | %11u | %15.2f | %15.2f\n",
               layer, modulus, coeff_l2, ntt_l2);
    }

    printf("\n  ✓ Each NTT is reversible (for computations)\n");
    printf("  ✓ But original secret still unrecoverable\n\n");

    // ========================================================================
    // SUMMARY
    // ========================================================================
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                    CASCADE SUMMARY                        ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    printf("  DIMENSIONAL FLOW (Non-Reversible):\n");
    printf("  ─────────────────────────────────\n\n");

    printf("    16384D ───[aggregate]──→ 256D ───[LWR]──→ 64D ───[NTT]──→ 64D\n");
    printf("    (Level 0)               (Level 1)        (Level 2)      (Level 3)\n\n");

    printf("    Blocks of secrets       Single secret    Public tag     NTT domain\n");
    printf("    Small values            Small values     Mod 12289      Mod qᵢ\n");
    printf("    ~%.0f bits            ~%.0f bits      ~%.0f bits   ~%.0f bits\n\n",
           l0_entropy * TOTAL_DIM,
           l1_entropy * RS_SECRET_DIM,
           log2(RS_P_SMALL) * RS_PUBLIC_DIM,
           log2(RS_P_SMALL) * RS_PUBLIC_DIM);

    printf("  KEY PROPERTIES:\n");
    printf("  ─────────────────────────────────\n\n");

    printf("    ✓ Dimension ratio:     %.1fx total compression\n",
           (double)TOTAL_DIM / RS_PUBLIC_DIM);
    printf("    ✓ Information loss:    ~%.0f bits discarded\n",
           l0_entropy * TOTAL_DIM - log2(RS_P_SMALL) * RS_PUBLIC_DIM);
    printf("    ✓ L2 norm change:      %.2f → %.2f → %.2f\n",
           l0_l2, l1_l2, l2_l2);
    printf("    ✓ ONE-WAY:             Cannot invert LWR projection\n");
    printf("    ✓ POST-QUANTUM:        Hard even for quantum computers\n\n");

    printf("  SECURITY BASIS:\n");
    printf("  ─────────────────────────────────\n\n");

    printf("    • Learning With Rounding (LWR) hardness\n");
    printf("    • Short Integer Solution (SIS) problem  \n");
    printf("    • Ring-LWE lattice assumptions\n");
    printf("    • Post-quantum secure\n\n");

    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                  ★ PROJECTION COMPLETE ★                 ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");

    return 0;
}
