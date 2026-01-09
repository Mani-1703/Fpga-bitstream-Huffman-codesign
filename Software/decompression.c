/*
 * decompression_main.c
 *
 * Software controller for FPGA-based Huffman decompression
 * and bitstream reconstruction using AXI4-Lite IP cores.
 *
 * Target platform : ZedBoard (Zynq-7000)
 * Toolchain       : Vivado / Vitis 2023.x
 *
 * This application performs decryption, Huffman decoding,
 * symbol merging, and final .rbt reconstruction.
 */

#include "xparameters.h"
#include "xil_io.h"
#include "xil_printf.h"
#include "ff.h"
#include "sdCard.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <sleep.h>
#include <stdio.h>
#include "xtime_l.h"

// ======================= File Names ========================================
#define ENCRYPT_FILE        "ENCR.bin"    // input encrypted file
#define DECRYPTED_FILE      "COMP.bin"    // decrypted output file
#define HEADER_FILE         "HEADER.txt"
#define CODEBOOK_FILE       "HMCODES.txt"
#define OUTPUT_FILE         "OUTPUT.txt"
#define SYMIN_FILE          "SYMIN.txt"
#define CODEWIN_FILE        "CODWIN.txt"
#define CODELEN_FILE        "CODLEN.txt"
#define OUTCW_FILE          "OTCW.txt"
#define OUTLEN_FILE         "OTLEN.txt"
#define PARRGN_FILE         "RGN.txt"
#define MERGED_FILE         "MERGED.txt"
#define DECOMP_FILE         "DECOMP.rbt"

// ======================= Decryption Parameters ============================
#define DECRYPT_KEY   0x5A   // must match encryption key from compression


// ======================= Huffman Decompressor IP BASE ADDR ======================
#define HUFFDEC_BASE_ADDR 0x43C10000  // change if needed

#define REG_LOAD_VALID     0x00
#define REG_LOAD_LENGTH    0x04
#define REG_LOAD_CODE      0x08
#define REG_LOAD_SYMBOL    0x0C
#define REG_LOAD_DONE      0x10
#define REG_CODELEN_IN     0x18
#define REG_CODEWORD_IN    0x1C
#define REG_SYMBOL_OUT     0x20

#define IP_WRITE(offset, value) Xil_Out32(HUFFDEC_BASE_ADDR + (offset), (value))
#define IP_READ(offset)         Xil_In32(HUFFDEC_BASE_ADDR + (offset))

// ======================= Merger IP BASE ADDR ===============================
#define MERGE_BASE_ADDR 0x43C00000
#define SLV_REG0        (MERGE_BASE_ADDR + 0x00)
#define SLV_REG1        (MERGE_BASE_ADDR + 0x04)
#define SLV_REG2        (MERGE_BASE_ADDR + 0x08)
#define SLV_REG3        (MERGE_BASE_ADDR + 0x0C)
#define OUT_WORD_REG    (MERGE_BASE_ADDR + 0x10)

#define IP_WRITE8(offset, value) Xil_Out8(offset, value)
#define IP_READ32(offset)        Xil_In32(offset)

// ======================= Decryption IP BASE ADDR ============================
#define DECRYPT_BASE_ADDR 0x43C20000  // base address of decryption IP

#define DECRYPT_REG0      (DECRYPT_BASE_ADDR + 0x00)  // data_in
#define DECRYPT_REG1      (DECRYPT_BASE_ADDR + 0x04)  // key
#define DECRYPT_REG2      (DECRYPT_BASE_ADDR + 0x08)  // data_out

// ======================= Helpers ==========================================
#define MAX_LINE_LEN  256
#define CLEANUP 1   // 1 = delete helper files after run, 0 = keep all for debug

//================== helpers / utility functions =============================

static inline int binstr_to_int(const char *s) {
    int v = 0;
    while (*s == '0' || *s == '1')
        v = (v << 1) | (*s++ - '0');
    return v;
}

static inline int is_binstr(const char *s) {
    if (!s || !*s) return 0;
    while (*s) {
        if (*s != '0' && *s != '1') return 0;
        s++;
    }
    return 1;
}

void uint_to_binstr(uint32_t val, int width, char* out) {
    for (int i = width - 1; i >= 0; i--) {
        out[width - 1 - i] = ((val >> i) & 1) ? '1' : '0';
    }
    out[width] = '\0';
}

static void rstrip(char *s) {
    if (!s) return;
    int n = (int)strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t')) {
        s[--n] = '\0';
    }
}

