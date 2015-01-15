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
void Funcunit_fmul(func_splitter_t &out, reg_func_t &in);
void Funcunit_fadd(func_splitter_t &out, reg_func_t &in);
void Funcunit_ftoi(func_splitter_t &out, reg_func_t &in);
void Funcunit_itof(func_splitter_t &out, reg_func_t &in);

// Interface with the modules
typedef ag<STP("stall"),     in<node>,
        ag<STP("valid_in"),  in<node>,
        ag<STP("valid_out"), out<node>,
        ag<STP("a"),         in<bvec<32> >,
        ag<STP("b"),         in<bvec<32> >,
        ag<STP("c"),         out<bvec<32> > > > > > > > interface_t;

// Keep additional signals in the same pipeline position as the computations.
template <typename T> T StraightPipe(node stall, const T &in, unsigned t = 0) {
  if (t == 0) return in;
  else        return Wreg(!stall, StraightPipe(stall, in, t-1));
}

void Funcunit_fmul(func_splitter_t &out, reg_func_t &in) {
  const unsigned LATENCY(3);
  node stall;
  reg_func_t in_d(StraightPipe(stall, in, LATENCY)); // Delayed input
  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  vec<L, interface_t> iface;
  for (unsigned l = 0; l < L; ++l) {
    Load("fmul.nand")("io", iface[l]);
    _(iface[l], "valid_in") = _(in, "valid") && !stall;
    _(iface[l], "stall") = stall;
    _(iface[l], "a") = _(_(_(in, "contents"), "rval"), "val0")[l];
    _(iface[l], "b") = _(_(_(in, "contents"), "rval"), "val1")[l];
    _(_(_(out, "contents"), "rwb"), "val")[l] = _(iface[l], "c");
  }


  tap("fmul_iface", iface);
  tap("fmul_in", in);
  tap("fmul_in_d", in_d);

  _(out, "valid") = _(in_d, "valid"); //_(iface[0], "valid_out");
  stall = _(out, "valid") && !_(out, "ready");
  _(_(out, "contents"), "warp") = _(_(in_d, "contents"), "warp");
  _(_(out, "contents"), "spawn") = Lit(0);

  bvec<L> pmask(_(_(_(in_d, "contents"), "pval"), "pmask")),
          active(_(_(_(in_d, "contents"), "warp"), "active"));
  _(_(_(out, "contents"), "rwb"), "mask") = pmask & active;
  _(_(_(out, "contents"), "rwb"), "dest") = inst.get_rdst();
  _(_(_(out, "contents"), "rwb"), "wid") = _(_(_(in_d, "contents"),"warp"),"id");
  _(_(_(out, "contents"), "rwb"), "clone") = Lit(0);
  _(_(_(out, "contents"), "pwb"), "mask") = Lit<L>(0);

  _(in, "ready") = !stall;
}

void Funcunit_fadd(func_splitter_t &out, reg_func_t &in) {
  const unsigned LATENCY(3);
  node stall;
  reg_func_t in_d(StraightPipe(stall, in, LATENCY)); // Delayed input
  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  vec<L, interface_t> iface;
  for (unsigned l = 0; l < L; ++l) {
    Load("fadd.nand")("io", iface[l]);
    _(iface[l], "valid_in") = _(in, "valid") && !stall;
    _(iface[l], "stall") = stall;
    _(iface[l], "a") = _(_(_(in, "contents"), "rval"), "val0")[l];
    _(iface[l], "b")[range<0,30>()] =
      _(_(_(in, "contents"), "rval"), "val1")[l][range<0,30>()];
    _(iface[l], "b")[31] = Xor(
      inst.get_opcode()[1],
      _(_(_(in, "contents"), "rval"), "val1")[l][31]
    );
    _(_(_(out, "contents"), "rwb"), "val")[l] = _(iface[l], "c");
  }

  tap("fadd_iface", iface);
  tap("fadd_in", in);
  tap("fadd_in_d", in_d);

  _(out, "valid") = _(iface[0], "valid_out");
  stall = _(out, "valid") && !_(out, "ready");
  _(_(out, "contents"), "warp") = _(_(in_d, "contents"), "warp");
  _(_(out, "contents"), "spawn") = Lit(0);

  bvec<L> pmask(_(_(_(in_d, "contents"), "pval"), "pmask")),
          active(_(_(_(in_d, "contents"), "warp"), "active"));
  _(_(_(out, "contents"), "rwb"), "mask") = pmask & active;
  _(_(_(out, "contents"), "rwb"), "dest") = inst.get_rdst();
  _(_(_(out, "contents"), "rwb"), "wid") = _(_(_(in_d, "contents"),"warp"),"id");
  _(_(_(out, "contents"), "rwb"), "clone") = Lit(0);
  _(_(_(out, "contents"), "pwb"), "mask") = Lit<L>(0);

  _(in, "ready") = !stall;
}

