#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "jpeg.h"
#include "huffman.h"

static uint16_t uint16_from_uchar(unsigned char* at){
    return at[1] + 256*at[0];
}

void jpeg_huffman_table_init(struct jpeg_huffman_table* table, struct jpeg_segment* from){
    table->lum_dc = malloc(sizeof(struct huffman_tree));
    huffman_tree_init(table->lum_dc);
    table->lum_ac = malloc(sizeof(struct huffman_tree));
    huffman_tree_init(table->lum_ac);
    table->color_dc = malloc(sizeof(struct huffman_tree));
    huffman_tree_init(table->color_dc);
    table->color_ac = malloc(sizeof(struct huffman_tree));
    huffman_tree_init(table->color_ac);

    unsigned char* at = from->data + 4;
    int found_lum_dc = 0;
    int found_lum_ac = 0;
    int found_color_dc = 0;
    int found_color_ac = 0;

    while(at - from->data < from->size){
        uint8_t info = *at;
        at++;

        int class = (info & 0xF0) / 16;
        int id = info & 0x0F;

        struct huffman_tree* target;
        if(class == 0 && id == 0){
            target = table->lum_dc;
            found_lum_dc++;
        }else if(class == 1 && id == 0){
            target = table->lum_ac;
            found_lum_ac++;
        }else if(class == 0 && id == 1){
            target = table->color_dc;
            found_color_dc++;
        }else if(class == 1 && id == 1){
            target = table->color_ac;
            found_color_ac++;
        } 

        int n_elements[16];
        for(int i=0; i<16; i++){
            n_elements[i] = *at;
            at++;
        }

        for(int depth=1; depth<=16; depth++){
            for(int i=0; i<n_elements[depth - 1]; i++){
                uint8_t element = *at;
                at++;

                assert(huffman_tree_insert_goleft(target, depth, element));
            }
        }
    }

    assert(from->data + from->size == at && found_lum_dc == 1 && found_lum_ac == 1 &&
            found_color_dc == 1 && found_color_ac == 1);
}

void jpeg_quantisation_table_init(struct jpeg_quantisation_table* table, struct jpeg_segment* from){
    table->lum_values = malloc(64 * sizeof(uint16_t));
    table->color_values = malloc(64 * sizeof(uint16_t));

    int found_lum = 0;
    int found_color = 0;

    unsigned char* at = from->data + 4;
    while(at - from->data < from->size){
        uint8_t info = *at;
        at++;

        int double_precision = (info & 0xF0);
        int id = info & 0x0F;
        uint16_t* target;
        if(id == 0){
            target = table->lum_values;
            found_lum++;
        }else{
            target = table->color_values;
            found_color++;
        }

        for(int i=0; i<64; i++){
            if(!double_precision){
                target[i] = *at;
                at++;
            }else{
                target[i] = uint16_from_uchar(at);
                at += 2;
            }
            assert(at - from->data <= from->size);
        }
    }

    assert(from->data + from->size == at && found_lum == 1 && found_color == 1);
}

void jpeg_segment_init(struct jpeg_segment* segment, struct jpeg* jpeg, long size, unsigned char* data){
    segment->size = size;
    segment->data = data;
    segment->jpeg = jpeg;
    segment->next_segment = 0;

    // Need at least a header
    assert(size > 1);
}

void jpeg_init(struct jpeg* jpeg, long size, unsigned char* data){
    jpeg->size = size;
    jpeg->data = data;

    struct jpeg_segment* seg = 0;
    for(long i=0; i<size; i++){
        if(data[i] == 0xFF){
            // Start of segment marker
            if(i<(size-1) && 
                    data[i+1] != 0x00 && // FF00 escapes FF
                    data[i+1] != 0xFF && // FFFF ?
                    (data[i+1] < 0xD0 || data[i+1] > 0xD7) // FFD0 - FFD8 are restart markers
            ){
                struct jpeg_segment* next_seg = malloc(sizeof(struct jpeg_segment));
                jpeg_segment_init(next_seg, jpeg, 2, data + i);
                if(seg){
                    seg->size = data + i - seg->data;
                    seg->next_segment = next_seg;
                }else{
                    jpeg->first_segment = next_seg;
                }
                seg = next_seg;

                // DB, C4 segment contain unescaped FF; length is stored a suint16_t after header
                if(i < (size - 3) && 
                        (data[i+1] == 0xDB || data[i+1] == 0xC4)
                ){
                    i += uint16_from_uchar(data + i + 2);
                }
            }
            i++;
        }
    }

    // Check baseline
    if(!jpeg_find_segment(jpeg, 0xC0) ||
            jpeg_find_segment(jpeg, 0xC1) ||
            jpeg_find_segment(jpeg, 0xC2) ||
            jpeg_find_segment(jpeg, 0xC3)){
        printf("Not baseline encoded");
        exit(1);
    }

    // Quantisation
    struct jpeg_segment* quantisation = jpeg_find_segment(jpeg, 0xDB);
    assert(quantisation);
    jpeg->quantisation_table = malloc(sizeof(struct jpeg_quantisation_table));
    jpeg_quantisation_table_init(jpeg->quantisation_table, quantisation);

    // Huffman
    struct jpeg_segment* huffman = jpeg_find_segment(jpeg, 0xC4);
    assert(huffman);
    jpeg->huffman_table = malloc(sizeof(struct jpeg_huffman_table));
    jpeg_huffman_table_init(jpeg->huffman_table, huffman);
}


void jpeg_print_segments(struct jpeg* jpeg){
    printf("------ Segment summary ----\n");
    for(struct jpeg_segment* cur = jpeg->first_segment; cur; cur = cur->next_segment){
        printf("\tSegment(%02X) of length %ld\n", cur->data[1], cur->size);
    }
}

struct jpeg_segment* jpeg_find_segment(struct jpeg* jpeg, unsigned char header){
    for(struct jpeg_segment* cur = jpeg->first_segment; cur; cur = cur->next_segment){
        if(cur->data[1] == header) return cur;
    }

    return 0;
}