static int f_readline(FIL *fp, char *buf, UINT len) {
    unsigned i = 0;
    char c;
    UINT br;
    while (i < len-1) {
        FRESULT rc = f_read(fp, &c, 1, &br);
        if (rc != FR_OK || br == 0) break;   // EOF
        if (c == '\n') break;
        if (c != '\r') buf[i++] = c;
    }
    buf[i] = '\0';
    return (i > 0) ? 1 : 0;
}

static inline uint8_t binstr_to_byte(const char *s) {
    uint8_t v = 0;
    for (int i = 0; i < 8 && (s[i] == '0' || s[i] == '1'); i++) {
        v = (v << 1) | (s[i] - '0');
    }
    return v;
}

static void uint32_to_binstr(uint32_t val, char *out) {
    for (int i = 31; i >= 0; i--) {
        out[31 - i] = ((val >> i) & 1) ? '1' : '0';
    }
    out[32] = '\0';
}
// ==========================================================================
// Part 1: Decrypt ENCR.bin -> COMP.bin
// ==========================================================================
int decrypt_file() {
    FIL *fp_in  = openFile(ENCRYPT_FILE,  'r');   // ENCR.bin
    FIL *fp_out = openFile(DECRYPTED_FILE,  'w');   // COMP.bin

    if (!fp_in || !fp_out) {
        xil_printf("ERROR: opening %s or creating %s\r\n",
                   ENCRYPT_FILE, DECRYPTED_FILE);
        if (fp_in)  closeFile(fp_in);
        if (fp_out) closeFile(fp_out);
        return -1;
    }

    xil_printf("---- Decrypting %s ----\r\n",
               ENCRYPT_FILE);

    UINT br, bw;
    const UINT BSZ = 4096;
    u8 buffer[BSZ];

    do {
        FRESULT rc = f_read(fp_in, buffer, BSZ, &br);
        if (rc != FR_OK) {
            xil_printf("ERROR: Reading %s\r\n", ENCRYPT_FILE);
            break;
        }

        for (UINT i = 0; i < br; i++) {
            // Send encrypted byte + key to Decrypt IP
            Xil_Out32(DECRYPT_REG0, buffer[i]);
            Xil_Out32(DECRYPT_REG1, DECRYPT_KEY);   // use defined key

            // Read decrypted byte
            u32 result = Xil_In32(DECRYPT_REG2);
            buffer[i] = (u8)(result & 0xFF);
        }

        rc = f_write(fp_out, buffer, br, &bw);
        if (rc != FR_OK || bw != br) {
            xil_printf("ERROR: Writing %s\r\n", DECRYPTED_FILE);
            break;
        }
    } while (br > 0);

    closeFile(fp_in);
    closeFile(fp_out);

    xil_printf("---- Decryption Complete ----\r\n");
    return 0;
}

int split_comp_bin() {
    FIL *fp_in     = openFile(DECRYPTED_FILE, 'r');   // COMP.bin
    FIL *fp_header = openFile(HEADER_FILE,   'w');    // HEADER.txt
    FIL *fp_codes  = openFile(CODEBOOK_FILE, 'w');    // HMCODES.txt
    FIL *fp_output = openFile(OUTPUT_FILE,   'w');    // OUTPUT.txt

    if (!fp_in || !fp_header || !fp_codes || !fp_output) {
        xil_printf("ERROR: opening COMP.bin or creating output files\r\n");
        if (fp_in)     closeFile(fp_in);
        if (fp_header) closeFile(fp_header);
        if (fp_codes)  closeFile(fp_codes);
        if (fp_output) closeFile(fp_output);
        return -1;
    }


    char line[MAX_LINE_LEN];
    int state = 0; // 0=header, 1=codebook, 2=output

    while (f_readline(fp_in, line, MAX_LINE_LEN)) {
        if (state == 0) {
            // Look for start of HMCODES section
            if (strncmp(line, "Symbol", 6) == 0) {
                state = 1;
                writeFile(fp_codes, strlen(line), (u32)line);
                writeFile(fp_codes, 2, (u32)"\r\n");
            } else {
                writeFile(fp_header, strlen(line), (u32)line);
                writeFile(fp_header, 2, (u32)"\r\n");
            }
        }
        else if (state == 1) {
            // Detect transition from CODEBOOK -> OUTPUT
            // Codebook lines always have 3 tokens (symbol, codeword, length).
            // Once we see a line with only a single binary token, switch to OUTPUT.
            int token_count = 0;
            int in_token = 0;
            for (int i=0; line[i]; i++) {
                if (!isspace((unsigned char)line[i])) {
                    if (!in_token) { token_count++; in_token = 1; }
                } else {
                    in_token = 0;
                }
            }

            if (token_count == 1 && is_binstr(line)) {
                state = 2;
                writeFile(fp_output, strlen(line), (u32)line);
                writeFile(fp_output, 2, (u32)"\r\n");
            } else {
                writeFile(fp_codes, strlen(line), (u32)line);
                writeFile(fp_codes, 2, (u32)"\r\n");
            }
        }
        else if (state == 2) {
            // All remaining lines are OUTPUT stream
            writeFile(fp_output, strlen(line), (u32)line);
            writeFile(fp_output, 2, (u32)"\r\n");
        }
    }

    closeFile(fp_in);
    closeFile(fp_header);
    closeFile(fp_codes);
    closeFile(fp_output);

    return 0;
}

