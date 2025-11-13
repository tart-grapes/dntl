#include "rs_params.h"
#include "rs_mats.h"
#include "rs_lwr.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

// ============================================================================
// TEST UTILITIES
// ============================================================================

// Fixed test seeds for determinism
static const uint8_t test_seed_ax[32] = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
    0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};

static const uint8_t test_seed_ay[32] = {
    0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA, 0x99, 0x88,
    0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11, 0x00,
    0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
    0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01
};

static const uint8_t test_seed_orb_x[32] = {
    0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00, 0x11,
    0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
    0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22,
    0x11, 0x00, 0xFF, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA
};

static const uint8_t test_seed_orb_y[32] = {
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0,
    0x0F, 0xED, 0xCB, 0xA9, 0x87, 0x65, 0x43, 0x21,
    0xF0, 0xDE, 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12,
    0x21, 0x43, 0x65, 0x87, 0xA9, 0xCB, 0xED, 0x0F
};

static const uint8_t test_seed_B[32] = {
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42,
    0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42
};

static const uint8_t test_seed_C[32] = {
    0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99,
    0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99
};

// Timing helper
static double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

// ============================================================================
// TEST 1: DETERMINISM
// ============================================================================

void test_determinism() {
    printf("=== TEST 1: Determinism ===\n");

    rs_params_t params;
    rs_params_init(&params,
                   test_seed_ax, test_seed_ay,
                   test_seed_orb_x, test_seed_orb_y,
                   test_seed_B, test_seed_C);

    // Test A matrix determinism
    rs_matrix_t A1, A2;
    rs_derive_A(&params, RS_FAMILY_AX, 2, 1, &A1);
    rs_derive_A(&params, RS_FAMILY_AX, 2, 1, &A2);

    int a_match = 1;
    for (int i = 0; i < RS_N && a_match; i++) {
        for (int j = 0; j < RS_N; j++) {
            if (A1.data[i][j] != A2.data[i][j]) {
                a_match = 0;
                break;
            }
        }
    }

    printf("  A matrix determinism: %s\n", a_match ? "PASS" : "FAIL");

    // Test B row determinism
    rs_row_t B1, B2;
    rs_derive_B_row(&params, 5, RS_FLAVOR_LWR, &B1);
    rs_derive_B_row(&params, 5, RS_FLAVOR_LWR, &B2);

    int b_match = 1;
    for (int j = 0; j < RS_SECRET_DIM; j++) {
        if (B1.data[j] != B2.data[j]) {
            b_match = 0;
            break;
        }
    }

    printf("  B row determinism: %s\n", b_match ? "PASS" : "FAIL");

    // Test C row determinism
    rs_row_t C1, C2;
    rs_derive_C_row(&params, 3, &C1);
    rs_derive_C_row(&params, 3, &C2);

    int c_match = 1;
    for (int j = 0; j < RS_SECRET_DIM; j++) {
        if (C1.data[j] != C2.data[j]) {
            c_match = 0;
            break;
        }
    }

    printf("  C row determinism: %s\n\n", c_match ? "PASS" : "FAIL");
}

// ============================================================================
// TEST 2: DOMAIN SEPARATION
// ============================================================================

void test_domain_separation() {
    printf("=== TEST 2: Domain Separation ===\n");

    rs_params_t params;
    rs_params_init(&params,
                   test_seed_ax, test_seed_ay,
                   test_seed_orb_x, test_seed_orb_y,
                   test_seed_B, test_seed_C);

    // Test different slots give different matrices
    rs_matrix_t A_slot0, A_slot1;
    rs_derive_A(&params, RS_FAMILY_AX, 1, 0, &A_slot0);
    rs_derive_A(&params, RS_FAMILY_AX, 1, 1, &A_slot1);

    int slots_differ = 0;
    for (int i = 0; i < RS_N && !slots_differ; i++) {
        for (int j = 0; j < RS_N; j++) {
            if (A_slot0.data[i][j] != A_slot1.data[i][j]) {
                slots_differ = 1;
                break;
            }
        }
    }

    printf("  Different slots → different A: %s\n", slots_differ ? "PASS" : "FAIL");

    // Test different families give different matrices
    rs_matrix_t A_ax, A_ay;
    rs_derive_A(&params, RS_FAMILY_AX, 1, 0, &A_ax);
    rs_derive_A(&params, RS_FAMILY_AY, 1, 0, &A_ay);

    int families_differ = 0;
    for (int i = 0; i < RS_N && !families_differ; i++) {
        for (int j = 0; j < RS_N; j++) {
            if (A_ax.data[i][j] != A_ay.data[i][j]) {
                families_differ = 1;
                break;
            }
        }
    }

    printf("  Different families → different A: %s\n", families_differ ? "PASS" : "FAIL");

    // Test different B flavors give different rows
    rs_row_t B_lwr, B_tagged;
    rs_derive_B_row(&params, 0, RS_FLAVOR_LWR, &B_lwr);
    rs_derive_B_row(&params, 0, RS_FLAVOR_TAGGED, &B_tagged);

    int flavors_differ = 0;
    for (int j = 0; j < RS_SECRET_DIM; j++) {
        if (B_lwr.data[j] != B_tagged.data[j]) {
            flavors_differ = 1;
            break;
        }
    }

    printf("  Different flavors → different B: %s\n\n", flavors_differ ? "PASS" : "FAIL");
}

// ============================================================================
// TEST 3: DISTRIBUTION SANITY
// ============================================================================

