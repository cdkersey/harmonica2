#include <fstream>

#include <chdl/chdl.h>
#include <chdl/cassign.h>
#include <chdl/egress.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"

using namespace std;
using namespace chdl;

void Funcunit_lsu(func_splitter_t &out, reg_func_t &in);

void MemSystem(mem_resp_t &out, mem_req_t &in);

// Load/store unit
void Funcunit_lsu(func_splitter_t &out, reg_func_t &in)
{
  HIERARCHY_ENTER();

  mem_req_t req;
  mem_resp_t resp;

  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  bvec<L> active(_(_(_(in, "contents"), "warp"), "active"));

  // Connect "in" to "req"
  _(in, "ready") = _(req, "ready");
  _(req, "valid") = _(in, "valid");
  _(_(req, "contents"), "warp") = _(_(in, "contents"), "warp");
  _(_(req, "contents"), "wr") = inst.is_store();
  _(_(req, "contents"), "mask") =
    _(_(_(in, "contents"), "pval"), "pmask") & active;
  for (unsigned l = 0; l < L; ++l) {
    _(_(req, "contents"), "a")[l] =
      Mux(inst.is_store(), 
        _(_(_(in, "contents"), "rval"), "val0")[l] + inst.get_imm(),
        _(_(_(in, "contents"), "rval"), "val1")[l] + inst.get_imm()
      );
    _(_(req, "contents"), "d")[l] =
      _(_(_(in, "contents"), "rval"), "val0")[l];
  }

  // Keep track of whether operations issued to memory system were stores
  node issue(_(in, "ready") && _(in, "valid"));
  bvec<L> ldMask(Wreg(issue,
    bvec<L>(inst.has_rdst()) & _(_(_(in, "contents"), "pval"), "pmask")
  ));
  bvec<RR> ldDest(Wreg(issue, inst.get_rdst()));

  // Connect "out" to resp
  _(resp, "ready") = _(out, "ready");
  _(out, "valid") = _(resp, "valid");
  _(_(out, "contents"), "warp") = _(_(resp, "contents"), "warp");
  _(_(_(out,"contents"),"rwb"),"wid") = _(_(_(resp,"contents"),"warp"),"id");
  _(_(_(out,"contents"),"rwb"),"mask") = ldMask;
  _(_(_(out,"contents"),"rwb"),"dest") = ldDest;
  _(_(_(out,"contents"),"rwb"),"val") = _(_(resp,"contents"),"q");

  MemSystem(resp, req);

  tap("lsu_out", out);
  tap("lsu_in", in);

  HIERARCHY_EXIT();
}

void DummyCache(cache_resp_t &out, cache_req_t &in) {
  node ready = _(out, "ready");
  _(in, "ready") = ready;

  node ldregs(ready && _(in, "valid")), next_full, full(Reg(next_full));

  Cassign(next_full).
    IF(!full && _(in, "valid") && ready, Lit(1)).
    IF(full && (!_(in, "valid") || !ready), Lit(0)).
    ELSE(full);

  _(out, "valid") = full;
  _(_(out, "contents"), "warp") = Wreg(ldregs, _(_(in, "contents"), "warp"));

  bvec<N> a(_(_(in, "contents"), "a"));
  bvec<10> devAddr(a[range<CLOG2(LINE*(N/8)), CLOG2(LINE*(N/8))+9>()]);

  vec<LINE, bvec<N> > memd(_(_(in, "contents"), "d"));
  vec<LINE, bvec<N> > memq;
  for (unsigned i = 0; i < LINE; ++i) {
    node wr(ready && _(in, "valid") && _(_(in, "contents"), "mask")[i] &&
              _(_(in, "contents"), "wr"));

    memq[i] = Syncmem(devAddr, memd[i], wr);

    if (i == 0 && SOFT_IO) {
      static unsigned consoleOutVal;
      node wrConsole(wr && a[N-1]);
      EgressInt(consoleOutVal, memd[i]);
      EgressFunc(
        [](bool x){if (x) cout << char(consoleOutVal);},
        wrConsole
      );
    }
  }

  if (DEBUG_MEM) {
    static unsigned long addrVal, dVal[LINE], qVal[LINE];
    static bool wrVal, maskVal[LINE];
    Egress(wrVal, Reg(ready && _(in, "valid") && _(_(in, "contents"), "wr")));
    for (unsigned l = 0; l < LINE; ++l) {
      EgressInt(dVal[l], Reg(memd[l]));
      Egress(maskVal[l], Reg(_(_(in, "contents"), "mask")[l]));
      EgressInt(qVal[l], memq[l]);
    }

    EgressInt(addrVal, Reg(a));
    EgressFunc([](bool x) {
      if (x) {
        cout << "Mem " << (wrVal?"store":"load") << ':' << endl;
        for (unsigned l = 0; l < LINE; ++l) {
          if (maskVal[l]) {
            cout << "  0x" << hex << addrVal + l*(N/8);
            if (wrVal) cout << hex << ": 0x" << dVal[l];
            else cout << hex << ": 0x" << qVal[l];
            cout << endl;
          }
        }
      }
    }, Reg(ready && _(in, "valid")));
  }

  vec<LINE, bvec<N> > held_memq;
  Flatten(held_memq) = Wreg(ldregs, Flatten(memq));

  tap("dummy_cache_memq", memq);
  TAP(devAddr);

  Flatten(_(_(out, "contents"), "q")) =
    Mux(ready && Reg(ready), Flatten(held_memq), Flatten(memq));

  _(_(out, "contents"), "lane") = Wreg(ldregs, _(_(in, "contents"), "lane"));  
}

