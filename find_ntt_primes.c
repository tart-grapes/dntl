/**
 * find_ntt_primes.c - Find smallest primes with NTT roots of unity
 *
 * For a dimension N:
 * - Cyclic NTT requires: p ≡ 1 (mod N)
 * - Negacyclic NTT requires: p ≡ 1 (mod 2N)
 *
 * This program finds the smallest primes satisfying both conditions.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// Modular exponentiation: (base^exp) mod mod
uint64_t mod_exp(uint64_t base, uint64_t exp, uint64_t mod) {
    uint64_t result = 1;
    base = base % mod;
    while (exp > 0) {
        if (exp & 1) {
            result = (__uint128_t)result * base % mod;
        }
        exp >>= 1;
        base = (__uint128_t)base * base % mod;
    }
    return result;
}

// Miller-Rabin primality test
bool is_prime_miller_rabin(uint64_t n, int iterations) {
    if (n < 2) return false;
    if (n == 2 || n == 3) return true;
    if (n % 2 == 0) return false;

    // Write n-1 as 2^r * d
    uint64_t d = n - 1;
    int r = 0;
    while (d % 2 == 0) {
        d /= 2;
        r++;
    }

    // Witnesses to test
    uint64_t witnesses[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37};
    int num_witnesses = (iterations < 12) ? iterations : 12;

    for (int i = 0; i < num_witnesses; i++) {
        uint64_t a = witnesses[i];
        if (a >= n) continue;

        uint64_t x = mod_exp(a, d, n);
        if (x == 1 || x == n - 1) continue;

        bool composite = true;
        for (int j = 0; j < r - 1; j++) {
            x = (__uint128_t)x * x % n;
            if (x == n - 1) {
                composite = false;
                break;
            }
        }
        if (composite) return false;
    }
    return true;
}

// Find primitive root modulo prime p
uint64_t find_primitive_root(uint64_t p) {
    if (p == 2) return 1;

    // Factor p-1 to find prime factors
    uint64_t phi = p - 1;
    uint64_t factors[64];
    int num_factors = 0;

    // Find all prime factors of phi
    uint64_t temp = phi;
    for (uint64_t i = 2; i * i <= temp; i++) {
        if (temp % i == 0) {
            factors[num_factors++] = i;
            while (temp % i == 0) {
                temp /= i;
            }
        }
    }
    if (temp > 1) {
        factors[num_factors++] = temp;
    }

    // Try candidates for primitive root
    for (uint64_t g = 2; g < p; g++) {
        bool is_primitive = true;
        for (int i = 0; i < num_factors; i++) {
            if (mod_exp(g, phi / factors[i], p) == 1) {
                is_primitive = false;
                break;
            }
        }
        if (is_primitive) {
            return g;
        }
    }
    return 0;
}

// Verify that appropriate roots exist for the given dimension
bool verify_ntt_roots(uint64_t p, uint64_t n, bool verbose) {
    // For negacyclic NTT: need p ≡ 1 (mod 2n)
    if ((p - 1) % (2 * n) != 0) {
        if (verbose) printf("  ✗ p-1 not divisible by 2N=%lu\n", 2 * n);
        return false;
    }

    // Find primitive root
    uint64_t g = find_primitive_root(p);
    if (g == 0) {
        if (verbose) printf("  ✗ Could not find primitive root\n");
        return false;
    }

    // Compute psi (primitive 2N-th root of unity)
    uint64_t psi = mod_exp(g, (p - 1) / (2 * n), p);

    // Verify psi^N = -1 (mod p)
    uint64_t psi_n = mod_exp(psi, n, p);
    if (psi_n != p - 1) {
        if (verbose) printf("  ✗ psi^N = %lu ≠ -1 (mod p)\n", psi_n);
        return false;
    }

    // Verify psi^(2N) = 1 (mod p)
    uint64_t psi_2n = mod_exp(psi, 2 * n, p);
    if (psi_2n != 1) {
        if (verbose) printf("  ✗ psi^(2N) = %lu ≠ 1 (mod p)\n", psi_2n);
        return false;
    }

    // Compute omega = psi^2 (N-th root of unity for cyclic case)
    uint64_t omega = (__uint128_t)psi * psi % p;
    uint64_t omega_n = mod_exp(omega, n, p);
    if (omega_n != 1) {
        if (verbose) printf("  ✗ omega^N = %lu ≠ 1 (mod p)\n", omega_n);
        return false;
    }

    if (verbose) {
        printf("  ✓ Valid NTT prime\n");
        printf("    Primitive root g = %lu\n", g);
        printf("    psi (2N-th root) = %lu\n", psi);
        printf("    omega (N-th root) = %lu\n", omega);
    }

    return true;
}

// Find smallest primes for given dimension
void find_ntt_primes(uint64_t n, int count) {
    printf("\n========================================\n");
    printf("Finding NTT primes for dimension N=%lu\n", n);
    printf("========================================\n");
    printf("Requirements:\n");
    printf("  - Cyclic NTT: p ≡ 1 (mod N=%lu)\n", n);
    printf("  - Negacyclic NTT: p ≡ 1 (mod 2N=%lu)\n", 2 * n);
    printf("\n");

    int found = 0;
    uint64_t candidate = 2 * n + 1; // Start with smallest possible candidate

    printf("Searching for %d smallest primes...\n\n", count);

    while (found < count) {
        // Check if candidate ≡ 1 (mod 2N)
        if ((candidate - 1) % (2 * n) == 0) {
            // Check if prime
            if (is_prime_miller_rabin(candidate, 12)) {
                printf("Prime #%d: %lu (%.2f bits)\n", found + 1, candidate,
                       __builtin_clzll(candidate) >= 64 ? 0.0 : 64.0 - __builtin_clzll(candidate));

                // Verify NTT roots
                if (verify_ntt_roots(candidate, n, true)) {
                    found++;
                    printf("\n");
                } else {
                    printf("  WARNING: Prime passed congruence test but failed root verification!\n\n");
                }
            }
        }
        candidate += 2 * n; // Next candidate (jump by 2N to maintain congruence)
    }
}

int main(int argc, char *argv[]) {
    // Default: find primes for N=4096
    uint64_t dimension = 4096;
    int num_primes = 10;

    if (argc >= 2) {
        dimension = strtoull(argv[1], NULL, 10);
    }
    if (argc >= 3) {
        num_primes = atoi(argv[2]);
    }

    printf("NTT Prime Finder\n");
    printf("================\n");
    printf("Usage: %s [dimension=%lu] [count=%d]\n", argv[0], dimension, num_primes);

    find_ntt_primes(dimension, num_primes);

    printf("\n========================================\n");
    printf("Summary\n");
    printf("========================================\n");
    printf("Found %d primes suitable for NTT with N=%lu\n", num_primes, dimension);
    printf("All primes satisfy:\n");
    printf("  1. Primality (Miller-Rabin test)\n");
    printf("  2. p ≡ 1 (mod 2N) for negacyclic NTT\n");
    printf("  3. Existence of 2N-th root psi with psi^N = -1\n");
    printf("  4. Existence of N-th root omega = psi^2\n");

    return 0;
}
