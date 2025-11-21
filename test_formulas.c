#include <stdio.h>
#include <stdint.h>

// Test both possible formulas
int main() {
    uint32_t state = 65536;
    uint32_t freq = 1365;
    uint32_t cumul = 2731;
    uint32_t total = 4096;
    
    printf("Initial state: %u\n\n", state);
    
    // Formula 1: (state / total) * freq + cumul + (state % total)
    uint32_t state1 = (state / total) * freq + cumul + (state % total);
    printf("Formula 1 (current): (state/total)*freq + cumul + (state%%total)\n");
    printf("  = (%u/%u)*%u + %u + (%u%%%u)\n", state, total, freq, cumul, state, total);
    printf("  = %u*%u + %u + %u = %u\n\n", state/total, freq, cumul, state%total, state1);
    
    // Formula 2: freq * (state / freq) + cumul + (state % freq) 
    uint32_t state2 = freq * (state / freq) + cumul + (state % freq);
    printf("Formula 2 (alternate): freq*(state/freq) + cumul + (state%%freq)\n");
    printf("  = %u*(%u/%u) + %u + (%u%%%u)\n", freq, state, freq, cumul, state, freq);
    printf("  = %u*%u + %u + %u = %u\n\n", freq, state/freq, cumul, state%freq, state2);
    
    // Formula 3: total * (state / freq) + cumul + (state % freq)
    uint32_t state3 = total * (state / freq) + cumul + (state % freq);
    printf("Formula 3: total*(state/freq) + cumul + (state%%freq)\n");
    printf("  = %u*(%u/%u) + %u + (%u%%%u)\n", total, state, freq, cumul, state, freq);
    printf("  = %u*%u + %u + %u = %u\n\n", total, state/freq, cumul, state%freq, state3);
    
    return 0;
}
