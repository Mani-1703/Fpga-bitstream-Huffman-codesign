// Module: bit_merger
// Description:
//   Bit Merger IP core.
//   Reconstructs the original 32-bit word by concatenating four
//   consecutive 8-bit symbols.
//
//   This module is the inverse of the Bit Parser IP and is used
//   during the decompression stage to restore the original
//   bitstream word format.
//
// Example:
//   Inputs : in0 = 11110000
//            in1 = 11111111
//            in2 = 00001111
//            in3 = 11110000
//   Output : 11110000111111110000111111110000
//
// Notes:
//   - Purely combinational logic
//   - No internal state or clock dependency

module bit_merger(
    input  wire [7:0] in0,   // Most significant byte
    input  wire [7:0] in1,
    input  wire [7:0] in2,
    input  wire [7:0] in3,   // Least significant byte
    output wire [31:0] out_word
);

    // Concatenate 4×8-bit symbols into one 32-bit word
    assign out_word = {in0, in1, in2, in3};

endmodule