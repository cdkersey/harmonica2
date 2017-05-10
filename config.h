#ifndef HARMONICA_CONFIG_H
#define HARMONICA_CONFIG_H

#include <chdl/chdl.h>

const unsigned W(16), // Total warps
               L(16), // Number of SIMD lanes
               N(32), // Number of bits in a machine word
               R(32), // Number of GPRs/predicate registers
               IPDOM_STACK_SZ(4), // Hardware stack used for control flow.
               BARRIERS(4), // Supported number of simultaneous barriers
               MEM_B(256),
               MEM_N(1),
               MEM_A(27),
               MEM_I(10);

// Doubled constants mean log base 2 of the corresponding constant. LL bits can
// hold a lane ID, RR bits can uniquely identify a register.
const unsigned WW(chdl::CLOG2(W)), RR(chdl::CLOG2(R)), LL(chdl::CLOG2(L)),
               NN(chdl::CLOG2(N));

const bool SRAM_REGS(true),  // Use SRAM for register files
           FPGA(true),       // Produce Verilog netlist for FPGA
           FPGA_IO(false),   // Console output from DummyCache
           FPU(false);       // Floating point operations

#define COALESCE
#define COALESCE_IMEM
#define START_PORT
#define MODULE_REGFILE

#endif
