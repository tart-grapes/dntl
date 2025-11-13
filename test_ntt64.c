#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "ntt64.h"

// Simple PRNG for testing
static uint64_t xorshift64_state = 123456789;

uint32_t rand32(void) {
    xorshift64_state ^= xorshift64_state << 13;
    xorshift64_state ^= xorshift64_state >> 7;
    xorshift64_state ^= xorshift64_state << 17;
    return (uint32_t)xorshift64_state;
}

// Generate random polynomial with coefficients in [0, q)
void random_poly(uint32_t poly[NTT_N], uint32_t q) {
    for (int i = 0; i < NTT_N; i++) {
        poly[i] = rand32() % q;
    }
}

// Negacyclic polynomial multiplication (schoolbook, for verification)
// Computes c = a * b mod (x^N + 1) mod q
void schoolbook_negacyclic_mul(uint32_t c[NTT_N],
                                const uint32_t a[NTT_N],
                                const uint32_t b[NTT_N],
                                uint32_t q) {
    // Use 128-bit arithmetic to avoid overflow for large moduli
    memset(c, 0, sizeof(uint32_t) * NTT_N);

    for (int i = 0; i < NTT_N; i++) {
        for (int j = 0; j < NTT_N; j++) {
            int idx = i + j;

            // Compute product
            uint64_t prod = (uint64_t)a[i] * (uint64_t)b[j];

            if (idx < NTT_N) {
                // Normal coefficient: add prod
                uint64_t sum = (uint64_t)c[idx] + prod;
                c[idx] = (uint32_t)(sum % q);
            } else {
                // Coefficient >= N: subtract prod (because x^N = -1)
                idx -= NTT_N;

                // c[idx] -= prod (mod q)
                uint64_t val = (uint64_t)c[idx];
                uint64_t prod_mod = prod % q;

                if (val >= prod_mod) {
                    c[idx] = (uint32_t)(val - prod_mod);
                } else {
                    c[idx] = (uint32_t)(q + val - prod_mod);
                }
            }
        }
    }
}

// Test NTT correctness for a given layer
int test_ntt_correctness(int layer) {
    uint32_t q = ntt64_get_modulus(layer);

    printf("Testing layer %d (q = %u):\n", layer, q);

    // Test 1: NTT(INTT(x)) = x
    printf("  [1/3] Testing NTT -> INTT = identity... ");
    uint32_t poly[NTT_N], poly_copy[NTT_N];
    random_poly(poly, q);
    memcpy(poly_copy, poly, sizeof(poly));

    ntt64_forward(poly, layer);
    ntt64_inverse(poly, layer);

    int passed = 1;
    for (int i = 0; i < NTT_N; i++) {
        if (poly[i] != poly_copy[i]) {
            printf("FAILED at index %d: %u != %u\n", i, poly[i], poly_copy[i]);
            passed = 0;
            break;
        }
    }
    if (passed) printf("PASSED\n");

    // Test 2: INTT(NTT(x)) = x
    printf("  [2/3] Testing INTT -> NTT = identity... ");
    random_poly(poly, q);
    memcpy(poly_copy, poly, sizeof(poly));

    ntt64_inverse(poly, layer);
    ntt64_forward(poly, layer);

    for (int i = 0; i < NTT_N; i++) {
        if (poly[i] != poly_copy[i]) {
            printf("FAILED at index %d: %u != %u\n", i, poly[i], poly_copy[i]);
            passed = 0;
            break;
        }
    }
    if (passed) printf("PASSED\n");

    // Test 3: Polynomial multiplication via NTT
    printf("  [3/3] Testing polynomial multiplication... ");
    uint32_t a[NTT_N], b[NTT_N];
    uint32_t c_ntt[NTT_N], c_schoolbook[NTT_N];

    random_poly(a, q);
    random_poly(b, q);

    // NTT-based multiplication
    uint32_t a_ntt[NTT_N], b_ntt[NTT_N];
    memcpy(a_ntt, a, sizeof(a));
    memcpy(b_ntt, b, sizeof(b));

    ntt64_forward(a_ntt, layer);
    ntt64_forward(b_ntt, layer);
    ntt64_pointwise_mul(c_ntt, a_ntt, b_ntt, layer);
    ntt64_inverse(c_ntt, layer);

    // Schoolbook multiplication
    schoolbook_negacyclic_mul(c_schoolbook, a, b, q);

    // Compare results
    for (int i = 0; i < NTT_N; i++) {
        if (c_ntt[i] != c_schoolbook[i]) {
            printf("FAILED at index %d: %u != %u\n", i, c_ntt[i], c_schoolbook[i]);
            passed = 0;
            break;
        }
    }
    if (passed) printf("PASSED\n");

    printf("\n");
    return passed;
}

// Benchmark NTT performance
void benchmark_ntt(int layer) {
    uint32_t q = ntt64_get_modulus(layer);
    uint32_t poly[NTT_N];
    random_poly(poly, q);

    const int iterations = 10000;

    // Benchmark forward NTT
    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        ntt64_forward(poly, layer);
    }
    clock_t end = clock();

    double time_forward = (double)(end - start) / CLOCKS_PER_SEC;
    double ops_per_sec = iterations / time_forward;

    printf("Layer %d (q=%-10u): Forward NTT: %.2f µs/op  (%'.0f ops/sec)\n",
           layer, q,
           time_forward * 1000000.0 / iterations,
           ops_per_sec);

    // Benchmark inverse NTT
    start = clock();
    for (int i = 0; i < iterations; i++) {
        ntt64_inverse(poly, layer);
    }
    end = clock();

    double time_inverse = (double)(end - start) / CLOCKS_PER_SEC;
    ops_per_sec = iterations / time_inverse;

    printf("Layer %d (q=%-10u): Inverse NTT: %.2f µs/op  (%'.0f ops/sec)\n",
           layer, q,
           time_inverse * 1000000.0 / iterations,
           ops_per_sec);
}

int main(void) {
    printf("========================================\n");
    printf("NTT64 Library Test Suite\n");
    printf("========================================\n");
    printf("N = %d (negacyclic)\n", NTT_N);
    printf("Layers: %d\n", NTT_NUM_LAYERS);
    printf("Constant-time implementation\n\n");

    int all_passed = 1;

    // Test all layers
    for (int layer = 0; layer < NTT_NUM_LAYERS; layer++) {
        if (!test_ntt_correctness(layer)) {
            all_passed = 0;
        }
    }

    if (all_passed) {
        printf("========================================\n");
        printf("ALL TESTS PASSED ✓\n");
        printf("========================================\n\n");
    } else {
        printf("========================================\n");
        printf("SOME TESTS FAILED ✗\n");
        printf("========================================\n\n");
        return 1;
    }

    // Benchmark
    printf("Performance Benchmarks:\n");
    printf("========================================\n");
    for (int layer = 0; layer < NTT_NUM_LAYERS; layer++) {
        benchmark_ntt(layer);
    }
    printf("========================================\n");

    return 0;
}
