#ifndef HUFFMAN_H
#define HUFFMAN_H 

#include <stdint.h>

#define E_INVALID_CODE -1

struct huffman_tree {
    int has_element;
    uint8_t element;

    struct huffman_tree* left;
    struct huffman_tree* right;
};

struct ibitstream {
    void* data;
    int (*read)(void*, uint8_t*);
};

void ibitstream_init(struct ibitstream* stream, void* data, int (*read)(void*, uint8_t*));
int ibitstream_read(struct ibitstream* stream, uint8_t* bit);

struct obitstream {
    void* data;
    int (*write)(void*, uint8_t);
};

void obitstream_init(struct obitstream* stream, void* data, int (*write)(void*, uint8_t));
int obitstream_write(struct obitstream* stream, uint8_t bit);

void huffman_tree_init(struct huffman_tree* tree);
int huffman_tree_insert_goleft(struct huffman_tree* tree, int depth, uint8_t element);
void huffman_tree_print(struct huffman_tree* tree, char* prefix);

int huffman_tree_decode(struct huffman_tree* tree, struct ibitstream* stream, uint8_t* result);

#endif
