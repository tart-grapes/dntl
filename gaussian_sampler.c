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
 * Fast and safe Gaussian distribution sampler with tail clipping
 *
 * Generates N=64 values from a Gaussian (normal) distribution using a 256-bit seed.
 * Uses Box-Muller transform for Gaussian generation and xoshiro256** PRNG.
 * Implements tail clipping via rejection sampling for cryptographic applications.
 *
 * @param seed        Pointer to 32-byte (256-bit) seed value
 * @param output      Pointer to array of at least N integers to store results
 * @param mean        Mean of the Gaussian distribution
 * @param stddev      Standard deviation of the Gaussian distribution
 * @param bound_sigma Number of standard deviations for tail clipping bound
 *                    (e.g., 6.0 means reject samples outside [-6σ, +6σ] from mean)
 *                    Use 0.0 to disable tail clipping
 */
void gaussian_sample(const uint8_t seed[32], int output[N], double mean, double stddev, double bound_sigma) {
    // Initialize PRNG state from 256-bit seed
    uint64_t state[4];
    memcpy(state, seed, 32);

    // Ensure state is never all zeros (degenerate case for xoshiro)
    if (state[0] == 0 && state[1] == 0 && state[2] == 0 && state[3] == 0) {
        state[0] = 0x123456789ABCDEF0ULL;
    }

    // Calculate tail clipping bounds
    double lower_bound = mean - bound_sigma * stddev;
    double upper_bound = mean + bound_sigma * stddev;
    int use_bounds = (bound_sigma > 0.0);

    // Helper macro for PRNG step
    #define NEXT_RANDOM(result_var) do { \
        result_var = rotl(state[1] * 5, 7) * 9; \
        uint64_t t = state[1] << 17; \
        state[2] ^= state[0]; \
        state[3] ^= state[1]; \
        state[1] ^= state[2]; \
        state[0] ^= state[3]; \
        state[2] ^= t; \
        state[3] = rotl(state[3], 45); \
    } while(0)

    // Generate N Gaussian samples using Box-Muller transform with rejection sampling
    for (int i = 0; i < N; i += 2) {
        double z0, z1;
        int valid = 0;

        // Rejection sampling loop - keep generating until we get valid samples
        // For bounds like 6σ, rejection probability is ~0.000001, so this rarely loops
        while (!valid) {
            // Generate two uniform random numbers using xoshiro256**
            uint64_t result1, result2;
            NEXT_RANDOM(result1);
            NEXT_RANDOM(result2);

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
            z0 = mag * cos(2.0 * M_PI * u2) + mean;
            z1 = mag * sin(2.0 * M_PI * u2) + mean;

            // Check if samples are within bounds (or if bounds disabled)
            if (!use_bounds) {
                valid = 1;
            } else {
                int z0_valid = (z0 >= lower_bound && z0 <= upper_bound);
                int z1_valid = (z1 >= lower_bound && z1 <= upper_bound);

                // Both samples must be valid (or we only need one for the last sample)
                if (i + 1 < N) {
                    valid = z0_valid && z1_valid;
                } else {
                    valid = z0_valid;
                }
            }
        }

        output[i] = (int)round(z0);
        if (i + 1 < N) {
            output[i + 1] = (int)round(z1);
        }
    }

    #undef NEXT_RANDOM
}

