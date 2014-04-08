#include <fstream>

#include <chdl/chdl.h>
#include <chdl/cassign.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"

using namespace std;
using namespace chdl;

void Fetch(fetch_pred_t &out, sched_fetch_t &in);

// We're foregoing an icache for now and just using a simple instruction ROM
void Fetch(fetch_pred_t &out, sched_fetch_t &in) {
  HIERARCHY_ENTER();
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
  HIERARCHY_EXIT();
}
