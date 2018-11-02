#include <stdint.h>

struct bitstream {
    int jpeg;
    unsigned char* at;
    uint8_t at_bit;
    long size_bytes;
};

void bitstream_init(struct bitstream* bitstream, unsigned char* data, long size, int escape_ff);
int bitstream_next(struct bitstream* bitstream, int* data);

struct huffman_tree {
    int has_element;
    uint8_t element;

    struct huffman_tree* left;
    struct huffman_tree* right;
};

void huffman_tree_init(struct huffman_tree* tree);
int huffman_tree_insert_goleft(struct huffman_tree* tree, int depth, uint8_t element);
void huffman_tree_print(struct huffman_tree* tree, char* prefix);
uint8_t huffman_tree_decode(struct huffman_tree* tree, struct bitstream* data);
