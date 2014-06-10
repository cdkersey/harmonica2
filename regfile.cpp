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

  bvec<WW> wid(_(_(_(in, "contents"), "warp"), "id")),
           wb_wid(_(_(wb, "contents"), "wid"));

  bvec<WW + RR> a_ipred(Cat(wid, inst.get_pred())),
                a_src0 (Cat(wid, inst.get_psrc0())),
                a_src1 (Cat(wid, inst.get_psrc1())),
                a_wb   (Cat(wb_wid, _(_(wb, "contents"), "dest")));

  bvec<L> wb_mask(_(_(wb, "contents"), "mask") & bvec<L>(_(wb, "valid"))),
          wb_val(_(_(wb, "contents"), "val"));

  vec<3, bvec<WW + RR> > rd_addr;
  rd_addr[0] = a_ipred;
  rd_addr[1] = a_src0;
  rd_addr[2] = a_src1;

  vec<L, vec<3, bvec<1> > > q;

  for (unsigned i = 0; i < L; ++i)
    q[i] = Syncmem(rd_addr, bvec<1>(wb_val[i]), a_wb, wb_mask[i]);
 
  bvec<L> pval0, pval1, pmask;

  for (unsigned i = 0; i < L; ++i) {
    pmask[i] = q[i][0][0];
    pval0[i] = q[i][1][0];
    pval1[i] = q[i][2][0];
  }

  _(wb, "ready") = Lit(1);  // Always ready to accept writebacks.

  tap("pred_wb_wid", _(_(wb, "contents"), "wid"));

  // Handle ready and valid signals
  node ready(_(out, "ready"));
  _(in, "ready") = ready;
  _(out, "valid") = Wreg(ready, _(in, "valid"));
  _(_(out, "contents"), "warp") = Wreg(ready, _(_(in, "contents"), "warp"));
  _(_(out, "contents"), "ir") = Wreg(ready, _(_(in, "contents"), "ir"));

  // The mask should be all 1s if the instruction is not predicated.
  _(_(_(out, "contents"), "pval"), "pmask") 
    = Latch(!ready, pmask | bvec<L>(Reg(!inst.has_pred())));

  _(_(_(out, "contents"), "pval"), "val0") = Latch(!ready, pval0);
  _(_(_(out, "contents"), "pval"), "val1") = Latch(!ready, pval1);

  HIERARCHY_EXIT();
}

void GpRegs(reg_func_t &out_buffered, pred_reg_t &in, splitter_reg_t &wb) {
  HIERARCHY_ENTER();
  reg_func_t out;

  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  bvec<RR> wb_dest(_(_(wb, "contents"), "dest"));
  bvec<WW> wid(_(_(_(in, "contents"), "warp"), "id")),
           wb_wid(_(_(wb, "contents"), "wid"));
  bvec<LL> wb_clonesrc(_(_(wb, "contents"), "clonesrc")),
           wb_clonedest(_(_(wb, "contents"), "clonedest"));
  vec<L, bvec<N> > wb_val(_(_(wb, "contents"), "val"));
  bvec<L> wbMask(_(_(wb, "contents"), "mask"));

  vec<R, bvec<N> > clonebus;

  _(wb, "ready") = Lit(1); // Always ready to accept writebacks.

  vec<L, bvec<N> > rval0, rval1, rval2;

  node clone(_(wb, "valid") && _(_(wb, "contents"), "clone"));
  bvec<L> wb_mask(_(_(wb, "contents"), "mask") &
                    bvec<L>(_(wb, "valid") && !clone));

  vec<L, vec<R, vec<2, bvec<N> > > > q;

  vec<2, bvec<WW> > a;
  a[0] = Mux(clone, wid, wb_wid);
  a[1] = wb_wid;

  for (unsigned l = 0; l < L; ++l) {
    for (unsigned i = 0; i < R; ++i) {
      node wr(wb_mask[l] && wb_dest == Lit<RR>(i) ||
                wb_clonedest == Lit<LL>(l) && clone);
      q[l][i] = Syncmem(a, Mux(clone, wb_val[l], clonebus[i]), wb_wid, wr);
    }
  }

  for (unsigned i = 0; i < R; ++i) {
    vec<L, bvec<N> > cb_slice;
    for (unsigned l = 0; l < L; ++l)
      cb_slice[l] = q[l][i][1];
    clonebus[i] = Mux(wb_clonesrc, cb_slice);
  }


  for (unsigned l = 0; l < L; ++l) {
    rval0[l] = Mux(Reg(inst.get_rsrc0()), q[l])[0];
    rval1[l] = Mux(Reg(inst.get_rsrc1()), q[l])[0];
    rval2[l] = Mux(Reg(inst.get_rsrc2()), q[l])[0];
  }

  TAP(rval0);
  TAP(rval1);
  TAP(rval2);

  TAP(wb_wid);

  node ready(_(out, "ready"));

  _(in, "ready") = ready;
  _(out, "valid") = Wreg(ready, _(in, "valid"));
  _(_(out, "contents"), "warp") = Wreg(ready, _(_(in, "contents"), "warp"));
  _(_(out, "contents"), "ir") = Wreg(ready, _(_(in, "contents"), "ir"));
  _(_(out, "contents"), "pval") = Wreg(ready, _(_(in, "contents"), "pval"));
  Flatten(_(_(_(out,"contents"),"rval"),"val0")) = Latch(!ready,Flatten(rval0));
  Flatten(_(_(_(out,"contents"),"rval"),"val1")) = Latch(!ready,Flatten(rval1));
  Flatten(_(_(_(out,"contents"),"rval"),"val2")) = Latch(!ready,Flatten(rval2));

  Buffer<1>(out_buffered, out);

  HIERARCHY_EXIT();
}