void Funcunit_ftoi(func_splitter_t &out, reg_func_t &in) {
  const unsigned LATENCY(3);
  node stall;
  reg_func_t in_d(StraightPipe(stall, in, LATENCY)); // Delayed input
  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  vec<L, interface_t> iface;
  for (unsigned l = 0; l < L; ++l) {
    Load("ftoi.nand")("io", iface[l]);
    _(iface[l], "valid_in") = _(in, "valid") && !stall;
    _(iface[l], "stall") = stall;
    _(iface[l], "a") = _(_(_(in, "contents"), "rval"), "val0")[l];
    _(_(_(out, "contents"), "rwb"), "val")[l] = _(iface[l], "c");
  }

  tap("ftoi_iface", iface);
  tap("ftoi_in", in);
  tap("ftoi_in_d", in_d);
  

  _(out, "valid") = _(iface[0], "valid_out");
  stall = _(out, "valid") && !_(out, "ready");
  _(_(out, "contents"), "warp") = _(_(in_d, "contents"), "warp");
  _(_(out, "contents"), "spawn") = Lit(0);

  bvec<L> pmask(_(_(_(in_d, "contents"), "pval"), "pmask")),
          active(_(_(_(in_d, "contents"), "warp"), "active"));
  _(_(_(out, "contents"), "rwb"), "mask") = pmask & active;
  _(_(_(out, "contents"), "rwb"), "dest") = inst.get_rdst();
  _(_(_(out, "contents"), "rwb"), "wid") = _(_(_(in_d, "contents"),"warp"),"id");
  _(_(_(out, "contents"), "rwb"), "clone") = Lit(0);
  _(_(_(out, "contents"), "pwb"), "mask") = Lit<L>(0);

  _(in, "ready") = !stall;
}

void Funcunit_itof(func_splitter_t &out, reg_func_t &in) {
  const unsigned LATENCY(3);
  node stall;
  reg_func_t in_d(StraightPipe(stall, in, LATENCY)); // Delayed input
  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  vec<L, interface_t> iface;
  for (unsigned l = 0; l < L; ++l) {
    Load("itof.nand")("io", iface[l]);
    _(iface[l], "valid_in") = _(in, "valid") && !stall;
    _(iface[l], "stall") = stall;
    _(iface[l], "a") = _(_(_(in, "contents"), "rval"), "val0")[l];
    _(_(_(out, "contents"), "rwb"), "val")[l] = _(iface[l], "c");
  }

  tap("itof_iface", iface);
  tap("itof_in", in);
  tap("itof_in_d", in_d);

  _(out, "valid") = _(iface[0], "valid_out");
  stall = _(out, "valid") && !_(out, "ready");
  _(_(out, "contents"), "warp") = _(_(in_d, "contents"), "warp");
  _(_(out, "contents"), "spawn") = Lit(0);

  bvec<L> pmask(_(_(_(in_d, "contents"), "pval"), "pmask")),
          active(_(_(_(in_d, "contents"), "warp"), "active"));
  _(_(_(out, "contents"), "rwb"), "mask") = pmask & active;
  _(_(_(out, "contents"), "rwb"), "dest") = inst.get_rdst();
  _(_(_(out, "contents"), "rwb"), "wid") = _(_(_(in_d, "contents"),"warp"),"id");
  _(_(_(out, "contents"), "rwb"), "clone") = Lit(0);
  _(_(_(out, "contents"), "pwb"), "mask") = Lit<L>(0);

  _(in, "ready") = !stall;
}

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

  _(out, "valid") = out_valid;

  
  
  HIERARCHY_EXIT();
}