void test_distribution() {
    printf("=== TEST 3: Distribution Sanity ===\n");

    rs_params_t params;
    rs_params_init(&params,
                   test_seed_ax, test_seed_ay,
                   test_seed_orb_x, test_seed_orb_y,
                   test_seed_B, test_seed_C);

    // Generate several matrices and check bit distribution
    int layer = 1;  // q = 3329
    int count_even = 0, count_odd = 0;
    int total_elements = 0;

    for (int slot = 0; slot < RS_SLOT_COUNT; slot++) {
        rs_matrix_t A;
        rs_derive_A(&params, RS_FAMILY_AX, layer, slot, &A);

        for (int i = 0; i < RS_N; i++) {
            for (int j = 0; j < RS_N; j++) {
                uint32_t val = A.data[i][j];
                if (val % 2 == 0) count_even++;
                else count_odd++;
                total_elements++;
            }
        }
    }

    double even_ratio = (double)count_even / total_elements;
    printf("  Even/Odd distribution (layer %d):\n", layer);
    printf("    Even: %d (%.2f%%)\n", count_even, even_ratio * 100);
    printf("    Odd:  %d (%.2f%%)\n", count_odd, (1.0 - even_ratio) * 100);
    printf("    Close to 50/50: %s\n\n",
           (even_ratio > 0.48 && even_ratio < 0.52) ? "PASS" : "WARN");
}

// ============================================================================
// TEST 4: PERFORMANCE BENCHMARK
// ============================================================================

void benchmark_performance() {
    printf("=== TEST 4: Performance Benchmark ===\n");

    rs_params_t params;
    rs_params_init(&params,
                   test_seed_ax, test_seed_ay,
                   test_seed_orb_x, test_seed_orb_y,
                   test_seed_B, test_seed_C);

    // Benchmark A matrix generation
    printf("  Generating all A matrices...\n");
    double start_time = get_time_ms();

    for (int family = 0; family < 4; family++) {
        for (int ell = 0; ell < RS_NUM_LAYERS; ell++) {
            for (int slot = 0; slot < RS_SLOT_COUNT; slot++) {
                rs_matrix_t A;
                rs_derive_A(&params, (rs_family_t)family, ell, slot, &A);
            }
        }
    }

    double end_time = get_time_ms();
    double total_ms = end_time - start_time;
    int total_matrices = 4 * RS_NUM_LAYERS * RS_SLOT_COUNT;
    double ms_per_matrix = total_ms / total_matrices;

    printf("    Total time: %.2f ms\n", total_ms);
    printf("    Per matrix: %.3f ms\n", ms_per_matrix);
    printf("    Throughput: %.0f matrices/sec\n\n", 1000.0 / ms_per_matrix);

    // Benchmark B row generation
    printf("  Generating %d B rows...\n", RS_PUBLIC_DIM);
    start_time = get_time_ms();

    rs_row_t *B_rows = malloc(sizeof(rs_row_t) * RS_PUBLIC_DIM);
    for (int i = 0; i < RS_PUBLIC_DIM; i++) {
        rs_derive_B_row(&params, i, RS_FLAVOR_LWR, &B_rows[i]);
    }

    end_time = get_time_ms();
    total_ms = end_time - start_time;
    double ns_per_row = (total_ms * 1000000.0) / RS_PUBLIC_DIM;

    printf("    Total time: %.2f ms\n", total_ms);
    printf("    Per row: %.0f ns\n\n", ns_per_row);

    // Benchmark LWR tag computation
    printf("  Computing LWR tags...\n");

    // Generate secret
    int32_t *s = malloc(sizeof(int32_t) * RS_SECRET_DIM);
    rs_generate_secret(s, test_seed_ax);

    // Warm up
    uint16_t t[RS_PUBLIC_DIM];
    rs_lwr_tag(B_rows, s, t);

    // Benchmark
    int num_iterations = 1000;
    start_time = get_time_ms();

    for (int iter = 0; iter < num_iterations; iter++) {
        rs_lwr_tag(B_rows, s, t);
    }

    end_time = get_time_ms();
    total_ms = end_time - start_time;
    double ns_per_tag = (total_ms * 1000000.0) / num_iterations;

    printf("    Iterations: %d\n", num_iterations);
    printf("    Total time: %.2f ms\n", total_ms);
    printf("    Per LWR tag: %.0f ns\n", ns_per_tag);
    printf("    Throughput: %.0f tags/sec\n\n", 1000000000.0 / ns_per_tag);

    // Print sample tag for sanity
    printf("  Sample tag (first 8 values): ");
    for (int i = 0; i < 8; i++) {
        printf("%u ", t[i]);
    }
    printf("\n\n");

    free(B_rows);
    free(s);
}

// ============================================================================
// MAIN
// ============================================================================

int main() {
    printf("========================================\n");
    printf("Ring-Switching System Test Suite\n");
    printf("========================================\n");
    printf("Configuration:\n");
    printf("  N = %d\n", RS_N);
    printf("  Layers = %d\n", RS_NUM_LAYERS);
    printf("  Slots = %d\n", RS_SLOT_COUNT);
    printf("  Public dim = %d\n", RS_PUBLIC_DIM);
    printf("  Secret dim = %d\n", RS_SECRET_DIM);
    printf("  LWR shift = %d\n", RS_LWR_SHIFT);
    printf("  LWR modulus p = %u\n\n", RS_P_SMALL);

    test_determinism();
    test_domain_separation();
    test_distribution();
    benchmark_performance();

    printf("========================================\n");
    printf("All tests complete!\n");
    printf("========================================\n");

    return 0;
}
