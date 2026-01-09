/*
 * main.c
 *
 * Top-level software controller for FPGA bitstream compression
 * and lightweight protection using custom AXI4-Lite IP cores.
 *
 * Target platform : ZedBoard (Zynq-7000)
 * Toolchain       : Vivado / Vitis 2023.x
 *
 * This application orchestrates all hardware-accelerated stages:
 * bit parsing, frequency counting, Huffman encoding, bundling,
 * and encryption.
 */
#include "xparameters.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "ff.h"
#include "sdCard.h"
#include <stdlib.h>
#include <string.h>
#include <sleep.h>
#include "xtime_l.h"

// ======================= IP BASE ADDRESSES ================================
#define BITPARSER_IP_BASE     0x43C00000
#define FREQ_COUNTER_IP_BASE  0x43C10000
#define HUFFMAN_IP_BASE       0x43C20000
#define ENCRYPT_IP_BASE       0x43C30000   // New encryption IP

// ======================= FREQUENCY COUNTER REGISTERS ======================
#define REG_SYMBOL        (FREQ_COUNTER_IP_BASE + 0x00)
#define REG_LOAD          (FREQ_COUNTER_IP_BASE + 0x04)
#define REG_DONE          (FREQ_COUNTER_IP_BASE + 0x08)
#define REG_FREQ          (FREQ_COUNTER_IP_BASE + 0x0C)
#define REG_ADDR          (FREQ_COUNTER_IP_BASE + 0x10)

// ======================= HUFFMAN ENCODER REGISTERS ========================
#define REG_SYMBOL_IN     0x00
#define REG_VALID_IN      0x04
#define REG_VALID_OUT     0x08
#define REG_CODEWORD      0x0C
#define REG_CODELEN       0x10
#define REG_LOAD_SYMBOL   0x14
#define REG_LOAD_CODE     0x18
#define REG_LOAD_LENGTH   0x1C
#define REG_LOAD_VALID    0x20
#define REG_LOAD_DONE     0x24

#define IP_WRITE(o,v)     Xil_Out32(HUFFMAN_IP_BASE + (o), (v))
#define IP_READ(o)        Xil_In32 (HUFFMAN_IP_BASE + (o))

// ======================= ENCRYPTION IP REGISTERS ==========================
#define ENC_REG_DATA_IN   0x00
#define ENC_REG_KEY       0x04
#define ENC_REG_DATA_OUT  0x08

#define ENC_WRITE(o,v)    Xil_Out32(ENCRYPT_IP_BASE + (o), (v))
#define ENC_READ(o)       Xil_In32 (ENCRYPT_IP_BASE + (o))

// ======================= MEMORY BUFFERS ===================================
#define MEMORY_BASE_ADDR  0x10000000
#define SYMBOL_BUF_ADDR   (MEMORY_BASE_ADDR)
#define FREQ_BUF_ADDR     (MEMORY_BASE_ADDR + 0x10000)

#define BUFFER_SIZE       4096
#define MAX_LINE_LEN      32
#define MAX_SYMBOLS       256

// ======================= FILE NAMES =======================================

// Input / Initial parsing
#define INPUT_FILE        "ZFO.rbt"
#define HEADER_FILE       "HEAZFO.txt"
#define PARSED_FILE       "PARZFO.txt"

// Frequency counter outputs
#define FREQ_FILE         "FREZFO.txt"
#define SYMBOL_FILE       "SYMZFO.txt"
#define COUNT_FILE        "COUZFO.txt"

// Codebook generation outputs
#define SYMIN_FILE        "SYMIZFO.txt"
#define CODEWIN_FILE      "CODEWZFO.txt"
#define CODELEN_FILE      "CODLZFO.txt"
#define CODEBOOK_FILE     "HMCZFO.txt"

// Huffman encoder output
#define OUTPUT_FILE       "OUTZFO.txt"

// Bundling & encryption
#define COMP_FILE         "COMPZFO.BIN"
#define ENCR_FILE         "ENCRZFO.BIN"
// key for encryption
#define ENCRYPT_KEY   0x5A

// ======================= CONFIG MACROS ====================================
#define CLEANUP           0   // 1 = delete helper files after run, 0 = keep for debug


// ----------------------- Utility functions (keep them all) ---------------------

