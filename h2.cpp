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

// Types of functional unit, padded to nearest power of two
enum funcunit_type_t {
  FU_ALU, FU_PLU, FU_MULT, FU_DIV, FU_LSU, FU_BRANCH, FU_PAD0, FU_PAD1, N_FU
};

// Functional Unit Routing Function
void RouteFunc(bvec<N_FU> &valid, const reg_func_int_t &in, node in_valid);

// Functional Units
void Funcunit_alu(func_splitter_t &out, reg_func_t &in);
void Funcunit_plu(func_splitter_t &out, reg_func_t &in);
void Funcunit_mult(func_splitter_t &out, reg_func_t &in);
void Funcuint_div(func_splitter_t &out, reg_func_t &in);
void Funcunit_lsu(func_splitter_t &out, reg_func_t &in);
void Funcunit_branch(func_splitter_t &out, reg_func_t &in);

// The Memory System
void MemSystem(mem_resp_t &out, mem_req_t &in);
template <size_t A, size_t B>
  void AddrRouteFunc(bvec<1u<<(B-A+1)> &valid,
                     const cache_req_int_t &in,
                     node in_valid);

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

// Provide the initial warp
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

void PredRegs(pred_reg_t &out, fetch_pred_t &in, splitter_pred_t &wb) {
  HIERARCHY_ENTER();
  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  vec<W, vec<R, bvec<L> > > pregs;

  _(wb, "ready") = Lit(1);  // Always ready to accept writebacks.

  // Instantiate the registers
  for (unsigned w = 0; w < W; ++w) {
    for (unsigned r = 0; r < R; ++r) {
      for (unsigned l = 0; l < L; ++l) {
        node inMask(_(_(wb, "contents"), "mask")[l]),
             wSel(_(_(wb, "contents"), "wid") == Lit<WW>(w)),
    	     rSel(_(_(wb, "contents"), "dest") == Lit<RR>(r)),
  	     wr(inMask && rSel && wSel && _(wb, "valid"));
        pregs[w][r][l] = Wreg(wr, _(_(wb, "contents"), "val")[l]);
      }
    }
  }

  // Handle ready and valid signals
  node ready(_(out, "ready"));
  _(in, "ready") = ready;
  _(out, "valid") = Wreg(ready, _(in, "valid"));
  _(_(out, "contents"), "warp") = Wreg(ready, _(_(in, "contents"), "warp"));
  _(_(out, "contents"), "ir") = Wreg(ready, _(_(in, "contents"), "ir"));

  bvec<WW> wid(_(_(_(in, "contents"), "warp"), "id"));

  bvec<L> pmask, pval0, pval1;

  // The mask should be all 1s if the instruction is not predicated.
  _(_(_(out, "contents"), "pval"), "pmask") 
    = Wreg(ready, pmask | bvec<L>(!inst.has_pred()));

  _(_(_(out, "contents"), "pval"), "val0") = Wreg(ready, pval0);
  _(_(_(out, "contents"), "pval"), "val1") = Wreg(ready, pval1);

  pmask = Mux(inst.get_pred(), Mux(wid, pregs));
  pval0 = Mux(inst.get_psrc0(), Mux(wid, pregs));
  pval1 = Mux(inst.get_psrc1(), Mux(wid, pregs));

  TAP(pregs);

  HIERARCHY_EXIT();
}

