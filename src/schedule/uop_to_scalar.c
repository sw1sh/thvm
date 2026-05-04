// schedule/uop_to_scalar.c - translate symbolic INDEX UOp trees into
// ScalarUop arena entries (Phase B3 wedge).
//
// `uop_to_scalar` walks a UOp Term that's known to be a symbolic
// INDEX expression (UOP_RANGE / UOP_CONST(int) / UOP_I*/UOP_IWHERE,
// possibly rooted at UOP_INDEX_E) and emits the equivalent ScalarUop
// slot id in `ke->scalar_uops[]`.  This is the bridge that lets
// B3's rangeify rerouting consume `uop_resolve_movement_chain`
// outputs without rewriting the per-USE address logic from scratch.
//
// The caller provides a UopRangeMap[] array mapping UOP_RANGE Terms
// to existing S_RANGE slot ids in the kernel's arena (rangeify
// builds those for LOOP / REDUCE axes during the per-kernel emit).
// Translation fails (returns 0) on UOP_INDEX_E whose buffer side
// hasn't been pre-mapped, on UOP_INVALID (caller should
// substitute), or on any non-INDEX-layer opcode.

// `UopRangeMap` is declared in thvm.h alongside `uop_to_scalar`.

static u8 uop_to_scalar_op(u32 uop_op) {
  switch (uop_op) {
    case UOP_IADD:   return S_IADD;
    case UOP_ISUB:   return S_ISUB;
    case UOP_IMUL:   return S_IMUL;
    case UOP_IDIV:   return S_IDIV;
    case UOP_IMOD:   return S_IMOD;
    case UOP_ILT:    return S_ILT;
    case UOP_IAND:   return S_IAND;
    default:         return 0;
  }
}

fn u32 uop_to_scalar(KernelEntry *ke, Term t,
                     UopRangeMap const *ranges, u32 n_ranges);

static u32 uop_to_scalar_const(KernelEntry *ke, Term t) {
  Term num = heap_read(term_val(t));
  if (term_tag(num) != TAG_NUM) return 0;
  u32 dtype = term_ext(num);
  if (!dtype_is_int(dtype)) return 0;
  i64 v = (i64)(i32)term_val(num);
  return rangeify_emit_leaf(ke, S_ICONST, DT_INT64, (u64)v);
}

static u32 uop_to_scalar_range(Term t, UopRangeMap const *ranges, u32 n_ranges) {
  for (u32 i = 0; i < n_ranges; i++) {
    if (ranges[i].axis_uop == t) return ranges[i].scalar_id;
  }
  return 0;  // not in the map -- caller missed it.
}

fn u32 uop_to_scalar(KernelEntry *ke, Term t,
                     UopRangeMap const *ranges, u32 n_ranges) {
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  switch (op) {
    case UOP_CONST:   return uop_to_scalar_const(ke, t);
    case UOP_RANGE:   return uop_to_scalar_range(t, ranges, n_ranges);
    case UOP_INVALID: return 0;  // caller substitutes a 0 / reduce identity.
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: {
      Term a_t = heap_read(term_val(t) + 0);
      Term b_t = heap_read(term_val(t) + 1);
      u32 a = uop_to_scalar(ke, a_t, ranges, n_ranges);
      u32 b = uop_to_scalar(ke, b_t, ranges, n_ranges);
      if (a == 0 || b == 0) return 0;
      u8 sop = uop_to_scalar_op(op);
      if (sop == 0) return 0;
      return rangeify_emit_binary(ke, sop, DT_INT64, a, b);
    }
    case UOP_IWHERE: {
      Term c_t = heap_read(term_val(t) + 0);
      Term y_t = heap_read(term_val(t) + 1);
      Term n_t = heap_read(term_val(t) + 2);
      u32 c = uop_to_scalar(ke, c_t, ranges, n_ranges);
      u32 y = uop_to_scalar(ke, y_t, ranges, n_ranges);
      u32 n = uop_to_scalar(ke, n_t, ranges, n_ranges);
      if (c == 0 || y == 0 || n == 0) return 0;
      u32 src[3] = {c, y, n};
      return rangeify_emit(ke, S_IWHERE, DT_INT64, 3, src, 0);
    }
    default: return 0;
  }
}