// --- Data helpers ---
void write_binary_string(FIL *fptr, u32 byte_value) {
    char buffer[9]; // 8 bits + newline
    for (int i = 7; i >= 0; i--) {
        buffer[7 - i] = ((byte_value >> i) & 1) ? '1' : '0';
    }
    buffer[8] = '\n';

    UINT bw;
    if (f_write(fptr, buffer, 9, &bw) != FR_OK) {
        xil_printf("ERROR: Failed to write binary string\r\n");
    }
}

void get_binary_string(u32 value, char *str) {
    for (int i = 7; i >= 0; i--) {
        str[7 - i] = (value & (1 << i)) ? '1' : '0';
    }
    str[8] = '\0';
}

static inline int binstr_to_int(const char *s) {
    int v = 0;
    while (*s == '0' || *s == '1')
        v = (v << 1) | (*s++ - '0');
    return v;
}

// --- Frequency Counter helpers ---
void send_symbol(u32 symbol) {
    Xil_Out32(REG_SYMBOL, symbol);
    Xil_Out32(REG_LOAD, 1);
    while ((Xil_In32(REG_DONE) & 0x1) == 0);
    Xil_Out32(REG_LOAD, 0);
}

u32 read_symbol_frequency(u32 symbol) {
    Xil_Out32(REG_ADDR, symbol);
    return (Xil_In32(REG_FREQ) & 0x00FFFFFF);
}

// --- Huffman IP helpers ---
static int wait_valid_out(void) {
    int to = 100000;
    while (!IP_READ(REG_VALID_OUT) && to--) usleep(10);
    return (to <= 0) ? -1 : 0;
}

// --- File reading helper ---
static int f_readline(FIL *fp, char *buf, UINT len) {
    unsigned i = 0; char c;
    UINT br;
    while (i < len-1 && f_read(fp, &c, 1, &br) == FR_OK && br > 0) {
        if (c == '\n') break;
        if (c != '\r') buf[i++] = c;
    }
    buf[i] = '\0';
    return (i == 0 && f_eof(fp)) ? 0 : 1;
}

static FRESULT copy_file(FIL *fin, FIL *fout, u8 *buf) {
    UINT br, bw;
    FRESULT rc;

    do {
        rc = f_read(fin, buf, BUFFER_SIZE, &br);
        if (rc != FR_OK) return rc;
        if (br > 0) {
            rc = f_write(fout, buf, br, &bw);
            if (rc != FR_OK || bw != br) return rc;
        }
    } while (br > 0);

    return FR_OK;
}

// --- Timer helper ---
double get_time_ms() {
    XTime tCur;
    XTime_GetTime(&tCur);
    return ((double)tCur) / (COUNTS_PER_SECOND / 1000.0);
}

// ======================= BIT PARSER STAGE (Hardware-accelerated) ================================
int stage_bit_parser() {
    FIL *input_file  = openFile(INPUT_FILE,  'r');
    FIL *header_file = openFile(HEADER_FILE, 'w');
    FIL *parsed_file = openFile(PARSED_FILE, 'w');

    if (!input_file || !header_file || !parsed_file) {
        xil_printf("ERROR: Failed to open files for Bit Parser stage.\r\n");
        if (input_file)  closeFile(input_file);
        if (header_file) closeFile(header_file);
        if (parsed_file) closeFile(parsed_file);
        return -1;
    }

    xil_printf("\n---- Bit Parsing Stage ----\r\n");

    int in_header = 1;
    char linebuf[256];
    u32 input_word = 0;
    int bit_count = 0;
    u32 words_processed = 0;

    while (f_readline(input_file, linebuf, sizeof(linebuf))) {
        if (in_header) {
            writeFile(header_file, strlen(linebuf), (u32)linebuf);
            writeFile(header_file, 1, (u32)"\n");

            if (strncmp(linebuf, "Bits:", 5) == 0) {
                in_header = 0;
            }
            continue;
        }

        for (int i = 0; linebuf[i] != '\0'; i++) {
            if (linebuf[i] == '0' || linebuf[i] == '1') {
                input_word = (input_word << 1) | (linebuf[i] - '0');
                bit_count++;

                if (bit_count == 32) {
                    // Send data to BITPARSER_IP hardware
                    Xil_Out32(BITPARSER_IP_BASE + 0, input_word);

                    // Read 4 output bytes
                    u32 out1 = Xil_In32(BITPARSER_IP_BASE + 4)  & 0xFF;
                    u32 out2 = Xil_In32(BITPARSER_IP_BASE + 8)  & 0xFF;
                    u32 out3 = Xil_In32(BITPARSER_IP_BASE + 12) & 0xFF;
                    u32 out4 = Xil_In32(BITPARSER_IP_BASE + 16) & 0xFF;

                    // Write outputs as binary strings
                    write_binary_string(parsed_file, out1);
                    write_binary_string(parsed_file, out2);
                    write_binary_string(parsed_file, out3);
                    write_binary_string(parsed_file, out4);

                    input_word = 0;
                    bit_count = 0;
                    words_processed++;

                    if (words_processed % 500000 == 0) {
                        xil_printf("Bit Parser: Processed %u 32-bit words.\r\n", words_processed);
                    }
                }
            }
        }
    }

    // Handle leftover bits (pad with zeros)
    if (bit_count > 0) {
        input_word <<= (32 - bit_count);
        Xil_Out32(BITPARSER_IP_BASE + 0, input_word);

        u32 out1 = Xil_In32(BITPARSER_IP_BASE + 4)  & 0xFF;
        u32 out2 = Xil_In32(BITPARSER_IP_BASE + 8)  & 0xFF;
        u32 out3 = Xil_In32(BITPARSER_IP_BASE + 12) & 0xFF;
        u32 out4 = Xil_In32(BITPARSER_IP_BASE + 16) & 0xFF;

        write_binary_string(parsed_file, out1);
        write_binary_string(parsed_file, out2);
        write_binary_string(parsed_file, out3);
        write_binary_string(parsed_file, out4);

        words_processed++;
    }

    xil_printf("Bit Parsing complete. Total 32-bit words processed: %u\r\n", words_processed);

    closeFile(input_file);
    closeFile(header_file);
    closeFile(parsed_file);
    return 0;
}