void GpRegs(reg_func_t &out, pred_reg_t &in, splitter_reg_t &wb) {
  HIERARCHY_ENTER();
  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  vec<W, vec<L, vec<R, bvec<N> > > > regs;

  bvec<WW> wb_wid(_(_(wb, "contents"), "wid"));
  bvec<LL> wb_clonesrc(_(_(wb, "contents"), "clonesrc")),
           wb_clonedest(_(_(wb, "contents"), "clonedest"));
  vec<L, bvec<N> > wb_val(_(_(wb, "contents"), "val"));
  bvec<L> wbMask(_(_(wb, "contents"), "mask"));

  vec<R, bvec<N> > clonebus(Mux(wb_clonesrc, Mux(wb_wid, regs)));

  _(wb, "ready") = Lit(1); // Always ready to accept writebacks.

  // Instantiate the registers
  for (unsigned w = 0; w < W; ++w) {
    node destWarp(wb_wid == Lit<WW>(w));
    for (unsigned r = 0; r < R; ++r) {
      node destReg(_(_(wb, "contents"), "dest") == Lit<RR>(r));
      for (unsigned l = 0; l < L; ++l) {
        node clone(_(_(wb, "contents"), "clone") && Lit<LL>(l) == wb_clonedest),
             wr(destWarp && _(wb, "valid") && (clone || destReg && wbMask[l]));
        unsigned initVal(0);
        if (r == 0) initVal = l;
        else if (r == 1) initVal = w;
        regs[w][l][r] = Wreg(wr, Mux(clone, wb_val[l], clonebus[r]), initVal);
      }
    }
  }

  node ready(_(out, "ready"));
  bvec<WW> wid(_(_(_(in, "contents"), "warp"), "id"));

  vec<L, bvec<N> > rval0, rval1, rval2;

  // Get the register values
  for (unsigned l = 0; l < L; ++l) {
    rval0[l] = Mux(inst.get_rsrc0(), Mux(wid, regs)[l]);
    rval1[l] = Mux(inst.get_rsrc1(), Mux(wid, regs)[l]);
    rval2[l] = Mux(inst.get_rsrc2(), Mux(wid, regs)[l]);
  }

  _(in, "ready") = ready;
  _(out, "valid") = Wreg(ready, _(in, "valid"));
  _(_(out, "contents"), "warp") = Wreg(ready, _(_(in, "contents"), "warp"));
  _(_(out, "contents"), "ir") = Wreg(ready, _(_(in, "contents"), "ir"));
  _(_(out, "contents"), "pval") = Wreg(ready, _(_(in, "contents"), "pval"));
  Flatten(_(_(_(out,"contents"),"rval"),"val0")) = Wreg(ready, Flatten(rval0));
  Flatten(_(_(_(out,"contents"),"rval"),"val1")) = Wreg(ready, Flatten(rval1));
  Flatten(_(_(_(out,"contents"),"rval"),"val2")) = Wreg(ready, Flatten(rval2));

  TAP(regs);

  HIERARCHY_EXIT();
}

void Execute(splitter_sched_t &out, splitter_pred_t &pwb, splitter_reg_t &rwb,
             reg_func_t &in)
{
  HIERARCHY_ENTER();

  // The input to the functional units includes pre-incremented program counter.
  reg_func_t fu_router_in, fu_router_in_postbuf;
  _(in, "ready") = _(fu_router_in, "ready");
  _(fu_router_in, "valid") = _(in, "valid");
  _(_(_(fu_router_in, "contents"), "warp"), "state")
    = _(_(_(in, "contents"), "warp"), "state");
  _(_(_(fu_router_in, "contents"), "warp"), "active")
    = _(_(_(in, "contents"), "warp"), "active");
  _(_(_(in, "contents"), "warp"), "id")
    = _(_(_(fu_router_in, "contents"), "warp"), "id");
  _(_(_(fu_router_in, "contents"), "warp"), "pc")
    = _(_(_(in, "contents"), "warp"), "pc") + Lit<N>(N/8);
  _(_(fu_router_in, "contents"), "ir") = _(_(in, "contents"), "ir");
  _(_(fu_router_in, "contents"), "pval") = _(_(in, "contents"), "pval");
  _(_(fu_router_in, "contents"), "rval") = _(_(in, "contents"), "rval");

  vec<N_FU, reg_func_t> fu_inputs;
  vec<N_FU, func_splitter_t> fu_outputs;

  Buffer<1>(fu_router_in_postbuf, fu_router_in);

  // TODO: ???
  Router(fu_inputs, RouteFunc, fu_router_in_postbuf);

  Funcunit_alu(fu_outputs[FU_ALU], fu_inputs[FU_ALU]);
  Funcunit_plu(fu_outputs[FU_PLU], fu_inputs[FU_PLU]);
  Funcunit_mult(fu_outputs[FU_MULT], fu_inputs[FU_MULT]);
  Funcuint_div(fu_outputs[FU_DIV], fu_inputs[FU_DIV]);
  Funcunit_lsu(fu_outputs[FU_LSU], fu_inputs[FU_LSU]);
  Funcunit_branch(fu_outputs[FU_BRANCH], fu_inputs[FU_BRANCH]);

  func_splitter_t fu_arbiter_out;
  Arbiter(fu_arbiter_out, ArbRR<N_FU>, fu_outputs);

  TAP(fu_inputs);
  TAP(fu_outputs);

  TAP(fu_router_in);
  TAP(fu_router_in_postbuf);
  TAP(fu_arbiter_out);

  // Now we assume the register/predicate writebacks are always ready
  // TODO: Handle ready signal on register/predicate writeback signals
  _(fu_arbiter_out, "ready") = _(out, "ready");

  _(out, "valid") = _(fu_arbiter_out, "valid");
  _(pwb, "valid") = _(fu_arbiter_out, "valid");
  _(rwb, "valid") = _(fu_arbiter_out, "valid");
  _(pwb, "contents") = _(_(fu_arbiter_out, "contents"), "pwb");
  _(rwb, "contents") = _(_(fu_arbiter_out, "contents"), "rwb");
  _(out, "contents") = _(_(fu_arbiter_out, "contents"), "warp"); 

  HIERARCHY_EXIT();
}

