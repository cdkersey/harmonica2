#ifndef HARMONICA_HARPINST_H
#define HARMONICA_HARPINST_H


// This structure defines an instruction type and provides a way to decode it.
// Issues:
//   Assumes, when reading predicate register fields, that R == P.
namespace chdl { // TODO: this is naughty (less so than a using directive)

enum argclass {
  AC_NONE, AC_1REG, AC_2REG, AC_3REG, AC_3REGSRC, AC_1IMM, AC_2IMM, AC_3IMM,
  AC_3IMMSRC, AC_PREG_REG, AC_2PREG, AC_3PREG, AC_2REGSRC, N_AC
};

template <unsigned N, unsigned R, unsigned P> struct harpinst {
  harpinst(bvec<N> inst): inst(inst), argclass(argclass_hw()) {}

  node    has_pred()  { return inst[N-1]; }
  bvec<P> get_pred()  { return inst[range<N-1-P, N-2>()]; }
  bvec<6> get_opcode(){ return inst[range<N-1-P-6, N-2-P>()]; }
  bvec<CLOG2(N_AC)> get_argclass() { return Enc(argclass); }

  node    has_pdst()  {
    return argclass[AC_PREG_REG] || argclass[AC_2PREG] || argclass[AC_3PREG];
  }
  bvec<P> get_pdst()  { return inst[range<N-1-P-6-R, N-1-P-7>()]; }

  node    has_psrc0() {
    return argclass[AC_2PREG] || argclass[AC_3PREG];
  }
  bvec<P> get_psrc0() { return inst[range<N-1-P-6-2*R, N-1-P-7-R>()]; }

  node    has_psrc1() { return argclass[AC_3PREG]; }
  bvec<P> get_psrc1() { return inst[range<N-1-P-6-3*R, N-1-P-7-2*R>()]; }

  node    has_rdst()  {
    return argclass[AC_2REG] || argclass[AC_3REG]
        || argclass[AC_2IMM] || argclass[AC_3IMM]
        || is_jal();
  }

  bvec<R> get_rdst()  { return inst[range<N-1-P-6-R, N-1-P-7>()]; }

  node    has_rsrc0() {
    return argclass[AC_1REG] || argclass[AC_2REG]
        || argclass[AC_3REG] || argclass[AC_3REGSRC]
        || argclass[AC_3IMM] || argclass[AC_3IMMSRC]
        || argclass[AC_PREG_REG] || argclass[AC_2REGSRC];
  }
  bvec<R> get_rsrc0() {
    return Mux(argclass[AC_1REG] || argclass[AC_2REGSRC] ||
               argclass[AC_3REGSRC] || argclass[AC_3IMMSRC],
                 inst[range<N-1-P-6-2*R, N-1-P-R-7>()], 
                 inst[range<N-1-P-6-R, N-1-P-7>()]);
  }

  node    has_rsrc1() {
    return argclass[AC_3REG] || argclass[AC_3REGSRC] || argclass[AC_3IMMSRC] ||
           argclass[AC_2REGSRC];
  }
  bvec<R> get_rsrc1() { return Mux(argclass[AC_3REGSRC] ||
                                   argclass[AC_3IMMSRC] || argclass[AC_2REGSRC],
                                     inst[range<N-1-P-6-3*R, N-1-P-2*R-7>()],
                                     inst[range<N-1-P-6-2*R, N-1-P-R-7>()]);
  }

  node    has_rsrc2() { return argclass[AC_3REGSRC]; }
  bvec<R> get_rsrc2() { return inst[range<N-1-P-6-3*R, N-1-P-2*R-7>()]; }

  node    has_imm() {
    return argclass[AC_1IMM] || argclass[AC_2IMM] || argclass[AC_3IMM]
        || argclass[AC_3IMMSRC];
  }
  bvec<N> get_imm() {
    bvec<N> imm_huge(Sext<N>(inst[range<0, N-1-P-7>()])),
            imm_large(Sext<N>(inst[range<0, N-1-P-7-R>()])),
            imm_small(Sext<N>(inst[range<0, N-1-P-7-2*R>()]));

    return Mux(argclass[AC_1IMM],
                 Mux(argclass[AC_2IMM], imm_small, imm_large),
                 imm_huge);
  }

  node is_jal() { return get_opcode() == Lit<6>(0x20)
                      || get_opcode() == Lit<6>(0x21)
                      || get_opcode() == Lit<6>(0x1b)
                      || get_opcode() == Lit<6>(0x1c); }

  node is_jmp() { return get_opcode() == Lit<6>(0x1b)
                      || get_opcode() == Lit<6>(0x1c)
                      || get_opcode() == Lit<6>(0x1d)
                      || get_opcode() == Lit<6>(0x1e)
                      || get_opcode() == Lit<6>(0x20)
                      || get_opcode() == Lit<6>(0x21)
                      || get_opcode() == Lit<6>(0x22)
                      || get_opcode() == Lit<6>(0x2f); }
  node is_store() { return get_opcode() == Lit<6>(0x24); }
  node is_halt() { return get_opcode() == Lit<6>(0x2d); }
  node is_clone() { return get_opcode() == Lit<6>(0x1f); }
  node is_wclone() { return get_opcode() == Lit<6>(0x3a); }
  node is_skep() { return get_opcode() == Lit<6>(0x30); }

  node is_spawn() { return get_opcode() == Lit<6>(0x20)
                        || get_opcode() == Lit<6>(0x21); }
  node is_term() { return get_opcode() == Lit<6>(0x22); } // jmprt
  node is_split() { return get_opcode() == Lit<6>(0x3a); } // split
  node is_join() { return get_opcode() == Lit<6>(0x3b); } // join

  node is_prot() { return get_opcode() == Lit<6>(0x30)    // skep
                       || get_opcode() == Lit<6>(0x01)    // di 
                       || get_opcode() == Lit<6>(0x02)    // ei
                       || get_opcode() == Lit<6>(0x03)    // tlbadd
                       || get_opcode() == Lit<6>(0x04)    // tlbflush
                       || get_opcode() == Lit<6>(0x32)    // tlbrm
                       || get_opcode() == Lit<6>(0x2f)    // jmpru
                       || get_opcode() == Lit<6>(0x31)    // reti
                       || get_opcode() == Lit<6>(0x2d); } // halt

  node is_ldst() { return get_opcode() == Lit<6>(0x23)
                       || get_opcode() == Lit<6>(0x24); }

  // Returns a bit vector representing the argument class of this instruction.
  bvec<N_AC> argclass_hw() {
    HIERARCHY_ENTER();
    bvec<6> op(get_opcode());
    bvec<N_AC> ac;

    ac[AC_NONE] = op == Lit<6>(0x2d);
    ac[AC_1REG] = op == Lit<6>(0x1e) || op == Lit<6>(0x1f)
               || op == Lit<6>(0x22) || op == Lit<6>(0x2f)
               || op == Lit<6>(0x30) || op == Lit<6>(0x32);
    ac[AC_2REG] = op == Lit<6>(0x05) || op == Lit<6>(0x06)
               || op == Lit<6>(0x1c) || op == Lit<6>(0x33)
               || op == Lit<6>(0x34) || op == Lit<6>(0x39);
    ac[AC_3REG] = op == Lit<6>(0x07) || op == Lit<6>(0x08)
               || op == Lit<6>(0x09) || op == Lit<6>(0x0a)
               || op == Lit<6>(0x0b) || op == Lit<6>(0x0c)
               || op == Lit<6>(0x0d) || op == Lit<6>(0x0e)
               || op == Lit<6>(0x0f) || op == Lit<6>(0x10)
               || op == Lit<6>(0x35) || op == Lit<6>(0x36)
               || op == Lit<6>(0x37) || op == Lit<6>(0x3a);
    ac[AC_3REGSRC] = Lit(0);
    ac[AC_1IMM] = op == Lit<6>(0x1d);
    ac[AC_2IMM] = op == Lit<6>(0x1b) || op == Lit<6>(0x25);
    ac[AC_3IMM] = op == Lit<6>(0x11) || op == Lit<6>(0x12)
               || op == Lit<6>(0x13) || op == Lit<6>(0x14)
               || op == Lit<6>(0x15) || op == Lit<6>(0x16)
               || op == Lit<6>(0x17) || op == Lit<6>(0x18)
               || op == Lit<6>(0x19) || op == Lit<6>(0x1a)
               || op == Lit<6>(0x23);
    ac[AC_3IMMSRC] = op == Lit<6>(0x24);
    ac[AC_PREG_REG] = op == Lit<6>(0x26) || op == Lit<6>(0x2b)
                   || op == Lit<6>(0x2c);
    ac[AC_2PREG] = op == Lit<6>(0x2a);
    ac[AC_3PREG] = op == Lit<6>(0x27) || op == Lit<6>(0x28)
                || op == Lit<6>(0x29);
    ac[AC_2REGSRC] = op == Lit<6>(0x3d);

    HIERARCHY_EXIT();

    return ac;  
  }

  bvec<N> inst;
  bvec<N_AC> argclass;
};
}
#endif
