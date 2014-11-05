#ifndef HARMONICA_INTERFACES_H
#define HARMONICA_INTERFACES_H

#include <chdl/chdl.h>
#include <chdl/ag.h>
#include <chdl/net.h>

#include "config.h"

namespace chdl {
  enum Tstate {
    TS_USER, TS_DIVERGENT_BR, TS_PENDING_INT, TS_KERNEL, N_TSTATES
  };

  const unsigned SS(CLOG2(N_TSTATES));

  // Basic warp variables
  typedef ag<STP("state"), bvec<SS>,
          ag<STP("pc"), bvec<N>,
          ag<STP("active"), bvec<L>, 
          ag<STP("id"), bvec<WW>,
          ag<STP("next_id"), bvec<WW> > > > > > warp_t;

  // Predicate read values
  typedef ag<STP("pmask"), bvec<L>,
          ag<STP("val0"), bvec<L>,
          ag<STP("val1"), bvec<L> > > > pval_t;

  // Writeback predicate values
  typedef ag<STP("mask"), bvec<L>,
          ag<STP("val"), bvec<L>,
          ag<STP("wid"), bvec<WW>, 
          ag<STP("dest"), bvec<RR> > > > > pwb_t;

  // Register read values
  typedef ag<STP("val0"), vec<L, bvec<N> >,
          ag<STP("val1"), vec<L, bvec<N> >,
          ag<STP("val2"), vec<L, bvec<N> > > > > rval_t;

  // Writeback register values
  typedef ag<STP("mask"), bvec<L>,
          ag<STP("val"), vec<L, bvec<N> >,
          ag<STP("wid"), bvec<WW>, 
          ag<STP("dest"), bvec<RR>,
          ag<STP("clone"), node,
          ag<STP("clonesrc"), bvec<LL>,
          ag<STP("clonedest"), bvec<LL> > > > > > > > rwb_t;

  // Sched->Fetch
  typedef flit<warp_t> sched_fetch_t;

  // Fetch->Predicate
  typedef flit<ag<STP("warp"), warp_t,
               ag<STP("ir"), bvec<N> > > > fetch_pred_t;

  // Predicate->Reg
  typedef flit<ag<STP("warp"), warp_t,
               ag<STP("ir"),   bvec<N>,
               ag<STP("pval"), pval_t> > > > pred_reg_t;

  // Reg->Dispatch, Dispatch->FU
  typedef ag<STP("warp"), warp_t,
          ag<STP("ir"), bvec<N>,
          ag<STP("pval"), pval_t,
          ag<STP("rval"), rval_t> > > > reg_func_int_t;

  typedef flit<reg_func_int_t> reg_func_t;

  // FU->Arbiter, Arbiter->Splitter
  typedef flit<ag<STP("warp"), warp_t,
               ag<STP("spawn"), node,
               ag<STP("spawn_pc"), bvec<N>,
               ag<STP("rwb"), rwb_t,
               ag<STP("pwb"), pwb_t> > > > > > func_splitter_t;

  // Splitter->Register
  typedef flit<rwb_t> splitter_reg_t;

  // Splitter->Predicate
  typedef flit<pwb_t> splitter_pred_t;

  // Splitter->Sched
  typedef flit<ag<STP("warp"), warp_t, 
               ag<STP("spawn"), node,
               ag<STP("spawn_pc"), bvec<N> > > > > splitter_sched_t;

  // Memory Request
  typedef flit<ag<STP("warp"), warp_t,
               ag<STP("wr"), node,
               ag<STP("mask"), bvec<L>,
               ag<STP("a"), vec<L, bvec<N> >,
               ag<STP("d"), vec<L, bvec<N> > > > > > > > h2_mem_req_t;

  // Cache Request
  typedef ag<STP("warp"), warp_t,
          ag<STP("lane"), bvec<LL>,
          ag<STP("wr"), node,
          ag<STP("mask"), bvec<LINE>,
          ag<STP("a"), bvec<N>,
          ag<STP("d"), vec<LINE, bvec<N> > > > > > > > cache_req_int_t;

  typedef flit<cache_req_int_t> cache_req_t;

  // Memory Response
  typedef flit<ag<STP("warp"), warp_t,
               ag<STP("q"), vec<L, bvec<N> > > > > h2_mem_resp_t;

  // Cache Response
  typedef flit<ag<STP("warp"), warp_t,
               ag<STP("lane"), bvec<LL>,
               ag<STP("q"), vec<LINE, bvec<N> > > > > > cache_resp_t;
}

#endif
