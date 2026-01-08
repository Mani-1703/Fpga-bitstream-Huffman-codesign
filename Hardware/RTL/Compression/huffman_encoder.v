// Module: huffman
// Description:
//   Huffman Encoder IP core.
//   Maps an 8-bit input symbol to a variable-length Huffman codeword
//   using a dynamically loaded codebook.
//
//   The Huffman codebook (codeword + length) is loaded from software
//   (Vitis) via an AXI-controlled interface before encoding begins.
//
// Notes:
//   - Maximum codeword length is limited to 16 bits
//   - Codebook must be fully loaded before asserting valid_in
//   - valid_in and load_valid are edge-detected (one-shot)

module huffman (
    input wire          clock,
    input wire          reset,

    // ------------------------------------------------------------------
    // Symbol input interface
    // ------------------------------------------------------------------
    input wire  [7:0]   symbol_in,   // 8-bit input symbol
    input wire          valid_in,     // Assert high to encode symbol

    // ------------------------------------------------------------------
    // Huffman encoded output
    // ------------------------------------------------------------------
    output reg          valid_out,    // Indicates valid code_word output
    output reg  [15:0]  code_word,    // Huffman codeword (MSB-aligned)
    output reg  [4:0]   code_length,  // Number of valid bits in code_word

    // ------------------------------------------------------------------
    // Huffman table load interface (from Vitis software)
    // ------------------------------------------------------------------
    input wire  [7:0]   load_symbol,  // Symbol index (0–255)
    input wire  [15:0]  load_code,    // Huffman codeword
    input wire  [4:0]   load_length,  // Codeword length
    input wire          load_valid,   // Load request (level signal)
    output reg          load_valid_out // One-cycle acknowledge pulse
);

    // ------------------------------------------------------------------
    // Huffman lookup tables
    // ------------------------------------------------------------------
    // Each symbol index directly maps to its Huffman code and length
    reg [15:0] huff_code   [0:255];
    reg [4:0]  huff_length[0:255];

    // ------------------------------------------------------------------
    // Edge detection for load_valid (software-controlled pulse)
    // ------------------------------------------------------------------
    reg  load_valid_d;
    wire load_valid_pulse;

    always @(posedge clock) begin
        load_valid_d <= load_valid;
    end

    // Detect rising edge of load_valid
    assign load_valid_pulse = load_valid & ~load_valid_d;

    // ------------------------------------------------------------------
    // Edge detection for valid_in (symbol input pulse)
    // ------------------------------------------------------------------
    reg  valid_in_d;
    wire valid_in_pulse;

    always @(posedge clock) begin
        valid_in_d <= valid_in;
    end

    // Detect rising edge of valid_in
    assign valid_in_pulse = valid_in & ~valid_in_d;

    // ------------------------------------------------------------------
    // Huffman table loading logic
    // ------------------------------------------------------------------
    // Loads one (symbol, codeword, length) entry per pulse.
    // load_valid_out acts as a one-cycle acknowledge to software.
    always @(posedge clock or posedge reset) begin
        if (reset) begin
            load_valid_out <= 0;
        end else begin
            if (load_valid_pulse) begin
                huff_code[load_symbol]   <= load_code;
                huff_length[load_symbol] <= load_length;
                load_valid_out           <= 1;
            end else if (!load_valid) begin
                // Deassert acknowledge when load_valid goes low
                load_valid_out <= 0;
            end
        end
    end

    // ------------------------------------------------------------------
    // Huffman encoding logic
    // ------------------------------------------------------------------
    // On valid_in rising edge:
    //   - Lookup Huffman codeword and length
    //   - Assert valid_out for one cycle
    always @(posedge clock or posedge reset) begin
        if (reset) begin
            valid_out   <= 0;
            code_word   <= 0;
            code_length <= 0;
        end else if (valid_in_pulse) begin
            code_word   <= huff_code[symbol_in];
            code_length <= huff_length[symbol_in];
            valid_out   <= 1;
        end else if (!valid_in) begin
            // Clear valid_out when input is idle
            valid_out <= 0;
        end
    end

endmodule