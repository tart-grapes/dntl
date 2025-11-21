#include <stdio.h>
#include <stdint.h>

int main() {
    uint32_t ANS_L = 16384;
    uint32_t total = 4096;
    uint32_t freq[3] = {1366, 1365, 1365};
    uint32_t cumul[4] = {0, 1366, 2731, 4096};
    
    // Simulate encoding in reverse: symbols 2, 0, 1
    uint32_t state = ANS_L;
    
    printf("=== ENCODING (reverse: 2, 0, 1) ===\n");
    printf("Initial state: %u\n\n", state);
    
    // Encode symbol 2
    printf("Encode symbol 2:\n");
    printf("  state_in = %u\n", state);
    printf("  quotient = %u / %u = %u\n", state, total, state/total);
    state = (state / total) * freq[2] + cumul[2] + (state % total);
    printf("  state_out = %u\n\n", state, state);
    
    // Encode symbol 0
    printf("Encode symbol 0:\n");
    printf("  state_in = %u\n", state);
    printf("  quotient = %u / %u = %u\n", state, total, state/total);
    state = (state / total) * freq[0] + cumul[0] + (state % total);
    printf("  state_out = %u\n\n", state);
    
    // Encode symbol 1
    printf("Encode symbol 1:\n");
    printf("  state_in = %u\n", state);
    printf("  quotient = %u / %u = %u\n", state, total, state/total);
    state = (state / total) * freq[1] + cumul[1] + (state % total);
    printf("  state_out = %u\n\n", state);
    
    printf("Final state: %u\n\n", state);
    
    // Now decode
    printf("=== DECODING (forward) ===\n");
    for (int i = 0; i < 3; i++) {
        uint32_t slot = state % total;
        printf("Iteration %d: state=%u, slot=%u", i, state, slot);
        
        int sym = -1;
        for (int j = 0; j < 3; j++) {
            if (slot < cumul[j+1]) {
                sym = j;
                break;
            }
        }
        printf(" -> symbol %d\n", sym);
        
        if (sym >= 0) {
            state = freq[sym] * (state / total) + (slot - cumul[sym]);
        }
    }
    
    return 0;
}
