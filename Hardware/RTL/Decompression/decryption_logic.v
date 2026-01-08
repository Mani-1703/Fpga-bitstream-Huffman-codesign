// Module: decrypt
// Description:
//   Lightweight decryption IP core.
//   Reverses the encryption applied by the Encrypt module using
//   the same 8-bit key.
//
//   Decryption operation:
//     data_out = ~(data_in ^ key)
//
// Notes:
//   - This module is the exact inverse of the Encrypt IP
//   - Requires the same key used during encryption
//   - Stateless and purely combinational
//   - Intended for lightweight protection, not cryptographic security

module decrypt(
    input  wire [7:0] data_in,   // Encrypted input byte
    input  wire [7:0] key,        // 8-bit decryption key
    output wire [7:0] data_out    // Decrypted output byte
);
    wire [7:0] dash_data_in;

    // XOR encrypted data with key (reverse XOR stage)
    assign dash_data_in = data_in ^ key;

    // Bitwise inversion to recover original data
    assign data_out = ~dash_data_in;

endmodule