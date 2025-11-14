#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "ntt64.h"
#include "ntt64_simd.h"

// Simple PRNG for testing
static uint64_t xorshift64_state = 123456789;

uint32_t rand32(void) {
    xorshift64_state ^= xorshift64_state << 13;
    xorshift64_state ^= xorshift64_state >> 7;
    xorshift64_state ^= xorshift64_state << 17;
    return (uint32_t)xorshift64_state;
}

void random_poly(uint32_t poly[NTT_N], uint32_t q) {
    for (int i = 0; i < NTT_N; i++) {
        poly[i] = rand32() % q;
    }
}

// Test that SIMD and scalar implementations produce the same results
int test_simd_correctness(const char* impl_name,
                           ntt64_forward_fn forward_fn,
                           ntt64_inverse_fn inverse_fn,
                           ntt64_pointwise_mul_fn pointwise_fn,
                           int layer) {
    uint32_t q = ntt64_get_modulus(layer);
    printf("Testing %s implementation for layer %d (q = %u):\n", impl_name, layer, q);

    int all_passed = 1;

    // Test 1: Forward NTT
    printf("  [1/4] Testing forward NTT... ");
    uint32_t poly_scalar[NTT_N], poly_simd[NTT_N];
    random_poly(poly_scalar, q);
    memcpy(poly_simd, poly_scalar, sizeof(poly_scalar));

    ntt64_forward_scalar(poly_scalar, layer);
    forward_fn(poly_simd, layer);

    int passed = 1;
    for (int i = 0; i < NTT_N; i++) {
        if (poly_scalar[i] != poly_simd[i]) {
            printf("FAILED at index %d: scalar=%u, simd=%u\n", i, poly_scalar[i], poly_simd[i]);
            passed = 0;
            all_passed = 0;
            break;
        }
    }
    if (passed) printf("PASSED\n");

    // Test 2: Inverse NTT
    printf("  [2/4] Testing inverse NTT... ");
    random_poly(poly_scalar, q);
    memcpy(poly_simd, poly_scalar, sizeof(poly_scalar));

    ntt64_inverse_scalar(poly_scalar, layer);
    inverse_fn(poly_simd, layer);

    passed = 1;
    for (int i = 0; i < NTT_N; i++) {
        if (poly_scalar[i] != poly_simd[i]) {
            printf("FAILED at index %d: scalar=%u, simd=%u\n", i, poly_scalar[i], poly_simd[i]);
            passed = 0;
            all_passed = 0;
            break;
        }
    }
    if (passed) printf("PASSED\n");

    // Test 3: Forward -> Inverse = identity
    printf("  [3/4] Testing NTT -> INTT = identity... ");
    random_poly(poly_simd, q);
    uint32_t poly_copy[NTT_N];
    memcpy(poly_copy, poly_simd, sizeof(poly_simd));

    forward_fn(poly_simd, layer);
    inverse_fn(poly_simd, layer);

    passed = 1;
    for (int i = 0; i < NTT_N; i++) {
        if (poly_simd[i] != poly_copy[i]) {
            printf("FAILED at index %d: %u != %u\n", i, poly_simd[i], poly_copy[i]);
            passed = 0;
            all_passed = 0;
            break;
        }
    }
    if (passed) printf("PASSED\n");

    // Test 4: Pointwise multiplication
    printf("  [4/4] Testing pointwise multiplication... ");
    uint32_t a_scalar[NTT_N], b_scalar[NTT_N], result_scalar[NTT_N];
    uint32_t a_simd[NTT_N], b_simd[NTT_N], result_simd[NTT_N];

    random_poly(a_scalar, q);
    random_poly(b_scalar, q);
    memcpy(a_simd, a_scalar, sizeof(a_scalar));
    memcpy(b_simd, b_scalar, sizeof(b_scalar));

    ntt64_pointwise_mul_scalar(result_scalar, a_scalar, b_scalar, layer);
    pointwise_fn(result_simd, a_simd, b_simd, layer);

    passed = 1;
    for (int i = 0; i < NTT_N; i++) {
        if (result_scalar[i] != result_simd[i]) {
            printf("FAILED at index %d: scalar=%u, simd=%u\n", i, result_scalar[i], result_simd[i]);
            passed = 0;
            all_passed = 0;
            break;
        }
    }
    if (passed) printf("PASSED\n");

    printf("\n");
    return all_passed;
}

