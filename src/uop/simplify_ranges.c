// uop/simplify_ranges.c - tinygrad's pm_simplify_ranges (codegen/simplify.py:
// 21-38) on a kernel's output ranges: two adjacent ones, r0 outer to r1, merge
// into one range R of extent s0*s1 when r0 -> R / s1 and r1 -> R % s1 leave the
// kernel with no more IDIV and IMOD nodes, so a row-major output loops as one
// axis.  tinygrad also merges a REDUCE's ranges; thvm keeps those.

// tinygrad's count_divmod (codegen/simplify.py:18-19).
static u32 simplify_ranges_count_divmod(Term root) {
  UOpWalk w = {0};
  uop_walk_push(&w, root);
  u32  n = 0;
  Term t;
  while ((t = uop_walk_pop(&w)) != 0) {
    u32 op = term_ext(t);
    if (op == UOP_IDIV || op == UOP_IMOD) n++;
    u32 ns = uop_src_count(t);
    for (u32 i = 0; i < ns; i++) uop_walk_push(&w, uop_src_at(t, i));
  }
  uop_walk_free(&w);
  return n;
}

// The kernel's LOOP ranges by axis id, the output dims in order; 0 past `cap`.
static u32 simplify_ranges_loops(Term root, Term *out, u32 cap) {
  UOpWalk w = {0};
  uop_walk_push(&w, root);
  u32  n = 0;
  Term t;
  while ((t = uop_walk_pop(&w)) != 0) {
    if (term_ext(t) == UOP_RANGE) {
      if (uop_range_axis_type(t) != KAX_LOOP) continue;
      if (n == cap) {
        n = 0;
        break;
      }
      out[n++] = t;
      continue;
    }
    u32 ns = uop_src_count(t);
    for (u32 i = 0; i < ns; i++) uop_walk_push(&w, uop_src_at(t, i));
  }
  uop_walk_free(&w);
  for (u32 i = 1; i < n; i++) {
    Term v = out[i];
    i32  j = (i32)i - 1;
    while (j >= 0 && uop_range_axis_id(out[j]) > uop_range_axis_id(v)) {
      out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = v;
  }
  return n;
}

fn Term uop_simplify_ranges(Term root) {
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return root;
  for (;;) {
    Term loops[MAX_AXES];
    u32  n = simplify_ranges_loops(root, loops, MAX_AXES);
    u32  divmods = simplify_ranges_count_divmod(root);
    Term merged  = 0;
    for (u32 i = 0; i + 1 < n && merged == 0; i++) {
      u32 s0 = uop_range_extent(loops[i]);
      u32 s1 = uop_range_extent(loops[i + 1]);
      if (kvar_extent_is_var(s0) || kvar_extent_is_var(s1)) continue;
      if ((u64)s0 * s1 > 0x7FFFFFFFu) continue;
      Term r    = uop_range(uop_range_axis_id(loops[i]), KAX_LOOP, s0 * s1);
      Term s    = uop_const(DT_INT32, s1);
      Term to[2] = { uop_int_binary(UOP_IDIV, r, s), uop_int_binary(UOP_IMOD, r, s) };
      Term cand = uop_substitute(root, &loops[i], to, 2);
      if (simplify_ranges_count_divmod(cand) <= divmods) merged = cand;
    }
    if (merged == 0) return root;
    root = merged;
  }
}
