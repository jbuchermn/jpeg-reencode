#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "huffman.h"

void bitstream_init(struct bitstream* bitstream, unsigned char* data, long size, int escape_ff){
    bitstream->at = data;
    bitstream->at_bit = 0;
    bitstream->size_bytes = size;
    bitstream->escape_ff = escape_ff;
}

int bitstream_next(struct bitstream* bitstream, int* data){
    if(bitstream->size_bytes == 0) return 0;

    *data = ((*bitstream->at) >> bitstream->at_bit) & 1;

    if(bitstream->at_bit == 7){
        if(bitstream->escape_ff && 
                bitstream->size_bytes > 1 && 
                bitstream->at[0] == 0xFF &&
                bitstream->at[1] == 0x00
        ){
            bitstream->at++;
            bitstream->size_bytes--;
        }
        bitstream->at_bit = 0;
        bitstream->at++;
        bitstream->size_bytes--;
    }

    return 1;
}

void huffman_tree_init(struct huffman_tree* tree){
    tree->has_element = 0;
    tree->element = 0;
    tree->left = 0;
    tree->right = 0;
}

int huffman_tree_insert_goleft(struct huffman_tree* tree, int depth, uint8_t element){
    if(tree->has_element){
        return 0;
    }

    if(depth == 0){
        assert(!tree->left && !tree->right);
        tree->has_element = 1;
        tree->element = element;
        return 1;
    }

    if(!tree->left){
        struct huffman_tree* left = malloc(sizeof(struct huffman_tree));
        huffman_tree_init(left);
        tree->left = left;
    }

    if(huffman_tree_insert_goleft(tree->left, depth - 1, element)){
        return 1;
    }

    if(!tree->right){
        struct huffman_tree* right = malloc(sizeof(struct huffman_tree));
        huffman_tree_init(right);
        tree->right = right;
    }

    if(huffman_tree_insert_goleft(tree->right, depth - 1, element)){
        return 1;
    }else{
        return 0;
    }
}

void huffman_tree_print(struct huffman_tree* tree, char* prefix){
    if(tree->has_element){
        printf("%s: %d\n", prefix, tree->element);
    }
    if(tree->left){
        int len = strlen(prefix);
        char* p = malloc(len + 1);
        strcpy(p, prefix);
        p[len] = '0';
        p[len + 1] = '\0';

        huffman_tree_print(tree->left, p);

        free(p);
    }

    if(tree->right){
        int len = strlen(prefix);
        char* p = malloc(len + 1);
        strcpy(p, prefix);
        p[len] = '1';
        p[len + 1] = '\0';

        huffman_tree_print(tree->right, p);

        free(p);
    }
}

uint8_t huffman_tree_decode(struct huffman_tree* tree, struct bitstream* data){
    if(tree->has_element){
        return tree->element;
    }

    int bit;
    assert(bitstream_next(data, &bit));
    if(bit){
        assert(tree->right);
        return huffman_tree_decode(tree->right, data);
    }else{
        assert(tree->left);
        return huffman_tree_decode(tree->left, data);
    }
}