void RouteFunc(bvec<N_FU> &valid, const reg_func_int_t &in, node in_valid) {
  harpinst<N, RR, RR> inst(_(in, "ir"));

  bvec<N_FU> v;

  v[FU_ALU] =
    inst.get_opcode() == Lit<6>(0x00) || // nop
    inst.get_opcode() == Lit<6>(0x05) || // neg
    inst.get_opcode() == Lit<6>(0x06) || // not
    inst.get_opcode() == Lit<6>(0x07) || // and
    inst.get_opcode() == Lit<6>(0x08) || // or
    inst.get_opcode() == Lit<6>(0x09) || // xor
    inst.get_opcode() == Lit<6>(0x0a) || // add
    inst.get_opcode() == Lit<6>(0x0b) || // sub
    inst.get_opcode() == Lit<6>(0x0f) || // shl
    inst.get_opcode() == Lit<6>(0x10) || // shr
    inst.get_opcode() == Lit<6>(0x11) || // andi
    inst.get_opcode() == Lit<6>(0x12) || // ori
    inst.get_opcode() == Lit<6>(0x13) || // xori
    inst.get_opcode() == Lit<6>(0x14) || // addi
    inst.get_opcode() == Lit<6>(0x15) || // subi
    inst.get_opcode() == Lit<6>(0x19) || // shli
    inst.get_opcode() == Lit<6>(0x1a) || // shri
    inst.get_opcode() == Lit<6>(0x25);   // ldi

  v[FU_PLU] =
    inst.get_opcode() == Lit<6>(0x26) || // rtop
    inst.get_opcode() == Lit<6>(0x2b) || // isneg
    inst.get_opcode() == Lit<6>(0x2c) || // iszero
    inst.get_opcode() == Lit<6>(0x27) || // andp
    inst.get_opcode() == Lit<6>(0x28) || // orp
    inst.get_opcode() == Lit<6>(0x29) || // xorp
    inst.get_opcode() == Lit<6>(0x2a);   // notp

  v[FU_MULT] =
    inst.get_opcode() == Lit<6>(0x0c) || // mul
    inst.get_opcode() == Lit<6>(0x16);   // muli

  v[FU_DIV] =
    inst.get_opcode() == Lit<6>(0x0d) || // div
    inst.get_opcode() == Lit<6>(0x0e) || // mod
    inst.get_opcode() == Lit<6>(0x17) || // divi
    inst.get_opcode() == Lit<6>(0x18);   // modi

  v[FU_LSU] =
    inst.get_opcode() == Lit<6>(0x23) || // ld
    inst.get_opcode() == Lit<6>(0x24);   // st

  v[FU_BRANCH] =
    inst.get_opcode() == Lit<6>(0x1b) || // jali
    inst.get_opcode() == Lit<6>(0x1c) || // jalr
    inst.get_opcode() == Lit<6>(0x1e) || // jmpr
    inst.get_opcode() == Lit<6>(0x1d); // jmpi

  valid = v & bvec<N_FU>(in_valid);
}