// Benchmark performance
void benchmark_implementation(const char* impl_name,
                               ntt64_forward_fn forward_fn,
                               ntt64_inverse_fn inverse_fn,
                               int layer,
                               int iterations) {
    uint32_t q = ntt64_get_modulus(layer);
    uint32_t poly[NTT_N];
    random_poly(poly, q);

    // Benchmark forward NTT
    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        forward_fn(poly, layer);
    }
    clock_t end = clock();
    double forward_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000000.0 / iterations;

    // Benchmark inverse NTT
    start = clock();
    for (int i = 0; i < iterations; i++) {
        inverse_fn(poly, layer);
    }
    end = clock();
    double inverse_time = ((double)(end - start)) / CLOCKS_PER_SEC * 1000000.0 / iterations;

    printf("  %-10s: forward=%.2f µs, inverse=%.2f µs\n",
           impl_name, forward_time, inverse_time);
}

int main(void) {
    printf("========================================\n");
    printf("NTT64 SIMD Implementation Test Suite\n");
    printf("========================================\n\n");

    // Detect CPU features
    int features = ntt64_detect_cpu_features();
    printf("CPU Features: ");
    if (features & NTT_CPU_AVX2) printf("AVX2 ");
    if (features & NTT_CPU_NEON) printf("NEON ");
    printf("SCALAR\n\n");

    // Test all layers with all available implementations
    int all_tests_passed = 1;

    printf("CORRECTNESS TESTS\n");
    printf("========================================\n\n");

    for (int layer = 0; layer < NTT_NUM_LAYERS; layer++) {
        // Always test scalar
        if (!test_simd_correctness("scalar",
                                    ntt64_forward_scalar,
                                    ntt64_inverse_scalar,
                                    ntt64_pointwise_mul_scalar,
                                    layer)) {
            all_tests_passed = 0;
        }

        #ifdef __AVX2__
        if (features & NTT_CPU_AVX2) {
            if (!test_simd_correctness("AVX2",
                                        ntt64_forward_avx2,
                                        ntt64_inverse_avx2,
                                        ntt64_pointwise_mul_avx2,
                                        layer)) {
                all_tests_passed = 0;
            }
        }
        #endif

        #ifdef __ARM_NEON
        if (features & NTT_CPU_NEON) {
            if (!test_simd_correctness("NEON",
                                        ntt64_forward_neon,
                                        ntt64_inverse_neon,
                                        ntt64_pointwise_mul_neon,
                                        layer)) {
                all_tests_passed = 0;
            }
        }
        #endif
    }

    printf("========================================\n");
    if (all_tests_passed) {
        printf("ALL CORRECTNESS TESTS PASSED ✓\n");
    } else {
        printf("SOME CORRECTNESS TESTS FAILED ✗\n");
        return 1;
    }
    printf("========================================\n\n");

    // Benchmark performance
    printf("PERFORMANCE BENCHMARKS\n");
    printf("========================================\n");
    const int bench_iterations = 10000;

    for (int layer = 0; layer < NTT_NUM_LAYERS; layer++) {
        uint32_t q = ntt64_get_modulus(layer);
        printf("\nLayer %d (q = %u):\n", layer, q);

        benchmark_implementation("scalar",
                                 ntt64_forward_scalar,
                                 ntt64_inverse_scalar,
                                 layer, bench_iterations);

        #ifdef __AVX2__
        if (features & NTT_CPU_AVX2) {
            benchmark_implementation("AVX2",
                                     ntt64_forward_avx2,
                                     ntt64_inverse_avx2,
                                     layer, bench_iterations);
        }
        #endif

        #ifdef __ARM_NEON
        if (features & NTT_CPU_NEON) {
            benchmark_implementation("NEON",
                                     ntt64_forward_neon,
                                     ntt64_inverse_neon,
                                     layer, bench_iterations);
        }
        #endif
    }

    printf("\n========================================\n");
    printf("BENCHMARK COMPLETE\n");
    printf("========================================\n");

    return 0;
}
