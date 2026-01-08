// Module: huffman_decoder
// Description:
//   Huffman Decoder IP core.
//   Decodes a variable-length Huffman codeword back into its
//   original 8-bit symbol using a dynamically loaded codebook.
//
//   The codebook (symbol, codeword, code length) is loaded from
//   software (Vitis) prior to decoding. Decoding is performed
//   by matching the incoming codeword and its length against
//   the stored table.
//
// Notes:
//   - This decoder uses a linear search over 256 entries
//   - Intended for correctness and simplicity, not high throughput
//   - Codebook must be fully loaded before decoding begins
//   - Designed to mirror the Huffman Encoder IP

module huffman_decoder(
    input  wire         clock,
    input  wire         reset,

    // ------------------------------------------------------------------
    // Huffman encoded input
    // ------------------------------------------------------------------
    input  wire [15:0]  code_word_in,    // Huffman codeword (MSB-aligned)
    input  wire [4:0]   code_length_in,  // Number of valid bits in code_word

    // ------------------------------------------------------------------
    // Decoded output
    // ------------------------------------------------------------------
    output reg  [7:0]   symbol_out,      // Decoded 8-bit symbol

    // ------------------------------------------------------------------
    // Huffman codebook load interface (from Vitis software)
    // ------------------------------------------------------------------
    input  wire [7:0]   load_symbol,     // Symbol index (0–255)
    input  wire [15:0]  load_code,       // Huffman codeword
    input  wire [4:0]   load_length,     // Huffman code length
    input  wire         load_valid,      // Load request (level signal)
    output reg          load_valid_out    // One-cycle acknowledge pulse
);

    // ------------------------------------------------------------------
    // Huffman codebook storage
    // ------------------------------------------------------------------
    // Each index corresponds directly to one symbol.
    // Tables are implemented as distributed RAM with asynchronous read.
    reg [7:0]   symbol_table     [0:255];  // Symbol values
    reg [15:0]  codeword_table   [0:255];  // Huffman codewords
    reg [4:0]   codelength_table [0:255];  // Codeword lengths

    integer i;

    // ------------------------------------------------------------------
    // Edge detection for load_valid
    // ------------------------------------------------------------------
    // Converts level-based load signal into a one-cycle pulse
    reg  load_valid_d;
    wire load_valid_pulse;

    always @(posedge clock) begin
        load_valid_d <= load_valid;
    end

    // Rising-edge detection
    assign load_valid_pulse = load_valid & ~load_valid_d;

    // ------------------------------------------------------------------
    // Codebook loading logic
    // ------------------------------------------------------------------
    // Loads one symbol entry per load_valid pulse.
    // load_valid_out acts as an acknowledge to software.
    always @(posedge clock or posedge reset) begin
        if (reset) begin
            load_valid_out <= 0;
        end else begin
            if (load_valid_pulse) begin
                symbol_table[load_symbol]     <= load_symbol;
                codeword_table[load_symbol]   <= load_code;
                codelength_table[load_symbol] <= load_length;
                load_valid_out                <= 1;
            end else if (!load_valid) begin
                load_valid_out <= 0;
            end
        end
    end

    // ------------------------------------------------------------------
    // Huffman decode logic
    // ------------------------------------------------------------------
    // Combinational lookup:
    //   - Compare incoming (codeword, length) with all table entries
    //   - Output the matching symbol
    //
    // Note:
    //   This linear search prioritizes simplicity and correctness.
    //   More advanced designs may use tree traversal or parallel decoding.
    always @* begin
        symbol_out = 0;  // Default value (safe reset)
        for (i = 0; i < 256; i = i + 1) begin
            if (codelength_table[i] == code_length_in &&
                codeword_table[i]   == code_word_in) begin
                symbol_out = symbol_table[i];
            end
        end
    end

endmodule