void Funcunit_alu(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();
  node ready(_(out, "ready")), next_full, full(Reg(next_full));
  _(in, "ready") = ready || !full;

  // TODO: is "full" defined correctly?
  Cassign(next_full).
    IF(!full).
      IF(_(in, "valid") && !_(out, "ready"), Lit(1)).
      ELSE(Lit(0)).
    END().ELSE().
      IF(_(out, "ready"), Lit(0)).
      ELSE(Lit(1));

  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  bvec<L> pmask(_(_(_(in, "contents"), "pval"), "pmask"));

  node ldregs(ready || !full);

  _(out, "valid") = Reg(_(in, "valid")) || full;
  _(_(out, "contents"), "warp") = Wreg(ldregs, _(_(in, "contents"), "warp"));

  _(_(_(out, "contents"), "rwb"), "mask") =
    Wreg(ldregs, bvec<L>(inst.has_rdst()) & pmask); // TODO: and w/ active lanes
  _(_(_(out, "contents"), "rwb"), "dest") = Wreg(ldregs, inst.get_rdst());

  vec<L, bvec<N> > out_val;

  for (unsigned l = 0; l < L; ++l) {
    bvec<N> rval0(_(_(_(in, "contents"), "rval"), "val0")[l]), 
            rval1(_(_(_(in, "contents"), "rval"), "val1")[l]);

    Cassign(out_val[l]).
      IF(inst.get_opcode() == Lit<6>(0x05), -rval0). // neg
      IF(inst.get_opcode() == Lit<6>(0x06), ~rval0). // not
      IF(inst.get_opcode() == Lit<6>(0x07), rval0 & rval1). // and
      IF(inst.get_opcode() == Lit<6>(0x08), rval0 | rval1). // or
      IF(inst.get_opcode() == Lit<6>(0x09), rval0 ^ rval1). // xor
      IF(inst.get_opcode() == Lit<6>(0x0a), rval0 + rval1).  // add
      IF(inst.get_opcode() == Lit<6>(0x0b), rval0 - rval1).  // sub
      IF(inst.get_opcode() == Lit<6>(0x0f), rval0<<Zext<CLOG2(N)>(rval1)).// shl
      IF(inst.get_opcode() == Lit<6>(0x10), rval0>>Zext<CLOG2(N)>(rval1)).// shr
      IF(inst.get_opcode() == Lit<6>(0x11), rval0 & inst.get_imm()). // andi
      IF(inst.get_opcode() == Lit<6>(0x12), rval0 | inst.get_imm()). // ori
      IF(inst.get_opcode() == Lit<6>(0x13), rval0 ^ inst.get_imm()). // xori
      IF(inst.get_opcode() == Lit<6>(0x14), rval0 + inst.get_imm()).  // addi
      IF(inst.get_opcode() == Lit<6>(0x15), rval0 - inst.get_imm()).  // subi
      IF(inst.get_opcode() == Lit<6>(0x19),
         rval0 << Zext<CLOG2(N)>(inst.get_imm())). // shli
      IF(inst.get_opcode() == Lit<6>(0x1a),
         rval0 >> Zext<CLOG2(N)>(inst.get_imm())). // shri
      IF(inst.get_opcode() == Lit<6>(0x25), inst.get_imm()). // ldi
      ELSE(Lit<N>(0));

    _(_(_(out, "contents"), "rwb"), "val")[l] = Wreg(ldregs, out_val[l]);
  }

  tap("alu_full", full);
  tap("alu_out", out);
  tap("alu_in", in);

  HIERARCHY_EXIT();
}

