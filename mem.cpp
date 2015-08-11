#include <fstream>

#include <chdl/chdl.h>
#include <chdl/cassign.h>
#include <chdl/egress.h>
#include <chdl/memreq.h>
#include <chdl/loader.h>

#include <chdl/counter.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"
#include "util.h"

using namespace std;
using namespace chdl;

void Funcunit_lsu(func_splitter_t &out_buf, reg_func_t &in);

// Register that is transparent during write; edge-triggered, but provides
// same-cycle access to written data. TODO: pipeline this instead
template <typename T> T TReg(node wr, const T &d) { 
  return Mux(wr, Wreg(wr, d), d);
}

// Load/store unit
void Funcunit_lsu(func_splitter_t &out_buffered, reg_func_t &in)
{
  HIERARCHY_ENTER();

  func_splitter_t out;

  tap("lsu_out", out);
  tap("lsu_in", in);

  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  node valid_req(_(in, "ready") && _(in, "valid")),
       load_inst(valid_req && !inst.is_store()),
       store_inst(valid_req && inst.is_store());

  bvec<L> active(_(_(_(in, "contents"), "warp"), "active")),
          pmask(_(_(_(in, "contents"), "pval"), "pmask")),
          lane_valid(bvec<L>(_(in, "ready")&&_(in, "valid")) & active & pmask);

  node clear_pending_lsb;

  node bypass(store_inst || (load_inst && !OrN(active & pmask)));

  bvec<CLOG2(L + 1)> lanes(PopCount(lane_valid));

  Counter("insts_ld", lanes, load_inst);
  Counter("insts_st", lanes, store_inst);

  const unsigned ABITS(N - CLOG2(N/8));
  vec<L, out_mem_port<8, N/8, ABITS, WW> > dmem;
  bvec<L> dmem_req_readys, dmem_resp_valids;
  vec<L, bvec<WW> > dmem_resp_wids;

  _(in, "ready") = AndN(dmem_req_readys) && _(out, "ready");;

  vec<W, warp_t> wtable;
  vec<W, bvec<RR> > rdest;
  vec<W, bvec<L> > waiting, // Set for each lane waiting for a response.
                   masktable; // Which lanes to write back.
  vec<W, vec<L, bvec<N> > > data; // Collected read data for each lane.
  bvec<W> finished, // Final response has arrived for given warp.
          pending; // Warp pending move to out.
  bvec<WW> wtable_idx;
  bvec<L> lane_finished; // Lane is providing final response for its warp.
  warp_t out_warp = Mux(wtable_idx, wtable);
  TAP(wtable);

  node write_wtable(load_inst && OrN(active & pmask));
  bvec<WW> wtable_wr_idx(_(_(_(in, "contents"), "warp"), "id"));

  bvec<W> inhibit; // Finishing warp can't drive output on this cycle.
  inhibit[0] = bypass || !_(out, "ready");
  for (unsigned w = 1; w < W; ++w)
    inhibit[w] = inhibit[w-1] || finished[w-1];

  for (unsigned w = 0; w < W; ++w) {
    node wr = write_wtable && wtable_wr_idx == Lit<WW>(w);
    wtable[w] = Wreg(wr, _(_(in, "contents"), "warp"));
    rdest[w] = Wreg(wr, inst.get_rdst());

    bvec<L> resp_lane;
    for (unsigned l = 0; l < L; ++l)
      resp_lane[l] = dmem_resp_wids[l] == Lit<WW>(w) &&
                       dmem_resp_valids[l];

    node set_pending, clear_pending;

    for (unsigned l = 0; l < L; ++l)
      data[w][l] = TReg(resp_lane[l],
                        Flatten(_(_(_(dmem[l], "resp"), "contents"), "data")));

    pending[w] = Reg((pending[w] && !clear_pending) || set_pending);

    set_pending = finished[w] && inhibit[w];
    clear_pending = (Lit<WW>(w) == Lsb(pending)) && clear_pending_lsb;

    bvec<L> next_waiting, next_masktable;
    waiting[w] = Reg(next_waiting);
    masktable[w] = Reg(next_masktable);

    Cassign(next_waiting).
      IF(write_wtable && wtable_wr_idx == Lit<WW>(w), active & pmask).
      IF(OrN(dmem_resp_valids), waiting[w] & ~resp_lane).
      ELSE(waiting[w]);

    Cassign(next_masktable).
      IF(write_wtable && wtable_wr_idx == Lit<WW>(w), active & pmask).
      ELSE(masktable[w]);

    finished[w] = OrN(waiting[w]) && !OrN(next_waiting);
  }

  TAP(bypass);
  TAP(waiting);
  TAP(finished);
  TAP(pending);
  TAP(data);
  TAP(inhibit);
  TAP(masktable);

  for (unsigned l = 0; l < L; ++l) {
    bvec<N> rval0(_(_(_(in, "contents"), "rval"), "val0")[l]),
            rval1(_(_(_(in, "contents"), "rval"), "val1")[l]),
            addrN(Mux(inst.is_store(), rval0, rval1) + inst.get_imm());
    bvec<ABITS> addr(addrN[range<CLOG2(N/8), N-1>()]);

    lane_finished[l] = Mux(dmem_resp_wids[l], finished);

    dmem_req_readys[l] = _(_(dmem[l], "req"), "ready");
    dmem_resp_valids[l] = _(_(dmem[l], "resp"), "valid") &&
                            !_(_(_(dmem[l], "resp"), "contents"), "wr");
    dmem_resp_wids[l] = _(_(_(dmem[l], "resp"), "contents"), "id");
    _(_(dmem[l], "resp"), "ready") = Lit(1);
    _(_(dmem[l], "req"), "valid") = valid_req && active[l] && pmask[l];
    _(_(_(dmem[l], "req"), "contents"), "wr") = inst.is_store();
    _(_(_(dmem[l], "req"), "contents"), "id") = wtable_wr_idx;
    _(_(_(dmem[l], "req"), "contents"), "addr") = addr;
    Flatten(_(_(_(dmem[l], "req"), "contents"), "data")) = rval0;
    _(_(_(dmem[l], "req"), "contents"), "mask") = ~Lit<N/8>(0);
  }

  node from_pending(!OrN(finished) && OrN(pending));

  _(out, "valid") = bypass || OrN(finished) || OrN(pending);

  clear_pending_lsb = _(out, "ready") && !bypass && from_pending;

  _(_(out, "contents"), "warp") =
    Mux(bypass, out_warp, _(_(in, "contents"), "warp"));

  _(_(_(out, "contents"), "rwb"), "wid") = wtable_idx;
  _(_(_(out, "contents"), "rwb"), "dest") = Mux(wtable_idx, rdest);
  _(_(_(out, "contents"), "rwb"), "val") = Mux(wtable_idx, data);
  _(_(_(out, "contents"), "rwb"), "mask") =
    bvec<L>(!bypass) & Mux(wtable_idx, masktable);

  Cassign(wtable_idx).
    IF(from_pending, Lsb(pending)).
    ELSE(Lsb(finished));

  if (EXT_DMEM) {
    for (unsigned l = 0; l < L; ++l) {
      ostringstream oss;
      oss << "dmem_" << l;
      Expose(oss.str(), dmem[l]);
    }
  } else {
    vec<L, mem_port<8, N/8, ABITS, WW> > dmem_undirected, dmem_buffered;
    mem_port<8, N/8, ABITS, WW + LL> dmem_scratchpad;

    for (unsigned l = 0; l < L; ++l) {
      dmem_undirected[l] = dmem[l];
      _(dmem_buffered[l], "resp") = _(dmem_undirected[l], "resp");
      Buffer<1>(_(dmem_buffered[l], "req"), _(dmem_undirected[l], "req"));
    }

    Share(dmem_scratchpad, dmem_buffered);

    Scratchpad<CLOG2(DUMMYCACHE_SZ)>(dmem_scratchpad);

    TAP(dmem_scratchpad);

    // Console I/O for FPGAs and simulation.
    if (SOFT_IO || FPGA_IO) {
      node wrConsole(_(_(dmem_scratchpad, "req"), "valid") &&
                     _(_(dmem_scratchpad, "req"), "ready") &&
                     _(_(_(dmem_scratchpad, "req"), "contents"), "wr") &&
                     _(_(_(dmem_scratchpad, "req"), "contents"), "addr") ==
                       Lit<ABITS>(0x80000000>>CLOG2(N/8)));

      bvec<8> consoleVal(_(_(_(dmem_scratchpad, "req"), "contents"), "data")[0]);

      if (SOFT_IO) {
        static int val_consoleVal;
        EgressInt(val_consoleVal, consoleVal);
        EgressFunc([](bool x){
          if (x) cout << (char)val_consoleVal;
        }, wrConsole);
        TAP(wrConsole);
        TAP(consoleVal);
      } else {
        OUTPUT(wrConsole);
        OUTPUT(consoleVal);
      }
    }
  }

  Buffer<1>(out_buffered, out);

  HIERARCHY_EXIT();
}
