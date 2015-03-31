#include <fstream>
#include <string>

#include <chdl/chdl.h>
#include <chdl/cassign.h>
#include <chdl/loader.h>
#include <chdl/memreq.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"

using namespace std;
using namespace chdl;

void Fetch(fetch_pred_t &out, sched_fetch_t &in, string romFile);

void Fetch(fetch_pred_t &out, sched_fetch_t &in, string romFile) {
  HIERARCHY_ENTER();
  // We default to a simple instruction ROM for simulation, but we now also
  // allow the instruction cache to stand in as the fetch stage.
  if (EXT_IMEM) {
    if (SIMULATE) {
      cerr << "Can't simulate with external instrunction mem enabled." << endl;
      abort();
    }

    typedef mem_req<8, N/8, N - (NN - 3), WW> req_t;
    typedef mem_resp<8, N/8, WW> resp_t;

    req_t req;
    resp_t resp;

    _(in, "ready") = _(req, "ready");
    _(resp, "ready") = _(out, "ready");
    _(out, "valid") = _(resp, "valid");

    bvec<WW> in_warp_id(_(_(in, "contents"), "id")),
             resp_warp_id(_(_(resp, "contents"), "id"));
    node in_valid(_(in, "ready") && _(in, "valid"));

    _(req, "valid") = in_valid;

    _(_(req, "contents"), "addr") =
      _(_(in, "contents"), "pc")[range<NN - 3, N-1>()];
    _(_(req, "contents"), "id") = _(_(in, "contents"), "id");
    
    // Using LLRam; asynchronous access is more important than chip area
    Flatten(_(_(out, "contents"), "warp")) =
      LLRam(in_warp_id, Flatten(_(in, "contents")), resp_warp_id, in_valid);

    _(_(out, "contents"), "ir") = Flatten(_(_(resp, "contents"), "data"));

    out_mem_port<8, N/8, N - (NN-3), WW> imem;
    Connect(_(imem, "req"), req);
    Connect(_(imem, "resp"), resp);
    EXPOSE(imem);
  } else {
    node ready(_(out, "ready"));

    _(in, "ready") = ready;
    _(out, "valid") = Wreg(ready, _(in, "valid"));
    _(_(out, "contents"), "warp") = Wreg(ready, _(in, "contents"));

    bvec<CLOG2(ROMSZ)> a(
      _(_(in, "contents"), "pc")[range<NN-3, (NN-3)+CLOG2(ROMSZ)-1>()]
    );

    _(_(out, "contents"), "ir") = Wreg(ready,
      LLRom<CLOG2(ROMSZ), N>(a, romFile)
    );
    HIERARCHY_EXIT();
  }
}
