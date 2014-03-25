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
void PredRegs(pred_reg_t &out, fetch_pred_t &in);
void GpRegs(reg_func_t &out, pred_reg_t &in);
void Execute(splitter_sched_t &out, reg_func_t &in);

// Scheduler stage auxiliary functions
void Starter(splitter_sched_t &out);

// Generic auxiliary functions
// //

void Harmonica2() {
  HIERARCHY_ENTER();

  // Assemble the pipeline
  sched_fetch_t sf;
  fetch_pred_t sp;
  pred_reg_t pr;
  reg_func_t rx;
  splitter_sched_t xs;

  Sched(sf, xs);
  Fetch(sp, sf);
  PredRegs(pr, sp);
  GpRegs(rx, pr);
  Execute(xs, rx);

  TAP(sf); TAP(sp); TAP(pr); TAP(rx); TAP(xs);
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

void Fetch(fetch_pred_t &out, sched_fetch_t &in) {
  
}

void PredRegs(pred_reg_t &out, fetch_pred_t &in) {
}

void GpRegs(reg_func_t &out, pred_reg_t &in) {
}

void Execute(splitter_sched_t &out, reg_func_t &in) {
}

int main(int argc, char **argv) {
  // Instantiate the processor
  Harmonica2();

  // Optimization/simulation
  optimize();

  ofstream vcd("h2.vcd");
  run(vcd, 1000);

  return 0;
}
