#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <math.h>
#include "ntt64.h"

// High-resolution timing
#ifdef __x86_64__
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
#define HAVE_RDTSC 1
#else
#define HAVE_RDTSC 0
static inline uint64_t rdtsc(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}
#endif

// Statistical functions
double compute_mean(uint64_t *samples, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += (double)samples[i];
    }
    return sum / n;
}

double compute_stddev(uint64_t *samples, int n, double mean) {
    double sum_sq = 0.0;
    for (int i = 0; i < n; i++) {
        double diff = (double)samples[i] - mean;
        sum_sq += diff * diff;
    }
    return sqrt(sum_sq / n);
}

double compute_coefficient_of_variation(uint64_t *samples, int n) {
    double mean = compute_mean(samples, n);
    double stddev = compute_stddev(samples, n, mean);
    return (stddev / mean) * 100.0; // as percentage
}

// T-test for comparing two distributions
double welch_t_test(uint64_t *samples1, int n1, uint64_t *samples2, int n2) {
    double mean1 = compute_mean(samples1, n1);
    double mean2 = compute_mean(samples2, n2);
    double var1 = compute_stddev(samples1, n1, mean1);
    double var2 = compute_stddev(samples2, n2, mean2);

    var1 = var1 * var1;
    var2 = var2 * var2;

    double t = (mean1 - mean2) / sqrt(var1/n1 + var2/n2);
    return fabs(t);
}

// Fill polynomial with specific pattern
void fill_pattern(uint32_t poly[NTT_N], const char* pattern, uint32_t q) {
    if (strcmp(pattern, "zeros") == 0) {
        memset(poly, 0, sizeof(uint32_t) * NTT_N);
    } else if (strcmp(pattern, "ones") == 0) {
        for (int i = 0; i < NTT_N; i++) {
            poly[i] = 1;
        }
    } else if (strcmp(pattern, "max") == 0) {
        for (int i = 0; i < NTT_N; i++) {
            poly[i] = q - 1;
        }
    } else if (strcmp(pattern, "alternating") == 0) {
        for (int i = 0; i < NTT_N; i++) {
            poly[i] = (i & 1) ? (q - 1) : 0;
        }
    } else if (strcmp(pattern, "random1") == 0) {
        srand(12345);
        for (int i = 0; i < NTT_N; i++) {
            poly[i] = rand() % q;
        }
    } else if (strcmp(pattern, "random2") == 0) {
        srand(54321);
        for (int i = 0; i < NTT_N; i++) {
            poly[i] = rand() % q;
        }
    }
}

// Measure timing for a specific pattern
void measure_pattern_timing(int layer, const char* pattern,
                           uint64_t *fwd_times, uint64_t *inv_times,
                           int num_samples) {
    uint32_t q = ntt64_get_modulus(layer);
    uint32_t poly[NTT_N];

    // Warm up CPU
    for (int i = 0; i < 100; i++) {
        fill_pattern(poly, pattern, q);
        ntt64_forward(poly, layer);
    }

    // Measure forward NTT
    for (int i = 0; i < num_samples; i++) {
        fill_pattern(poly, pattern, q);

        uint64_t start = rdtsc();
        ntt64_forward(poly, layer);
        uint64_t end = rdtsc();

        fwd_times[i] = end - start;
    }

    // Measure inverse NTT
    for (int i = 0; i < num_samples; i++) {
        fill_pattern(poly, pattern, q);
        ntt64_forward(poly, layer); // Put in NTT domain first

        uint64_t start = rdtsc();
        ntt64_inverse(poly, layer);
        uint64_t end = rdtsc();

        inv_times[i] = end - start;
    }
}

