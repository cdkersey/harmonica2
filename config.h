#ifndef HARMONICA_CONFIG_H
#define HARMONICA_CONFIG_H

#include <chdl/chdl.h>

const unsigned W(8),        // Total warps
               L(8),        // Number of SIMD lanes
               N(32),       // Number of bits in a machine word
               R(32),       // Number of registers (and predicate registers)
               LINE(16),    // Words per cache line
               ROMSZ(4096); // Instruction ROM size

// Doubled constants mean log base 2 of the corresponding constant. LL bits can
// hold a lane ID, RR bits can uniquely identify a register.
const unsigned WW(chdl::CLOG2(W)), RR(chdl::CLOG2(R)), LL(chdl::CLOG2(L)),
               NN(chdl::CLOG2(N));
#endif
