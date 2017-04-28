#include <fstream>

#include <chdl/chdl.h>
#include <chdl/cassign.h>
#include <chdl/egress.h>
#include <chdl/memreq.h>
#include <chdl/loader.h>
#include <chdl/sort.h>

#include <chdl/counter.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"
#include "util.h"

using namespace std;
using namespace chdl;


template<unsigned P, unsigned B, unsigned N, unsigned A, unsigned I,
         unsigned BQ, unsigned NQ, unsigned AQ, unsigned IQ>
  void Coalescer(vec<P, mem_port<B, N, A, I> > &p, mem_port<BQ, NQ, AQ, IQ> &q)
{
  const unsigned L = (BQ*NQ)/(B*N), LL = CLOG2(L);
  
  // Port buffers
  vec<P, mem_req<B, N, A, I> > req;
  for (unsigned i = 0; i < P; ++i)
    RegBuffer<0>(req[i], _(p[i], "req"));

  // Input signals
  bvec<P> valid_in, wr_in;
  vec<P, bvec<A> > addr_in;
  vec<P, bvec<LL> > offset_in;
  vec<P, bvec<B*N> > data_in;
  vec<P, bvec<A - CLOG2((BQ*NQ)/(B*N))> > tag_in;

  for (unsigned i = 0; i < P; ++i) {
    valid_in[i] = _(req[i], "valid");
    addr_in[i] = _(_(req[i], "contents"), "addr");
    wr_in[i] = _(_(req[i], "contents"), "wr");
    tag_in[i] = addr_in[i][range<LL,A-1>()];
    offset_in[i] = addr_in[i][range<0,LL-1>()];
    data_in[i] = Flatten(_(_(req[i], "contents"), "data"));
  }
  
  // Line state
  node valid, dirty;
  bvec<A - CLOG2((BQ*NQ)/(B*N))> tag;
  bvec<BQ*NQ> data;

  // Control
  node clear_valid, set_valid, clear_dirty, set_dirty;

  valid = Wreg(clear_valid || set_valid, set_valid);
  dirty = Wreg(clear_dirty || set_dirty, set_dirty);

  // Hit/Miss logic
  bvec<P> tag_match, hit, miss;
  node any_miss = OrN(miss);
  
  for (unsigned i = 0; i < P; ++i) {
    tag_match[i] = (tag == tag_in[i]);
    hit[i] = tag_match[i] && valid && valid_in[i];
    miss[i] = (!valid || !tag_match[i]) && valid_in[i];
  }

  // Downstream request gneration
  node any_write_hit = OrN(hit & wr_in);
  node do_read, waiting, read_resp;
  node writeback = any_miss && dirty;
  bvec<CLOG2(P)> current_miss = Log2(miss);
  bvec<P> current_miss_1h = Zext<P>(Decoder(current_miss, any_miss));
  bvec<A> miss_addr = Mux(current_miss, addr_in);
  vec<L, bvec<B*N> > read_data;
 

  _(_(q, "req"), "valid") = writeback || do_read;
  _(_(_(q, "req"), "contents"), "wr") = writeback;
  _(_(_(q, "req"), "contents"), "addr") =
    Mux(writeback, Zext<AQ>(miss_addr[range<LL,A-1>()]), Zext<AQ>(tag));

  set_dirty = any_write_hit;
  clear_dirty = writeback && _(_(q, "req"), "valid") && _(_(q, "req"), "ready");

  do_read = !waiting && !writeback;
  read_resp = _(_(q, "resp"), "valid") && !_(_(_(q, "resp"), "contents"), "wr");
  Flatten(read_data) = Flatten(_(_(_(q, "resp"), "contents"), "data"));
  waiting = Wreg(do_read || read_resp, do_read);

  set_valid = read_resp;
  clear_valid = writeback || do_read;

  tag = Wreg(read_resp, miss_addr[range<LL,A-1>()]);
  Flatten(_(_(_(q, "req"), "contents"), "data")) = data;
  _(_(_(q, "req"), "contents"), "mask") = ~Lit<NQ>(0);
  
  // Downstream response handling
  _(_(q, "resp"), "ready") = Lit(1);

  vec<L, bvec<B*N> > data_v;
  for (unsigned i = 0; i < BQ*NQ; ++i) {
    data_v[i/(B*N)][i%(B*N)] = data[i];
  }

  bvec<L> write;
  vec<P, bvec<L> > wr_match_t;
  vec<L, bvec<P> > wr_match;
  vec<L, bvec<CLOG2(P)> > wr_port_sel;
  for (unsigned i = 0; i < P; ++i)
    wr_match_t[i] = Decoder(offset_in[i], hit[i] && wr_in[i]);
  
  for (unsigned i = 0; i < L; ++i) {
    for (unsigned j = 0; j < P; ++j)
      wr_match[i][j] = wr_match_t[j][i];
    write[i] = OrN(wr_match[i]);
    wr_port_sel[i] = Log2(wr_match[i]);
    data_v[i] = Wreg(write[i] || read_resp,
                  Mux(read_resp, Mux(wr_port_sel[i], data_in), read_data[i]));
  }
  
  // Upstream connections
  for (unsigned i = 0; i < P; ++i) {
    _(req[i], "ready") = hit[i] && _(_(p[i], "resp"), "ready");
    _(_(p[i], "resp"), "valid") = hit[i];
    _(_(_(p[i], "resp"), "contents"), "id") =
      _(_(_(p[i], "req"), "contents"), "id");
    Flatten(_(_(_(p[i], "resp"), "contents"), "data")) =
      Mux(offset_in[i], data_v);
  }

  #ifdef DEBUG_COALESCER
  TAP(valid);
  TAP(dirty);
  TAP(tag);
  TAP(data);
  TAP(tag_match);
  TAP(hit);
  TAP(miss);
  TAP(current_miss);
  TAP(current_miss_1h);
  TAP(miss_addr);
  TAP(do_read);
  TAP(waiting);
  TAP(writeback);
  TAP(addr_in);
  TAP(tag_in);
  TAP(offset_in);
  TAP(data_in);
  TAP(write);
  TAP(wr_port_sel);
  #endif
}

