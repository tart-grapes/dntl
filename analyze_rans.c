#include <stdio.h>
#include <stdint.h>

int main() {
    // Alphabet: [-3, 5, 7]
    // Frequencies: [1366, 1365, 1365], total = 4096
    // Cumulative: [0, 1366, 2731, 4096]
    
    uint32_t ANS_L = 1024;
    uint32_t total = 4096;
    uint32_t freq[3] = {1366, 1365, 1365};
    uint32_t cumul[4] = {0, 1366, 2731, 4096};
    
    // Simulate encoding in reverse: symbols 2, 0, 1
    uint32_t state = ANS_L;
    
    printf("=== ENCODING ===\n");
    printf("Initial state: %u (0x%x)\n\n", state, state);
    
    // Encode symbol 2
    printf("Encode symbol 2 (freq=%u, cumul=%u):\n", freq[2], cumul[2]);
    printf("  state_in = %u\n", state);
    state = (state / total) * freq[2] + cumul[2] + (state % total);
    printf("  state_out = %u (0x%x)\n\n", state, state);
    
    // Encode symbol 0
    printf("Encode symbol 0 (freq=%u, cumul=%u):\n", freq[0], cumul[0]);
    printf("  state_in = %u\n", state);
    state = (state / total) * freq[0] + cumul[0] + (state % total);
    printf("  state_out = %u (0x%x)\n\n", state, state);
    
    // Encode symbol 1
    printf("Encode symbol 1 (freq=%u, cumul=%u):\n", freq[1], cumul[1]);
    printf("  state_in = %u\n", state);
    state = (state / total) * freq[1] + cumul[1] + (state % total);
    printf("  state_out = %u (0x%x)\n\n", state, state);
    
    printf("Final state: %u (0x%x)\n\n", state, state);
    
    // Now decode
    printf("=== DECODING ===\n");
    printf("Initial state: %u (0x%x)\n\n", state, state);
    
    for (int i = 0; i < 3; i++) {
        uint32_t slot = state % total;
        printf("Decode iteration %d:\n", i);
        printf("  state = %u, slot = %u\n", state, slot);
        
        int sym = -1;
        for (int j = 0; j < 3; j++) {
            if (slot < cumul[j+1]) {
                sym = j;
                break;
            }
        }
        printf("  decoded symbol = %d\n", sym);
        
        if (sym >= 0) {
            uint32_t old_state = state;
            state = freq[sym] * (state / total) + (slot - cumul[sym]);
            printf("  state: %u -> %u\n\n", old_state, state);
        }
    }
    
    return 0;
}
