# RTL – Verilog Modules

This directory contains the **custom Verilog HDL implementations**
of all hardware modules used in the FPGA bitstream compression and
decompression pipeline.

The RTL is organized into two stages:

- **Compression**: bit parsing, frequency counting, Huffman encoding,
  and lightweight encryption.
- **Decompression**: lightweight decryption, Huffman decoding, and
  bit merging to reconstruct the original bitstream format.

All modules are handwritten, synthesizable Verilog and are designed
to be packaged as custom IP cores in AMD Vivado and controlled via
software running on the Zynq Processing System.