// ======================= FREQUENCY COUNTER STAGE ========================
int stage_freq_counter() {
    xil_printf("\n---- Frequency Counting Stage ----\r\n");

    FIL *input_file  = openFile(PARSED_FILE, 'r');
    if (!input_file) {
        xil_printf("ERROR: Cannot open %s\r\n", PARSED_FILE);
        return -1;
    }

    FIL *output_file = openFile(FREQ_FILE, 'w');
    if (!output_file) {
        xil_printf("ERROR: Cannot create %s\r\n", FREQ_FILE);
        closeFile(input_file);
        return -1;
    }

    u32 file_size = readFile(input_file, MEMORY_BASE_ADDR);
    if (file_size <= 0) {
        xil_printf("ERROR: File read error or empty file.\r\n");
        closeFile(input_file);
        closeFile(output_file);
        return -1;
    }

    u8 *file_buffer = (u8 *)MEMORY_BASE_ADDR;
    u32 symbol_value = 0;
    int bit_count = 0;
    u32 symbol_counter = 0;

    for (u32 i = 0; i < file_size; i++) {
        if (file_buffer[i] == '0' || file_buffer[i] == '1') {
            symbol_value = (symbol_value << 1) | (file_buffer[i] - '0');
            bit_count++;
            if (bit_count == 8) {
                send_symbol(symbol_value);
                bit_count = 0;
                symbol_value = 0;
                symbol_counter++;
            }
        }
    }

    // Write main frequency table
    writeFile(output_file, strlen("Symbol        Frequency\r\n"), (u32)"Symbol        Frequency\r\n");
    writeFile(output_file, strlen("-------------------------\r\n"), (u32)"-------------------------\r\n");

    for (int symbol = 0; symbol < 256; symbol++) {
        u32 freq = read_symbol_frequency(symbol);
        if (freq > 0) {
            char symbol_str[9], line_buffer[50];
            get_binary_string(symbol, symbol_str);
            int len = sprintf(line_buffer, "%s        %u\r\n", symbol_str, freq);
            writeFile(output_file, len, (u32)line_buffer);
        }
    }

    // Write separate symbol/frequency helper files
    FIL *sym_file = openFile(SYMBOL_FILE, 'w');
    FIL *cnt_file = openFile(COUNT_FILE, 'w');

    for (int symbol = 0; symbol < 256; symbol++) {
        u32 freq = read_symbol_frequency(symbol);
        if (freq > 0) {
            char symbol_str[9];
            get_binary_string(symbol, symbol_str);
            writeFile(sym_file, 8, (u32)symbol_str);
            writeFile(sym_file, 1, (u32)"\n");

            char freq_str[20];
            int freq_len = sprintf(freq_str, "%u\n", freq);
            writeFile(cnt_file, freq_len, (u32)freq_str);
        }
    }

    closeFile(sym_file);
    closeFile(cnt_file);
    closeFile(input_file);
    closeFile(output_file);

    xil_printf("Frequency Counting Stage Complete: %u symbols processed\r\n", symbol_counter);
    return 0;
}

