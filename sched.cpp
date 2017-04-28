#include <fstream>

#include <chdl/chdl.h>
#include <chdl/cassign.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"

using namespace std;
using namespace chdl;

// The full Processor
void Harmonica2();

// The pipeline stages
void Sched(sched_fetch_t &out, splitter_sched_t &in);

// Inject the initial warp
void Starter(splitter_sched_t &out) {
  HIERARCHY_ENTER();

  #ifdef START_PORT
  node start = Input("start");
  bvec<N> initial_pc = Input<N>("initial_pc");
  #else
  node start(Reg(Lit(0), 1));
  bvec<N> initial_pc = Lit<N>(0);
  #endif

  _(out, "valid") = start;
  // TODO: TS_KERNEL
  _(_(_(out, "contents"), "warp"), "state") = Lit<SS>(TS_USER);
  _(_(_(out, "contents"), "warp"), "pc") = initial_pc;
  _(_(_(out, "contents"), "warp"), "active") = Lit<L>(1);
  _(_(_(out, "contents"), "warp"), "id") = Lit<WW>(0);

  ASSERT(!(start && !_(out, "ready")));
  HIERARCHY_EXIT();
}

void Sched(sched_fetch_t &out, splitter_sched_t &in) {
  HIERARCHY_ENTER();
  splitter_sched_t starter_out, arb_out; Starter(starter_out);
  sched_fetch_t buf_in;
  vec<2, splitter_sched_t> arb_in = {in, starter_out};
  Arbiter(arb_out, ArbUniq<2>, arb_in);
  TAP(starter_out);

  node next_spawnState, spawnState(Reg(next_spawnState)),
       ldspawn(!spawnState && _(buf_in, "ready") && _(in, "valid")
               && _(_(in, "contents"), "spawn"));


  Cassign(next_spawnState).
    IF(spawnState && _(buf_in, "ready"), Lit(0)).
    IF(ldspawn, Lit(1)).
    ELSE(spawnState);

  
  warp_t spawnWarp;
  _(spawnWarp, "state") = Lit<SS>(TS_USER);
  _(spawnWarp, "pc") = Wreg(ldspawn, _(_(in, "contents"), "spawn_pc"));
  _(spawnWarp, "active") = Lit<L>(1);
  _(spawnWarp, "id") = Wreg(ldspawn, _(spawnWarp, "id") + Lit<WW>(1));

  _(arb_out, "ready") = _(buf_in, "ready") && !spawnState;
  _(buf_in, "valid") = _(arb_out, "valid") || spawnState;
  const unsigned WARPSZ(sz<warp_t>::value);
  _(buf_in, "contents") = Mux(spawnState,
                            bvec<WARPSZ>(_(_(arb_out, "contents"), "warp")),
                            bvec<WARPSZ>(spawnWarp));
  _(_(buf_in, "contents"), "next_id") = _(spawnWarp, "id") + Lit<WW>(1);

  RegBuffer<WW>(out, buf_in);
  TAP(buf_in); TAP(in);
  TAP(spawnState);
  TAP(next_spawnState);
  HIERARCHY_EXIT();
}