void Funcunit_lsu(func_splitter_t &out_buf, reg_func_t &in);

// Register that is transparent during write; edge-triggered, but provides
// same-cycle access to written data. TODO: pipeline this instead
template <typename T> T TReg(node wr, const T &d) { 
  return Mux(wr, Wreg(wr, d), d);
}

#ifdef COALESCE_IMEM
extern mem_port<8, N/8, N - (NN - 3), WW> *imem_p;
#endif

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
  #ifdef COALESCE
  #ifdef COALESCE_IMEM
  vec<L + 1, mem_port<8, N/8, ABITS, WW> > dmem;
  Connect(_(dmem[L], "req"), _(*imem_p, "req"));
  Connect(_(*imem_p, "resp"), _(dmem[L], "resp"));
  #else
  vec<L, mem_port<8, N/8, ABITS, WW> > dmem;
  #endif
  mem_port<MEM_B, MEM_N, MEM_A, MEM_I> dmem_c_nondir;
  out_mem_port<MEM_B, MEM_N, MEM_A, MEM_I> dmem_c(dmem_c_nondir);

  Coalescer(dmem, dmem_c_nondir);

  #else
  vec<L, out_mem_port<8, N/8, ABITS, WW> > dmem;
  #endif
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

  #ifdef COALESCE
  EXPOSE(dmem_c);
  #else
  for (unsigned l = 0; l < L; ++l) {
    ostringstream oss;
    oss << "dmem_" << l;
    Expose(oss.str(), dmem[l]);
  }
  #endif

  RegBuffer<0>(out_buffered, out);

  HIERARCHY_EXIT();
}
