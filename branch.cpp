#include <fstream>

#include <chdl/chdl.h>
#include <chdl/cassign.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"

using namespace std;
using namespace chdl;

void Funcunit_branch(func_splitter_t &out, reg_func_t &in);

typedef ag<STP("fallthrough"), node,
        ag<STP("mask"), bvec<L>,
        ag<STP("pc"), bvec<N> > > > ipdom_stack_entry_t;

template <unsigned N, typename T> T IpdomStack(
  node &full, node &empty, node push, node pop, const T& tval, const T& bval
);

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

  bvec<LL> dest_idx(Lsb(active & pmask));
  node taken(OrN(active & pmask));

  bvec<N> rval0(Mux(dest_idx, _(_(_(in, "contents"), "rval"), "val0"))),
          rval1(Mux(dest_idx, _(_(_(in, "contents"), "rval"), "val1")));

  bvec<CLOG2(L+1)> lanesVal(Zext<CLOG2(L+1)>(
    Mux(inst.get_opcode()[0], rval0, rval1)
  ));

  bvec<N> destVal(Mux(inst.get_opcode()==Lit<6>(0x21), rval0, rval1));

  _(_(_(out, "contents"), "warp"), "active") = Wreg(ldregs, outMask);

  _(_(_(out, "contents"), "rwb"), "mask") =
    Wreg(ldregs, pmask & active & bvec<L>(inst.has_rdst()));
  _(_(_(out, "contents"), "rwb"), "wid") =
    Wreg(ldregs, _(_(_(in, "contents"), "warp"), "id"));
  _(_(_(out, "contents"), "rwb"), "dest") = Wreg(ldregs, inst.get_rdst());
  _(_(_(out, "contents"), "rwb"), "val") = Wreg(ldregs, pc);

  // Handle split and join instructions
  node stack_full, stack_empty, push, pop;
  ipdom_stack_entry_t otherbranch, fallthrough;
  _(otherbranch, "fallthrough") = Lit(0);
  _(fallthrough, "fallthrough") = Lit(1);
  _(otherbranch, "mask") = active & ~pmask;
  _(fallthrough, "mask") = active;
  _(otherbranch, "pc") = pc;
  _(fallthrough, "pc") = Lit<N>(0);

  ipdom_stack_entry_t top(IpdomStack<IPDOM_STACK_SZ>(
    stack_full, stack_empty, push, pop, otherbranch, fallthrough
  ));

  push = inst.get_opcode() == Lit<6>(0x3b) && _(in, "valid"); // split
  pop = inst.get_opcode() == Lit<6>(0x3c) && _(in, "valid"); // join

  Cassign(outMask).
    IF(taken).
      IF(inst.get_opcode() == Lit<6>(0x20)  // jal{i, r}s
         || inst.get_opcode() == Lit<6>(0x21),
           Zext<L>((Lit<L+1>(1) << Zext<CLOG2(L+1)>(lanesVal)) - Lit<L+1>(1))).
      IF(inst.get_opcode() == Lit<6>(0x22), Lit<L>(1)).     // jmprt
      IF(inst.get_opcode() == Lit<6>(0x3b), active & pmask).
      IF(inst.get_opcode() == Lit<6>(0x3c), _(top, "mask")).
      ELSE(active).END().ELSE(active);

  bvec<N> out_pc;

  Cassign(out_pc).
    IF(taken).
      IF(inst.get_opcode() == Lit<6>(0x1c)
         || inst.get_opcode() == Lit<6>(0x1e) // jalr[s], jmpr[t]
         || inst.get_opcode() == Lit<6>(0x21)
         || inst.get_opcode() == Lit<6>(0x22), destVal).
      IF(inst.get_opcode() == Lit<6>(0x1b)
         || inst.get_opcode() == Lit<6>(0x1d) // jali[s], jmpi
         || inst.get_opcode() == Lit<6>(0x20), pc+inst.get_imm()).
      IF(inst.get_opcode()==Lit<6>(0x3c) && !_(top,"fallthrough"), _(top,"pc")).
    ELSE(pc).END().ELSE(pc);

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

// The stack used in IPDOM pushes two entries at a time and pops one at a time.
// We implement it here with registers.
// It does not have to handle simultaneous pushes and pops.
template <unsigned N, typename T> T IpdomStack(
  node &full, node &empty, node push, node pop, const T& tval, const T& bval
)
{
  bvec<N+1> next_count, count(Reg(next_count));
  Cassign(next_count).
    IF(push, count + Lit<N+1>(2)).
    IF(pop, count - Lit<N+1>(1)).
    ELSE(count);  

  full = (count == Cat(Lit(1), Lit<N>(0)));
  empty = (count == Lit<N+1>(0));

  vec<(1<<N), T> store;
  T top(Mux(Zext<N>(count - Lit<N+1>(1)), store));
  for (unsigned i = 0; i < (1<<N); ++i) {
    T next;
    Cassign(next).
      IF(push && count == Lit<N+1>(i - 1), tval).
      IF(push && count == Lit<N+1>(i), bval).
      ELSE(store[i]);
    store[i] = Reg(next);
  }

  tap("ipdom_stack_count", count);
  tap("ipdom_stack_store", store);
  tap("ipdom_stack_top", top);

  return top;
}
