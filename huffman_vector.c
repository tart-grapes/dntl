/**
 * huffman_vector.c
 *
 * Huffman encoding implementation for dense vectors
 */

#include "huffman_vector.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_SYMBOLS 256
#define MAX_CODE_LENGTH 32

/* Huffman tree node */
typedef struct huff_node {
    uint32_t freq;
    uint16_t symbol;
    uint8_t is_leaf;
    struct huff_node *left;
    struct huff_node *right;
} huff_node_t;

/* Code table entry */
typedef struct {
    uint32_t code;      /* Huffman code */
    uint8_t length;     /* Code length in bits */
} code_entry_t;

/* Bit writer for encoding */
typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t byte_pos;
    uint8_t bit_pos;
    uint64_t bit_buffer;
    uint8_t bits_in_buffer;
} bit_writer_t;

/* Bit reader for decoding */
typedef struct {
    const uint8_t *buffer;
    size_t size;
    size_t byte_pos;
    uint8_t bit_pos;
} bit_reader_t;

/* ========== Bit Writer ========== */

static bit_writer_t* bit_writer_create(size_t capacity) {
    bit_writer_t *bw = malloc(sizeof(bit_writer_t));
    if (!bw) return NULL;

    bw->buffer = calloc(capacity, 1);
    if (!bw->buffer) {
        free(bw);
        return NULL;
    }

    bw->capacity = capacity;
    bw->byte_pos = 0;
    bw->bit_pos = 0;
    bw->bit_buffer = 0;
    bw->bits_in_buffer = 0;

    return bw;
}

static void bit_writer_write(bit_writer_t *bw, uint32_t bits, uint8_t num_bits) {
    bw->bit_buffer = (bw->bit_buffer << num_bits) | bits;
    bw->bits_in_buffer += num_bits;

    /* Flush complete bytes */
    while (bw->bits_in_buffer >= 8) {
        bw->bits_in_buffer -= 8;
        if (bw->byte_pos < bw->capacity) {
            bw->buffer[bw->byte_pos++] = (bw->bit_buffer >> bw->bits_in_buffer) & 0xFF;
        }
    }
}

static void bit_writer_flush(bit_writer_t *bw) {
    if (bw->bits_in_buffer > 0) {
        if (bw->byte_pos < bw->capacity) {
            bw->buffer[bw->byte_pos++] = (bw->bit_buffer << (8 - bw->bits_in_buffer)) & 0xFF;
        }
        bw->bits_in_buffer = 0;
        bw->bit_buffer = 0;
    }
}

/* ========== Bit Reader ========== */

static bit_reader_t* bit_reader_create(const uint8_t *data, size_t size) {
    bit_reader_t *br = malloc(sizeof(bit_reader_t));
    if (!br) return NULL;

    br->buffer = data;
    br->size = size;
    br->byte_pos = 0;
    br->bit_pos = 0;

    return br;
}

static uint32_t bit_reader_read(bit_reader_t *br, uint8_t num_bits) {
    uint32_t result = 0;

    for (uint8_t i = 0; i < num_bits; i++) {
        if (br->byte_pos >= br->size) return 0;

        uint8_t bit = (br->buffer[br->byte_pos] >> (7 - br->bit_pos)) & 1;
        result = (result << 1) | bit;

        br->bit_pos++;
        if (br->bit_pos == 8) {
            br->bit_pos = 0;
            br->byte_pos++;
        }
    }

    return result;
}

/* ========== Huffman Tree Building ========== */

static huff_node_t* create_node(uint32_t freq, uint16_t symbol, uint8_t is_leaf) {
    huff_node_t *node = malloc(sizeof(huff_node_t));
    if (!node) return NULL;

    node->freq = freq;
    node->symbol = symbol;
    node->is_leaf = is_leaf;
    node->left = NULL;
    node->right = NULL;

    return node;
}

static void free_tree(huff_node_t *node) {
    if (!node) return;
    free_tree(node->left);
    free_tree(node->right);
    free(node);
}

/* Simple insertion sort for priority queue (efficient for small n) */
static void insert_sorted(huff_node_t **queue, int *size, huff_node_t *node) {
    int i = *size - 1;
    while (i >= 0 && queue[i]->freq > node->freq) {
        queue[i + 1] = queue[i];
        i--;
    }
    queue[i + 1] = node;
    (*size)++;
}

static huff_node_t* build_huffman_tree(uint32_t *frequencies, int num_symbols) {
    huff_node_t *queue[MAX_SYMBOLS * 2];
    int queue_size = 0;

    /* Create leaf nodes for symbols with non-zero frequency */
    for (int i = 0; i < num_symbols; i++) {
        if (frequencies[i] > 0) {
            huff_node_t *node = create_node(frequencies[i], i, 1);
            if (!node) return NULL;
            insert_sorted(queue, &queue_size, node);
        }
    }

    if (queue_size == 0) return NULL;
    if (queue_size == 1) {
        /* Single symbol - create dummy parent */
        huff_node_t *root = create_node(queue[0]->freq, 0, 0);
        root->left = queue[0];
        return root;
    }

    /* Build tree by combining lowest frequency nodes */
    while (queue_size > 1) {
        huff_node_t *left = queue[0];
        huff_node_t *right = queue[1];

        /* Remove two lowest frequency nodes */
        for (int i = 0; i < queue_size - 2; i++) {
            queue[i] = queue[i + 2];
        }
        queue_size -= 2;

        /* Create parent node */
        huff_node_t *parent = create_node(left->freq + right->freq, 0, 0);
        if (!parent) {
            free_tree(left);
            free_tree(right);
            return NULL;
        }
        parent->left = left;
        parent->right = right;

        /* Insert back into queue */
        insert_sorted(queue, &queue_size, parent);
    }

    return queue[0];
}

