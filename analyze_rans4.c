#include <stdio.h>
#include <stdint.h>

int main() {
    uint32_t ANS_L = 65536;
    uint32_t total = 4096;
    uint32_t freq[3] = {1366, 1365, 1365};
    uint32_t cumul[4] = {0, 1366, 2731, 4096};
    
    // Encoding in reverse: symbols 2, 0, 1
    uint32_t state = ANS_L;
    
    printf("=== ENCODING (reverse: 2, 0, 1) ===\n");
    printf("Initial state: %u\n\n", state);
    
    // Encode symbol 2
    printf("Encode symbol 2: state %u", state);
    state = (state / total) * freq[2] + cumul[2] + (state % total);
    printf(" -> %u\n", state);
    
    // Encode symbol 0
    printf("Encode symbol 0: state %u", state);
    state = (state / total) * freq[0] + cumul[0] + (state % total);
    printf(" -> %u\n", state);
    
    // Encode symbol 1
    printf("Encode symbol 1: state %u", state);
    state = (state / total) * freq[1] + cumul[1] + (state % total);
    printf(" -> %u\n\n", state);
    
    printf("Final state: %u\n\n", state);
    
    // Now decode
    printf("=== DECODING ===\n");
    for (int i = 0; i < 3; i++) {
        uint32_t slot = state % total;
        int sym = -1;
        for (int j = 0; j < 3; j++) {
            if (slot < cumul[j+1]) {
                sym = j;
                break;
            }
        }
        printf("Decode %d: state %u, slot %u -> symbol %d", i, state, slot, sym);
        
        if (sym >= 0) {
            state = freq[sym] * (state / total) + (slot - cumul[sym]);
            printf(", new state %u\n", state);
        } else {
            printf("\n");
        }
    }
    
    return 0;
}
