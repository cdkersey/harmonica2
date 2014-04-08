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
  node firstCyc(Reg(Lit(0), 1));

  _(out, "valid") = firstCyc;
  _(_(out, "contents"), "state") = Lit<SS>(TS_USER); // TODO: TS_KERNEL
  _(_(out, "contents"), "pc") = Lit<N>(0);
  _(_(out, "contents"), "active") = Lit<L>(1);
  _(_(out, "contents"), "id") = Lit<WW>(0);

  ASSERT(!(firstCyc && !_(out, "ready")));
  HIERARCHY_EXIT();
}

void Sched(sched_fetch_t &out, splitter_sched_t &in) {
  HIERARCHY_ENTER();
  splitter_sched_t starter_out, buf_in; Starter(starter_out);
  vec<2, splitter_sched_t> arb_in = {in, starter_out};
  Arbiter(buf_in, ArbUniq<2>, arb_in);
  TAP(starter_out);
  Buffer<WW>(out, buf_in);
  TAP(buf_in); TAP(in);
  HIERARCHY_EXIT();
}