// ======================= CODEBOOK GENERATOR STAGE =======================

typedef struct {
    int symbol;
    int freq;
    char code[256];
    int code_len;
} HuffmanNode;

typedef struct HuffNode {
    int symbol;
    int freq;
    struct HuffNode *left, *right;
} HuffNode;

typedef struct {
    HuffNode *nodes[MAX_SYMBOLS];
    int size;
} MinHeap;

HuffmanNode huff_table[MAX_SYMBOLS];
int freq_table[MAX_SYMBOLS] = {0};
HuffNode node_pool[2 * MAX_SYMBOLS];
int node_index = 0;

HuffNode *new_node(int symbol, int freq, HuffNode *left, HuffNode *right) {
    HuffNode *n = &node_pool[node_index++];
    n->symbol = symbol;
    n->freq = freq;
    n->left = left;
    n->right = right;
    return n;
}

void heap_push(MinHeap *heap, HuffNode *node) {
    int i = heap->size++;
    while (i > 0 && node->freq < heap->nodes[(i - 1) / 2]->freq) {
        heap->nodes[i] = heap->nodes[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    heap->nodes[i] = node;
}

HuffNode *heap_pop(MinHeap *heap) {
    HuffNode *res = heap->nodes[0];
    HuffNode *last = heap->nodes[--heap->size];
    int i = 0;
    while (2 * i + 1 < heap->size) {
        int smallest = 2 * i + 1;
        if (smallest + 1 < heap->size && heap->nodes[smallest + 1]->freq < heap->nodes[smallest]->freq)
            smallest++;
        if (last->freq <= heap->nodes[smallest]->freq)
            break;
        heap->nodes[i] = heap->nodes[smallest];
        i = smallest;
    }
    heap->nodes[i] = last;
    return res;
}

void assign_codes(HuffNode *node, char *prefix) {
    if (!node->left && !node->right) {
        strcpy(huff_table[node->symbol].code, prefix);
        huff_table[node->symbol].code_len = strlen(prefix);
        return;
    }
    char left_code[256], right_code[256];
    sprintf(left_code, "%s0", prefix);
    sprintf(right_code, "%s1", prefix);
    if (node->left) assign_codes(node->left, left_code);
    if (node->right) assign_codes(node->right, right_code);
}

void generate_huffman_codes() {
    MinHeap heap = { .size = 0 };
    node_index = 0;
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (freq_table[i] > 0) {
            HuffNode *n = new_node(i, freq_table[i], NULL, NULL);
            heap_push(&heap, n);
        }
    }
    while (heap.size > 1) {
        HuffNode *left = heap_pop(&heap);
        HuffNode *right = heap_pop(&heap);
        HuffNode *merged = new_node(-1, left->freq + right->freq, left, right);
        heap_push(&heap, merged);
    }
    if (heap.size == 1) assign_codes(heap.nodes[0], "");
}

void parse_sym_freq_files(u8 *sym_buf, u32 sym_size, u8 *freq_buf, u32 freq_size) {
    u32 si = 0, fi = 0;
    int line_num = 0;
    while (si < sym_size && fi < freq_size && line_num < MAX_SYMBOLS) {
        char sym_line[32] = {0}, freq_line[32] = {0};
        int sj = 0, fj = 0;

        while (si < sym_size && sym_buf[si] != '\n' && sj < 31) {
            if (sym_buf[si] != '\r') sym_line[sj++] = sym_buf[si];
            si++;
        } sym_line[sj] = '\0'; si++;

        while (fi < freq_size && freq_buf[fi] != '\n' && fj < 31) {
            if (freq_buf[fi] != '\r') freq_line[fj++] = freq_buf[fi];
            fi++;
        } freq_line[fj] = '\0'; fi++;

        int symbol = strtol(sym_line, NULL, 2);
        int freq   = atoi(freq_line);
        if (symbol >= 0 && symbol < MAX_SYMBOLS && freq > 0) {
            freq_table[symbol] = freq;
            huff_table[symbol].symbol = symbol;
            huff_table[symbol].freq = freq;
            line_num++;
        }
    }
}

int stage_codebook_gen() {
    xil_printf("\n---- Huffman Codebook Generator Stage ----\r\n");

    FIL *sym_file = openFile(SYMBOL_FILE, 'r');
    FIL *cnt_file = openFile(COUNT_FILE, 'r');
    if (!sym_file || !cnt_file) {
        xil_printf("ERROR: File open failed %s or %s\r\n", SYMBOL_FILE, COUNT_FILE);
        return -1;
    }

    u32 sym_size = readFile(sym_file, SYMBOL_BUF_ADDR);
    u32 cnt_size = readFile(cnt_file, FREQ_BUF_ADDR);
    u8 *sym_buf  = (u8 *)SYMBOL_BUF_ADDR;
    u8 *cnt_buf  = (u8 *)FREQ_BUF_ADDR;

    parse_sym_freq_files(sym_buf, sym_size, cnt_buf, cnt_size);
    generate_huffman_codes();

    FIL *out       = openFile(CODEBOOK_FILE, 'w');
    FIL *sym_out   = openFile(SYMIN_FILE, 'w');
    FIL *codew_out = openFile(CODEWIN_FILE, 'w');
    FIL *codelen_out = openFile(CODELEN_FILE, 'w');

    char header[] = "Symbol       Codeword         Length\r\n"
                    "--------------------------------------\r\n";
    writeFile(out, strlen(header), (u32)header);

    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (huff_table[i].freq > 0) {
            char line[256];

            // Symbol 8-bit binary
            char sym_bin[9];
            for (int b = 7; b >= 0; b--)
                sym_bin[7 - b] = (i >> b) & 1 ? '1' : '0';
            sym_bin[8] = '\0';
            int len = sprintf(line, "%s\r\n", sym_bin);
            writeFile(sym_out, len, (u32)line);

            // Codeword (up to 16-bit binary string)
            unsigned int codeword = 0;
            for (int b = 0; b < huff_table[i].code_len; b++)
                if (huff_table[i].code[b] == '1')
                    codeword |= (1 << (huff_table[i].code_len - 1 - b));

            char code_bin[17];
            for (int b = 15; b >= 0; b--)
                code_bin[15 - b] = (codeword >> b) & 1 ? '1' : '0';
            code_bin[16] = '\0';

            len = sprintf(line, "%s\r\n", code_bin);
            writeFile(codew_out, len, (u32)line);

            // Code length (5-bit binary string)
            unsigned int clen = huff_table[i].code_len;
            char len_bin[6];
            for (int b = 4; b >= 0; b--)
                len_bin[4 - b] = (clen >> b) & 1 ? '1' : '0';
            len_bin[5] = '\0';

            len = sprintf(line, "%s\r\n", len_bin);
            writeFile(codelen_out, len, (u32)line);

            // Debug table
            len = sprintf(line, "%-10s %-20s %2d\r\n",
                          sym_bin, huff_table[i].code, huff_table[i].code_len);
            writeFile(out, len, (u32)line);
        }
    }

    closeFile(sym_file);
    closeFile(cnt_file);
    closeFile(out);
    closeFile(sym_out);
    closeFile(codew_out);
    closeFile(codelen_out);

    xil_printf("Huffman Codebook Generation : done.\r\n");
    return 0;
}

