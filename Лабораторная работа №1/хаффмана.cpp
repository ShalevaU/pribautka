//Shaleva
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NODES 512

typedef struct Node {
    unsigned char ch;
    int freq;           // используем int для согласованности
    struct Node* left, * right;
} Node;

Node* new_node(unsigned char ch, int freq, Node* left, Node* right) {
    Node* node = (Node*)malloc(sizeof(Node));
    if (!node) return NULL;
    node->ch = ch;
    node->freq = freq;
    node->left = left;
    node->right = right;
    return node;
}

void free_tree(Node* root) {
    if (!root) return;
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

int node_cmp(const void* a, const void* b) {
    Node* x = *(Node**)a;
    Node* y = *(Node**)b;
    if (x->freq < y->freq) return -1;
    if (x->freq > y->freq) return 1;
    return 0;
}

Node* build_tree(int freq[256]) {
    Node* pool[MAX_NODES];
    int n = 0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            pool[n++] = new_node(i, freq[i], NULL, NULL);
        }
    }
    if (n == 0) return NULL;
    if (n == 1) {
        // Добавляем фиктивный узел с нулевой частотой, но другим символом
        pool[n++] = new_node(255, 0, NULL, NULL);
    }

    while (n > 1) {
        qsort(pool, n, sizeof(Node*), (int (*)(const void*, const void*))node_cmp);
        Node* left = pool[0];
        Node* right = pool[1];
        Node* parent = new_node(0, left->freq + right->freq, left, right);
        for (int i = 2; i < n; i++) {
            pool[i - 2] = pool[i];
        }
        pool[n - 2] = parent;
        n--;
    }
    return pool[0];
}

void save_tree(FILE* out, Node* node) {
    if (!node) return;
    if (!node->left && !node->right) {
        fputc(1, out);
        fputc(node->ch, out);
    }
    else {
        fputc(0, out);
        save_tree(out, node->left);
        save_tree(out, node->right);
    }
}

Node* load_tree(FILE* in) {
    int marker = fgetc(in);
    if (marker == EOF) return NULL;
    if (marker == 1) {
        int ch = fgetc(in);
        if (ch == EOF) return NULL;
        return new_node((unsigned char)ch, 0, NULL, NULL);
    }
    else if (marker == 0) {
        Node* left = load_tree(in);
        Node* right = load_tree(in);
        if (!left || !right) return NULL;
        return new_node(0, 0, left, right);
    }
    return NULL;
}

void build_codes(Node* root, char codes[256][256], char* prefix, int len) {
    if (!root) return;
    if (!root->left && !root->right) {
        prefix[len] = '\0';
        strcpy(codes[root->ch], prefix);
        return;
    }
    prefix[len] = '0';
    build_codes(root->left, codes, prefix, len + 1);
    prefix[len] = '1';
    build_codes(root->right, codes, prefix, len + 1);
}

unsigned char bits_to_byte(const char* bits) {
    unsigned char byte = 0;
    for (int i = 0; i < 8; i++) {
        if (bits[i] == '1') {
            byte |= (1U << (7 - i));
        }
    }
    return byte;
}

void byte_to_bits(unsigned char byte, char* bits) {
    for (int i = 0; i < 8; i++) {
        bits[i] = (byte & (1U << (7 - i))) ? '1' : '0';
    }
    bits[8] = '\0';
}

int encode(const char* in_name, const char* out_name) {
    FILE* in = fopen(in_name, "rb");
    if (!in) {
        perror("Cannot open input file");
        return 1;
    }

    int freq[256] = { 0 };
    unsigned char c;
    int total = 0;
    while (fread(&c, 1, 1, in) == 1) {
        freq[c]++;
        total++;
    }
    fclose(in);

    if (total == 0) {
        printf("Error: Input file is empty\n");
        return 1;
    }

    Node* root = build_tree(freq);
    if (!root) {
        printf("Failed to build Huffman tree\n");
        return 1;
    }

    char codes[256][256] = { {0} };
    char prefix[256] = {0};
    build_codes(root, codes, prefix, 0);

    FILE* out = fopen(out_name, "wb");
    if (!out) {
        perror("Cannot create output file");
        free_tree(root);
        return 1;
    }

    // Записываем total как 8 байт (фиксированный размер)
    fwrite(&total, sizeof(int), 1, out);
    save_tree(out, root);

    in = fopen(in_name, "rb");
    if (!in) {
        perror("Cannot reopen input file");
        fclose(out);
        free_tree(root);
        return 1;
    }

    char bit_buffer[256] = { 0 };
    int bit_count = 0;
    unsigned char output_byte;

    while (fread(&c, 1, 1, in) == 1) {
        const char* code = codes[c];
        int len = strlen(code);
        if (bit_count + len >= 256) {
            fprintf(stderr, "Bit buffer overflow\n");
            fclose(in); fclose(out); free_tree(root);
            return 1;
        }
        strcat(bit_buffer, code);
        bit_count += len;

        while (bit_count >= 8) {
            output_byte = bits_to_byte(bit_buffer);
            fwrite(&output_byte, 1, 1, out);
            memmove(bit_buffer, bit_buffer + 8, bit_count - 8 + 1);
            bit_count -= 8;
        }
    }

    // Записываем остаток (дополняем нулями до 8 бит)
    if (bit_count > 0) {
        while (bit_count < 8) {
            bit_buffer[bit_count++] = '0';
        }
        bit_buffer[8] = '\0';
        output_byte = bits_to_byte(bit_buffer);
        fwrite(&output_byte, 1, 1, out);
    }

    fclose(in);
    fclose(out);
    free_tree(root);
    printf("Encoding completed. Total symbols: %llu\n", (unsigned long long)total);
    return 0;
}

