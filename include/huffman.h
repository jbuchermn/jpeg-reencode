struct huffman_tree {
    int has_element;
    uint8_t element;

    struct huffman_tree* left;
    struct huffman_tree* right;
};

void huffman_tree_init(struct huffman_tree* tree);
int huffman_tree_insert_goleft(struct huffman_tree* tree, int depth, uint8_t element);
void huffman_tree_print(struct huffman_tree* tree, char* prefix);
