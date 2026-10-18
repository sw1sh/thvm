// Whether the body at HEAP[lam_loc] holds a VAR(lam_loc), for LAM_ERA_MASK: 0
// only when the binder is certainly unused, 1 when used or the walk can't tell.
// HVM4 counts on its parse tree (TinyHVM/HVM4/clang/parse/count_uses.c).

#define LAM_BODY_MAX_VISITS 4096

// Cells that can hold a child Term.
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
        // Movement ops: cell 0 is the input; the NUM tail cannot hold the VAR.
        case UOP_RESHAPE: case UOP_PERMUTE: case UOP_EXPAND:
        case UOP_PAD:     case UOP_SHRINK:                 return 1;
        // A select, index- or value-level (uop_where): [cond, then, else].
        case UOP_IWHERE:                                   return 3;
        // Indexed ops: the Term children lead the NUM payload.
        case UOP_GATHER:                                   return 2;
        case UOP_SCATTER:                                  return 3;
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

    // REF and ALO hide their bodies until reduced: assume the binder used.
    if (tag == TAG_REF || tag == TAG_ALO) {
      return 1;
    }
    if (tag == TAG_UOP) {
      u8 op = term_ext(t);
      if (op == UOP_KERNEL) {
        // The walk cannot see kernel inputs (KernelEntry.input_terms).
        return 1;
      }
    }

    // CTR children sit at val+1..val+n, past the arity cell, so the generic
    // val+i loop below would miss the last one.
    if (tag == TAG_CTR) {
      u32 n = term_ctr_n(t);
      if (n == 0) continue;
      u8 seen_ctr = 0;
      for (u32 i = 0; i < v_count; i++) {
        if (visited[i] == val) { seen_ctr = 1; break; }
      }
      if (seen_ctr) continue;
      if (v_count >= LAM_BODY_MAX_VISITS) return 1;
      visited[v_count++] = val;
      for (u32 i = 0; i < n; i++) {
        if (s_pos >= LAM_BODY_MAX_VISITS) return 1;
        stack[s_pos++] = term_ctr_at(t, i);
      }
      continue;
    }

    u32 ar = lam_body_arity(t);
    if (ar == 0) continue;

    // Nodes sharing a base loc share their children: scan them once.
    u8 seen = 0;
    for (u32 i = 0; i < v_count; i++) {
      if (visited[i] == val) { seen = 1; break; }
    }
    if (seen) continue;
    if (v_count >= LAM_BODY_MAX_VISITS) {
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

fn u32 lam_seal_ext(u64 lam_loc, u32 base_ext) {
  if (lam_body_uses_var(lam_loc)) return base_ext;
  return base_ext | LAM_ERA_MASK;
}