int decode(const char* in_name, const char* out_name) {
    FILE* in = fopen(in_name, "rb");
    if (!in) {
        perror("Cannot open encoded file");
        return 1;
    }

    int total;
    if (fread(&total, sizeof(int), 1, in) != 1) {
        printf("Invalid header in encoded file\n");
        fclose(in);
        return 1;
    }

    Node* root = load_tree(in);
    if (!root) {
        printf("Failed to load Huffman tree\n");
        fclose(in);
        return 1;
    }

    FILE* out = fopen(out_name, "wb");
    if (!out) {
        perror("Cannot create decoded file");
        free_tree(root);
        fclose(in);
        return 1;
    }

    Node* current = root;
    int decoded = 0;
    unsigned char byte;
    char bits[9];

    while (fread(&byte, 1, 1, in) == 1 && decoded < total) {
        byte_to_bits(byte, bits);
        for (int i = 0; i < 8 && decoded < total; i++) {
            if (bits[i] == '0') {
                current = current->left;
            }
            else {
                current = current->right;
            }

            if (!current->left && !current->right) {
                fputc(current->ch, out);
                current = root;
                decoded++;
            }
        }
    }

    fclose(in);
    fclose(out);
    free_tree(root);
    printf("Decoding completed. Decoded symbols: %llu\n", (unsigned long long)decoded);
    return 0;
}

int compare_files(const char* f1, const char* f2) {
    FILE* F1 = fopen(f1, "rb");
    FILE* F2 = fopen(f2, "rb");
    if (!F1 || !F2) {
        if (F1) fclose(F1);
        if (F2) fclose(F2);
        return 0;
    }
    int A, B;
    while (1) {
        A = fgetc(F1);
        B = fgetc(F2);
        if (A != B) {
            fclose(F1);
            fclose(F2);
            return 0;
        }
        if (A == EOF || B == EOF) break;
    }
    fclose(F1);
    fclose(F2);
    return 1;
}

int main(void) {
    const char* input_file = "text.txt";
    const char* encoded_file = "encoded.txt";
    const char* decoded_file = "restored.txt";
    int choice;

    // Проверка существования входного файла
    FILE* check = fopen(input_file, "rb");
    if (!check) {
        printf("ERROR: File '%s' not found in current directory!\n", input_file);
        printf("Please create 'text.txt' with some content and try again.\n");
        return 1;
    }
    fclose(check);

    printf("=== Huffman Coder ===\n");
    printf("[1] Encode file\n");
    printf("[2] Decode file\n");
    printf("Select option: ");
    scanf("%d", &choice);

    if (choice != 1 && choice != 2) {
        printf("Invalid choice.\n");
        return 1;
    }

    if (choice == 1) {
        printf("\n[1] Encoding to '%s'...\n", encoded_file);
        if (encode(input_file, encoded_file) != 0) {
            fprintf(stderr, "Encoding failed!\n");
            return 1;
        }
    }
    else if (choice == 2) {
        printf("\n[2] Decoding to '%s'...\n", decoded_file);
        if (decode(encoded_file, decoded_file) != 0) {
            fprintf(stderr, "Decoding failed!\n");
            return 1;
        }
        printf("\n Verifying integrity...\n");
        if (compare_files(input_file, decoded_file)) {
            fprintf(stderr, "the file matches the decoded one\n");
        }
        else
            printf("unluck\n");
    }

    return 0;
}