// ==========================================================================
// Part 3: Generate SYMIN / CODEWIN / CODELEN from HMCODES.TXT
// ==========================================================================
int generate_huffman_table_files_from_HMCODES() {
    FIL *fin   = openFile(CODEBOOK_FILE, 'r');   // HMCODES.TXT
    FIL *fsym  = openFile(SYMIN_FILE,    'w');   // SYMIN.txt
    FIL *fcode = openFile(CODEWIN_FILE,  'w');   // CODEWIN.txt
    FIL *flen  = openFile(CODELEN_FILE,  'w');   // CODELEN.txt

    if (!fin || !fsym || !fcode || !flen) {
        xil_printf("ERROR: opening %s or creating output files\r\n", CODEBOOK_FILE);
        if (fin)   closeFile(fin);
        if (fsym)  closeFile(fsym);
        if (fcode) closeFile(fcode);
        if (flen)  closeFile(flen);
        return -1;
    }

    char line[MAX_LINE_LEN];
    char symbol[64], codeword[128], lenstr[64];
    xil_printf("---- Regenerating Helper Files ----\r\n");

    // Skip header line if present
    if (f_readline(fin, line, MAX_LINE_LEN)) {
        if (strncmp(line, "Symbol", 6) == 0)
            f_readline(fin, line, MAX_LINE_LEN);
    }

    uint32_t count = 0;
    while (f_readline(fin, line, MAX_LINE_LEN)) {
        rstrip(line);
        if (!line[0]) continue;

        char *tokens[3] = {symbol, codeword, lenstr};
        int tlen[3] = {sizeof(symbol)-1, sizeof(codeword)-1, sizeof(lenstr)-1};
        for (int t = 0; t < 3; t++) tokens[t][0] = '\0';

        int i=0,j=0,idx=0,n=strlen(line);
        while (idx < 3 && i < n) {
            while (i < n && isspace((unsigned char)line[i])) i++;
            if (i >= n) break;
            j=0;
            while (i<n && !isspace((unsigned char)line[i]) && j<tlen[idx]) {
                tokens[idx][j++] = line[i++];
            }
            tokens[idx][j] = '\0';
            idx++;
        }
        if (idx < 3) continue;

        if (strlen(symbol) != 8 || !is_binstr(symbol)) continue;
        if (!is_binstr(codeword)) continue;
        int length = atoi(lenstr);
        if (length < 0 || length > 31) continue;

        int cwlen = strlen(codeword);
        if (cwlen > 16) {
            xil_printf("WARN: codeword >16 bits\r\n");
            return -1;
        }

        writeFile(fsym, 8, (u32)symbol);
        writeFile(fsym, 2, (u32)"\r\n");

        uint32_t codeval = 0;
        for (int c=0; codeword[c]; c++)
            codeval = (codeval<<1)|(codeword[c]-'0');

        char code16[17];
        uint_to_binstr(codeval,16,code16);
        writeFile(fcode,16,(u32)code16);
        writeFile(fcode,2,(u32)"\r\n");

        char len5[6];
        uint_to_binstr((uint32_t)length,5,len5);
        writeFile(flen,5,(u32)len5);
        writeFile(flen,2,(u32)"\r\n");

        if ((++count % 1000)==0)
            xil_printf("  %lu entries...\r\n",(unsigned long)count);
    }

    closeFile(fin);
    closeFile(fsym);
    closeFile(fcode);
    closeFile(flen);

    return 0;
}