void Funcunit_plu(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();

  node ready(_(out, "ready")), next_full, full(Reg(next_full));
  _(in, "ready") = ready || !full;

  Cassign(next_full).
    IF(!full).
      IF(_(in, "valid") && !_(out, "ready"), Lit(1)).
      ELSE(Lit(0)).
    END().ELSE().
      IF(_(out, "ready"), Lit(0)).
      ELSE(Lit(1));

  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  node ldregs(ready || !full);

  _(out, "valid") = Reg(_(in, "valid")) || full;
  _(_(out, "contents"), "warp") = Wreg(ldregs, _(_(in, "contents"), "warp"));

  bvec<L> pmask(_(_(_(in, "contents"), "pval"), "pmask"));

  _(_(_(out, "contents"), "pwb"), "mask") =
    Wreg(ldregs, bvec<L>(inst.has_pdst()) & pmask); // TODO: and w/ active lanes
  _(_(_(out, "contents"), "pwb"), "dest") = Wreg(ldregs, inst.get_pdst());

  bvec<L> out_val;

  for (unsigned l = 0; l < L; ++l) {
    bvec<N> rval0(_(_(_(in, "contents"), "rval"), "val0")[l]);
    node pval0(_(_(_(in, "contents"), "pval"), "val0")[l]),
         pval1(_(_(_(in, "contents"), "pval"), "val1")[l]);

    Cassign(out_val[l]).
      IF(inst.get_opcode() == Lit<6>(0x26), OrN(rval0)).     // rtop
      IF(inst.get_opcode() == Lit<6>(0x2c), !OrN(rval0)).    // iszero
      IF(inst.get_opcode() == Lit<6>(0x2b), rval0[N-1]).     // isneg
      IF(inst.get_opcode() == Lit<6>(0x27), pval0 && pval1). // andp
      IF(inst.get_opcode() == Lit<6>(0x28), pval0 || pval1). // orp
      IF(inst.get_opcode() == Lit<6>(0x29), pval0 != pval1). // xorp
      IF(inst.get_opcode() == Lit<6>(0x2a), !pval0). // notp
      ELSE(Lit('0'));
  }

  _(_(_(out, "contents"), "pwb"), "val") = Wreg(ldregs, out_val);

  tap("plu_full", full);
  tap("plu_out", out);
  tap("plu_in", in);

  HIERARCHY_EXIT();
}

void Funcunit_mult(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();
  HIERARCHY_EXIT();
}

void Funcuint_div(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();
  HIERARCHY_EXIT();
}