void test_constant_time_layer(int layer) {
    uint32_t q = ntt64_get_modulus(layer);
    const int num_samples = 1000;
    const char* patterns[] = {"zeros", "ones", "max", "alternating", "random1", "random2"};
    const int num_patterns = 6;

    printf("\n=== Layer %d (q = %u) ===\n", layer, q);

    // Storage for timing measurements
    uint64_t **fwd_times = malloc(num_patterns * sizeof(uint64_t*));
    uint64_t **inv_times = malloc(num_patterns * sizeof(uint64_t*));

    for (int i = 0; i < num_patterns; i++) {
        fwd_times[i] = malloc(num_samples * sizeof(uint64_t));
        inv_times[i] = malloc(num_samples * sizeof(uint64_t));
    }

    // Measure timing for each pattern
    for (int i = 0; i < num_patterns; i++) {
        measure_pattern_timing(layer, patterns[i], fwd_times[i], inv_times[i], num_samples);
    }

    // Analyze results
    printf("\nForward NTT timing statistics:\n");
    printf("%-15s %12s %12s %12s %12s\n", "Pattern", "Mean", "StdDev", "Min", "Max");
    printf("---------------------------------------------------------------\n");

    for (int i = 0; i < num_patterns; i++) {
        double mean = compute_mean(fwd_times[i], num_samples);
        double stddev = compute_stddev(fwd_times[i], num_samples, mean);

        uint64_t min_val = fwd_times[i][0];
        uint64_t max_val = fwd_times[i][0];
        for (int j = 1; j < num_samples; j++) {
            if (fwd_times[i][j] < min_val) min_val = fwd_times[i][j];
            if (fwd_times[i][j] > max_val) max_val = fwd_times[i][j];
        }

        printf("%-15s %12.0f %12.0f %12llu %12llu\n",
               patterns[i], mean, stddev,
               (unsigned long long)min_val, (unsigned long long)max_val);
    }

    printf("\nInverse NTT timing statistics:\n");
    printf("%-15s %12s %12s %12s %12s\n", "Pattern", "Mean", "StdDev", "Min", "Max");
    printf("---------------------------------------------------------------\n");

    for (int i = 0; i < num_patterns; i++) {
        double mean = compute_mean(inv_times[i], num_samples);
        double stddev = compute_stddev(inv_times[i], num_samples, mean);

        uint64_t min_val = inv_times[i][0];
        uint64_t max_val = inv_times[i][0];
        for (int j = 1; j < num_samples; j++) {
            if (inv_times[i][j] < min_val) min_val = inv_times[i][j];
            if (inv_times[i][j] > max_val) max_val = inv_times[i][j];
        }

        printf("%-15s %12.0f %12.0f %12llu %12llu\n",
               patterns[i], mean, stddev,
               (unsigned long long)min_val, (unsigned long long)max_val);
    }

    // Statistical comparison: zeros vs max (should have similar timing)
    printf("\nStatistical Tests (Welch's t-test):\n");
    printf("Comparing 'zeros' vs other patterns (lower t-value = more similar):\n");
    printf("%-15s %15s %15s\n", "Pattern Pair", "Forward t-stat", "Inverse t-stat");
    printf("---------------------------------------------------------------\n");

    for (int i = 1; i < num_patterns; i++) {
        double t_fwd = welch_t_test(fwd_times[0], num_samples, fwd_times[i], num_samples);
        double t_inv = welch_t_test(inv_times[0], num_samples, inv_times[i], num_samples);

        printf("zeros vs %-7s %15.2f %15.2f", patterns[i], t_fwd, t_inv);

        // Flag if timing difference is statistically significant
        // t > 3.0 is typically considered significant
        if (t_fwd > 3.0 || t_inv > 3.0) {
            printf("  ⚠️  TIMING VARIATION DETECTED");
        }
        printf("\n");
    }

    // Cleanup
    for (int i = 0; i < num_patterns; i++) {
        free(fwd_times[i]);
        free(inv_times[i]);
    }
    free(fwd_times);
    free(inv_times);
}

int main(void) {
    printf("========================================\n");
    printf("Constant-Time Verification Test\n");
    printf("========================================\n");

#if HAVE_RDTSC
    printf("Using RDTSC for high-resolution timing\n");
#else
    printf("Using clock_gettime for timing\n");
#endif

    printf("\nThis test measures timing variations across different input patterns.\n");
    printf("For constant-time code, all patterns should have similar timing.\n");
    printf("t-statistic > 3.0 indicates potential timing leakage.\n");

    // Test each layer
    for (int layer = 0; layer < NTT_NUM_LAYERS; layer++) {
        test_constant_time_layer(layer);
    }

    printf("\n========================================\n");
    printf("Summary:\n");
    printf("========================================\n");
    printf("If t-statistics are all < 3.0, the implementation is likely constant-time.\n");
    printf("Higher values indicate input-dependent timing variations.\n");
    printf("\nNote: Some variation is normal due to CPU effects (cache, branch prediction, etc.)\n");
    printf("but should be minimal for truly constant-time code.\n");

    return 0;
}
