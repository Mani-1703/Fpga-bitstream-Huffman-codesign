// Module: Encrypt
// Description:
//   Lightweight encryption IP core.
//   Applies a reversible transformation to each 8-bit input byte
//   using bitwise inversion followed by XOR with an 8-bit key.
//
//   Encryption operation:
//     data_out = (~data_in) ^ key
//
// Notes:
//   - This is NOT cryptographically secure encryption
//   - Intended as lightweight protection for compressed bitstreams
//   - Fully reversible when the same key is used for decryption
//   - Stateless and purely combinational

module Encrypt(
    input  wire [7:0] data_in,   // Input data byte
    input  wire [7:0] key,        // 8-bit encryption key
    output wire [7:0] data_out    // Encrypted output byte
);
    wire [7:0] dash_data_in;

    // Bitwise inversion of input data
    // (equivalent to 255 - data_in for 8-bit values)
    assign dash_data_in = ~data_in;

    // XOR inverted data with key
    assign data_out = dash_data_in ^ key;

endmodule