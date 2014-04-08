#include <fstream>

#include <chdl/chdl.h>
#include <chdl/cassign.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"

using namespace std;
using namespace chdl;

template <unsigned N>
   bvec<N> SerialMul(node &busy, bvec<N> a, bvec<N> b, node &start);

void Funcunit_mult(func_splitter_t &out, reg_func_t &in);
void Funcuint_div(func_splitter_t &out, reg_func_t &in);

void Funcunit_mult(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();
  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  node imm(inst.get_opcode()[4]), busy, start,
       next_full, full(Reg(next_full));

  bvec<N> immVal(inst.get_imm());

  vec<L, bvec<N> > a(_(_(_(in,"contents"),"rval"),"val0")),
                   b(_(_(_(in,"contents"),"rval"),"val1")),
                   mulOut;

  for (unsigned l = 0; l < L; ++l)
    mulOut[l] = SerialMul(busy, a[l], Mux(imm, b[l], immVal), start);

  Cassign(next_full).
    IF(start, Lit(1)).
    IF(_(out, "ready") && full, Lit(0)).
    ELSE(full);

  _(in, "ready") = !busy && !full;
  start = _(in, "valid") && _(in, "ready");
  _(out, "valid") = full && !busy;

  bvec<L> pmask(_(_(_(in, "contents"), "pval"), "pmask"));

  _(_(out, "contents"), "warp") = Wreg(start, _(_(in, "contents"), "warp"));
  _(_(_(out, "contents"), "rwb"), "mask") = Wreg(start, pmask);
  _(_(_(out, "contents"), "rwb"), "val") = mulOut;

  tap("mul_in", in);
  tap("mul_out", out);

  HIERARCHY_EXIT();
}

void Funcuint_div(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();
  // TODO
  HIERARCHY_EXIT();
}

template <unsigned N>
  bvec<N> SerialMul(node &busy, bvec<N> a, bvec<N> b, node &start)
{
  bvec<N> shrreg;
  bvec<CLOG2(N+1)> next_state, state(Reg(next_state));
  Cassign(next_state).
    IF(state == Lit<CLOG2(N+1)>(0)).
    IF(start, Lit<CLOG2(N+1)>(2)).
    ELSE(Lit<CLOG2(N+1)>(0)).
    END().
    IF(state == Lit<CLOG2(N+1)>(N), Lit<CLOG2(N+1)>(0)).
    IF(OrN(shrreg), state + Lit<CLOG2(N+1)>(2)).
    ELSE(Lit<CLOG2(N+1)>(0));

  TAP(state);

  busy = OrN(state);

  bvec<N> next_shlreg, shlreg(Reg(next_shlreg));
  Cassign(next_shlreg).
    IF(start, b).
    ELSE(shlreg << Lit<CLOG2(N)>(2));

  bvec<N> next_shrreg;
  shrreg = Reg(next_shrreg);
  Cassign(next_shrreg).
    IF(start, a).
    ELSE(shrreg >> Lit<CLOG2(N)>(2));

  bvec<N> shlreg2(Cat(shlreg[range<0,N-2>()], Lit(0))),
          shlreg3(shlreg2 + shlreg);

  TAP(shlreg); TAP(shrreg); TAP(shlreg2); TAP(shlreg3);

  bvec<N> next_resultreg, resultreg(Reg(next_resultreg));
  Cassign(next_resultreg).
    IF(start, Lit<N>(0)).
    IF(busy && shrreg[0] && !shrreg[1], resultreg + shlreg).
    IF(busy && !shrreg[0] && shrreg[1], resultreg + shlreg2).
    IF(busy && shrreg[0] && shrreg[1], resultreg + shlreg3).
    ELSE(resultreg);

  return resultreg;
}
