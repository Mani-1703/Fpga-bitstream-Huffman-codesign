# FPGA Bitstream Compression using Huffman Coding
## with Hardware–Software Co-Design and Lightweight Protection

## Overview
This project implements a complete lossless compression,
lightweight protection, and decompression pipeline for
AMD/Xilinx FPGA bitstreams (.rbt format).

The system is designed using a hardware–software co-design
approach on a Zynq-7000 SoC (ZedBoard).

## Key Features
- Pure Huffman-based lossless compression
- 32-bit to 8-bit symbol-space reduction
- Dynamic Huffman codebook loading via AXI-Lite
- Custom Huffman Encoder and Decoder IPs
- Lightweight XOR/NOT-based reversible protection
- End-to-end verification via FPGA reprogramming

## Architecture
Input .rbt → Bit Parser → Frequency Counter →
Huffman Encoder → Encryption → Storage
→ Decryption → Huffman Decoder → Bit Merger → Output .rbt

## Results
- Average compression: ~63%
- Verified lossless recovery by reprogramming FPGA

## Tools and Platform

- AMD Vivado 2023.1 – RTL design, synthesis, implementation
- AMD Vitis 2023.1 – Software control and HW–SW integration
- ZedBoard (Zynq-7000, xc7z020) – Target hardware platform
- Tera Term – UART-based debug and runtime logs
