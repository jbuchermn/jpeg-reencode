#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#include "jpeg.h"

int main(int argc, char** argv){
    if (argc < 3){
        printf("Usage jpeg-reencode file.jpg output.jpg");
        exit(1);
    }

    FILE* f = fopen(argv[1], "rb");
    fseek(f, 0, SEEK_END);
    long bytes_input = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* input_buffer = malloc(bytes_input);
    fread(input_buffer, bytes_input, 1, f);
    fclose(f);

    struct jpeg jpeg;

    clock_t init_time = clock();
    int status = jpeg_init(&jpeg, bytes_input, input_buffer);
    assert(status == 0);
    init_time = clock() - init_time;

    printf("Read header in %fms\n", 1000.*init_time/CLOCKS_PER_SEC);

    /* jpeg_print_sizes(&jpeg); */
    /* jpeg_print_segments(&jpeg); */
    /* jpeg_print_components(&jpeg); */
    /* jpeg_print_quantisation_tables(&jpeg); */
    /* jpeg_print_huffman_tables(&jpeg); */

    clock_t decode_time = clock();
    status = jpeg_decode_huffman(&jpeg);
    assert(status == 0);
    decode_time = clock() - decode_time;

    printf("Decoded: %ldkB in %fms\n", bytes_input/1000, 1000.*decode_time/CLOCKS_PER_SEC);

    unsigned char* output_buffer = malloc(bytes_input);
    memset(output_buffer, 0, bytes_input);

    for(int i=0; i<jpeg.n_quantisation_tables; i++){
        jpeg_quantisation_table_init_recompress(jpeg.quantisation_tables[i], 10.);
    }

    clock_t encode_time = clock();
    long bytes_header = jpeg_write_recompress_header(&jpeg, output_buffer, bytes_input);
    assert(bytes_header > 0);
    long bytes_scan = jpeg_encode_huffman(&jpeg, output_buffer + bytes_header, bytes_input - bytes_header);
    assert(bytes_scan > 0);
    encode_time = clock() - encode_time;

    long bytes_output = bytes_header + bytes_scan;

    printf("Encoded: %ldkB in %fms\n", bytes_output/1000, 1000.*encode_time/CLOCKS_PER_SEC);

    f = fopen(argv[2], "wb");  
    fwrite(output_buffer, 1, bytes_output, f);
    fclose(f);

    jpeg_destroy(&jpeg);

    free(input_buffer);
    free(output_buffer);
    return 0;
}
