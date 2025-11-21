#include <stdio.h>
#include <stdlib.h>
#include "sparse_delta.h"

int main() {
    int total_size = 0;
    for (int seed = 1000; seed < 1020; seed++) {
        srand(seed);
        int8_t vector[2048] = {0};
        int count = 0;
        for (int i = 0; i < 2048 && count < 97; i++) {
            if (rand() % 100 < 5) {
                vector[i] = (rand() % 60) - 30;
                if (vector[i] != 0) count++;
            }
        }
        sparse_delta_t *encoded = sparse_delta_encode(vector, 2048);
        if (encoded) {
            total_size += encoded->size;
            sparse_delta_free(encoded);
        }
    }
    printf("Phase 1 (sparse_delta) average: %.1f bytes\n", total_size / 20.0);
    return 0;
}