// ======================= HUFFMAN ENCODER STAGE ==========================
int stage_huffman_encode() {
    xil_printf("\n---- Huffman Compression Stage ----\r\n");
    xil_printf("Loading Huffman table into hardware...\r\n");

    FIL *f_symin   = openFile(SYMIN_FILE,   'r');
    FIL *f_codewin = openFile(CODEWIN_FILE, 'r');
    FIL *f_codelen = openFile(CODELEN_FILE, 'r');
    FIL *f_parsed  = openFile(PARSED_FILE,  'r');
    FIL *f_out     = openFile(OUTPUT_FILE,  'w');

    if (!f_symin || !f_codewin || !f_codelen || !f_parsed || !f_out) {
        xil_printf("ERROR: Opening table or output files failed\r\n");
        return -1;
    }

    char lsym[MAX_LINE_LEN];
    char lcode[MAX_LINE_LEN];
    char llen[MAX_LINE_LEN];

    // --- Load Huffman table into IP ---
    while ( f_readline(f_symin , lsym ,  MAX_LINE_LEN) &&
            f_readline(f_codewin,lcode, MAX_LINE_LEN) &&
            f_readline(f_codelen,llen , MAX_LINE_LEN) )
    {
        uint8_t  symbol = binstr_to_int(lsym);
        uint32_t code   = binstr_to_int(lcode);
        uint8_t  len    = binstr_to_int(llen);

        IP_WRITE(REG_LOAD_SYMBOL,  symbol);
        IP_WRITE(REG_LOAD_CODE,    code);
        IP_WRITE(REG_LOAD_LENGTH,  len);
        IP_WRITE(REG_LOAD_VALID,   1);

        int to = 10000;
        while (!IP_READ(REG_LOAD_DONE) && to--) usleep(10);
        IP_WRITE(REG_LOAD_VALID, 0);

        if (!to) {
            xil_printf("ERROR: Timeout loading symbol %02X\r\n", symbol);
            return -1;
        }
    }

    // --- Rewind parsed file for encoding ---
    f_lseek(f_parsed, 0);
    uint32_t total = 0;

    while (f_readline(f_parsed, lsym, MAX_LINE_LEN)) {
        uint8_t symbol = binstr_to_int(lsym);

        IP_WRITE(REG_SYMBOL_IN, symbol);
        IP_WRITE(REG_VALID_IN, 1);

        if (wait_valid_out() != 0) {
            xil_printf("ERROR: TIMEOUT @symbol %u\r\n", total);
            break;
        }

        uint32_t cw16 = IP_READ(REG_CODEWORD) & 0xFFFFFF;
        uint8_t  len5 = IP_READ(REG_CODELEN)  & 0x1F;

        IP_WRITE(REG_VALID_IN, 0);
        while (IP_READ(REG_VALID_OUT)) { usleep(5); }

        // Write trimmed codeword only
        for (int i = len5 - 1; i >= 0; i--)
            lcode[len5 - 1 - i] = (cw16 >> i) & 1 ? '1' : '0';
        lcode[len5] = '\0';

        writeFile(f_out, strlen(lcode), (u32)lcode);
        writeFile(f_out, 2, (u32)"\r\n");

        if (++total % 500000 == 0) {
            xil_printf("  %u Symbols Processed\r\n", total);
        }
    }

    xil_printf("Huffman Compression: DONE. Encoded %u symbols\r\n", total);

    closeFile(f_symin);
    closeFile(f_codewin);
    closeFile(f_codelen);
    closeFile(f_parsed);
    closeFile(f_out);

    return 0;
}