/* Generate Huffman codes by tree traversal */
static void generate_codes_recursive(huff_node_t *node, code_entry_t *codes,
                                     uint32_t code, uint8_t length) {
    if (!node) return;

    if (node->is_leaf) {
        codes[node->symbol].code = code;
        codes[node->symbol].length = length;
        return;
    }

    generate_codes_recursive(node->left, codes, code << 1, length + 1);
    generate_codes_recursive(node->right, codes, (code << 1) | 1, length + 1);
}

/* ========== Encoding ========== */

encoded_result_t* huffman_encode(const uint8_t *vector, size_t dimension) {
    if (!vector || dimension == 0) return NULL;

    /* Build frequency table */
    uint32_t frequencies[MAX_SYMBOLS] = {0};
    uint8_t max_symbol = 0;

    for (size_t i = 0; i < dimension; i++) {
        frequencies[vector[i]]++;
        if (vector[i] > max_symbol) max_symbol = vector[i];
    }

    int num_symbols = max_symbol + 1;

    /* Build Huffman tree */
    huff_node_t *root = build_huffman_tree(frequencies, num_symbols);
    if (!root) return NULL;

    /* Generate code lengths from tree */
    code_entry_t temp_codes[MAX_SYMBOLS] = {0};
    generate_codes_recursive(root, temp_codes, 0, 0);

    /* Convert to canonical Huffman codes */
    code_entry_t codes[MAX_SYMBOLS] = {0};
    uint32_t code = 0;

    for (int len = 1; len <= MAX_CODE_LENGTH; len++) {
        for (int symbol = 0; symbol < num_symbols; symbol++) {
            if (temp_codes[symbol].length == len) {
                codes[symbol].code = code;
                codes[symbol].length = len;
                code++;
            }
        }
        code <<= 1;
    }

    /* Calculate output size (conservative estimate) */
    size_t estimated_bits = dimension * 16;  /* Conservative */
    size_t estimated_bytes = (estimated_bits / 8) + 1024;  /* + header */

    bit_writer_t *bw = bit_writer_create(estimated_bytes);
    if (!bw) {
        free_tree(root);
        return NULL;
    }

    /* Write header */
    bit_writer_write(bw, dimension & 0xFFFF, 16);  /* Dimension (lower 16 bits) */
    bit_writer_write(bw, (dimension >> 16) & 0xFFFF, 16);  /* Dimension (upper 16 bits) */
    bit_writer_write(bw, num_symbols, 8);  /* Number of symbols */

    /* Write code lengths (canonical Huffman) */
    for (int i = 0; i < num_symbols; i++) {
        bit_writer_write(bw, codes[i].length, 5);  /* Max length = 32, needs 5 bits */
    }

    /* Write encoded data */
    for (size_t i = 0; i < dimension; i++) {
        uint8_t symbol = vector[i];
        if (codes[symbol].length > 0) {
            bit_writer_write(bw, codes[symbol].code, codes[symbol].length);
        }
    }

    bit_writer_flush(bw);

    /* Create result */
    encoded_result_t *result = malloc(sizeof(encoded_result_t));
    if (!result) {
        free(bw->buffer);
        free(bw);
        free_tree(root);
        return NULL;
    }

    result->data = bw->buffer;
    result->size = bw->byte_pos;

    free(bw);
    free_tree(root);

    return result;
}

/* ========== Decoding ========== */

int huffman_decode(const uint8_t *encoded_data, size_t encoded_size,
                   uint8_t *vector, size_t dimension) {
    if (!encoded_data || !vector || encoded_size < 5) return -1;

    bit_reader_t *br = bit_reader_create(encoded_data, encoded_size);
    if (!br) return -1;

    /* Read header */
    uint32_t dim_lower = bit_reader_read(br, 16);
    uint32_t dim_upper = bit_reader_read(br, 16);
    size_t stored_dim = (dim_upper << 16) | dim_lower;

    if (stored_dim != dimension) {
        free(br);
        return -1;
    }

    uint8_t num_symbols = bit_reader_read(br, 8);

    /* Read code lengths */
    uint8_t code_lengths[MAX_SYMBOLS] = {0};
    for (int i = 0; i < num_symbols; i++) {
        code_lengths[i] = bit_reader_read(br, 5);
    }

    /* Rebuild canonical Huffman codes */
    code_entry_t codes[MAX_SYMBOLS] = {0};
    uint32_t code = 0;

    for (int len = 1; len <= MAX_CODE_LENGTH; len++) {
        for (int symbol = 0; symbol < num_symbols; symbol++) {
            if (code_lengths[symbol] == len) {
                codes[symbol].code = code;
                codes[symbol].length = len;
                code++;
            }
        }
        code <<= 1;
    }

    /* Decode data using lookup */
    for (size_t i = 0; i < dimension; i++) {
        uint32_t code_val = 0;
        uint8_t code_len = 0;
        uint8_t found = 0;

        /* Read bits one at a time until we match a code */
        for (code_len = 1; code_len <= MAX_CODE_LENGTH; code_len++) {
            code_val = (code_val << 1) | bit_reader_read(br, 1);

            /* Check all symbols with this code length */
            for (int sym = 0; sym < num_symbols; sym++) {
                if (codes[sym].length == code_len && codes[sym].code == code_val) {
                    vector[i] = sym;
                    found = 1;
                    break;
                }
            }

            if (found) break;
        }

        if (!found) {
            free(br);
            return -1;
        }
    }

    free(br);

    return 0;
}

/* ========== Cleanup ========== */

void huffman_free(encoded_result_t *result) {
    if (!result) return;
    free(result->data);
    free(result);
}
