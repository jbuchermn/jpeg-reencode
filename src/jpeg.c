#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "jpeg.h"
#include "huffman.h"

static uint16_t uint16_from_uchar(unsigned char* at){
    return at[1] + 256*at[0];
}

int jpeg_huffman_table_init(struct jpeg_huffman_table* table, unsigned char* at){
    unsigned char* at_orig = at;
    uint8_t info = *at;
    at++;

    table->class = (info & 0xF0) / 16;
    table->id = info & 0x0F;
    table->huffman_tree = malloc(sizeof(struct huffman_tree));
    huffman_tree_init(table->huffman_tree);


    int n_elements[16];
    for(int i=0; i<16; i++){
        n_elements[i] = *at;
        at++;
    }

    for(int depth=1; depth<=16; depth++){
        for(int i=0; i<n_elements[depth - 1]; i++){
            uint8_t element = *at;
            at++;

            assert(huffman_tree_insert_goleft(table->huffman_tree, depth, element));
        }
    }

    return at - at_orig;
}

int jpeg_quantisation_table_init(struct jpeg_quantisation_table* table, unsigned char* at){
    unsigned char* at_orig = at;

    uint8_t info = *at;
    at++;

    int double_precision = (info & 0xF0);
    table->id = info & 0x0F;

    for(int i=0; i<64; i++){
        if(!double_precision){
            table->values[i] = *at;
            at++;
        }else{
            table->values[i] = uint16_from_uchar(at);
            at += 2;
        }
    }

    return at - at_orig;
}

int jpeg_component_init(struct jpeg_component* component, unsigned char* at){
    component->id = at[0];
    uint8_t huffman = at[1];
    component->dc_huffman_id = (huffman & 0xF0) / 16;
    component->ac_huffman_id = huffman & 0x0F;
    return 2;
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

    for(int i=0; i<MAX_TABLES; i++){
        jpeg->quantisation_tables[i] = 0;
        jpeg->ac_huffman_tables[i] = 0;
        jpeg->dc_huffman_tables[i] = 0;
    }

    struct jpeg_segment* seg = 0;
    for(long i=0; i<size; i++){
        if(data[i] == 0xFF){
            // Start of segment marker
            if(i<(size-1) && 
                    data[i+1] != 0x00 && // FF00 escapes FF
                    data[i+1] != 0xFF && // FFFF is not a marker
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

                // DA, DB, C0, C4 store length as uint16_t after header
                if(i < (size - 3) && 
                        (data[i+1] == 0xDA || data[i+1] == 0xDB || data[i+1] == 0xC0 || data[i+1] == 0xC4)
                ){
                    // We always include the marker in the size
                    seg->size = uint16_from_uchar(data + i + 2) + 2;
                    i += seg->size - 2;
                }

                // Start of scan marker
                if(seg->data[1] == 0xDA){
                    break;
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
    unsigned char* at = quantisation->data + 4;
    while(at - quantisation->data < quantisation->size){
        struct jpeg_quantisation_table* quantisation_table = malloc(sizeof(struct jpeg_quantisation_table));
        at += jpeg_quantisation_table_init(quantisation_table, at);
        assert(quantisation_table->id < MAX_TABLES);
        jpeg->quantisation_tables[quantisation_table->id] = quantisation_table;
    }
    assert(at - quantisation->data == quantisation->size);

    // Huffman
    struct jpeg_segment* huffman = jpeg_find_segment(jpeg, 0xC4);
    assert(huffman);
    at = huffman->data + 4;
    while(at - huffman->data < huffman->size){
        struct jpeg_huffman_table* huffman_table = malloc(sizeof(struct jpeg_huffman_table));
        at += jpeg_huffman_table_init(huffman_table, at);
        assert(huffman_table->id < MAX_TABLES);
        if(huffman_table->class){
            jpeg->ac_huffman_tables[huffman_table->id] = huffman_table;
        }else{
            jpeg->dc_huffman_tables[huffman_table->id] = huffman_table;
        }
    }
    assert(at - huffman->data == huffman->size);

    // Width, height, ...
    struct jpeg_segment* sof = jpeg_find_segment(jpeg, 0xC0);
    assert(sof);
    assert(sof->size >= 10);
    jpeg->height = uint16_from_uchar(sof->data + 5);
    jpeg->width = uint16_from_uchar(sof->data + 7);
    jpeg->n_components = *(sof->data + 9);

    // Set start of scan data
    struct jpeg_segment* sos = jpeg_find_segment(jpeg, 0xDA);
    assert(sos);
    jpeg->scan_data = sos->data + sos->size;
    assert(jpeg->n_components == *(sos->data + 4));
    at = sos->data + 5;
    for(int i=0; at - sos->data < sos->size && i < jpeg->n_components; i++){
        struct jpeg_component* component = malloc(sizeof(struct jpeg_component));
        at += jpeg_component_init(component, at);
        jpeg->components[component->id - 1] = component;
    }
    // Three empty bytes before scan data
    assert(at + 3 - sos->data == sos->size);
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
void jpeg_decode_huffman(struct jpeg* jpeg){


}
