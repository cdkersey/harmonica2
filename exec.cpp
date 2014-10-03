#include <fstream>

#include <chdl/chdl.h>
#include <chdl/cassign.h>

#include "config.h"
#include "interfaces.h"
#include "harpinst.h"

using namespace std;
using namespace chdl;

void Execute(splitter_sched_t&, splitter_pred_t&, splitter_reg_t&, reg_func_t&);

enum funcunit_type_t {
  FU_ALU, FU_PLU, FU_MULT, FU_DIV, FU_LSU, FU_BRANCH, FU_PAD0, FU_PAD1, N_FU
};

// Functional Unit Routing Function
void RouteFunc(bvec<N_FU> &valid, const reg_func_int_t &in, node in_valid);

// Functional Units
void Funcunit_alu(func_splitter_t &out, reg_func_t &in);
void Funcunit_plu(func_splitter_t &out, reg_func_t &in);
void Funcunit_mult(func_splitter_t &out, reg_func_t &in);
void Funcunit_div(func_splitter_t &out, reg_func_t &in);
void Funcunit_lsu(func_splitter_t &out, reg_func_t &in);
void Funcunit_branch(func_splitter_t &out, reg_func_t &in);

// The execute pipeline stage
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
  _(_(_(fu_router_in, "contents"), "warp"), "id")
    = _(_(_(in, "contents"), "warp"), "id");
  _(_(_(fu_router_in, "contents"), "warp"), "next_id")
    = _(_(_(in, "contents"), "warp"), "next_id");
  _(_(_(fu_router_in, "contents"), "warp"), "pc")
    = _(_(_(in, "contents"), "warp"), "pc") + Lit<N>(N/8);
  _(_(fu_router_in, "contents"), "ir") = _(_(in, "contents"), "ir");
  _(_(fu_router_in, "contents"), "pval") = _(_(in, "contents"), "pval");
  _(_(fu_router_in, "contents"), "rval") = _(_(in, "contents"), "rval");

  vec<N_FU, reg_func_t> fu_inputs;
  vec<N_FU, func_splitter_t> fu_outputs;

  Router(fu_inputs, RouteFunc, fu_router_in);

  Funcunit_alu(fu_outputs[FU_ALU], fu_inputs[FU_ALU]);
  Funcunit_plu(fu_outputs[FU_PLU], fu_inputs[FU_PLU]);
  Funcunit_mult(fu_outputs[FU_MULT], fu_inputs[FU_MULT]);
  Funcunit_div(fu_outputs[FU_DIV], fu_inputs[FU_DIV]);
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
  _(_(out, "contents"), "warp") = _(_(fu_arbiter_out, "contents"), "warp");
  _(_(out, "contents"), "spawn") = _(_(fu_arbiter_out, "contents"), "spawn");
  _(_(out, "contents"), "spawn_pc") =
     _(_(fu_arbiter_out, "contents"), "spawn_pc");

  HIERARCHY_EXIT();
}

void RouteFunc(bvec<N_FU> &valid, const reg_func_int_t &in, node in_valid) {
  HIERARCHY_ENTER();
  harpinst<N, RR, RR> inst(_(in, "ir"));

  bvec<N_FU> v;

  v[FU_ALU] =
    ((inst.get_opcode() == Lit<6>(0x00)   || // nop
      inst.get_opcode() == Lit<6>(0x05)   || // neg
      inst.get_opcode() == Lit<6>(0x06)   || // not
      inst.get_opcode() == Lit<6>(0x07)   || // and
      inst.get_opcode() == Lit<6>(0x08))  || // or
     (inst.get_opcode() == Lit<6>(0x09)   || // xor
      inst.get_opcode() == Lit<6>(0x0a)   || // add
      inst.get_opcode() == Lit<6>(0x0b)   || // sub
      inst.get_opcode() == Lit<6>(0x0f)   || // shl
      inst.get_opcode() == Lit<6>(0x10))) || // shr
    ((inst.get_opcode() == Lit<6>(0x11)   || // andi
      inst.get_opcode() == Lit<6>(0x12)   || // ori
      inst.get_opcode() == Lit<6>(0x13)   || // xori
      inst.get_opcode() == Lit<6>(0x14)   || // addi
      inst.get_opcode() == Lit<6>(0x15))  || // subi
     (inst.get_opcode() == Lit<6>(0x19)   || // shli
      inst.get_opcode() == Lit<6>(0x1a)   || // shri
      inst.get_opcode() == Lit<6>(0x25)   || // ldi
      inst.get_opcode() == Lit<6>(0x1f)));   // clone

  v[FU_PLU] =
    (inst.get_opcode() == Lit<6>(0x26)  || // rtop
     inst.get_opcode() == Lit<6>(0x2b)  || // isneg
     inst.get_opcode() == Lit<6>(0x2c)) || // iszero
   ((inst.get_opcode() == Lit<6>(0x27)  || // andp
     inst.get_opcode() == Lit<6>(0x28)) || // orp
    (inst.get_opcode() == Lit<6>(0x29)  || // xorp
     inst.get_opcode() == Lit<6>(0x2a)));  // notp

  v[FU_MULT] =
    inst.get_opcode() == Lit<6>(0x0c) || // mul
    inst.get_opcode() == Lit<6>(0x16);   // muli

  v[FU_DIV] =
    (inst.get_opcode() == Lit<6>(0x0d)  || // div
     inst.get_opcode() == Lit<6>(0x0e)) || // mod
    (inst.get_opcode() == Lit<6>(0x17)  || // divi
     inst.get_opcode() == Lit<6>(0x18));   // modi

  v[FU_LSU] =
    inst.get_opcode() == Lit<6>(0x23) || // ld
    inst.get_opcode() == Lit<6>(0x24);   // st

  v[FU_BRANCH] =
    (inst.get_opcode() == Lit<6>(0x1b)  || // jali
     inst.get_opcode() == Lit<6>(0x20)  || // jalis
     inst.get_opcode() == Lit<6>(0x1c)  || // jalr
     inst.get_opcode() == Lit<6>(0x21)) || // jalrs
    (inst.get_opcode() == Lit<6>(0x1e)  || // jmpr
     inst.get_opcode() == Lit<6>(0x22)  || // jmprt
     inst.get_opcode() == Lit<6>(0x1d)  || // jmpi
     inst.get_opcode() == Lit<6>(0x3a)) || // wspawn
     inst.get_opcode() == Lit<6>(0x3b)  || // split
     inst.get_opcode() == Lit<6>(0x3c);    // join

  valid = v & bvec<N_FU>(in_valid);
  HIERARCHY_EXIT();
}
