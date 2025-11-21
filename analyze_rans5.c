#include <stdio.h>
#include <stdint.h>

int main() {
    uint32_t ANS_L = 65536;
    uint32_t total = 4096;
    uint32_t freq[3] = {1366, 1365, 1365};
    uint32_t cumul[4] = {0, 1366, 2731, 4096};
    
    // Encoding in reverse: symbols 2, 0, 1 with NEW formula
    uint32_t state = ANS_L;
    
    printf("=== ENCODING (reverse: 2, 0, 1) - NEW FORMULA ===\n");
    printf("Initial state: %u\n\n", state);
    
    // Encode symbol 2: total * (state / freq) + cumul + (state % freq)
    printf("Encode symbol 2: state %u\n", state);
    uint32_t q = state / freq[2];
    uint32_t r = state % freq[2];
    printf("  q=%u/%u=%u, r=%u%%%u=%u\n", state, freq[2], q, state, freq[2], r);
    state = total * q + cumul[2] + r;
    printf("  state' = %u*%u + %u + %u = %u\n\n", total, q, cumul[2], r, state);
    
    // Encode symbol 0
    printf("Encode symbol 0: state %u\n", state);
    q = state / freq[0];
    r = state % freq[0];
    printf("  q=%u/%u=%u, r=%u%%%u=%u\n", state, freq[0], q, state, freq[0], r);
    state = total * q + cumul[0] + r;
    printf("  state' = %u*%u + %u + %u = %u\n\n", total, q, cumul[0], r, state);
    
    // Encode symbol 1
    printf("Encode symbol 1: state %u\n", state);
    q = state / freq[1];
    r = state % freq[1];
    printf("  q=%u/%u=%u, r=%u%%%u=%u\n", state, freq[1], q, state, freq[1], r);
    state = total * q + cumul[1] + r;
    printf("  state' = %u*%u + %u + %u = %u\n\n", total, q, cumul[1], r, state);
    
    printf("Final state: %u\n\n", state);
    
    // Now decode with current formula: freq * (state / total) + (slot - cumul)
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
        printf("Decode %d: state %u, slot %u -> symbol %d\n", i, state, slot, sym);
        
        if (sym >= 0) {
            uint32_t old_state = state;
            state = freq[sym] * (state / total) + (slot - cumul[sym]);
            printf("  state: %u -> %u\n\n", old_state, state);
        }
    }
    
    return 0;
}
