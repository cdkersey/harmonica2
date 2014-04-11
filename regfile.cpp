#include <fstream>

#include <chdl/chdl.h>
#include <chdl/cassign.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"

using namespace std;
using namespace chdl;

void PredRegs(pred_reg_t &out, fetch_pred_t &in, splitter_pred_t &wb);
void GpRegs(reg_func_t &out, pred_reg_t &in, splitter_reg_t &wb);

void PredRegs(pred_reg_t &out, fetch_pred_t &in, splitter_pred_t &wb) {
  HIERARCHY_ENTER();
  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  vec<W, vec<R, bvec<L> > > pregs;

  _(wb, "ready") = Lit(1);  // Always ready to accept writebacks.

  // Instantiate the registers
  for (unsigned w = 0; w < W; ++w) {
    for (unsigned r = 0; r < R; ++r) {
      for (unsigned l = 0; l < L; ++l) {
        node inMask(_(_(wb, "contents"), "mask")[l]),
             wSel(_(_(wb, "contents"), "wid") == Lit<WW>(w)),
    	     rSel(_(_(wb, "contents"), "dest") == Lit<RR>(r)),
  	     wr(inMask && rSel && wSel && _(wb, "valid"));
        pregs[w][r][l] = Wreg(wr, _(_(wb, "contents"), "val")[l]);
      }
    }
  }

  tap("pred_wb_wid", wSel);

  // Handle ready and valid signals
  node ready(_(out, "ready"));
  _(in, "ready") = ready;
  _(out, "valid") = Wreg(ready, _(in, "valid"));
  _(_(out, "contents"), "warp") = Wreg(ready, _(_(in, "contents"), "warp"));
  _(_(out, "contents"), "ir") = Wreg(ready, _(_(in, "contents"), "ir"));

  bvec<WW> wid(_(_(_(in, "contents"), "warp"), "id"));

  bvec<L> pmask, pval0, pval1;

  // The mask should be all 1s if the instruction is not predicated.
  _(_(_(out, "contents"), "pval"), "pmask") 
    = Wreg(ready, pmask | bvec<L>(!inst.has_pred()));

  _(_(_(out, "contents"), "pval"), "val0") = Wreg(ready, pval0);
  _(_(_(out, "contents"), "pval"), "val1") = Wreg(ready, pval1);

  pmask = Mux(inst.get_pred(), Mux(wid, pregs));
  pval0 = Mux(inst.get_psrc0(), Mux(wid, pregs));
  pval1 = Mux(inst.get_psrc1(), Mux(wid, pregs));

  TAP(pregs);

  HIERARCHY_EXIT();
}

void GpRegs(reg_func_t &out, pred_reg_t &in, splitter_reg_t &wb) {
  HIERARCHY_ENTER();
  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  vec<W, vec<L, vec<R, bvec<N> > > > regs;

  bvec<WW> wb_wid(_(_(wb, "contents"), "wid"));
  bvec<LL> wb_clonesrc(_(_(wb, "contents"), "clonesrc")),
           wb_clonedest(_(_(wb, "contents"), "clonedest"));
  vec<L, bvec<N> > wb_val(_(_(wb, "contents"), "val"));
  bvec<L> wbMask(_(_(wb, "contents"), "mask"));

  vec<R, bvec<N> > clonebus(Mux(wb_clonesrc, Mux(wb_wid, regs)));

  _(wb, "ready") = Lit(1); // Always ready to accept writebacks.

  // Instantiate the registers
  for (unsigned w = 0; w < W; ++w) {
    node destWarp(wb_wid == Lit<WW>(w));
    for (unsigned r = 0; r < R; ++r) {
      node destReg(_(_(wb, "contents"), "dest") == Lit<RR>(r));
      for (unsigned l = 0; l < L; ++l) {
        node clone(_(_(wb, "contents"), "clone") && Lit<LL>(l) == wb_clonedest),
             wr(destWarp && _(wb, "valid") && (clone || destReg && wbMask[l]));
        unsigned initVal(0);
        if (r == 0) initVal = l;
        else if (r == 1) initVal = w;
        regs[w][l][r] = Wreg(wr, Mux(clone, wb_val[l], clonebus[r]), initVal);
      }
    }
  }

  TAP(wb_wid);

  node ready(_(out, "ready"));
  bvec<WW> wid(_(_(_(in, "contents"), "warp"), "id"));

  vec<L, bvec<N> > rval0, rval1, rval2;

  // Get the register values
  for (unsigned l = 0; l < L; ++l) {
    rval0[l] = Mux(inst.get_rsrc0(), Mux(wid, regs)[l]);
    rval1[l] = Mux(inst.get_rsrc1(), Mux(wid, regs)[l]);
    rval2[l] = Mux(inst.get_rsrc2(), Mux(wid, regs)[l]);
  }

  _(in, "ready") = ready;
  _(out, "valid") = Wreg(ready, _(in, "valid"));
  _(_(out, "contents"), "warp") = Wreg(ready, _(_(in, "contents"), "warp"));
  _(_(out, "contents"), "ir") = Wreg(ready, _(_(in, "contents"), "ir"));
  _(_(out, "contents"), "pval") = Wreg(ready, _(_(in, "contents"), "pval"));
  Flatten(_(_(_(out,"contents"),"rval"),"val0")) = Wreg(ready, Flatten(rval0));
  Flatten(_(_(_(out,"contents"),"rval"),"val1")) = Wreg(ready, Flatten(rval1));
  Flatten(_(_(_(out,"contents"),"rval"),"val2")) = Wreg(ready, Flatten(rval2));

  TAP(regs);

  HIERARCHY_EXIT();
}
