#ifndef HARMONICA_CONFIG_H
#define HARMONICA_CONFIG_H

#include <chdl/chdl.h>

const unsigned W(4),        // Total warps
               L(4),        // Number of SIMD lanes
               N(32),       // Number of bits in a machine word
               R(8),        // Number of registers (and predicate registers)
               LINE(16),    // Words per cache line
               ROMSZ(4096); // Instruction ROM size

// Doubled constants mean log base 2 of the corresponding constant. LL bits can
// hold a lane ID, RR bits can uniquely identify a register.
const unsigned WW(chdl::CLOG2(W)), RR(chdl::CLOG2(R)), LL(chdl::CLOG2(L)),
               NN(chdl::CLOG2(N));

const bool FPGA(false),      // Produce Verilog netlist for FPGA
           FPGA_IO(false),   // Console output from DummyCache
           NETLIST(false),   // Produce a .netl file
           SIMULATE(true),   // Run a simulation
           SOFT_IO(true),    // Software I/O. Console output to stdout
           DEBUG_MEM(false); // Print all memory operations to stderr

#endif
