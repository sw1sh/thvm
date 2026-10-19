// Whether the body at HEAP[lam_loc] holds a VAR(lam_loc), for LAM_ERA_MASK: 0
// only when the binder is certainly unused, 1 when used or the walk can't tell.
// HVM4 counts on its parse tree (TinyHVM/HVM4/clang/parse/count_uses.c).
// Nodes are sized by term_cells, so every child cell is seen.

#define LAM_BODY_MAX_VISITS 4096

// A VAR's or a book projection's cell is another binder's (or the book's), not
// part of the body.
static int lam_body_leaf(u8 tag) {
  return tag == TAG_VAR || tag == TAG_BJ0 || tag == TAG_BJ1;
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
    if (lam_body_leaf(tag)) {
      continue;
    }

    // REF and ALO hide their bodies until reduced: assume the binder used.
    if (tag == TAG_REF || tag == TAG_ALO) {
      return 1;
    }
    if (tag == TAG_UOP && term_ext(t) == UOP_KERNEL) {
      // The walk cannot see kernel inputs (KernelEntry.input_terms).
      return 1;
    }

    u32 ar = term_cells(t, HEAP);
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
