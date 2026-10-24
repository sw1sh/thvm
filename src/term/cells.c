// Cells the node at term_val(t) owns in `mem` (HEAP or BOOK_HEAP, which share
// every layout); 0 for an atom.  The collector, the book snapshot, the unfold
// and the preserve mark all size nodes here, so a tag or assigned opcode with
// no case would be skipped by all four (44-50 are unassigned).  A wnf frame
// owns the cells of the node it points at.
_Static_assert(TAG_COUNT == 35, "a new tag needs its heap size in term_cells");
_Static_assert(UOP_COUNT == 56, "a new UOp opcode needs its heap size in term_cells");
fn u32 term_cells(Term t, Term const *mem) {
  u8  tag = term_tag(t);
  u64 loc = term_val(t);
  switch (tag) {
    case TAG_NUM: case TAG_TEN: case TAG_REF: case TAG_ERA:
    case TAG_ANY: case TAG_FVR:
      return 0;
    case TAG_LAM: case TAG_DUP: case TAG_INC: case TAG_BRI:
    case TAG_VAR: case TAG_BJ0: case TAG_BJ1:
      return 1;
    case TAG_DP0: case TAG_DP1:
      return (term_ext(t) & DUP_GRAD_FLAG) ? 3 : 1;
    case TAG_APP: case TAG_SUP: case TAG_OP2: case TAG_MAT:
    case TAG_ALO: case TAG_EQL: case TAG_AND: case TAG_OR:
    case TAG_WHEN: case TAG_ANN:
    case TAG_F_OP2_NUM: case TAG_F_EQL_R:
      return 2;
    case TAG_DSU: case TAG_DDU:
    case TAG_F_DSU_LAB: case TAG_F_DDU_LAB:
      return 3;
    case TAG_F_UOP_CHILD:                     // ext bits 8..15: the parent opcode
      return term_cells(term_new(0, TAG_UOP, (term_ext(t) >> 8) & 0xFF, loc), mem);
    case TAG_CTR: {
      Term n_cell = mem[loc];
      if (term_tag(n_cell) != TAG_NUM) return 0;
      return 1 + (u32)term_val(n_cell);
    }
    case TAG_PRI: {
      if (loc == 0) return 0;
      Term cnt = mem[loc];
      if (term_tag(cnt) != TAG_NUM) return 0;
      return 1 + (u32)term_val(cnt);
    }
    case TAG_UOP: {
      u32 op = term_ext(t);
      switch (op) {
        case UOP_CONST: return 1;
        case UOP_KERNEL: case UOP_ASSIGN: return 2;
        case UOP_NEG: case UOP_RECIP: case UOP_EXP2: case UOP_SIN:
        case UOP_LOG2: case UOP_SQRT: case UOP_LOAD:
          return 1;
        case UOP_COPY: return 2;              // [src, NUM(device+1)]
        case UOP_ADD: case UOP_MUL:
        case UOP_CMPLT: case UOP_CMPEQ:
          return 2;
        // CAST/BITCAST = [src, NUM(dst_dtype)]; INDEX_E/AFTER and the integer
        // ALU ops = two srcs.
        case UOP_CAST: case UOP_BITCAST:
        case UOP_INDEX_E: case UOP_AFTER:
        case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
        case UOP_IMOD: case UOP_ILT: case UOP_IAND: case UOP_IOR:
        case UOP_IXOR: case UOP_ISHR: case UOP_ISHL:
          return 2;
        case UOP_INVALID:     return 1;       // [NUM(0)]
        case UOP_DEFINE_VAR:  return 1;       // [NUM(packed_extent)]
        case UOP_DETACH:      return 1;       // [src]
        case UOP_RANGE:                       // [NUM(axis_id), NUM(axis_type), NUM(extent)]
        case UOP_IWHERE: case UOP_STORE:      // [cond,then,else] / [buf,addr,value]
        case UOP_OPT:                         // [target, NUM(kind), NUM(factor)]
        case UOP_GATHER:                      // [src, idx, NUM(dim)]
          return 3;
        // [base, idx, src, NUM(dim), NUM(kind), NUM(include_self)]
        case UOP_SCATTER:
          return 6;
        case UOP_FLIP:   return 2;
        // [src, NUM(kind), NUM(n_axes), NUM(axis_0), ...]
        case UOP_REDUCE: {
          Term n_cell = mem[loc + 2];
          if (term_tag(n_cell) != TAG_NUM) return 0;
          return 3 + (u32)term_val(n_cell);
        }
        case UOP_RESHAPE: case UOP_EXPAND: case UOP_PERMUTE: {
          Term ndim_cell = mem[loc + 1];
          if (term_tag(ndim_cell) != TAG_NUM) return 0;
          return 2 + (u32)term_val(ndim_cell);
        }
        case UOP_PAD: case UOP_SHRINK: {
          Term ndim_cell = mem[loc + 1];
          if (term_tag(ndim_cell) != TAG_NUM) return 0;
          return 2 + 2 * (u32)term_val(ndim_cell);
        }
        // UOP_BUFFER = [NUM(scope), NUM(dtype), NUM(ndim), NUM(instance),
        //               NUM(d0), ..].
        case UOP_BUFFER: {
          Term ndim_cell = mem[loc + 2];
          if (term_tag(ndim_cell) != TAG_NUM) return 0;
          return 4 + (u32)term_val(ndim_cell);
        }
        // UOP_BUFFERIZE = [value, NUM(addrspace), NUM(removable), NUM(n),
        //                  range_0, .., range_{n-1}].
        case UOP_BUFFERIZE: {
          Term n_cell = mem[loc + 3];
          if (term_tag(n_cell) != TAG_NUM) return 0;
          return 4 + (u32)term_val(n_cell);
        }
        default: return 0;
      }
    }
    default: return 0;
  }
}
