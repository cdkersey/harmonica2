#include <fstream>

#include <chdl/chdl.h>
#include <chdl/cassign.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"

using namespace std;
using namespace chdl;

void Funcunit_branch(func_splitter_t &out, reg_func_t &in);

// Execute branch instructions
void Funcunit_branch(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();
  bvec<N> pc(_(_(_(in, "contents"), "warp"), "pc"));

  node ready(_(out, "ready")), next_full, full(Reg(next_full));
  _(in, "ready") = !full || ready;

  Cassign(next_full).
    IF(full && _(out, "ready") && !_(in, "valid"), Lit(0)).
    IF(!full && _(in, "valid"), Lit(1)).
    ELSE(full);

  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  node ldregs(_(in, "valid") && _(in, "ready"));

  bvec<L> active(_(_(_(in, "contents"), "warp"), "active"));

  _(out, "valid") = full;
  _(_(_(out, "contents"), "warp"), "state") =
    Wreg(ldregs, _(_(_(in, "contents"), "warp"), "state"));
  _(_(_(out, "contents"), "warp"), "id") =
    Wreg(ldregs, _(_(_(in, "contents"), "warp"), "id"));

  bvec<L> pmask(_(_(_(in, "contents"), "pval"), "pmask")),
          outMask;

  bvec<N> rval0(_(_(_(in, "contents"), "rval"), "val0")[0]),
          rval1(_(_(_(in, "contents"), "rval"), "val1")[0]);

  bvec<CLOG2(L+1)> lanesVal(Zext<CLOG2(L+1)>(
    Mux(inst.get_opcode()[0], rval0, rval1)
  ));

  bvec<N> destVal(Mux(inst.get_opcode()==Lit<6>(0x21), rval0, rval1));

  Cassign(outMask).
    IF(inst.get_opcode() == Lit<6>(0x20)  // jal{i, r}s
       || inst.get_opcode() == Lit<6>(0x21),
         Zext<L>((Lit<L+1>(1) << Zext<CLOG2(L+1)>(lanesVal)) - Lit<L+1>(1))).
    IF(inst.get_opcode() == Lit<6>(0x22), Lit<L>(1)).     // jmprt
    ELSE(bvec<L>(active));

  _(_(_(out, "contents"), "warp"), "active") = Wreg(ldregs, outMask);

  _(_(_(out, "contents"), "rwb"), "mask") =
    Wreg(ldregs, pmask & active & bvec<L>(inst.has_rdst()));
  _(_(_(out, "contents"), "rwb"), "wid") =
    Wreg(ldregs, _(_(_(in, "contents"), "warp"), "id"));
  _(_(_(out, "contents"), "rwb"), "dest") = Wreg(ldregs, inst.get_rdst());
  _(_(_(out, "contents"), "rwb"), "val") = Wreg(ldregs, pc);

  bvec<N> out_pc;

  Cassign(out_pc).
    IF(pmask[0]).
      IF(inst.get_opcode() == Lit<6>(0x1c)
         || inst.get_opcode() == Lit<6>(0x1e) // jalr[s], jmpr[t]
         || inst.get_opcode() == Lit<6>(0x21)
         || inst.get_opcode() == Lit<6>(0x22), destVal).
      IF(inst.get_opcode() == Lit<6>(0x1b)
         || inst.get_opcode() == Lit<6>(0x1d) // jali[s], jmpi
         || inst.get_opcode() == Lit<6>(0x20), pc+inst.get_imm()).
      ELSE(pc).
    END().
    ELSE(pc);

  _(_(_(out, "contents"), "warp"), "pc") = Wreg(ldregs, out_pc);

  // Handle the wspawn instruction
  _(_(out, "contents"), "spawn") =
    Wreg(ldregs, inst.get_opcode() == Lit<6>(0x3a));
  _(_(out, "contents"), "spawn_pc") = Wreg(ldregs, rval0);

  tap("branch_full", full);
  tap("branch_out", out);
  tap("branch_in", in);
  HIERARCHY_EXIT();
}
