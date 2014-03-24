#ifndef HARMONICA_INTERFACES_H
#define HARMONICA_INTERFACES_H

#include <chdl/chdl.h>
#include <chdl/ag.h>
#include <chdl/net.h>

#include "config.h"

enum Tstate {
  TS_USER, TS_DIVERGENT_BR, TS_PENDING_INT, TS_KERNEL, N_TSTATES
};

const unsigned SS(CLOG2(N_TSTATES));

// Basic warp variables
typedef ag<STP("state"), bvec<SS>,
        ag<STP("pc"), bvec<N>,
        ag<STP("active"), bvec<W> > > > warp_t;

// Predicate read values
typedef ag<STP("pmask"), bvec<L>,
        ag<STP("pval0"), bvec<L>,
        ag<STP("pval1"), bvec<L> > > > pval_t;

// Writeback predicate values
typedef ag<STP("mask"), bvec<L>,
        ag<STP("val"), bvec<L>,
        ag<STP("dest"), bvec<RR> > > > pwb_t;

// Register read values
typedef ag<STP("val0"), vec<L, bvec<N> >,
        ag<STP("val1"), vec<L, bvec<N> >,
        ag<STP("val2"), vec<L, bvec<N> > > > > rval_t;

// Writeback register values
typedef ag<STP("mask"), bvec<L>,
        ag<STP("val"), vec<L, bvec<N> >,
        ag<STP("dest"), bvec<RR> > > > rwb_t;

// Sched->Fetch
typedef flit_t<warp_t> sched_fetch_t;

// Fetch->Predicate
typedef flit_t<ag<STP("warp"), warp_t,
               ag<STP("ir"), bvec<N> > > > fetch_pred_t;

// Predicate->Reg
typedef flit_t<ag<STP("warp"), warp_t,
               ag<STP("ir"),   bvec<N>,
               ag<STP("pval"), pval_t> > > > pred_reg_t;

// Reg->Dispatch, Dispatch->FU
typedef flit_t<ag<STP("warp"), warp_t,
               ag<STP("ir"), bvec<N>,
               ag<STP("pval"), pval_t,
               ag<STP("rval"), rval_t> > > > > reg_func_t;

// FU->Arbiter, Arbiter->Splitter
typedef flit_t<ag<STP("warp"), warp_t,
               ag<STP("rwb"), rwb_t,
               ag<STP("pwb"), pwb_t> > > > func_sched_t;

// Splitter->Register
typedef flit_t<rwb_t> splitter_reg_t;

// Splitter->Predicate
typedef flit_t<pwb_t> splitter_pred_t;

// Splitter->Sched
typedef flit_t<warp> splitter_sched_t;

#endif
