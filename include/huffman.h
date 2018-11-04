#ifndef HUFFMAN_H
#define HUFFMAN_H 

#include <stdint.h>

#define E_INVALID_CODE -16
#define E_NO_CODE -17

struct jpeg_ibitstream;
struct jpeg_obitstream;

struct huffman_tree {
    int has_element;
    uint8_t element;

    struct huffman_tree* left;
    struct huffman_tree* right;
};

void huffman_tree_init(struct huffman_tree* tree);
void huffman_tree_destroy(struct huffman_tree* tree);

int huffman_tree_insert_goleft(struct huffman_tree* tree, int depth, uint8_t element);
void huffman_tree_print(struct huffman_tree* tree, char* prefix);

int huffman_tree_decode(struct huffman_tree* tree, struct jpeg_ibitstream* stream, uint8_t* result);

struct huffman_inv_element {
    uint8_t exists;
    uint8_t size;
    uint16_t bits;
};

struct huffman_inv {
    int size;
    struct huffman_inv_element* data;
};

void huffman_inv_init(struct huffman_inv* inv, struct huffman_tree* from);
void huffman_inv_destroy(struct huffman_inv* inv);
int huffman_inv_encode(struct huffman_inv* inv, struct jpeg_obitstream* stream, uint8_t data);


#endif