// ==========================================================================
// Part 4: Generate OUTCW / OUTLEN from OUTPUT.txt
// ==========================================================================
int generate_out_stream_files_from_OUTPUT() {
    FIL *fin  = openFile(OUTPUT_FILE,  'r');   // OUTPUT.txt
    FIL *fcw  = openFile(OUTCW_FILE,   'w');   // OUTCW.txt
    FIL *flen = openFile(OUTLEN_FILE,  'w');   // OUTLEN.txt

    if (!fin || !fcw || !flen) {
        xil_printf("ERROR: opening %s or creating helper files\r\n", OUTPUT_FILE);
        if (fin)  closeFile(fin);
        if (fcw)  closeFile(fcw);
        if (flen) closeFile(flen);
        return -1;
    }

    char line[MAX_LINE_LEN];
    uint32_t total = 0;

    while (f_readline(fin, line, MAX_LINE_LEN)) {
        rstrip(line);

        // Keep only 0/1 characters
        int w = 0;
        for (int r = 0; line[r]; r++) {
            if (line[r] == '0' || line[r] == '1')
                line[w++] = line[r];
        }
        line[w] = '\0';
        if (!line[0]) continue;

        int len = strlen(line);
        if (len > 16) {
            xil_printf("ERROR: Codeword longer than 16 bits in %s\r\n", OUTPUT_FILE);
            closeFile(fin);
            closeFile(fcw);
            closeFile(flen);
            return -1;
        }

        // Convert binary string to int
        uint32_t codeval = 0;
        for (int i = 0; line[i]; i++)
            codeval = (codeval << 1) | (line[i] - '0');

        // Write 16-bit codeword
        char code16[17];
        uint_to_binstr(codeval, 16, code16);
        writeFile(fcw, 16, (u32)code16);
        writeFile(fcw, 2, (u32)"\r\n");

        // Write 5-bit length
        char len5[6];
        uint_to_binstr((uint32_t)len, 5, len5);
        writeFile(flen, 5, (u32)len5);
        writeFile(flen, 2, (u32)"\r\n");
    }

    closeFile(fin);
    closeFile(fcw);
    closeFile(flen);

    xil_printf("---- Helper Files Regenerated ----\r\n",
               OUTPUT_FILE, (unsigned long)total);
    return 0;
}

// ==========================================================================
// Part 5: Load Huffman Table into Huffman Decompressor IP
// ==========================================================================
int load_huffman_table_from_files() {
    FIL *fsym  = openFile(SYMIN_FILE,   'r');   // SYMIN.txt
    FIL *fcode = openFile(CODEWIN_FILE, 'r');   // CODEWIN.txt
    FIL *flen  = openFile(CODELEN_FILE, 'r');   // CODELEN.txt

    if (!fsym || !fcode || !flen) {
        xil_printf("ERROR: opening Huffman helper files (%s, %s, %s)\r\n",
                   SYMIN_FILE, CODEWIN_FILE, CODELEN_FILE);
        if (fsym)  closeFile(fsym);
        if (fcode) closeFile(fcode);
        if (flen)  closeFile(flen);
        return -1;
    }

    char lsym[MAX_LINE_LEN], lcode[MAX_LINE_LEN], llen[MAX_LINE_LEN];
    xil_printf("---- Loading Huffman Table ----\r\n");

    while (f_readline(fsym, lsym, MAX_LINE_LEN) &&
           f_readline(fcode, lcode, MAX_LINE_LEN) &&
           f_readline(flen, llen, MAX_LINE_LEN))
    {
        // Validate field lengths: 8-bit symbol, 16-bit codeword, 5-bit length
        if (strlen(lsym) != 8 || strlen(lcode) != 16 || strlen(llen) != 5)
            continue;

        uint8_t  symbol = (uint8_t)binstr_to_int(lsym);
        uint32_t code   = (uint32_t)binstr_to_int(lcode);
        uint8_t  len    = (uint8_t)binstr_to_int(llen);

        // Write entry into Huffman IP
        IP_WRITE(REG_LOAD_SYMBOL, symbol);
        IP_WRITE(REG_LOAD_CODE,   code);
        IP_WRITE(REG_LOAD_LENGTH, len);
        IP_WRITE(REG_LOAD_VALID,  1);

        // Wait for done flag
        int to = 10000;
        while (!IP_READ(REG_LOAD_DONE) && to--)
            usleep(10);

        // Clear valid strobe
        IP_WRITE(REG_LOAD_VALID, 0);

        if (!to) {
            xil_printf("Timeout loading symbol %02X\r\n", symbol);
            closeFile(fsym);
            closeFile(fcode);
            closeFile(flen);
            return -1;
        }
    }

    closeFile(fsym);
    closeFile(fcode);
    closeFile(flen);

    xil_printf("---- Huffman Table Loaded ----\r\n");
    return 0;
}

