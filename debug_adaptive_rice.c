#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

int main() {
    srand(1000);
    
    int8_t vector[2048] = {0};
    uint16_t positions[200];
    int count = 0;
    
    // Generate test vector
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
    
    // Simulate adaptive Rice parameters
    uint16_t gap_history[3] = {16, 16, 16};
    int fixed_r4_bits = 0, adaptive_bits = 0;
    
    printf("Gap | Fixed(r=4) | Adaptive(r) | History\n");
    printf("-----+------------+-------------+---------------\n");
    
    for (int i = 0; i < 20 && i < count; i++) {
        uint16_t gap = (i == 0) ? gaps[0] : gaps[i];
        
        // Fixed r=4
        int fixed_bits = 4 + (gap >> 4) + 1;  // Unary part + quotient bits
        fixed_r4_bits += fixed_bits;
        
        // Adaptive r
        uint32_t mean_gap = (gap_history[0] + gap_history[1] + gap_history[2]) / 3;
        uint8_t r = 1;
        if (mean_gap > 0) {
            while ((1U << r) < mean_gap && r < 8) r++;
        }
        if (r > 8) r = 8;
        
        int adaptive_bits_this = r + (gap >> r) + 1;
        adaptive_bits += adaptive_bits_this;
        
        printf("%3d | %2d bits     | r=%d: %2d bits | [%d,%d,%d]\n",
               gap, fixed_bits, r, adaptive_bits_this,
               gap_history[0], gap_history[1], gap_history[2]);
        
        // Update history
        if (i > 0) {
            gap_history[0] = gap_history[1];
            gap_history[1] = gap_history[2];
            gap_history[2] = gap;
        }
    }
    
    printf("\nFirst 20 gaps: fixed=%d bits, adaptive=%d bits (diff=%+d)\n",
           fixed_r4_bits, adaptive_bits, adaptive_bits - fixed_r4_bits);
    
    return 0;
}
