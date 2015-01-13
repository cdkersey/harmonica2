#include <fstream>

#include <chdl/chdl.h>
#include <chdl/loader.h>
#include <chdl/dir.h>
#include <chdl/numeric.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"

using namespace std;
using namespace chdl;

// Functional Units
void Funcunit_fpu(func_splitter_t &out, reg_func_t &in);

// Interface with the modules
typedef ag<STP("stall"),     in<node>,
        ag<STP("valid_in"),  in<node>,
        ag<STP("valid_out"), out<node>,
        ag<STP("a"),         in<bvec<32> >,
        ag<STP("b"),         in<bvec<32> >,
        ag<STP("c"),         out<bvec<32> > > > > > > > interface_t;

// Functional Unit: Floating Point Add/Multiply
void Funcunit_fpu(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();
  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));
  vec<L, interface_t> adder, multiplier, itof, ftoi;

  TAP(adder);
  TAP(multiplier);
  TAP(itof);
  TAP(ftoi);
  
  node do_add(inst.get_opcode() == Lit<6>(0x35)),
       do_sub(inst.get_opcode() == Lit<6>(0x36)),
       do_mul(inst.get_opcode() == Lit<6>(0x37)),
       do_itof(inst.get_opcode() == Lit<6>(0x33)),
       do_ftoi(inst.get_opcode() == Lit<6>(0x34)),
       out_valid(_(multiplier[0], "valid_out") ||
		 _(adder[0], "valid_out") ||
		 _(ftoi[0], "valid_out") ||
		 _(itof[0], "valid_out")),
       stall(out_valid && !_(out, "ready"));

  for (unsigned l = 0; l < L; ++l) {
    Load("fmul.nand")("io", multiplier[l]);
    Load("fadd.nand")("io", adder[l]);
    Load("itof.nand")("io", itof[l]);
    Load("ftoi.nand")("io", ftoi[l]);

    _(multiplier[l], "valid_in") = _(in, "valid") && do_mul;
    _(adder[l], "valid_in") = _(in, "valid") && (do_add || do_sub);
    _(itof[l], "valid_in") = _(in, "valid") && do_itof;
    _(ftoi[l], "valid_in") = _(in, "valid") && do_ftoi;

    // The adder's b input sign bit has to be xored so we can support
    // subtraction.
    bvec<32> adder_in_b;
    adder_in_b[range<0, 30>()] =
      _(_(_(in, "contents"), "rval"), "val1")[l][range<0, 30>()];
    adder_in_b[31] = Xor(_(_(_(in, "contents"),"rval"),"val1")[l][31], do_sub);

    _(multiplier[l], "a") = _(_(_(in, "contents"), "rval"), "val0")[l];
    _(multiplier[l], "b") = _(_(_(in, "contents"), "rval"), "val1")[l];
    _(adder[l], "a") = _(_(_(in, "contents"), "rval"), "val0")[l];
    _(adder[l], "b") = adder_in_b;
    _(itof[l], "a") = _(_(_(in, "contents"), "rval"), "val0")[l];
    _(ftoi[l], "a") = _(_(_(in, "contents"), "rval"), "val0")[l];
    
    _(multiplier[l], "stall")
     = _(adder[l], "stall")
     = _(itof[l], "stall")
     = _(ftoi[l], "stall") = stall;

    // TODO: route output.
  }

  
  
  HIERARCHY_EXIT();
}
