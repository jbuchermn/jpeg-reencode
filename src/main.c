#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#include "jpeg.h"

#define REENCODE

int main(int argc, char** argv){
    if (argc < 4){
        printf("Usage jpeg-reencode <factor> file.jpg output.jpg");
        exit(1);
    }

    float factor = atof(argv[1]);

    FILE* f = fopen(argv[2], "rb");
    fseek(f, 0, SEEK_END);
    long bytes_input = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* input_buffer = malloc(bytes_input);
    fread(input_buffer, bytes_input, 1, f);
    fclose(f);

    struct jpeg jpeg;

    clock_t init_time = clock();
    int status = jpeg_init(&jpeg, bytes_input, input_buffer);
    if(status){
        printf("Error: %d\n", status);
        exit(1);
    }
    init_time = clock() - init_time;

    printf("Read header in %fms\n", 1000.*init_time/CLOCKS_PER_SEC);
    printf("Image size: %dx%d, %dMP\n", jpeg.width, jpeg.height, jpeg.width * jpeg.height / 1000000);

    /* jpeg_print_sizes(&jpeg); */
    /* jpeg_print_segments(&jpeg); */
    /* jpeg_print_components(&jpeg); */
    /* jpeg_print_quantisation_tables(&jpeg); */
    /* jpeg_print_huffman_tables(&jpeg); */

    unsigned char* output_buffer = malloc(bytes_input);
    memset(output_buffer, 0, bytes_input);

    for(int i=0; i<jpeg.n_quantisation_tables; i++){
        jpeg_quantisation_table_init_recompress(jpeg.quantisation_tables[i], factor);
    }

    clock_t header_time = clock();
    long bytes_header = jpeg_write_recompress_header(&jpeg, output_buffer, bytes_input);
    if(bytes_header < 0){
        printf("Error: %ld\n", bytes_header);
        exit(1);
    }
    header_time = clock() - header_time;
    printf("Wrote header in %fms\n", 1000.*header_time/CLOCKS_PER_SEC);

#ifndef REENCODE
    clock_t decode_time = clock();
    status = jpeg_decode_huffman(&jpeg);
    if(status == E_SIZE_MISMATCH){
        printf("Error: Wrong number of MCUs\n");
        exit(1);
    }else if(status){
        printf("Error: %d\n", status);
        exit(1);
    }
    decode_time = clock() - decode_time;

    printf("Decoded: %ldkB in %fms\n", bytes_input/1000, 1000.*decode_time/CLOCKS_PER_SEC);

    clock_t encode_time = clock();
    long bytes_scan = jpeg_encode_huffman(&jpeg, output_buffer + bytes_header, bytes_input - bytes_header);
    if(bytes_scan < 0){
        printf("Error: %ld\n", bytes_scan);
        exit(1);
    }
    encode_time = clock() - encode_time;

    long bytes_output = bytes_header + bytes_scan;

    printf("Encoded: %ldkB in %fms\n", bytes_output/1000, 1000.*encode_time/CLOCKS_PER_SEC);

    printf("\t\t\t\t\t %fMbps\n",
            bytes_input * 8 * CLOCKS_PER_SEC/(decode_time + encode_time) * 1.e-6
    );

#else

    clock_t reencode_time = clock();
    long bytes_scan = jpeg_reencode_huffman(&jpeg, output_buffer + bytes_header, bytes_input - bytes_header);
    if(bytes_scan < 0){
        printf("Error: %ld\n", bytes_scan);
        exit(1);
    }
    reencode_time = clock() - reencode_time;

    long bytes_output = bytes_header + bytes_scan;

    printf("Reencoded: %ldkB to %ldkB in %fms\n",
            bytes_input/1000,
            bytes_output/1000,
            1000.*reencode_time/CLOCKS_PER_SEC
    );

    printf("\t\t\t\t\t %fMbps\n",
            bytes_input * 8 * CLOCKS_PER_SEC/reencode_time * 1.e-6
    );

#endif

    f = fopen(argv[3], "wb");  
    fwrite(output_buffer, 1, bytes_output, f);
    fclose(f);

    jpeg_destroy(&jpeg);

    free(input_buffer);
    free(output_buffer);
    return 0;
}
