#include <fstream>
#include <string>

#include <chdl/chdl.h>
#include <chdl/cassign.h>
#include <chdl/counter.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"

using namespace std;
using namespace chdl;

// The full Processor
void Harmonica2();

bvec<1> PopCount(const bvec<1> &x) { return bvec<1>(x[0]); }

template <unsigned N> bvec<LOG2(N) + 1> PopCount(const bvec<N> &x) {
  bvec<N/2> a(x[range<0,N/2-1>()]);
  bvec<N - N/2> b(x[range<N/2,N-1>()]);
  return Zext<LOG2(N) + 1>(PopCount(a)) + Zext<LOG2(N) + 1>(PopCount(b));
} 

// The pipeline stages
void Sched(sched_fetch_t &out, splitter_sched_t &in);
void Fetch(fetch_pred_t &out, sched_fetch_t &in, string romFile);
void PredRegs(pred_reg_t &out, fetch_pred_t &in, splitter_pred_t &wb);
void GpRegs(reg_func_t &out, pred_reg_t &in, splitter_reg_t &wb);
void Execute(splitter_sched_t&, splitter_pred_t&, splitter_reg_t&, reg_func_t&);

// Implementations
void Harmonica2(string romFile) {
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
  Fetch(fp, sf, romFile);
  PredRegs(pr, fp, xp);
  GpRegs(rx, pr, xr);
  Execute(xs, xp, xr, rx);

  TAP(sf); TAP(fp); TAP(pr); TAP(rx); TAP(xs); TAP(xp); TAP(xr);

  Counter("cycles", Lit(1));
  Counter("insts",
          PopCount(_(_(_(xs, "contents"), "warp"), "active")),
          _(xs, "ready") && _(xs, "valid"));

  HIERARCHY_EXIT();
}

int main(int argc, char **argv) {
  // Instantiate the processor
  string romFile(argc == 1 ? "rom.hex" : argv[1]);
  Harmonica2(romFile);

  // Optimize and simulate/dump netlist
  if (cycdet()) return 1;
  optimize();

  // Do a critical path report
  ofstream cp_report("h2.crit");
  critpath_report(cp_report);

  if (FPGA) {
    // Emit verilog
    ofstream vl("h2.v");
    print_verilog("h2", vl);
  }

  if (NETLIST) {
    // Emit netlist
    ofstream nl("h2.nand");
    print_netlist(nl);
  }
 
  if (SIMULATE) {
    // Run a simulation
    string vcdFile(argc < 2 ? "h2.vcd" : argv[2]);
    ofstream vcd(vcdFile);
    run(vcd, 10000);
  }

  return 0;
}
