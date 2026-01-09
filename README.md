# FPGA Bitstream Compression using Huffman Coding
## Hardware–Software Co-Design with Lightweight Protection

## Overview
This project implements a complete **lossless compression,
lightweight protection, and decompression pipeline** for
AMD/Xilinx FPGA bitstreams (`.rbt` format).

The system is developed using a **hardware–software co-design**
approach on a **Zynq-7000 SoC (ZedBoard)**, combining custom
Verilog RTL IP cores with bare-metal software control.

---

## Key Features
- Pure Huffman-based **lossless compression**
- 32-bit to 8-bit symbol-space reduction
- Dynamic Huffman codebook loading via **AXI4-Lite**
- Custom Huffman Encoder and Decoder IP cores
- Lightweight XOR/NOT-based **reversible protection**
- End-to-end verification via **real FPGA reprogramming**

## Architecture
Input .rbt → Bit Parser → Frequency Counter →
Huffman Encoder → Encryption → Storage
→ Decryption → Huffman Decoder → Bit Merger → Output .rbt

---

## Results
- Bitstream size reduction of approximately **63%**
- Verified **lossless recovery** by successfully reprogramming
  the FPGA with the decompressed bitstream

---

## Tools and Platform
- **AMD Vivado 2023.1** – RTL design, synthesis, implementation
- **AMD Vitis 2023.1** – Software control and HW–SW integration
- **ZedBoard (Zynq-7000, xc7z020)** – Target hardware platform
- **Tera Term** – UART-based debugging and runtime logs

---

## License
This project is released under the **MIT License**.  
© 2026 Manivannan C.

See the `LICENSE` file for full details.

---

## Disclaimer
This repository represents **academic work** carried out during an
internship. The views, design choices, and implementations are those
of the author and do not represent official positions of
**NIT Tiruchirappalli**.