// ==========================================================================
// Part 6: Decompress OUTCW / OUTLEN -> PARRGN.txt
// ==========================================================================
int decompress_from_files() {
    FIL *fcw  = openFile(OUTCW_FILE,  'r');   // OUTCW.txt
    FIL *flen = openFile(OUTLEN_FILE, 'r');   // OUTLEN.txt
    FIL *fout = openFile(PARRGN_FILE, 'w');   // PARRGN.txt

    if (!fcw || !flen || !fout) {
        xil_printf("ERROR: opening %s, %s or creating %s\r\n",
                   OUTCW_FILE, OUTLEN_FILE, PARRGN_FILE);
        if (fcw)  closeFile(fcw);
        if (flen) closeFile(flen);
        if (fout) closeFile(fout);
        return -1;
    }

    char lcode[MAX_LINE_LEN], llen[MAX_LINE_LEN], outbin[9];
    uint32_t total = 0;

    xil_printf("---- Decompressing ----\r\n");

    while (f_readline(fcw, lcode, MAX_LINE_LEN) &&
           f_readline(flen, llen, MAX_LINE_LEN)) {

        if (strlen(lcode) != 16 || strlen(llen) != 5) {
            xil_printf("WARN: bad widths in %s/%s; skipping\r\n",
                       OUTCW_FILE, OUTLEN_FILE);
            continue;
        }

        uint32_t code = (uint32_t)binstr_to_int(lcode);
        uint8_t  len  = (uint8_t)binstr_to_int(llen);

        // Send codeword + length to Huffman Decompressor IP
        IP_WRITE(REG_CODEWORD_IN, code);
        IP_WRITE(REG_CODELEN_IN, len);

        // Read back symbol
        uint8_t sym = IP_READ(REG_SYMBOL_OUT) & 0xFF;

        // Write symbol (8-bit binary string) to output file
        uint_to_binstr(sym, 8, outbin);
        writeFile(fout, 8, (u32)outbin);
        writeFile(fout, 2, (u32)"\r\n");

        if (++total % 500000 == 0)
            xil_printf("  %u symbols decompressed\r\n", total);
    }

    closeFile(fcw);
    closeFile(flen);
    closeFile(fout);

    xil_printf("---- Decompression Done: %u symbols ----\r\n", total);
    return 0;
}

// ==========================================================================
// Part 7: Merge PARRGN -> MERGED.txt
// ==========================================================================
int merge_symbols_to_words() {
    FIL *fp_in  = openFile(PARRGN_FILE, 'r');   // PARRGN.txt
    FIL *fp_out = openFile(MERGED_FILE, 'w');   // MERGED.txt

    if (!fp_in || !fp_out) {
        xil_printf("ERROR: opening %s or creating %s\r\n",
                   PARRGN_FILE, MERGED_FILE);
        if (fp_in)  closeFile(fp_in);
        if (fp_out) closeFile(fp_out);
        return -1;
    }

    char line[MAX_LINE_LEN];
    uint8_t symbols[4];
    int idx = 0;
    uint32_t merged_count = 0;

    xil_printf("==== Bit Merger IP ====\r\n");

    while (f_readline(fp_in, line, sizeof(line))) {
        if (!is_binstr(line) || strlen(line) != 8) continue;

        uint8_t val = binstr_to_byte(line);
        symbols[idx++] = val;

        if (idx == 4) {
            // Write 4 bytes into Merger IP
            IP_WRITE8(SLV_REG0, symbols[0]);
            IP_WRITE8(SLV_REG1, symbols[1]);
            IP_WRITE8(SLV_REG2, symbols[2]);
            IP_WRITE8(SLV_REG3, symbols[3]);

            // Read merged 32-bit word
            uint32_t merged = IP_READ32(OUT_WORD_REG);

            // Write as binary string to MERGED.txt
            char bin32[33];
            uint32_to_binstr(merged, bin32);
            writeFile(fp_out, 32, (u32)bin32);
            writeFile(fp_out, 2, (u32)"\r\n");

            merged_count++;
            idx = 0;

            // Progress update every 500k merged words
            if (merged_count % 500000 == 0) {
                xil_printf("  %lu words merged so far\r\n",
                           (unsigned long)merged_count);
            }
        }
    }

    closeFile(fp_in);
    closeFile(fp_out);

    xil_printf("Total number of 32-bit words merged: %lu\r\n",
               (unsigned long)merged_count);

    return 0;
}

