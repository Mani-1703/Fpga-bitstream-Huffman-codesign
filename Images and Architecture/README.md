# Architecture and Block Diagrams

This folder contains **system-level architecture diagrams** and
**Vivado block design images** used to illustrate the hardware–software
co-design of the FPGA bitstream compression and decompression framework.

These diagrams provide a **visual overview of data flow,
custom IP integration, and AXI4-Lite connectivity**, complementing the
RTL and software implementation.

---

## Contents

### 1. System Pipeline Diagram
- `final_pipeline.pdf`
- Shows the complete end-to-end workflow:
  - Bitstream compression and lightweight protection
  - Decryption and decompression
- Clearly separates **software-controlled operations** and
  **custom hardware IP cores**
- Highlights header handling, Huffman codebook flow, and key-based
  protection.

---

### 2. Vivado Block Design – Compression Side
- Illustrates the hardware integration for the compression stage
- Shows:
  - Zynq Processing System (PS)
  - AXI Interconnect
  - Custom IPs:
    - Bit Parser
    - Frequency Counter
    - Huffman Encoder
    - Encryption IP
- All custom IPs are connected to the PS via **AXI4-Lite slave interfaces**.

---

### 3. Vivado Block Design – Decompression Side
- Illustrates the hardware integration for the decompression stage
- Shows:
  - Zynq Processing System (PS)
  - AXI Interconnect
  - Custom IPs:
    - Decryption IP
    - Huffman Decoder
    - Bits Merger
- Mirrors the compression-side architecture using **AXI4-Lite** control
  for correct end-to-end recovery.

---

## Notes

- The AXI4-Lite interconnect is directly represented by the Vivado
  block design diagrams shown here.
- These images focus on **architectural connectivity**, not register-level
  or timing details.
- The term *pipeline* refers to a sequential multi-stage workflow and
  not a deeply pipelined hardware datapath.