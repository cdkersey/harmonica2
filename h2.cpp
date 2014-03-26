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
void Fetch(fetch_pred_t &out, sched_fetch_t &in);
void PredRegs(pred_reg_t &out, fetch_pred_t &in, splitter_pred_t &wb);
void GpRegs(reg_func_t &out, pred_reg_t &in, splitter_reg_t &wb);
void Execute(splitter_sched_t&, splitter_pred_t&, splitter_reg_t&, reg_func_t&);

// Scheduler stage auxiliary functions
void Starter(splitter_sched_t &out);

// Generic auxiliary functions

// Implementations
void Harmonica2() {
  HIERARCHY_ENTER();

  // Assemble the pipeline
  sched_fetch_t sf;
  fetch_pred_t fp;
  pred_reg_t pr;
  reg_func_t rx;
  splitter_sched_t xs;
  splitter_pred_t xp;
  splitter_reg_t xr;

  Sched(sf, xs);
  Fetch(fp, sf);
  PredRegs(pr, fp, xp);
  GpRegs(rx, pr, xr);
  Execute(xs, xp, xr, rx);

  _(sf, "ready") = Lit(1); // TODO: remove this when we complete the cycle

  TAP(sf); TAP(fp); TAP(pr); TAP(rx); TAP(xs);
}

// Provide the initial warp
void Starter(splitter_sched_t &out) {
  node firstCyc(Reg(Lit(0), 1));

  _(out, "valid") = firstCyc;
  _(_(out, "contents"), "state") = Lit<SS>(TS_USER); // TODO: TS_KERNEL
  _(_(out, "contents"), "pc") = Lit<N>(0);
  _(_(out, "contents"), "active") = Lit<L>(1);
  _(_(out, "contents"), "id") = Lit<WW>(0);

  ASSERT(!(firstCyc && !_(out, "ready")));
}

void Sched(sched_fetch_t &out, splitter_sched_t &in) {
  splitter_sched_t starter_out, buf_in; Starter(starter_out);
  vec<2, splitter_sched_t> arb_in = {in, starter_out};
  Arbiter(buf_in, ArbUniq<2>, arb_in);
  TAP(starter_out);
  Buffer<WW>(out, buf_in);
}

// We're foregoing an icache for now and just using a simple instruction ROM
void Fetch(fetch_pred_t &out, sched_fetch_t &in) {
  node ready(_(out, "ready"));

  _(in, "ready") = ready;
  _(out, "valid") = Wreg(ready, _(in, "valid"));
  _(_(out, "contents"), "warp") = Wreg(ready, _(in, "contents"));

  bvec<CLOG2(ROMSZ)> a(
    _(_(in, "contents"), "pc")[range<NN-3, (NN-3)+CLOG2(ROMSZ)-1>()]
  );

  _(_(out, "contents"), "ir") = Wreg(ready,
    LLRom<CLOG2(ROMSZ), N>(a, "rom.hex")
  );
}

void PredRegs(pred_reg_t &out, fetch_pred_t &in, splitter_pred_t &wb) {
}

void GpRegs(reg_func_t &out, pred_reg_t &in, splitter_reg_t &wb) {
}

void Execute(splitter_sched_t &out, splitter_pred_t &pwb, splitter_reg_t &rwb,
             reg_func_t &in)
{
}

int main(int argc, char **argv) {
  // Instantiate the processor
  Harmonica2();

  // Optimization/simulation
  if (cycdet()) return 1;
  optimize();

  ofstream vcd("h2.vcd");
  run(vcd, 1000);

  return 0;
}
