# Software – Vitis Applications

This folder contains the **bare-metal Vitis C applications**
used to control and orchestrate the hardware IP cores
implemented in the FPGA fabric.

The software communicates with custom RTL IPs through
**AXI4-Lite slave interfaces** and manages file I/O via
an SD card using the **xilffs (FatFs)** library.

---

## Contents

### 1. `compression.c`
- Implements the **compression pipeline controller**
- Responsibilities:
  - Bit parsing using hardware IP
  - Frequency counting using hardware IP
  - Huffman codebook generation in software
  - Huffman encoding using hardware IP
  - Bundling of header, codebook, and compressed output
  - Lightweight encryption using hardware IP
- Runs sequentially and mirrors the system architecture

---

### 2. `decompression.c`
- Implements the **decompression pipeline controller**
- Responsibilities:
  - Decryption using hardware IP
  - Separation of bundled file components
  - Regeneration of Huffman helper files
  - Huffman decoding using hardware IP
  - Symbol merging and final bitstream reconstruction
- Produces the recovered `.rbt` file

---

### 3. `sdCard.c / sdCard.h`
- Lightweight SD card and file-system helper layer
- Uses **xilffs (FatFs)** for FAT32 support
- Provides basic file operations:
  - Initialization and eject
  - File open, read, write, and close
- Keeps file-system logic separate from application logic

---

## Notes

- All applications are **bare-metal** (no OS)
- File names follow **FAT32 8.3 naming rules**
- Intermediate files are optionally preserved for debugging
- Base addresses used in the code must match Vivado address mapping

---

## Build & Run

- Import the sources into an **AMD Vitis** application project
- Ensure `xilffs` is enabled in the BSP
- Program the FPGA with the corresponding Vivado bitstream
- Run the application on the Zynq PS via UART