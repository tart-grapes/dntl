#include <stdio.h>
#include <stdint.h>

#define N 64

// Declaration from gaussian_sampler.c
void gaussian_sample(const uint8_t seed[32], int output[N], double mean, double stddev, int min_value, int max_value);

int main(void) {
    // Example 256-bit seed
    uint8_t seed[32] = {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
        0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
    };

    int output[N];

    // Generate samples with mean=0.0, stddev=1.0 (standard normal distribution)
    // Clip values to [-5, 5] to remove tail outliers
    gaussian_sample(seed, output, 0.0, 1.0, -5, 5);

    // Print first 10 values and statistics
    printf("First 10 samples:\n");
    for (int i = 0; i < 10; i++) {
        printf("  output[%d] = %d\n", i, output[i]);
    }

    // Calculate mean and variance of generated samples
    double sum = 0.0, sum_sq = 0.0;
    for (int i = 0; i < N; i++) {
        sum += (double)output[i];
        sum_sq += (double)output[i] * (double)output[i];
    }
    double sample_mean = sum / N;
    double sample_variance = (sum_sq / N) - (sample_mean * sample_mean);

    printf("\nStatistics for %d samples:\n", N);
    printf("  Sample mean:     %f (expected: ~0.0)\n", sample_mean);
    printf("  Sample variance: %f (expected: <1.0 due to clipping)\n", sample_variance);
    printf("  Note: Values are clipped to [-5, 5] range\n");

    return 0;
}
