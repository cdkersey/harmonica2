#include <fstream>

#include <chdl/chdl.h>
#include <chdl/cassign.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"

using namespace std;
using namespace chdl;

// Functional Units
void Funcunit_alu(func_splitter_t &out, reg_func_t &in);
void Funcunit_plu(func_splitter_t &out, reg_func_t &in);

// Functional Unit: (Integer) Arithmetic/Logic Unit
void Funcunit_alu(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();
  node ready(_(out, "ready")), next_full, full(Reg(next_full));
  _(in, "ready") = ready || !full;

  // TODO: is "full" defined correctly?
  Cassign(next_full).
    IF(!full).
      IF(_(in, "valid") && !_(out, "ready"), Lit(1)).
      ELSE(Lit(0)).
    END().ELSE().
      IF(_(out, "ready"), Lit(0)).
      ELSE(Lit(1));

  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  bvec<L> pmask(_(_(_(in, "contents"), "pval"), "pmask"));

  node ldregs(ready || !full);

  _(out, "valid") = Reg(_(in, "valid")) || full;
  _(_(out, "contents"), "warp") = Wreg(ldregs, _(_(in, "contents"), "warp"));

  _(_(_(out, "contents"), "rwb"), "mask") =
    Wreg(ldregs, bvec<L>(inst.has_rdst()) & pmask); // TODO: and w/ active lanes
  _(_(_(out, "contents"), "rwb"), "dest") = Wreg(ldregs, inst.get_rdst());

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

  tap("alu_full", full);
  tap("alu_out", out);
  tap("alu_in", in);

  HIERARCHY_EXIT();
}

// Predicate Logic Unit
void Funcunit_plu(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();

  node ready(_(out, "ready")), next_full, full(Reg(next_full));
  _(in, "ready") = ready || !full;

  Cassign(next_full).
    IF(!full).
      IF(_(in, "valid") && !_(out, "ready"), Lit(1)).
      ELSE(Lit(0)).
    END().ELSE().
      IF(_(out, "ready"), Lit(0)).
      ELSE(Lit(1));

  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  node ldregs(ready || !full);

  _(out, "valid") = Reg(_(in, "valid")) || full;
  _(_(out, "contents"), "warp") = Wreg(ldregs, _(_(in, "contents"), "warp"));

  bvec<L> pmask(_(_(_(in, "contents"), "pval"), "pmask"));

  _(_(_(out, "contents"), "pwb"), "mask") =
    Wreg(ldregs, bvec<L>(inst.has_pdst()) & pmask); // TODO: and w/ active lanes
  _(_(_(out, "contents"), "pwb"), "dest") = Wreg(ldregs, inst.get_pdst());

  bvec<L> out_val;

  for (unsigned l = 0; l < L; ++l) {
    bvec<N> rval0(_(_(_(in, "contents"), "rval"), "val0")[l]);
    node pval0(_(_(_(in, "contents"), "pval"), "val0")[l]),
         pval1(_(_(_(in, "contents"), "pval"), "val1")[l]);

    Cassign(out_val[l]).
      IF(inst.get_opcode() == Lit<6>(0x26), OrN(rval0)).     // rtop
      IF(inst.get_opcode() == Lit<6>(0x2c), !OrN(rval0)).    // iszero
      IF(inst.get_opcode() == Lit<6>(0x2b), rval0[N-1]).     // isneg
      IF(inst.get_opcode() == Lit<6>(0x27), pval0 && pval1). // andp
      IF(inst.get_opcode() == Lit<6>(0x28), pval0 || pval1). // orp
      IF(inst.get_opcode() == Lit<6>(0x29), pval0 != pval1). // xorp
      IF(inst.get_opcode() == Lit<6>(0x2a), !pval0). // notp
      ELSE(Lit('0'));
  }

  _(_(_(out, "contents"), "pwb"), "val") = Wreg(ldregs, out_val);

  tap("plu_full", full);
  tap("plu_out", out);
  tap("plu_in", in);

  HIERARCHY_EXIT();
}
