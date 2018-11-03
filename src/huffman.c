#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "huffman.h"

void ibitstream_init(struct ibitstream* stream, void* data, int (*read)(void*, uint8_t*)){
    stream->data = data;
    stream->read = read;
}

int ibitstream_read(struct ibitstream* stream, uint8_t* bit){
    return (*stream->read)(stream->data, bit);
}

void obitstream_init(struct obitstream* stream, void* data, int (*write)(void*, uint8_t)){
    stream->data = data;
    stream->write = write;
}

int obitstream_write(struct obitstream* stream, uint8_t bit){
    return (*stream->write)(stream->data, bit);
}

void huffman_tree_init(struct huffman_tree* tree){
    tree->has_element = 0;
    tree->element = 0;
    tree->left = 0;
    tree->right = 0;
}

void huffman_tree_destroy(struct huffman_tree* tree){
    if(tree->left){
        huffman_tree_destroy(tree->left);
        free(tree->left);
        tree->left = 0;
    }
    if(tree->right){
        huffman_tree_destroy(tree->right);
        free(tree->right);
        tree->left = 0;
    }
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
    }else{
        int len = strlen(prefix);
        char* p = malloc(len + 1);
        strcpy(p, prefix);
        p[len + 1] = '\0';

        if(tree->left){
            p[len] = '0';
            huffman_tree_print(tree->left, p);
        }else{
            printf("\tNo left child\n");
        }

        if(tree->right){
            p[len] = '1';
            huffman_tree_print(tree->right, p);
        }else{
            printf("\tNo right child\n");
        }

        free(p);
    }
}

int huffman_tree_decode(struct huffman_tree* tree, struct ibitstream* stream, uint8_t* result){
    if(tree->has_element){
        *result = tree->element;
        return 0;
    }

    uint8_t bit;
    int status = ibitstream_read(stream, &bit);
    if(status){
        return status;
    }

    if(bit){
        if(!tree->right){
            return E_INVALID_CODE;
        }
        return huffman_tree_decode(tree->right, stream, result);
    }else{
        if(!tree->left){
            return E_INVALID_CODE;
        }
        return huffman_tree_decode(tree->left, stream, result);
    }
}

static void huffman_inv_init_rec(struct huffman_inv* inv, struct huffman_tree* from, uint16_t current, uint8_t current_size){
    if(from->has_element){
        assert(from->element < inv->size);
        inv->data[from->element].exists = 1;
        inv->data[from->element].size = current_size;
        inv->data[from->element].bits = current;
    }

    if(from->left){
        huffman_inv_init_rec(inv, from->left, current, current_size + 1);
    }
    
    if(from->right){
        current |= (1 << (15 - current_size));
        huffman_inv_init_rec(inv, from->right, current, current_size + 1);
    }
}

void huffman_inv_init(struct huffman_inv* inv, struct huffman_tree* from){
    /* For now */
    inv->size = 256;

    inv->data = malloc(256 * sizeof(struct huffman_inv_element));
    for(int i=0; i<256; i++){
        inv->data[i].exists = 0;
    }

    huffman_inv_init_rec(inv, from, 0, 0);
}

void huffman_inv_destroy(struct huffman_inv* inv){
    free(inv->data);
    inv->data = 0;
}

int huffman_inv_encode(struct huffman_inv* inv, struct obitstream* stream, uint8_t data){
    if(data >= inv->size || !inv->data[data].exists){
        printf("%d\n", data);
        return E_NO_CODE;
    }

    uint16_t val = inv->data[data].bits;
    for(int i=0; i<inv->data[data].size; i++){
        uint8_t bit = (val >> (15 - i)) & 1;
        int status = obitstream_write(stream, bit);
        if(status){
            return status;
        }
    }

    return 0;
}