// ======================= BUNDLING STAGE ===============================
int stage_create_comp_bin() {
    xil_printf("\n---- Bundling Stage ----\r\n");

    FIL *f_header   = openFile(HEADER_FILE,   'r');
    FIL *f_codebook = openFile(CODEBOOK_FILE, 'r');
    FIL *f_output   = openFile(OUTPUT_FILE,   'r');
    FIL *f_comp     = openFile(COMP_FILE,     'w');

    if (!f_header || !f_codebook || !f_output || !f_comp) {
        xil_printf("ERROR: Cannot open one or more input files or create %s\r\n", COMP_FILE);
        if (f_header)   closeFile(f_header);
        if (f_codebook) closeFile(f_codebook);
        if (f_output)   closeFile(f_output);
        if (f_comp)     closeFile(f_comp);
        return -1;
    }

    u8 *buf = (u8*)MEMORY_BASE_ADDR;
    FRESULT rc;

    if ((rc = copy_file(f_header,   f_comp, buf)) != FR_OK)
        xil_printf("ERROR copying %s\r\n", HEADER_FILE);

    if ((rc = copy_file(f_codebook, f_comp, buf)) != FR_OK)
        xil_printf("ERROR copying %s\r\n", CODEBOOK_FILE);

    if ((rc = copy_file(f_output,   f_comp, buf)) != FR_OK)
        xil_printf("ERROR copying %s\r\n", OUTPUT_FILE);

    closeFile(f_header);
    closeFile(f_codebook);
    closeFile(f_output);
    closeFile(f_comp);

    xil_printf("Successfully Completed Bundling.\r\n");
    return 0;
}

