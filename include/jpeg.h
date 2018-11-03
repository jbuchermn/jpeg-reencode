struct huffman_tree;
struct jpeg_quantisation_table;
struct jpeg_segment;
struct jpeg;

#define MAX_TABLES 4
#define MAX_COMPONENTS 4

struct jpeg_segment {
    long size;
    unsigned char* data;

    struct jpeg* jpeg;
    struct jpeg_segment* next_segment;
};

void jpeg_segment_init(struct jpeg_segment* segment, struct jpeg* jpeg, long size, unsigned char* data);

struct jpeg_huffman_table{
    int id;
    int class;
    struct huffman_tree* huffman_tree;
};

int jpeg_huffman_table_init(struct jpeg_huffman_table* table, unsigned char* at);

struct jpeg_quantisation_table {
    int id;
    uint16_t values[64];
};

int jpeq_quantisation_table_init(struct jpeg_quantisation_table* table, unsigned char* at);

struct jpeg_component {
    int id;
    int vertical_sampling;
    int horizontal_sampling;
    int quantisation_id;
    int dc_huffman_id;
    int ac_huffman_id;
};

int jpeg_component_init(struct jpeg_component* component, unsigned char* at);
int jpeg_component_add_huffman(struct jpeg_component* component, unsigned char* at);

struct jpeg_block {
    int component_id;
    int8_t values[64];
};

void jpeg_block_init(struct jpeg_block* block, int component_id);

struct jpeg {
    long size;
    unsigned char* data;

    int width;
    int height;

    struct jpeg_segment* first_segment;

    int n_components;
    struct jpeg_component* components[MAX_COMPONENTS];

    int n_quantisation_tables;
    struct jpeg_quantisation_table* quantisation_tables[MAX_TABLES];

    int n_ac_huffman_tables;
    struct jpeg_huffman_table* ac_huffman_tables[MAX_TABLES];

    int n_dc_huffman_tables;
    struct jpeg_huffman_table* dc_huffman_tables[MAX_TABLES];

    int n_blocks;
    struct jpeg_block* blocks;
};

int jpeg_init(struct jpeg* jpeg, long size, unsigned char* data);

void jpeg_print_sizes(struct jpeg* jpeg);
void jpeg_print_segments(struct jpeg* jpeg);
void jpeg_print_components(struct jpeg* jpeg);
void jpeg_print_quantisation_tables(struct jpeg* jpeg);
void jpeg_print_huffman_tables(struct jpeg* jpeg);

struct jpeg_segment* jpeg_find_segment(struct jpeg* jpeg, unsigned char header, struct jpeg_segment* after);
int jpeg_decode_huffman(struct jpeg* jpeg);
