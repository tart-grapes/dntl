#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "ntt64.h"

// Test modular inverse for a given layer
int test_inverse(int layer) {
    uint32_t q = ntt64_get_modulus(layer);
    printf("Testing layer %d (q = %u):\n", layer, q);

    int passed = 1;
    int tests = 0;

    // Test specific values
    uint32_t test_values[] = {1, 2, 3, 5, 7, 11, 13, 17, 100, 1000, q/2, q-1, q-2};

    for (int i = 0; i < sizeof(test_values) / sizeof(test_values[0]); i++) {
        uint32_t a = test_values[i] % q;
        if (a == 0) continue;

        uint32_t inv = ntt64_inv_mod(a, layer);
        if (inv == 0) {
            printf("  FAILED: Could not compute inverse of %u\n", a);
            passed = 0;
            continue;
        }

        // Verify: a * inv ≡ 1 (mod q)
        uint32_t product = ntt64_mul_mod(a, inv, layer);
        if (product != 1) {
            printf("  FAILED: %u * %u = %u (mod %u), expected 1\n", a, inv, product, q);
            passed = 0;
        }
        tests++;
    }

    // Test that 64^(-1) matches precomputed N_INV
    uint32_t inv_64 = ntt64_inv_mod(64, layer);
    uint32_t product_64 = ntt64_mul_mod(64, inv_64, layer);

    if (product_64 != 1) {
        printf("  FAILED: 64^(-1) verification failed\n");
        passed = 0;
    }
    tests++;

    // Test add/sub/mul properties
    uint32_t a = 12345 % q;
    uint32_t b = 67890 % q;

    // Test: (a + b) - b = a
    uint32_t sum = ntt64_add_mod(a, b, layer);
    uint32_t diff = ntt64_sub_mod(sum, b, layer);
    if (diff != a) {
        printf("  FAILED: (a+b)-b != a\n");
        passed = 0;
    }
    tests++;

    // Test: a * 1 = a
    uint32_t prod = ntt64_mul_mod(a, 1, layer);
    if (prod != a) {
        printf("  FAILED: a*1 != a\n");
        passed = 0;
    }
    tests++;

    // Test: a * a^(-1) = 1
    if (a != 0) {
        uint32_t inv_a = ntt64_inv_mod(a, layer);
        uint32_t prod_a = ntt64_mul_mod(a, inv_a, layer);
        if (prod_a != 1) {
            printf("  FAILED: a * a^(-1) != 1\n");
            passed = 0;
        }
        tests++;
    }

    if (passed) {
        printf("  PASSED (%d tests)\n", tests);
    } else {
        printf("  FAILED (some of %d tests failed)\n", tests);
    }

    printf("\n");
    return passed;
}

int main(void) {
    printf("========================================\n");
    printf("Field Arithmetic Test Suite\n");
    printf("========================================\n\n");

    int all_passed = 1;

    // Test all layers
    for (int layer = 0; layer < NTT_NUM_LAYERS; layer++) {
        if (!test_inverse(layer)) {
            all_passed = 0;
        }
    }

    if (all_passed) {
        printf("========================================\n");
        printf("ALL TESTS PASSED ✓\n");
        printf("========================================\n");
        return 0;
    } else {
        printf("========================================\n");
        printf("SOME TESTS FAILED ✗\n");
        printf("========================================\n");
        return 1;
    }
}
