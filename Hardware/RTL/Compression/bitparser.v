// Module: bit_parser
// Description:
//   Bit Parser IP core.
//   Splits a 32-bit input word into four independent 8-bit symbols.
//   This reduces the symbol space from 2^32 to 2^8, making
//   frequency counting and Huffman coding hardware-feasible.
//
// Example:
//   Input  : 11110000111111110000111111110000
//   Output : 11110000, 11111111, 00001111, 11110000
//
// Notes:
//   - Purely combinational logic
//   - No clock or state elements

module bit_parser(
    input  [31:0] data_in,   // 32-bit input word
    output [7:0]  out_1,     // Most significant byte
    output [7:0]  out_2,
    output [7:0]  out_3,
    output [7:0]  out_4      // Least significant byte
);
    assign out_1 = data_in[31:24];
    assign out_2 = data_in[23:16];
    assign out_3 = data_in[15:8];
    assign out_4 = data_in[7:0];
endmodule