void MemSystem(mem_resp_t &out, mem_req_t &in) {
  HIERARCHY_ENTER();
  // TODO: Add an associative table to enable cache_resps to arrive out-of-order
  // e.g. from a non-blocking cache.

  node next_full, full(Reg(next_full)), fill, empty;
  _(in, "ready") = !full;
  next_full = (full && !empty) || (!full && fill && !empty);
  fill = _(in, "valid") && !full;

  vec<L, bvec<L> > eqmat; // Coalesce matrix: which addresses are equal?
  bvec<L> covered, // Is my request covered by that of a prior lane?
          mask(_(_(in, "contents"), "mask"));
  cache_req_t cache_req;
  cache_resp_t cache_resp;

  vec<L, bvec<N> > a(_(_(in, "contents"), "a"));

  for (unsigned i = 0; i < L; ++i) {
    for (unsigned j = i; j < L; ++j)
      eqmat[i][j] = Lit(0);
    for (unsigned j = 0; j < i; ++j) {
      bvec<N-CLOG2(LINE*(N/8))> ai(a[i][range<CLOG2(LINE*(N/8)), N-1>()]),
                                aj(a[j][range<CLOG2(LINE*(N/8)), N-1>()]);
      eqmat[i][j] = ai == aj && mask[i] && mask[j];
    }
    covered[i] = OrN(eqmat[i]);
  }

  bvec<L> allReqMask(Wreg(fill, ~covered & mask)),
          next_sentReqMask, sentReqMask(Reg(next_sentReqMask)),
          next_returnedReqMask, returnedReqMask(Reg(next_returnedReqMask));

  bvec<L> ldqReg;
  vec<L, bvec<L> > eqmatReg;
  vec<L, bvec<N> > aReg, dReg, qReg;
  for (unsigned l = 0; l < L; ++l) {
    for (unsigned i = 0; i < L; ++i) {
      if (i == l) eqmatReg[l][i] = Lit(1);
      else        eqmatReg[l][i] = Wreg(fill, eqmat[i][l]);
    }
    aReg[l] = Wreg(fill, a[l]);
    dReg[l] = Wreg(fill, _(_(in, "contents"), "d")[l]);
    qReg[l] = Wreg(ldqReg[l] && _(cache_resp,"valid") && _(cache_resp,"ready"),
                   Mux(aReg[l][range<CLOG2(N/8), CLOG2(N/8*LINE)-1>()],
                       _(_(cache_resp,"contents"),"q")) >>
                   Cat(aReg[l][range<0, 1>()], Lit<3>(0)));
  }

  bvec<LL> sel(Lsb(allReqMask & ~sentReqMask));

  TAP(eqmat); TAP(covered); TAP(allReqMask); TAP(sentReqMask);
  TAP(returnedReqMask); TAP(sel);

  Cassign(next_sentReqMask).
    IF(fill, Lit<L>(0)).
    IF(_(cache_req, "ready") && (sentReqMask != allReqMask),
       sentReqMask | Lit<L>(1)<<sel).
    ELSE(sentReqMask);

  bvec<N> reqAddr(Mux(sel, aReg) & ~Lit<N>(LINE*(N/8)-1));

  _(_(cache_req, "contents"), "a") = reqAddr;
  _(_(cache_req, "contents"), "lane") = sel;
  _(_(cache_req, "contents"), "warp") = Wreg(fill, _(_(in, "contents"),"warp"));
  _(_(cache_req, "contents"), "wr") = Wreg(fill, _(_(in, "contents"), "wr"));

  tap("mem_aReg", aReg);  tap("mem_dReg", dReg); tap("mem_qReg", qReg);

  for (unsigned i = 0; i < LINE; ++i) {
    const unsigned NB(CLOG2(N/8)), // log2(N), expressed in (8-bit) bytes
                   LB(NB + CLOG2(LINE)); // log2(bytes in a cache line)

    bvec<L> maskBits;
    for (unsigned l = 0; l < L; ++l)
      maskBits[l] =
        (aReg[l][range<NB, LB-1>()]==Lit<CLOG2(LINE)>(i)) &&
        (aReg[l][range<LB, N-1>()] == reqAddr[range<LB, N-1>()]) &&
        allReqMask[l];
    
    _(_(cache_req, "contents"), "mask")[i] = OrN(maskBits);
    _(_(cache_req, "contents"), "d")[i] = Mux(Log2(maskBits), dReg);
  }

  TAP(cache_req);
  TAP(cache_resp);

  _(cache_req, "valid") = full && sentReqMask != allReqMask;

  DummyCache(cache_resp, cache_req);

  _(cache_resp, "ready") = full && returnedReqMask != allReqMask;

  Cassign(next_returnedReqMask).
    IF(fill, Lit<L>(0)).
    IF(_(cache_resp, "ready") && _(cache_resp, "valid"),
      returnedReqMask | Lit<L>(1)<<_(_(cache_resp, "contents"),"lane")).
    ELSE(returnedReqMask);

  // Load these lanes' q registers for this response.
  ldqReg = Mux(_(_(cache_resp,"contents"),"lane"), eqmatReg);

  ASSERT(!OrN(returnedReqMask & ~sentReqMask));

  _(out, "valid") = full && (returnedReqMask == allReqMask);

  _(_(out, "contents"), "warp") = Wreg(fill, _(_(in, "contents"), "warp"));
  _(_(out, "contents"), "q") = qReg;

  empty = _(out, "valid") && _(out, "ready");

  tap("mem_fill", fill);
  tap("mem_empty", empty);
  tap("mem_full", full);
  tap("mem_in", in);
  tap("mem_out", out);
  tap("mem_reqAddr", reqAddr);
  tap("mem_ldqReg", ldqReg);
  tap("mem_eqmatReg", eqmatReg);

  HIERARCHY_EXIT();
}
