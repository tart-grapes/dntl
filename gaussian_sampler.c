#include <stdint.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define N 64

// Helper function for rotation
static inline uint64_t rotl(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

/**
 * Fast and safe Gaussian distribution sampler
 *
 * Generates N=64 values from a Gaussian (normal) distribution using a 256-bit seed.
 * Uses Box-Muller transform for Gaussian generation and xoshiro256** PRNG.
 *
 * @param seed      Pointer to 32-byte (256-bit) seed value
 * @param output    Pointer to array of at least N doubles to store results
 * @param mean      Mean of the Gaussian distribution
 * @param stddev    Standard deviation of the Gaussian distribution
 */
void gaussian_sample(const uint8_t seed[32], double output[N], double mean, double stddev) {
    // Initialize PRNG state from 256-bit seed
    uint64_t state[4];
    memcpy(state, seed, 32);

    // Ensure state is never all zeros (degenerate case for xoshiro)
    if (state[0] == 0 && state[1] == 0 && state[2] == 0 && state[3] == 0) {
        state[0] = 0x123456789ABCDEF0ULL;
    }

    // Generate N Gaussian samples using Box-Muller transform
    for (int i = 0; i < N; i += 2) {
        // xoshiro256** PRNG - generate two uniform random numbers
        // https://prng.di.unimi.it/

        // First random number
        uint64_t result1 = rotl(state[1] * 5, 7) * 9;
        uint64_t t = state[1] << 17;
        state[2] ^= state[0];
        state[3] ^= state[1];
        state[1] ^= state[2];
        state[0] ^= state[3];
        state[2] ^= t;
        state[3] = rotl(state[3], 45);

        // Second random number
        uint64_t result2 = rotl(state[1] * 5, 7) * 9;
        t = state[1] << 17;
        state[2] ^= state[0];
        state[3] ^= state[1];
        state[1] ^= state[2];
        state[0] ^= state[3];
        state[2] ^= t;
        state[3] = rotl(state[3], 45);

        // Convert to uniform doubles in (0, 1)
        // Use upper 53 bits for double precision
        double u1 = (result1 >> 11) * 0x1.0p-53;
        double u2 = (result2 >> 11) * 0x1.0p-53;

        // Ensure u1 is never exactly 0 to avoid log(0)
        if (u1 == 0.0) {
            u1 = 0x1.0p-53; // Smallest positive normalized double in this range
        }

        // Box-Muller transform
        double mag = stddev * sqrt(-2.0 * log(u1));
        double z0 = mag * cos(2.0 * M_PI * u2) + mean;
        double z1 = mag * sin(2.0 * M_PI * u2) + mean;

        output[i] = z0;
        if (i + 1 < N) {
            output[i + 1] = z1;
        }
    }
}

