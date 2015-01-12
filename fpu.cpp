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
typedef ag<STP("stall"),     out<node>,
        ag<STP("valid_in"),  out<node>,
        ag<STP("valid_out"), in<node>,
        ag<STP("a"),         out<fp32>,
        ag<STP("b"),         out<fp32>,
        ag<STP("c"),         in<fp32> > > > > > > interface_t;

// Functional Unit: Floating Point Add/Multiply
void Funcunit_fpu(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();
  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));
  interface_t adder[L], multiplier[L];

  for (unsigned l = 0; l < L; ++l) {
    Load("fmul.nand")("io", multiplier[l]);
    Load("fadd.nand")("io", adder[l]);
  }

  node add(inst.get_opcode() == Lit<6>(0x35)),
       sub(inst.get_opcode() == Lit<6>(0x36)),
       mul(inst.get_opcode() == Lit<6>(0x37));
  
  #if 0
  node ready(_(out, "ready")), next_full, full(Reg(next_full));
  _(in, "ready") = !full || ready;

  Cassign(next_full).
    IF(full && _(out, "ready") && !_(in, "valid"), Lit(0)).
    IF(!full && _(in, "valid"), Lit(1)).
    ELSE(full);

  bvec<L> active(_(_(_(in, "contents"), "warp"), "active"));

  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  bvec<L> pmask(_(_(_(in, "contents"), "pval"), "pmask"));


  node ldregs(_(in, "valid") && _(in, "ready"));


  _(out, "valid") = full;
  _(_(out, "contents"), "warp") = Wreg(ldregs, _(_(in, "contents"), "warp"));

  _(_(_(out, "contents"), "rwb"), "mask") =
    Wreg(ldregs, bvec<L>(inst.has_rdst()) & pmask & active);
  _(_(_(out, "contents"), "rwb"), "dest") = Wreg(ldregs, inst.get_rdst());
  _(_(_(out, "contents"), "rwb"), "wid") =
    Wreg(ldregs, _(_(_(in, "contents"), "warp"), "id"    ));

  vec<L, bvec<N> > out_val;

  for (unsigned l = 0; l < L; ++l) {
    bvec<N> rval0(_(_(_(in, "contents"), "rval"), "val0")[l]), 
            rval1(_(_(_(in, "contents"), "rval"), "val1")[l]);

    Cassign(out_val[l]).
      IF(inst.get_opcode() == Lit<6>(0x05), -rval0). // neg
      IF(inst.get_opcode() == Lit<6>(0x06), ~rval0). // not
      IF(inst.get_opcode() == Lit<6>(0x07), rval0 & rval1). // and
      IF(inst.get_opcode() == Lit<6>(0x08), rval0 | rval1). // or
      IF(inst.get_opcode() == Lit<6>(0x09), rval0 ^ rval1). // xor
      IF(inst.get_opcode() == Lit<6>(0x0a), rval0 + rval1).  // add
      IF(inst.get_opcode() == Lit<6>(0x0b), rval0 - rval1).  // sub
      IF(inst.get_opcode() == Lit<6>(0x0f), rval0<<Zext<CLOG2(N)>(rval1)).// shl
      IF(inst.get_opcode() == Lit<6>(0x10), rval0>>Zext<CLOG2(N)>(rval1)).// shr
      IF(inst.get_opcode() == Lit<6>(0x11), rval0 & inst.get_imm()). // andi
      IF(inst.get_opcode() == Lit<6>(0x12), rval0 | inst.get_imm()). // ori
      IF(inst.get_opcode() == Lit<6>(0x13), rval0 ^ inst.get_imm()). // xori
      IF(inst.get_opcode() == Lit<6>(0x14), rval0 + inst.get_imm()).  // addi
      IF(inst.get_opcode() == Lit<6>(0x15), rval0 - inst.get_imm()).  // subi
      IF(inst.get_opcode() == Lit<6>(0x19),
         rval0 << Zext<CLOG2(N)>(inst.get_imm())). // shli
      IF(inst.get_opcode() == Lit<6>(0x1a),
         rval0 >> Zext<CLOG2(N)>(inst.get_imm())). // shri
      IF(inst.get_opcode() == Lit<6>(0x25), inst.get_imm()). // ldi
      ELSE(Lit<N>(0));

    _(_(_(out, "contents"), "rwb"), "val")[l] = Wreg(ldregs, out_val[l]);
  }

  _(_(_(out, "contents"), "rwb"), "clone") = Wreg(ldregs, inst.is_clone());
  _(_(_(out, "contents"), "rwb"), "clonedest") =
    Wreg(ldregs, _(_(_(in, "contents"), "rval"), "val0")[0][range<0, LL-1>()]);

  tap("alu_full", full);
  tap("alu_out", out);
  tap("alu_in", in);
  
  #endif
  HIERARCHY_EXIT();
}
