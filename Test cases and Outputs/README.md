# Test Cases and Outputs

This folder contains **reference input files, intermediate helper files,
and execution logs** used to validate the FPGA bitstream compression and
decompression workflow on real hardware.

The contents are intended to demonstrate **correctness, scalability,
and practical execution**, rather than minimal test vectors.

---

## Contents

### Reference Input and Outputs
- `INPUT.rbt`  
  A representative FPGA bitstream file (~33 MB) used as a **realistic
  input** to evaluate compression behavior and size reduction.

- `Compressed.bin`  
  The compressed binary output generated from the input bitstream
  after Huffman encoding and lightweight protection.

---

### Execution Logs
- `teraterm_log_compression.txt`  
  UART log captured during the **compression pipeline execution**.

- `teraterm_log_decompression.txt`  
  UART log captured during the **decompression and reconstruction
  pipeline execution**.

These logs serve as reference evidence of **successful real-hardware
runs** on the ZedBoard.

---

### Helper Files

The `helper/` subfolder contains **intermediate text files**
generated during the compression process:

- `HEADER.txt`     – Extracted bitstream header
- `PARSED.txt`     – Parsed 8-bit symbol stream
- `FREQUENCY.txt`  – Symbol frequency table
- `HMCODES.txt`    – Generated Huffman codebook
- `OUTPUT.txt`     – Huffman-encoded output stream

These files are included for **verification, and clarity**.

---

## Notes

- The large `.rbt` file is included intentionally to demonstrate
  compression on a **realistic-scale input**.
- Only a limited number of large files are provided to avoid
  unnecessary repository bloat.
- Helper files and logs are provided for transparency and
  reproducibility.