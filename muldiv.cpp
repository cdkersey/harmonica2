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

template <unsigned N>
  void SerialDiv(
    bvec<N> &q, bvec<N> &r, node &ready, chdl::node &waiting,
    bvec<N> n, bvec<N> d, node start, node stall
  );

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
    IF(!busy && _(out, "ready"), Lit(0)).
    ELSE(full);

  _(in, "ready") = !busy && !full;
  start = _(in, "valid") && _(in, "ready");
  _(out, "valid") = full && !busy;

  bvec<L> pmask(_(_(_(in, "contents"), "pval"), "pmask"));

  _(_(out, "contents"), "warp") = Wreg(start, _(_(in, "contents"), "warp"));
  _(_(_(out, "contents"), "rwb"), "dest") = Wreg(start, inst.get_rdst());
  _(_(_(out, "contents"), "rwb"), "mask") = Wreg(start, pmask);
  _(_(_(out, "contents"), "rwb"), "val") = mulOut;

  tap("mul_full", full);
  tap("mul_busy", busy);
  tap("mul_start", start);
  tap("mul_in", in);
  tap("mul_out", out);

  HIERARCHY_EXIT();
}

void Funcuint_div(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();
  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  node imm(inst.get_opcode()[4]), start;

  bvec<L> ready, valid;

  bvec<N> immVal(inst.get_imm());

  vec<L, bvec<N> > a(_(_(_(in,"contents"),"rval"),"val0")),
                   b(_(_(_(in,"contents"),"rval"),"val1")),
                   q, r;

  for (unsigned l = 0; l < L; ++l) {
    SerialDiv(q[l], r[l], valid[l], ready[l],
              a[l], Mux(imm, b[l], immVal), start, !_(out, "ready"));
  }

  _(in, "ready") = AndN(ready);
  start = _(in, "valid") && _(in, "ready");
  _(out, "valid") = AndN(valid);

  bvec<L> pmask(_(_(_(in, "contents"), "pval"), "pmask"));

  _(_(out, "contents"), "warp") = Wreg(start, _(_(in, "contents"), "warp"));
  _(_(_(out, "contents"), "rwb"), "dest") = Wreg(start, inst.get_rdst());
  _(_(_(out, "contents"), "rwb"), "mask") = Wreg(start, pmask);
  for (unsigned l = 0; l < L; ++l)
    _(_(_(out, "contents"), "rwb"), "val")[l] = Mux(inst.get_opcode()[0], r[l], q[l]);

  tap("div_valid", valid);
  tap("div_ready", ready);
  tap("div_start", start);
  tap("div_in", in);
  tap("div_out", out);

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

  bvec<N> next_resultreg, resultreg(Reg(next_resultreg));
  Cassign(next_resultreg).
    IF(start, Lit<N>(0)).
    IF(busy && shrreg[0] && !shrreg[1], resultreg + shlreg).
    IF(busy && !shrreg[0] && shrreg[1], resultreg + shlreg2).
    IF(busy && shrreg[0] && shrreg[1], resultreg + shlreg3).
    ELSE(resultreg);

  return resultreg;
}

template <unsigned N, bool D>
  chdl::bvec<N> Shiftreg(
    chdl::bvec<N> in, chdl::node load, chdl::node shift, chdl::node shin
  )
{
  using namespace chdl;
  using namespace std;

  HIERARCHY_ENTER();  

  bvec<N+1> val;
  val[D?N:0] = shin;

  if (D) {
    for (int i = N-1; i >= 0; --i)
      val[i] = Reg(Mux(load, Mux(shift, val[i], val[i+1]), in[i]));
    HIERARCHY_EXIT();
    return val[range<0, N-1>()];
  } else {
    for (unsigned i = 1; i < N; ++i)
      val[i] = Reg(Mux(load, Mux(shift, val[i], val[i-1]), in[i-1]));
    HIERARCHY_EXIT();
    return val[range<1, N>()];
  }
}

template <unsigned N>
  chdl::bvec<N> Lshiftreg(
    chdl::bvec<N> in, chdl::node load, chdl::node shift,
    chdl::node shin = chdl::Lit(0)
  )
{ return Shiftreg<N, false>(in, load, shift, shin); }

template <unsigned N>
  chdl::bvec<N> Rshiftreg(
    chdl::bvec<N> in, chdl::node load, chdl::node shift,
    chdl::node shin = chdl::Lit(0)
  )
{ return Shiftreg<N, true>(in, load, shift, shin); }

template <unsigned N>
  void SerialDiv(
    bvec<N> &q, bvec<N> &r, node &ready, node &waiting,
    bvec<N> n, bvec<N> d, node start, node stall
  )
{
  // The controller
  bvec<CLOG2(N+3)> next_state, state(Reg(next_state));
  Cassign(next_state).
    IF(state == Lit<CLOG2(N+3)>(0)).
      IF(start, Lit<CLOG2(N+3)>(1)).
      ELSE(Lit<CLOG2(N+3)>(0)).
    END().IF(state == Lit<CLOG2(N+3)>(N+2)).
      IF(stall, Lit<CLOG2(N+3)>(N+2)).
      ELSE(Lit<CLOG2(N+3)>(0)).
    END().ELSE(state + Lit<CLOG2(N+3)>(1));

  tap("div_state", state);

  ready = (state == Lit<CLOG2(N+3)>(N+2));
  waiting = (state == Lit<CLOG2(N+3)>(0));

  tap("div_waiting", waiting);

  // The data path
  bvec<2*N> s(Rshiftreg(Cat(d, Lit<N>(0)), start, Lit(1)));
  node qbit(Cat(Lit<N>(0), r) >= s);
  r = Reg(Mux(start, Mux(qbit, r, r - s[range<0, N-1>()]), n));
  q = Lshiftreg(Lit<N>(0), start, !ready, qbit);

  tap("div_s", s);
  tap("div_r", r);
  tap("div_q", q);
}
