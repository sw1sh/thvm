// ! &L{x0, x1} = UOP_op(...)
// ----------------------- DUP-UOP (commute)
//
// Mirrors HVM4's DUP-CON commutation, applied to tensor compute UOPs.
// We replicate the UOP at each projection side and push DUPs into its
// compute-input slots (NUM "parameter" slots are atomic and copied
// verbatim into both sides):
//
//   ! &L { x0, x1 } = UOP_op(s0, s1, ..., NUM_p, NUM_q, ...)
//   --->
//   ! &L { S0_0, S0_1 } = s0
//   ! &L { S1_0, S1_1 } = s1
//   ...
//   x0 = UOP_op(S0_0, S1_0, ..., NUM_p, NUM_q, ...)
//   x1 = UOP_op(S0_1, S1_1, ..., NUM_p, NUM_q, ...)
//
// Two design points:
//
//  - "ACTIVE" UOPs (KERNEL/GRAD/FWD/ASSIGN) NEVER commute here -- they
//    might evolve into SUP-bearing structures via their own
//    interactions (interact_grad emits SUPs at TEN leaves, kernel may
//    rewrite into output TENs, etc.), and discarding the DUP's label
//    before that happens would lose the label.  Caller leaves them
//    stuck (the wnf default branch puts the body back and stops).
//
//  - PERMUTE / PAD / SHRINK store NUM(ndim) at heap offset 1 (same
//    layout as RESHAPE / EXPAND), so uop_dup_arity reads ndim from
//    the cell directly without a shape walk.
//
// uop_dup_arity returns the heap arity (total cell count).

fn u32 uop_dup_arity(Term uop) {
  u8  op  = term_ext(uop);
  u64 loc = term_val(uop);
  switch (op) {
    case UOP_CONST:                                      return 1;
    case UOP_ADD: case UOP_MUL:
    case UOP_CMPLT: case UOP_CMPEQ:                      return 2;
    case UOP_NEG: case UOP_RECIP:
    case UOP_EXP2: case UOP_LOG2:
    case UOP_SQRT: case UOP_LOAD:                        return 1;
    case UOP_REDUCE:                                     return 3;
    case UOP_FLIP:                                       return 2;
    case UOP_RESHAPE: case UOP_EXPAND: {
      Term ndim_cell = heap_read(loc + 1);
      if (term_tag(ndim_cell) != TAG_NUM) return 0;
      return 2 + (u32)term_val(ndim_cell);
    }
    case UOP_PERMUTE: {
      Term ndim_cell = heap_read(loc + 1);
      if (term_tag(ndim_cell) != TAG_NUM) return 0;
      return 2 + (u32)term_val(ndim_cell);
    }
    case UOP_PAD: case UOP_SHRINK: {
      Term ndim_cell = heap_read(loc + 1);
      if (term_tag(ndim_cell) != TAG_NUM) return 0;
      return 2 + 2 * (u32)term_val(ndim_cell);
    }
    default: return 0;   // KERNEL/ASSIGN: bail (active)
  }
}

fn Term interact_dup_uop(u32 lab, u64 loc, u8 side, Term uop) {
  u32 arity = uop_dup_arity(uop);
  if (arity == 0) return 0;   // signal to caller: stay stuck

  u8  op      = term_ext(uop);
  u64 uop_loc = term_val(uop);

  ITRS++;

  // Two new heap blocks for the duplicated UOPs (one per projection).
  u64 u0 = heap_alloc(arity);
  u64 u1 = heap_alloc(arity);

  for (u32 i = 0; i < arity; i++) {
    Term child = heap_read(uop_loc + i);
    if (term_tag(child) == TAG_NUM) {
      // NUM is atomic: copy verbatim to both sides (DUP-NUM annihilate
      // would do the same, but we shortcut here).
      heap_set(u0 + i, child);
      heap_set(u1 + i, child);
    } else {
      // Compute slot: insert DUP^lab so the projections drive the
      // child's reduction independently.
      u64 dup = heap_alloc(1);
      heap_set(dup, child);
      heap_set(u0 + i, term_new(0, TAG_DP0, lab, dup));
      heap_set(u1 + i, term_new(0, TAG_DP1, lab, dup));
    }
  }

  Term x0 = term_new(0, TAG_UOP, op, u0);
  Term x1 = term_new(0, TAG_UOP, op, u1);
  return heap_subst_cop(side, loc, x0, x1);
}
