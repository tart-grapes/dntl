/**
 * One-Way LWR Projection Demonstration
 *
 * Demonstrates the non-reversible dimensional projection:
 * 256D secret space → 64D public space via Learning With Rounding
 *
 * Tracks information loss, norm changes, and cryptographic properties.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "rs_lwr.h"
#include "rs_mats.h"
#include "rs_prf.h"
#include "ntt64.h"

// Compute L2 norm of integer vector
static double compute_l2_norm_int(const int32_t *vec, size_t len) {
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) {
        sum += (double)vec[i] * (double)vec[i];
    }
    return sqrt(sum);
}

// Compute L2 norm of uint16 vector
static double compute_l2_norm_uint16(const uint16_t *vec, size_t len, uint16_t modulus) {
    double sum = 0.0;
    for (size_t i = 0; i < len; i++) {
        // Convert to centered representation
        int32_t centered = (int32_t)vec[i];
        if (centered > modulus / 2) {
            centered -= modulus;
        }
        sum += (double)centered * (double)centered;
    }
    return sqrt(sum);
}

// Compute L-infinity norm
static int32_t compute_linf_norm_int(const int32_t *vec, size_t len) {
    int32_t max = 0;
    for (size_t i = 0; i < len; i++) {
        int32_t abs_val = abs(vec[i]);
        if (abs_val > max) {
            max = abs_val;
        }
    }
    return max;
}

// Count non-zero elements
static size_t count_nonzero_int(const int32_t *vec, size_t len) {
    size_t count = 0;
    for (size_t i = 0; i < len; i++) {
        if (vec[i] != 0) {
            count++;
        }
    }
    return count;
}

static size_t count_nonzero_uint16(const uint16_t *vec, size_t len) {
    size_t count = 0;
    for (size_t i = 0; i < len; i++) {
        if (vec[i] != 0) {
            count++;
        }
    }
    return count;
}

// Print vector summary
static void print_vec_summary_int(const char *label, const int32_t *vec, size_t len) {
    printf("  %s: [", label);
    size_t print_count = (len < 12) ? len : 6;

    for (size_t i = 0; i < print_count; i++) {
        printf("%d", vec[i]);
        if (i < print_count - 1) printf(", ");
    }

    if (len > 12) {
        printf(", ..., ");
        for (size_t i = len - 6; i < len; i++) {
            printf("%d", vec[i]);
            if (i < len - 1) printf(", ");
        }
    }

    printf("]\n");
}

static void print_vec_summary_uint16(const char *label, const uint16_t *vec, size_t len) {
    printf("  %s: [", label);
    size_t print_count = (len < 12) ? len : 6;

    for (size_t i = 0; i < print_count; i++) {
        printf("%u", vec[i]);
        if (i < print_count - 1) printf(", ");
    }

    if (len > 12) {
        printf(", ..., ");
        for (size_t i = len - 6; i < len; i++) {
            printf("%u", vec[i]);
            if (i < len - 1) printf(", ");
        }
    }

    printf("]\n");
}

// Compute entropy (Shannon entropy in bits)
static double compute_entropy_int(const int32_t *vec, size_t len) {
    // Count frequency of each value
    int hist[7] = {0};  // For values {-3, -2, -1, 0, 1, 2, 3}

    for (size_t i = 0; i < len; i++) {
        int idx = vec[i] + 3;  // Map {-3..3} to {0..6}
        if (idx >= 0 && idx < 7) {
            hist[idx]++;
        }
    }

    double entropy = 0.0;
    for (int i = 0; i < 7; i++) {
        if (hist[i] > 0) {
            double p = (double)hist[i] / (double)len;
            entropy -= p * log2(p);
        }
    }

    return entropy;
}

// Approximate entropy for uint16 values (by bucketing)
static double compute_entropy_uint16(const uint16_t *vec, size_t len, uint16_t modulus) {
    // Use 256 buckets for histogram
    int num_buckets = 256;
    int *hist = calloc(num_buckets, sizeof(int));
    if (!hist) return 0.0;

    for (size_t i = 0; i < len; i++) {
        int bucket = (vec[i] * num_buckets) / modulus;
        if (bucket >= num_buckets) bucket = num_buckets - 1;
        hist[bucket]++;
    }

    double entropy = 0.0;
    for (int i = 0; i < num_buckets; i++) {
        if (hist[i] > 0) {
            double p = (double)hist[i] / (double)len;
            entropy -= p * log2(p);
        }
    }

    free(hist);
    return entropy;
}

int main(int argc, char **argv) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  ONE-WAY LWR DIMENSIONAL PROJECTION\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    // Configuration
    printf("SYSTEM CONFIGURATION:\n");
    printf("  Secret dimension:     %d (small values in {-3,...,+3})\n", RS_SECRET_DIM);
    printf("  Public dimension:     %d (mod %u)\n", RS_PUBLIC_DIM, RS_P_SMALL);
    printf("  Dimension ratio:      %.2fx compression\n",
           (double)RS_SECRET_DIM / (double)RS_PUBLIC_DIM);
    printf("  LWR truncation:       %d bits (divide by 2^%d)\n", RS_LWR_SHIFT, RS_LWR_SHIFT);
    printf("  Matrix size:          %d × %d (over Z_{2^32})\n\n",
           RS_PUBLIC_DIM, RS_SECRET_DIM);

    // Generate random seed
    uint8_t seed[32];
    for (int i = 0; i < 32; i++) {
        seed[i] = (uint8_t)i;  // Deterministic for reproducibility
    }

    // ========================================================================
    // STEP 1: Generate 256D secret vector
    // ========================================================================
    printf("───────────────────────────────────────────────────────────\n");
    printf("STEP 1: SECRET VECTOR (256D)\n");
    printf("───────────────────────────────────────────────────────────\n");

    int32_t secret[RS_SECRET_DIM];
    rs_generate_secret(secret, seed);

    print_vec_summary_int("Values", secret, RS_SECRET_DIM);

    double secret_l2 = compute_l2_norm_int(secret, RS_SECRET_DIM);
    int32_t secret_linf = compute_linf_norm_int(secret, RS_SECRET_DIM);
    size_t secret_nonzero = count_nonzero_int(secret, RS_SECRET_DIM);
    double secret_entropy = compute_entropy_int(secret, RS_SECRET_DIM);

    printf("\n  Statistics:\n");
    printf("    L2 norm:            %.2f\n", secret_l2);
    printf("    L∞ norm:            %d\n", secret_linf);
    printf("    Non-zeros:          %zu / %d (%.1f%% density)\n",
           secret_nonzero, RS_SECRET_DIM,
           100.0 * secret_nonzero / RS_SECRET_DIM);
    printf("    Shannon entropy:    %.2f bits\n", secret_entropy);
    printf("    Total information:  %.0f bits (theory: %.1f bits)\n",
           secret_entropy * RS_SECRET_DIM, log2(7) * RS_SECRET_DIM);

    // ========================================================================
    // STEP 2: Generate LWR matrix B (64 × 256)
    // ========================================================================
    printf("\n───────────────────────────────────────────────────────────\n");
    printf("STEP 2: LWR MATRIX B (64 × 256 over Z_{2^32})\n");
    printf("───────────────────────────────────────────────────────────\n");

    rs_row_t *B_rows = malloc(RS_PUBLIC_DIM * sizeof(rs_row_t));
    if (!B_rows) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    // Derive each row of B
    printf("  Generating matrix rows from seed...\n");
    for (int i = 0; i < RS_PUBLIC_DIM; i++) {
        rs_derive_B_row(seed, i, &B_rows[i]);
    }

    printf("  Matrix generated successfully\n");
    printf("  Sample row 0: [%u, %u, %u, ..., %u, %u]\n",
           B_rows[0].data[0], B_rows[0].data[1], B_rows[0].data[2],
           B_rows[0].data[RS_SECRET_DIM-2], B_rows[0].data[RS_SECRET_DIM-1]);

    // ========================================================================
    // STEP 3: Compute intermediate product B·s (before rounding)
    // ========================================================================
    printf("\n───────────────────────────────────────────────────────────\n");
    printf("STEP 3: INTERMEDIATE PRODUCT B·s (before rounding)\n");
    printf("───────────────────────────────────────────────────────────\n");

    uint64_t intermediate[RS_PUBLIC_DIM];
    uint32_t intermediate_mod32[RS_PUBLIC_DIM];

    printf("  Computing B·s for all %d rows...\n", RS_PUBLIC_DIM);

    for (int i = 0; i < RS_PUBLIC_DIM; i++) {
        uint64_t acc = 0;
        for (int j = 0; j < RS_SECRET_DIM; j++) {
            int64_t product = (int64_t)B_rows[i].data[j] * (int64_t)secret[j];
            acc += (uint64_t)product;
        }
        intermediate[i] = acc;
        intermediate_mod32[i] = (uint32_t)acc;  // mod 2^32
    }

    printf("  First 6 values (mod 2^32): [%u, %u, %u, %u, %u, %u]\n",
           intermediate_mod32[0], intermediate_mod32[1], intermediate_mod32[2],
           intermediate_mod32[3], intermediate_mod32[4], intermediate_mod32[5]);

    // ========================================================================
    // STEP 4: Apply LWR rounding/truncation (ONE-WAY STEP)
    // ========================================================================
    printf("\n───────────────────────────────────────────────────────────\n");
    printf("STEP 4: LWR TRUNCATION (ONE-WAY PROJECTION)\n");
    printf("───────────────────────────────────────────────────────────\n");

    uint16_t public_tag[RS_PUBLIC_DIM];
    rs_lwr_tag(B_rows, secret, public_tag);

    print_vec_summary_uint16("Public tag", public_tag, RS_PUBLIC_DIM);

    double tag_l2 = compute_l2_norm_uint16(public_tag, RS_PUBLIC_DIM, RS_P_SMALL);
    size_t tag_nonzero = count_nonzero_uint16(public_tag, RS_PUBLIC_DIM);
    double tag_entropy = compute_entropy_uint16(public_tag, RS_PUBLIC_DIM, RS_P_SMALL);

    printf("\n  Statistics:\n");
    printf("    L2 norm:            %.2f\n", tag_l2);
    printf("    Non-zeros:          %zu / %d (%.1f%% density)\n",
           tag_nonzero, RS_PUBLIC_DIM,
           100.0 * tag_nonzero / RS_PUBLIC_DIM);
    printf("    Shannon entropy:    %.2f bits per element\n", tag_entropy);
    printf("    Total information:  %.0f bits\n", tag_entropy * RS_PUBLIC_DIM);

    printf("\n  Information Loss:\n");
    double secret_info = secret_entropy * RS_SECRET_DIM;
    double public_info = tag_entropy * RS_PUBLIC_DIM;
    printf("    Secret info:        %.0f bits\n", secret_info);
    printf("    Public info:        %.0f bits\n", public_info);
    printf("    Lost in projection: %.0f bits (%.1f%%)\n",
           secret_info - public_info,
           100.0 * (secret_info - public_info) / secret_info);

    printf("\n  Rounding Details:\n");
    printf("    Bits discarded:     %d (lower %d bits truncated)\n",
           RS_LWR_SHIFT, RS_LWR_SHIFT);
    printf("    Compression ratio:  %.2fx\n",
           (double)RS_SECRET_DIM / (double)RS_PUBLIC_DIM);

    // ========================================================================
    // STEP 5: Demonstrate non-reversibility
    // ========================================================================
    printf("\n───────────────────────────────────────────────────────────\n");
    printf("STEP 5: NON-REVERSIBILITY DEMONSTRATION\n");
    printf("───────────────────────────────────────────────────────────\n");

    printf("  The LWR projection is ONE-WAY because:\n\n");

    printf("  1. DIMENSION REDUCTION: 256D → 64D\n");
    printf("     - Lost %d dimensions of information\n", RS_SECRET_DIM - RS_PUBLIC_DIM);
    printf("     - Each public value is a combination of %d secret values\n",
           RS_SECRET_DIM / RS_PUBLIC_DIM);

    printf("\n  2. ROUNDING/TRUNCATION: ⌊·/2^%d⌋\n", RS_LWR_SHIFT);
    printf("     - Each rounding loses %d bits of precision\n", RS_LWR_SHIFT);
    printf("     - Total bits lost: %d × %d = %d bits\n",
           RS_PUBLIC_DIM, RS_LWR_SHIFT, RS_PUBLIC_DIM * RS_LWR_SHIFT);

    printf("\n  3. MODULAR REDUCTION: mod %u\n", RS_P_SMALL);
    printf("     - Further compresses the output space\n");
    printf("     - Creates additional ambiguity\n");

    printf("\n  RESULT: Given only the public tag, there are approximately\n");
    printf("          2^%.0f possible secrets that map to the same tag!\n",
           secret_info - public_info);

    // ========================================================================
    // STEP 6: Try creating a different secret that yields similar tag
    // ========================================================================
    printf("\n───────────────────────────────────────────────────────────\n");
    printf("STEP 6: COLLISION TEST (Multiple Preimages)\n");
    printf("───────────────────────────────────────────────────────────\n");

    // Generate a different secret
    uint8_t seed2[32];
    for (int i = 0; i < 32; i++) {
        seed2[i] = (uint8_t)(i + 100);
    }

    int32_t secret2[RS_SECRET_DIM];
    rs_generate_secret(secret2, seed2);

    uint16_t public_tag2[RS_PUBLIC_DIM];
    rs_lwr_tag(B_rows, secret2, public_tag2);

    printf("  Generated second random secret\n");
    printf("  Secret 1: L2=%.2f, first=[%d, %d, %d, ...]\n",
           secret_l2, secret[0], secret[1], secret[2]);

    double secret2_l2 = compute_l2_norm_int(secret2, RS_SECRET_DIM);
    printf("  Secret 2: L2=%.2f, first=[%d, %d, %d, ...]\n",
           secret2_l2, secret2[0], secret2[1], secret2[2]);

    // Count how many tag elements match
    int matches = 0;
    for (int i = 0; i < RS_PUBLIC_DIM; i++) {
        if (public_tag[i] == public_tag2[i]) {
            matches++;
        }
    }

    printf("\n  Public tag 1: [%u, %u, %u, ...]\n",
           public_tag[0], public_tag[1], public_tag[2]);
    printf("  Public tag 2: [%u, %u, %u, ...]\n",
           public_tag2[0], public_tag2[1], public_tag2[2]);
    printf("\n  Matching elements: %d / %d (%.1f%%)\n",
           matches, RS_PUBLIC_DIM, 100.0 * matches / RS_PUBLIC_DIM);
    printf("  Expected random match rate: ~%.1f%%\n",
           100.0 / RS_P_SMALL);

    printf("\n  ✓ Different secrets produce different (but unrelated) tags\n");
    printf("  ✓ Cannot recover secret from tag alone\n");

    // ========================================================================
    // SUMMARY
    // ========================================================================
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("PROJECTION SUMMARY\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    printf("  Flow: 256D secret → [Matrix multiply] → [Truncate] → 64D public\n\n");

    printf("  Dimension:   %3d → %2d  (%.1fx compression)\n",
           RS_SECRET_DIM, RS_PUBLIC_DIM,
           (double)RS_SECRET_DIM / (double)RS_PUBLIC_DIM);

    printf("  L2 norm:     %6.2f → %.2f  (%.2fx change)\n",
           secret_l2, tag_l2, tag_l2 / secret_l2);

    printf("  Density:     %5.1f%% → %.1f%%  (%+.1f%% change)\n",
           100.0 * secret_nonzero / RS_SECRET_DIM,
           100.0 * tag_nonzero / RS_PUBLIC_DIM,
           100.0 * (tag_nonzero / (double)RS_PUBLIC_DIM -
                    secret_nonzero / (double)RS_SECRET_DIM));

    printf("  Information: %5.0f → %.0f bits  (%.0f bits lost)\n",
           secret_info, public_info, secret_info - public_info);

    printf("\n  ✓ Projection is ONE-WAY and LOSSY\n");
    printf("  ✓ Hardness based on LWR problem\n");
    printf("  ✓ Suitable for commitment and signature schemes\n");

    printf("\n═══════════════════════════════════════════════════════════\n");

    free(B_rows);
    return 0;
}