// ======================= ENCRYPTION STAGE ==============================
int stage_encrypt_comp_bin(const char *infile, const char *outfile, u8 key) {
    xil_printf("\n---- Encryption Stage ----\r\n");

    FIL *fin  = openFile((char*)infile,  'r');
    FIL *fout = openFile((char*)outfile, 'w');
    if (!fin || !fout) {
        xil_printf("ERROR: opening %s or creating %s\r\n", infile, outfile);
        if (fin)  closeFile(fin);
        if (fout) closeFile(fout);
        return -1;
    }

    u8 *buf = (u8*)MEMORY_BASE_ADDR;
    UINT br, bw;
    const UINT BSZ = BUFFER_SIZE;
    FRESULT rc;

    do {
        rc = f_read(fin, buf, BSZ, &br);
        if (rc != FR_OK) {
            xil_printf("ERROR: Reading %s\r\n", infile);
            break;
        }
        for (UINT i = 0; i < br; ++i) {
            ENC_WRITE(ENC_REG_DATA_IN, buf[i]);
            ENC_WRITE(ENC_REG_KEY, key);
            u32 result = ENC_READ(ENC_REG_DATA_OUT);
            buf[i] = (u8)(result & 0xFF);
        }
        rc = f_write(fout, buf, br, &bw);
        if (rc != FR_OK || bw != br) {
            xil_printf("ERROR: Writing %s\r\n", outfile);
            break;
        }
    } while (br > 0);

    closeFile(fin);
    closeFile(fout);

    xil_printf("Encryption complete: %s -> %s (key=0x%02X)\r\n",
               infile, outfile, key);
    return 0;
}

// ============================ FILE CLEANUP STAGE ===========================
void cleanup_helper_files() {
    if (CLEANUP == 0) {
        xil_printf("Cleanup disabled. Helper files are kept.\r\n");
        return;
    }

    const char* helperFiles[] = {
    		SYMBOL_FILE,
			COUNT_FILE,
			SYMIN_FILE,
			CODEWIN_FILE,
			CODELEN_FILE,
			COMP_FILE,
			OUTPUT_FILE,
			CODEBOOK_FILE,
			FREQ_FILE,
			PARSED_FILE,
			HEADER_FILE
    };

    int numFiles = sizeof(helperFiles) / sizeof(helperFiles[0]);

    xil_printf("Cleanup enabled, deleting helper files...\r\n");
        for (int i = 0; i < numFiles; i++) {
            FRESULT res = f_unlink(helperFiles[i]);
            if (res != FR_OK && res != FR_NO_FILE) {
                        xil_printf("  Error deleting %s (err=%d)\r\n",
                                   helperFiles[i], res);
            }
        }
        xil_printf("Cleanup complete.\r\n");
    }

// ======================= MAIN: RUN ALL STAGES SEQUENTIALLY ==============
int main() {
    XTime tStart, tEnd;

    xil_printf("\n==== Huffman Compression + Encryption Chain: START ====\r\n");
    XTime_GetTime(&tStart);

    if (SD_Init() != XST_SUCCESS) {
        xil_printf("SD card init failed\r\n");
        return -1;
    }

    if (stage_bit_parser()      != 0) { xil_printf("Bit Parser failed\r\n");          goto done; }
    if (stage_freq_counter()    != 0) { xil_printf("Frequency Counter failed\r\n");   goto done; }
    if (stage_codebook_gen()    != 0) { xil_printf("Codebook Generation failed\r\n"); goto done; }
    if (stage_huffman_encode()  != 0) { xil_printf("Huffman Encoding failed\r\n");    goto done; }
    if (stage_create_comp_bin() != 0) { xil_printf("Bundling failed\r\n");            goto done; }
    if (stage_encrypt_comp_bin(COMP_FILE, ENCR_FILE, ENCRYPT_KEY) != XST_SUCCESS) {
        xil_printf("Encryption failed\r\n");
        goto done;
    }

    cleanup_helper_files();   // run cleanup according to CLEANUP flag

done:
    SD_Eject();
    XTime_GetTime(&tEnd);

    // ====================== Print total execution time ======================
    double elapsed_sec = 1.0 * (tEnd - tStart) / COUNTS_PER_SECOND;
    int minutes = (int)(elapsed_sec / 60);
    int seconds = (int)(elapsed_sec) % 60;

    xil_printf("==== Total execution time: %d:%02d (min:sec) ====\r\n", minutes, seconds);
    xil_printf("==== Huffman Compression + Encryption Chain: DONE ====\r\n");

    return 0;
}