// ==========================================================================
// Part 8: Merge HEADER.txt and MERGED.txt into DECOMP.rbt
// ==========================================================================
int merge_header_and_data() {
    FIL *fp_header = openFile(HEADER_FILE, 'r');   // HEADER.txt
    FIL *fp_data   = openFile(MERGED_FILE, 'r');   // MERGED.txt
    FIL *fp_out    = openFile(DECOMP_FILE, 'w');   // DECOMP.rbt

    if (!fp_header || !fp_data || !fp_out) {
        xil_printf("ERROR: opening %s, %s, or creating %s\r\n",
                   HEADER_FILE, MERGED_FILE, DECOMP_FILE);
        if (fp_header) closeFile(fp_header);
        if (fp_data)   closeFile(fp_data);
        if (fp_out)    closeFile(fp_out);
        return -1;
    }

    char line[MAX_LINE_LEN];
    xil_printf("==== Merging Files to create %s ====\r\n",
               DECOMP_FILE);

    // Copy HEADER file content into DECOMP.rbt
    while (f_readline(fp_header, line, MAX_LINE_LEN)) {
        writeFile(fp_out, strlen(line), (u32)line);
        writeFile(fp_out, 2, (u32)"\r\n");  // preserve CRLF
    }

    // Append MERGED file content into DECOMP.rbt
    while (f_readline(fp_data, line, MAX_LINE_LEN)) {
        writeFile(fp_out, strlen(line), (u32)line);
        writeFile(fp_out, 2, (u32)"\r\n");
    }

    closeFile(fp_header);
    closeFile(fp_data);
    closeFile(fp_out);

    xil_printf("==== Created final decompressed file: %s ====\r\n", DECOMP_FILE);
    return 0;
}

// ============================ File Cleanup Stage ===========================
void cleanup_helper_files() {
    if (CLEANUP == 0) {
        xil_printf("Cleanup disabled. Helper files are kept.\r\n");
        return;
    }

    // List of intermediate files to delete after decompression
    const char* helperFiles[] = {
        SYMIN_FILE,
        CODEWIN_FILE,
        CODELEN_FILE,
        OUTCW_FILE,
        OUTLEN_FILE,
        MERGED_FILE,
		CODEBOOK_FILE,
		HEADER_FILE,
		OUTPUT_FILE,
		PARRGN_FILE,
		DECRYPTED_FILE
    };

    int numFiles = sizeof(helperFiles) / sizeof(helperFiles[0]);

    xil_printf("Cleanup enabled. Deleting intermediate helper files...\r\n");

    for (int i = 0; i < numFiles; i++) {
        FRESULT res = f_unlink(helperFiles[i]);
        if (res != FR_OK && res != FR_NO_FILE) {
            xil_printf("  Error deleting %s (err=%d)\r\n",
                       helperFiles[i], res);
        }
    }
    xil_printf("Cleanup complete\r\n");
}
// ============================ Main Function ================================
int main() {
    XTime tStart, tEnd;
    xil_printf("==== Decryption & Huffman Decompression Pipeline START ====\r\n");

    if (SD_Init() != XST_SUCCESS) {
        xil_printf("ERROR: SD card initialization failed\r\n");
        return -1;
    }

    XTime_GetTime(&tStart);

    if (decrypt_file() != 0) goto fail;
    if (split_comp_bin() != 0) goto fail;
    if (generate_huffman_table_files_from_HMCODES() != 0) goto fail;
    if (generate_out_stream_files_from_OUTPUT() != 0) goto fail;
    if (load_huffman_table_from_files() != 0) goto fail;
    if (decompress_from_files() != 0) goto fail;
    if (merge_symbols_to_words() != 0) goto fail;
    if (merge_header_and_data() != 0) goto fail;

    cleanup_helper_files();

    XTime_GetTime(&tEnd);

    double elapsed_sec = 1.0 * (tEnd - tStart) / COUNTS_PER_SECOND;
    int minutes = (int)(elapsed_sec / 60);
    int seconds = (int)(elapsed_sec) % 60;

    xil_printf("==== Total execution time: %d:%02d (min:sec) ====\r\n",
               minutes, seconds);
    xil_printf("==== Huffman Decompression Pipeline COMPLETE ====\r\n");

    SD_Eject();
    return 0;

fail:
    xil_printf("Pipeline failed. Aborting.\r\n");
    SD_Eject();
    return -1;
}
