#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

int main() {
    // Generate 5 test vectors and analyze gap distributions
    for (int seed = 1000; seed < 1005; seed++) {
        srand(seed);
        
        int8_t vector[2048] = {0};
        uint16_t positions[200];
        int count = 0;
        
        // Generate ~97 non-zeros
        for (int i = 0; i < 2048 && count < 97; i++) {
            if (rand() % 100 < 5) {
                vector[i] = (rand() % 60) - 30;
                if (vector[i] != 0) {
                    positions[count++] = i;
                }
            }
        }
        
        // Compute gaps
        uint16_t gaps[200];
        gaps[0] = positions[0];
        for (int i = 1; i < count; i++) {
            gaps[i] = positions[i] - positions[i-1] - 1;
        }
        
        // Analyze gap statistics
        double sum = 0, sum_sq = 0;
        int min_gap = gaps[0], max_gap = gaps[0];
        
        for (int i = 0; i < count; i++) {
            sum += gaps[i];
            sum_sq += gaps[i] * gaps[i];
            if (gaps[i] < min_gap) min_gap = gaps[i];
            if (gaps[i] > max_gap) max_gap = gaps[i];
        }
        
        double mean = sum / count;
        double variance = (sum_sq / count) - (mean * mean);
        double stddev = sqrt(variance);
        
        // Compute optimal Rice parameter (log2 of mean)
        int optimal_r = 0;
        if (mean > 0) {
            optimal_r = (int)(log2(mean) + 0.5);
            if (optimal_r < 0) optimal_r = 0;
            if (optimal_r > 15) optimal_r = 15;
        }
        
        printf("Seed %d: count=%d, mean=%.1f, stddev=%.1f, min=%d, max=%d, optimal_r=%d\n",
               seed, count, mean, stddev, min_gap, max_gap, optimal_r);
        
        // Show first 20 gaps
        printf("  First 20 gaps: ");
        for (int i = 0; i < 20 && i < count; i++) {
            printf("%d ", gaps[i]);
        }
        printf("\n");
    }
    
    return 0;
}
