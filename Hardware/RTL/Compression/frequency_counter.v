// Module: frequency_counter
// Description:
//   Frequency Counter IP core.
//   Counts the number of occurrences of each 8-bit symbol (0–255)
//   and stores the results in an internal frequency table.
//
//   This module is typically used prior to Huffman codebook
//   generation, where symbol statistics are required.
//
// Example (conceptual output):
//   Symbol        Frequency
//   -------------------------
//   00000000        405
//   00000001         41
//   00000010         22
//   00000011         14
//   00000100         15
//   00000101          8
//   00000110          1
//   ........        ....
//
// Notes:
//   - One symbol is counted per load pulse
//   - Frequency table supports up to 256 symbols
//   - Frequencies are readable asynchronously via addr

module frequency_counter (
    input clk,
    input reset,

    // ------------------------------------------------------------------
    // Symbol input interface
    // ------------------------------------------------------------------
    input [7:0] symbol,      // 8-bit symbol to be counted
    input load,              // Load request (level signal)

    // ------------------------------------------------------------------
    // Status and read interface (to processor / Vitis)
    // ------------------------------------------------------------------
    output reg done,         // One-cycle acknowledge pulse
    output [23:0] freq_out,  // Frequency read data
    input  [7:0] addr        // Address to read freq_table
);

    // ------------------------------------------------------------------
    // Frequency table
    // ------------------------------------------------------------------
    // Each index corresponds directly to one 8-bit symbol
    // Width allows counting up to 2^24 occurrences per symbol
    reg [23:0] freq_table [0:255];
    integer i;

    // ------------------------------------------------------------------
    // Edge detection for load signal
    // ------------------------------------------------------------------
    // Converts level-based load signal into a one-cycle pulse
    reg  load_d;
    wire load_pulse;

    always @(posedge clk) begin
        load_d <= load;
    end

    // Rising-edge detection
    assign load_pulse = load & ~load_d;

    // ------------------------------------------------------------------
    // Frequency counting logic
    // ------------------------------------------------------------------
    always @(posedge clk or posedge reset) begin
        if (reset) begin
            // Clear frequency table on reset
            for (i = 0; i < 256; i = i + 1)
                freq_table[i] <= 0;
            done <= 0;
        end else begin
            if (load_pulse) begin
                // Increment frequency of the input symbol
                freq_table[symbol] <= freq_table[symbol] + 1;
                done <= 1;  // Acknowledge to processor
            end else if (!load) begin
                // Clear done when load is deasserted
                done <= 0;
            end
        end
    end

    // ------------------------------------------------------------------
    // Frequency readout
    // ------------------------------------------------------------------
    // Allows processor to read any symbol's frequency
    assign freq_out = freq_table[addr];

endmodule