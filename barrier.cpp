#include <fstream>

#include <chdl/chdl.h>
#include <chdl/cassign.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"

using namespace std;
using namespace chdl;

// Types and functions for barrier implementation
typedef ag<STP("valid"), node,
        ag<STP("pc"), bvec<N>,
        ag<STP("id"), bvec<WW>,
        ag<STP("active"), bvec<L> > > > > barrier_warp_t;

typedef ag<STP("valid"), node,
        ag<STP("arrived"), bvec<WW>,
        ag<STP("warps"), vec<W, barrier_warp_t> > > > barrier_t;

// Replace the idx-th entry of in with val. Optionally replace all entries.
template <unsigned N, typename T>
  void Replace(vec<N, T> out, vec<N, T> in,
               bvec<CLOG2(N)> idx, const T &val, node all=Lit(0));


void Funcunit_bar(func_splitter_t &out, reg_func_t &in);

void Funcunit_bar(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();

  // The state machine that controls this unit. We're OK with introducing some
  // 1-cycle delays, so there is no pipelining.
  enum { ST_IDLE, ST_WRITE, ST_RELEASE, ST_CLEAR, N_STATES } state_t;
  const unsigned SS(CLOG2(N_STATES));
  bvec<SS> next_state, state(Reg(next_state));
  bvec<N_STATES> state_1h(Zext<N_STATES>(Decoder(state)));

  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));
  bvec<L> active(_(_(_(in, "contents"), "warp"), "active")),
          pmask(_(_(_(in, "contents"), "pval"), "pmask"));

  vec<L, bvec<N> > n_v(_(_(_(in,"contents"),"rval"),"val1")),
                   id_v(_(_(_(in,"contents"),"rval"),"val0"));

  bvec<LL> lane_sel(Lsb(active & pmask));
  node arrive(_(in, "valid") && OrN(active & pmask));

  bvec<CLOG2(BARRIERS)> id(Latch(
    !arrive, Zext<CLOG2(BARRIERS)>(Mux(lane_sel, id_v))
  ));
  bvec<CLOG2(W + 1)> n(Latch(
    !arrive, Zext<CLOG2(W+1)>(Mux(lane_sel, n_v))
  ));

  TAP(arrive);
  TAP(lane_sel);
  TAP(id);
  TAP(n);
  TAP(state);

  node wr(state_1h[ST_WRITE] || state_1h[ST_CLEAR]), filled, all_released;
  barrier_t d, q(Syncmem(id, Flatten(d), wr));

  Cassign(next_state).
    IF(state_1h[ST_IDLE] && arrive, Lit<SS>(ST_WRITE)).
    IF(state_1h[ST_WRITE] && filled, Lit<SS>(ST_RELEASE)).
    IF(state_1h[ST_WRITE] && !filled, Lit<SS>(ST_IDLE)).
    IF(state_1h[ST_RELEASE] && all_released, Lit<SS>(ST_CLEAR)).
    IF(state_1h[ST_CLEAR], Lit<SS>(ST_IDLE)).
    ELSE(state);
  TAP(d); TAP(q);

  filled = (n == Lit<CLOG2(W + 1)>(1)) ||
           _(q, "arrived") == Zext<WW>(n - Lit<CLOG2(W + 1)>(1));

  TAP(filled);

  // Valid bit is cleared for clear, set for write.
  _(d, "valid") = state_1h[ST_WRITE];
  bvec<WW> idx;
  Cassign(idx).
    IF(_(q, "valid"), _(q, "arrived")).
    ELSE(Lit<WW>(0));
  _(d, "arrived") = Mux(state_1h[ST_CLEAR], idx + Lit<WW>(1), Lit<WW>(0));

  // In release mode, maintain a counter that increments every time we send
  // another warp back out into the pipeline to join its friends.
  node release(state_1h[ST_RELEASE] && _(out, "ready")),
       reset_release_ctr(!state_1h[ST_RELEASE]);
  bvec<CLOG2(W + 1)> next_release_ctr, release_ctr(Reg(next_release_ctr));
  Cassign(next_release_ctr).
    IF(reset_release_ctr, Lit<CLOG2(W + 1)>(0)).
    IF(release, release_ctr + Lit<CLOG2(W + 1)>(1)).
    ELSE(release_ctr);
  all_released = (next_release_ctr == n);

  TAP(release);
  TAP(release_ctr);
  TAP(all_released);

  barrier_warp_t w;
  Replace(_(d, "warps"), _(q, "warps"), idx, w, state_1h[ST_CLEAR]);
  _(w, "pc") = Latch(!arrive, _(_(_(in, "contents"), "warp"), "pc"));
  _(w, "id") = Latch(!arrive, _(_(_(in, "contents"), "warp"), "id"));
  _(w, "active") = Latch(!arrive, _(_(_(in, "contents"), "warp"), "active"));
  _(w, "valid") = state_1h[ST_WRITE];

  // Never writes anything back.
  _(_(_(out, "contents"), "rwb"), "mask") = Lit<L>(0);

  _(in, "ready") = state_1h[ST_IDLE];
  _(out, "valid") = state_1h[ST_RELEASE];

  barrier_warp_t rw(Mux(Zext<WW>(release_ctr), _(q, "warps")));  
  _(_(_(out, "contents"), "warp"), "id") = _(rw, "id");
  _(_(_(out, "contents"), "warp"), "active") = _(rw, "active");
  _(_(_(out, "contents"), "warp"), "pc") = _(rw, "pc");

  HIERARCHY_EXIT();
}

template <unsigned N, typename T>
  void Replace(vec<N, T> out, vec<N, T> in,
               bvec<CLOG2(N)> idx, const T &val, node all)
{
  for (unsigned i = 0; i < N; ++i)
    Cassign(out[i]).
      IF(all || idx == Lit<CLOG2(N)>(i), val).
      ELSE(in[i]);
}
