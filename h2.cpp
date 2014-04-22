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

  TAP(sf); TAP(fp); TAP(pr); TAP(rx); TAP(xs); TAP(xp); TAP(xr);

  HIERARCHY_EXIT();
}

int main(int argc, char **argv) {
  // Instantiate the processor
  Harmonica2();

  // Optimization/simulation
  if (cycdet()) return 1;
  optimize();

  // Do a critical path report
  ofstream cp_report("h2.crit");
  critpath_report(cp_report);

  ofstream vcd("h2.vcd");
  run(vcd, 10000);

  return 0;
}
