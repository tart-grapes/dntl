#include "sparse_ultimate.h"
#include <stdio.h>
#include <string.h>

int main() {
    int8_t vector[2048] = {0};
    int8_t decoded[2048];

    vector[10] = 5;
    vector[100] = -3;
    vector[500] = 7;

    printf("Testing sparse_ultimate:\n");
    printf("Original: [10]=5, [100]=-3, [500]=7\n");

    sparse_ultimate_t *enc = sparse_ultimate_encode(vector, 2048);
    if (!enc) {
        printf("Encoding failed\n");
        return 1;
    }
    printf("Encoded: %zu bytes\n", enc->size);

    memset(decoded, 0, sizeof(decoded));
    int result = sparse_ultimate_decode(enc, decoded, 2048);
    
    if (result < 0) {
        printf("Decoding failed\n");
        sparse_ultimate_free(enc);
        return 1;
    }

    printf("Decoded: [10]=%d, [100]=%d, [500]=%d\n", decoded[10], decoded[100], decoded[500]);
    
    int errors = 0;
    if (decoded[10] != 5) errors++;
    if (decoded[100] != -3) errors++;
    if (decoded[500] != 7) errors++;
    
    printf("%s\n", errors == 0 ? "✓ PASS" : "✗ FAIL");
    
    sparse_ultimate_free(enc);
    return errors;
}
