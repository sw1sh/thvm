// lam/body_uses_var.c -- detect whether a LAM's binder is referenced
// inside its body, used to set LAM_ERA_MASK at construction sites.
//
// Walk the heap subgraph rooted at HEAP[lam_loc] and look for a TAG_VAR
// whose val == lam_loc.  Returns 1 if found, 0 if certainly absent.
//
// Mirrors HVM4's parse/count_uses.c (zero-vs-nonzero) but operates on
// the live heap, not the parse tree:
//   - HVM4 walks the in-memory parse term before sealing the LAM.
//   - We walk the post-sealing heap cells (the body has already been
//     installed at lam_loc), reading children via heap_read.
//
// Visited-cell dedup keeps the walk linear in the body's distinct heap
// cells; without it a SUP / DUP that shares cells across both children
// would re-scan the same subgraph.  Capacity LAM_BODY_MAX_VISITS covers
// every body we currently emit (LAM bodies are tens to low hundreds of
// cells in practice).  If the visit set fills up or the cell-walk depth
// would exceed the cap, return 1 (conservative: assume the binder is
// used).  Conservative bias means the optimisation simply doesn't fire
// on pathological inputs; correctness is preserved.
//
// Arity is computed inline rather than reusing schedule/uop_meta.c's
// uop_arity, because this file is included EARLY in the TU (right
// after lam/shape.c) so it's visible to alo_realize and
// clone_to_book_rec, which both need to call it.  The schedule
// pipeline isn't loaded yet at that point.

#define LAM_BODY_MAX_VISITS 4096

// Per-tag arity for the heap-cell walk.  Mirrors term_arity in
// wnf/redex.c.  UOP arity is inlined here (kept in sync with
// schedule/uop_meta.c's uop_arity) since this file precedes that one.
static u32 lam_body_arity(Term t) {
  u8 tag = term_tag(t);
  switch (tag) {
    case TAG_LAM: return 1;
    case TAG_APP: return 2;
    case TAG_SUP: return 2;
    case TAG_DUP: return 1;
    case TAG_OP2: return 2;
    case TAG_MAT: return 2;
    case TAG_ALO: return 2;
    case TAG_DP0: case TAG_DP1: {
      if (term_ext(t) & DUP_GRAD_FLAG) return 3;
      return 1;
    }
    case TAG_UOP: {
      u8 op = term_ext(t);
      switch (op) {
        case UOP_CONST:                                    return 1;
        case UOP_ADD: case UOP_MUL:
        case UOP_CMPLT: case UOP_CMPEQ:
        case UOP_KERNEL: case UOP_ASSIGN:                  return 2;
        case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
        case UOP_LOG2: case UOP_SQRT:
        case UOP_LOAD: case UOP_FLIP:
        case UOP_CAST: case UOP_BITCAST:                   return 1;
        case UOP_REDUCE:                                   return 1;
        // Movement ops with a NUM-tail of metadata: walk only the
        // input cell (offset 0).  The remaining cells hold NUMs
        // (axes / dims / pad amounts) which can't be VAR(lam_loc).
        case UOP_RESHAPE: case UOP_PERMUTE: case UOP_EXPAND:
        case UOP_PAD:     case UOP_SHRINK:                 return 1;
        default:                                           return 0;
      }
    }
    case TAG_EQL: return 2;
    case TAG_AND: return 2;
    case TAG_OR:  return 2;
    case TAG_INC: return 1;
    case TAG_WHEN: return 2;
    case TAG_ANN: return 2;
    case TAG_BRI: return 1;
    default: return 0;
  }
}

fn int lam_body_uses_var(u64 lam_loc) {
  // Per-call dedup + DFS stack; sized once and reused across calls
  // (function-local statics are zero-init, but we don't rely on that
  // since v_count / s_pos are reset on every call).
  static u64  visited[LAM_BODY_MAX_VISITS];
  static Term stack[LAM_BODY_MAX_VISITS];
  u32 v_count = 0;
  u32 s_pos   = 0;

  Term root = heap_read(lam_loc);
  stack[s_pos++] = root;

  while (s_pos > 0) {
    Term t = stack[--s_pos];
    u8  tag = term_tag(t);
    u64 val = term_val(t);

    if (tag == TAG_VAR && val == lam_loc) {
      return 1;
    }

    // Opaque references that may carry a hidden VAR binding to
    // lam_loc.  We can't see inside without forcing reduction (TAG_REF
    // / TAG_ALO) or walking a side table (UOP_KERNEL inputs), so be
    // conservative and bail.
    if (tag == TAG_REF || tag == TAG_ALO) {
      return 1;
    }
    if (tag == TAG_UOP) {
      u8 op = term_ext(t);
      if (op == UOP_KERNEL) {
        // Kernel input bindings live in a side table (KernelEntry.
        // input_terms), not in heap cells.  A TAG_VAR(lam_loc) input
        // wouldn't be visible to a heap walk; assume the binder is
        // used.
        return 1;
      }
    }

    u32 ar = lam_body_arity(t);
    if (ar == 0) continue;

    // Dedup by base-of-cells loc (val).  Two compound nodes that share
    // the same heap base have the same children; revisiting them just
    // re-scans the subgraph.
    u8 seen = 0;
    for (u32 i = 0; i < v_count; i++) {
      if (visited[i] == val) { seen = 1; break; }
    }
    if (seen) continue;
    if (v_count >= LAM_BODY_MAX_VISITS) {
      // Bail conservatively -- the body is bigger than our cap so we
      // can't prove the binder is unused.
      return 1;
    }
    visited[v_count++] = val;

    for (u32 i = 0; i < ar; i++) {
      if (s_pos >= LAM_BODY_MAX_VISITS) {
        return 1;
      }
      stack[s_pos++] = heap_read(val + i);
    }
  }

  return 0;
}

// Convenience: compute the LAM ext to seal with given the binder loc
// and a base ext (usually 0).  Sets LAM_ERA_MASK iff lam_body_uses_var
// proves the binder is unreferenced.
fn u32 lam_seal_ext(u64 lam_loc, u32 base_ext) {
  if (lam_body_uses_var(lam_loc)) return base_ext;
  return base_ext | LAM_ERA_MASK;
}
