# Known Issues and Implementation Pitfalls

This folder documents **practical challenges** encountered while
transitioning from standalone RTL simulation to a complete
**hardware–software co-designed system** using Vivado and Vitis.

These notes are included to help readers understand common pitfalls
associated with AXI-based custom IP integration and embedded software
deployment on Zynq platforms.

---

## Summary of Key Issues

### 1. AXI Interfacing Constraints

RTL modules that function correctly in a Verilog testbench may fail
when wrapped as AXI4-Lite slave IPs if protocol timing and handshaking
requirements are not strictly followed.

Careful validation of control signals, reset behavior, and transaction
timing is essential when integrating custom logic with AXI interfaces.

---

### 2. Makefile Bug in AXI Slave IPs

When custom IPs are packaged with AXI interfaces, Vivado generates a
Makefile to support compilation in Vitis. In some toolchain versions,
this Makefile incorrectly defines object files using wildcard patterns
(e.g., `OUTS = *.o`), which can lead to compilation errors such as:

"xparameters.h: No such file or directory"

The issue can be resolved by modifying the Makefile to explicitly
generate object lists using wildcard expansion, as recommended in
official AMD/Xilinx support documentation.

After applying the fix, the IP **must be repackaged** before exporting
to Vitis; otherwise, the faulty Makefile will continue to be used.

Reference:
- AMD/Xilinx Answer Record 75527
[link](https://www.xilinx.com/support/answers/75527.html)
---

### 3. Vitis FAT32 and SD Card File Naming Constraints

SD card access in Vitis relies on the FAT32 file system, which follows
the 8.3 (Short File Name) convention. File names must be uppercase and
limited to 8 characters for the name and 3 for the extension.

Additionally, the `xilffs` (FatFs) library must be enabled in the BSP
to support standard file I/O operations (`f_open`, `f_read`, `f_write`).

To ensure compatibility, all files used in the workflow were renamed
using short, descriptive 8.3-compliant names (e.g., `ENCR.BIN`,
`DECOMP.RBT`).