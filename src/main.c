#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "jpeg.h"

int main(int argc, char** argv){
    if (argc < 2){
        printf("Usage jpeg-quality-changer file.jpg");
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
    int status = jpeg_init(&jpeg, bytes_input, input_buffer);
    assert(status == 0);

    jpeg_print_sizes(&jpeg);
    jpeg_print_segments(&jpeg);
    jpeg_print_components(&jpeg);
    jpeg_print_quantisation_tables(&jpeg);
    jpeg_print_huffman_tables(&jpeg);

    clock_t decode_time = clock();
    status = jpeg_decode_huffman(&jpeg);
    printf("Result: %d\n", status);
    assert(status == 0);
    decode_time = clock() - decode_time;

    printf("Successfully decoded JPEG (%dkB) in %fms\n", bytes_input/1000, 1000.*decode_time/CLOCKS_PER_SEC);

    unsigned char* output_buffer = malloc(bytes_input);
    memset(output_buffer, 0, bytes_input);

    clock_t encode_time = clock();
    int bytes_output = jpeg_encode_huffman(&jpeg, output_buffer, bytes_input);
    printf("Result: %d\n", bytes_output);
    assert(bytes_output > 0);
    encode_time = clock() - encode_time;

    printf("Successfully encoded JPEG (%dkB) in %fms\n", bytes_output/1000, 1000.*encode_time/CLOCKS_PER_SEC);

    f = fopen("./tmp.jpg", "wb");  
    fwrite(output_buffer, 1, bytes_output, f);
    fclose(f);

    printf("Wrote to tmp.jpg\n");

    free(input_buffer);
    free(output_buffer);
    return 0;
}
