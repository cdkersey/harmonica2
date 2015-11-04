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

  out_mem_port<8, N/8, N - (NN - 3), WW> imem;
  out_mem_req<8, N/8, N - (NN - 3), WW> &req(_(imem, "req"));
  in_mem_resp<8, N/8, WW> &resp(_(imem, "resp"));

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
    LLRam(resp_warp_id, Flatten(_(in, "contents")), in_warp_id, in_valid);

  _(_(out, "contents"), "ir") = Flatten(_(_(resp, "contents"), "data"));

  EXPOSE(imem);

  HIERARCHY_EXIT();
}