void Funcunit_lsu(func_splitter_t &out, reg_func_t &in)
{
  HIERARCHY_ENTER();

  mem_req_t req;
  mem_resp_t resp;

  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  // Connect "in" to "req"
  _(in, "ready") = _(req, "ready");
  _(req, "valid") = _(in, "valid");
  _(_(req, "contents"), "warp") = _(_(in, "contents"), "warp");
  _(_(req, "contents"), "wr") = inst.is_store();
  _(_(req, "contents"), "mask") = _(_(_(in, "contents"), "pval"), "pmask");
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

void Funcunit_branch(func_splitter_t &out, reg_func_t &in) {
  HIERARCHY_ENTER();
  bvec<N> pc(_(_(_(in, "contents"), "warp"), "pc"));
  node ready(_(out, "ready")), next_full, full(Reg(next_full));
  _(in, "ready") = ready || !full;

  Cassign(next_full).
    IF(!full).
      IF(_(in, "valid") && !_(out, "ready"), Lit(1)).
      ELSE(Lit(0)).
    END().ELSE().
      IF(_(out, "ready"), Lit(0)).
      ELSE(Lit(1));

  harpinst<N, RR, RR> inst(_(_(in, "contents"), "ir"));

  node ldregs(ready || !full);

  _(out, "valid") = Reg(_(in, "valid")) || full;
  _(_(_(out, "contents"), "warp"), "state") =
    Wreg(ldregs, _(_(_(in, "contents"), "warp"), "state"));
  _(_(_(out, "contents"), "warp"), "active") =
    Wreg(ldregs, _(_(_(in, "contents"), "warp"), "active"));
  _(_(_(out, "contents"), "warp"), "id") =
    Wreg(ldregs, _(_(_(in, "contents"), "warp"), "id"));

  bvec<L> pmask(_(_(_(in, "contents"), "pval"), "pmask"));

  _(_(_(out, "contents"), "rwb"), "mask") =
    Wreg(ldregs, bvec<L>(inst.has_rdst()) & pmask); // TODO: and w/ active lanes
  _(_(_(out, "contents"), "rwb"), "dest") = Wreg(ldregs, inst.get_rdst());
  _(_(_(out, "contents"), "rwb"), "val") = Wreg(ldregs, pc);

  bvec<N> out_pc;

  bvec<N> rval0(_(_(_(in, "contents"), "rval"), "val0")[0]);

  Cassign(out_pc).
    IF(pmask[0]).
      IF(inst.get_opcode() == Lit<6>(0x1c)
         || inst.get_opcode() == Lit<6>(0x1e), rval0). // jalr, jmpr
      IF(inst.get_opcode() == Lit<6>(0x1b)
         || inst.get_opcode() == Lit<6>(0x1d), pc+inst.get_imm()). // jali, jmpi
      ELSE(Lit<N>(0)).
    END().
    ELSE(pc);

  _(_(_(out, "contents"), "warp"), "pc") = Wreg(ldregs, out_pc);

  tap("branch_full", full);
  tap("branch_out", out);
  tap("branch_in", in);
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
    node wr(_(_(in, "contents"), "mask")[i] && _(_(in, "contents"), "wr"));

    memq[i] = Syncmem(devAddr, memd[i], wr);
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
  // TODO: what is the value of empty (the verb, not the adjective)?

  vec<L, bvec<L> > eqmat; // Coalesce matrix: which addresses are equal?
  bvec<L> covered; // Is my request covered by that of a prior lane?
  cache_req_t cache_req;
  cache_resp_t cache_resp;

  vec<L, bvec<N> > a(_(_(in, "contents"), "a"));

  for (unsigned i = 0; i < L; ++i) {
    for (unsigned j = i; j < L; ++j)
      eqmat[i][j] = Lit(0);
    for (unsigned j = 0; j < i; ++j) {
      bvec<N-CLOG2(LINE*(N/8))> ai(a[i][range<CLOG2(LINE*(N/8)), N-1>()]),
                                aj(a[j][range<CLOG2(LINE*(N/8)), N-1>()]);
      eqmat[i][j] = ai == aj;
    }
    covered[i] = OrN(eqmat[i]);
  }

  bvec<L> allReqMask(Wreg(fill, ~covered & _(_(in, "contents"), "mask"))),
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
    qReg[l] = Wreg(ldqReg[l], Mux(aReg[l][range<CLOG2(N/8), CLOG2(N/8*LINE)-1>()], _(_(cache_resp,"contents"),"q")));
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
    bvec<L> maskBits;
    // TODO: D.R.Y. (log2 of line size in bytes)
    for (unsigned l = 0; l < L; ++l)
      maskBits[l] = (aReg[l][range<CLOG2(N/8), CLOG2((N/8)*LINE)-1>()] == Lit<CLOG2(LINE)>(i)) && (aReg[l][range<CLOG2((N/8)*LINE), N-1>()] == reqAddr[range<CLOG2((N/8)*LINE), N-1>()]);
    
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

  _(_(out, "contents"), "warp") = Wreg(_(cache_resp, "valid") && _(cache_resp, "ready"), _(_(cache_resp, "contents"), "warp"));
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

template <unsigned A, unsigned B>
  void AddrRouteFunc(bvec<1u<<(B-A+1)> &valid,
                     const cache_req_int_t &in,
                     node in_valid)
  {
    bvec<1u<<(B-A+1)> sel(_(in, "a")[range<A, B>()]);

    valid = Dec(sel, in_valid);
  }

int main(int argc, char **argv) {
  // Instantiate the processor
  Harmonica2();

  // Optimization/simulation
  if (cycdet()) return 1;
  optimize();

  ofstream vcd("h2.vcd");
  run(vcd, 10000);

  return 0;
}
