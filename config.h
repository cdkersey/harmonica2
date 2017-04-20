#ifndef HARMONICA_CONFIG_H
#define HARMONICA_CONFIG_H

#include <chdl/chdl.h>

const unsigned W(8), // Total warps
               L(2), // Number of SIMD lanes
               N(32), // Number of bits in a machine word
               R(8), // Number of GPRs/predicate registers
               IPDOM_STACK_SZ(4), // Hardware stack used for control flow.
               BARRIERS(4); // Supported number of simultaneous barriers

// Doubled constants mean log base 2 of the corresponding constant. LL bits can
// hold a lane ID, RR bits can uniquely identify a register.
const unsigned WW(chdl::CLOG2(W)), RR(chdl::CLOG2(R)), LL(chdl::CLOG2(L)),
               NN(chdl::CLOG2(N));

const bool SRAM_REGS(true),  // Use SRAM for register files
           FPGA(true),      // Produce Verilog netlist for FPGA
           FPGA_IO(false),   // Console output from DummyCache
           FPU(false);       // Floating point operations
#endif
