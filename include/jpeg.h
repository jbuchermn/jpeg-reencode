struct huffman_tree;
struct jpeg_huffman_table;
struct jpeg_quantisation_table;
struct jpeg_segment;
struct jpeg;

struct jpeg_huffman_table {
    struct huffman_tree* lum_dc;
    struct huffman_tree* lum_ac;
    struct huffman_tree* color_dc;
    struct huffman_tree* color_ac;
};

void jpeg_huffman_table_init(struct jpeg_huffman_table* table, struct jpeg_segment* from);

struct jpeg_quantisation_table {
    uint16_t* lum_values;
    uint16_t* color_values;
};

void jpeq_quantisation_table_init(struct jpeg_quantisation_table* table, struct jpeg_segment* from);


struct jpeg_segment {
    long size;
    unsigned char* data;

    struct jpeg* jpeg;
    struct jpeg_segment* next_segment;
};

void jpeg_segment_init(struct jpeg_segment* segment, struct jpeg* jpeg, long size, unsigned char* data);

struct jpeg {
    long size;
    unsigned char* data;

    struct jpeg_segment* first_segment;
    struct jpeg_quantisation_table* quantisation_table;
    struct jpeg_huffman_table* huffman_table;
};

void jpeg_init(struct jpeg* jpeg, long size, unsigned char* data);
void jpeg_print_segments(struct jpeg* jpeg);
struct jpeg_segment* jpeg_find_segment(struct jpeg* jpeg, unsigned char header);
