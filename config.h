#ifndef HARMONICA_CONFIG_H
#define HARMONICA_CONFIG_H

#include <chdl/chdl.h>

const unsigned W(4), // Total warps
               L(4), // Number of SIMD lanes
               N(32), // Number of bits in a machine word
               R(8), // Number of GPRs/predicate registers
               LINE(16), // Words per cache line
               ROMSZ(4096), // Instruction ROM size
               IPDOM_STACK_SZ(2), // Hardware stack used for control flow.
               BARRIERS(4), // Supported number of simultaneous barriers
               DUMMYCACHE_SZ(64); // Cache lines in main memory

// Doubled constants mean log base 2 of the corresponding constant. LL bits can
// hold a lane ID, RR bits can uniquely identify a register.
const unsigned WW(chdl::CLOG2(W)), RR(chdl::CLOG2(R)), LL(chdl::CLOG2(L)),
               NN(chdl::CLOG2(N));

const bool SRAM_REGS(false), // Use SRAM for register files
           FPGA(false), // Produce Verilog netlist for FPGA
           FPGA_IO(false), // Console output from DummyCache
           NETLIST(false), // Produce a .netl file
           SIMULATE(true), // Run a simulation
           SOFT_IO(true),  // Software I/O. Console output to stdout
           DEBUG_MEM(false), // Print all memory operations to stderr
           FPU(false);
#endif
