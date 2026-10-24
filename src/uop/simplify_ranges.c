// uop/simplify_ranges.c - tinygrad's pm_split_ranges and pm_simplify_ranges
// (codegen/simplify.py:21-75) on a kernel's output ranges, in its order: a range
// read modulo a divisor of its extent splits in two, then two adjacent ones, r0
// outer to r1, merge into one range R of extent s0*s1 when r0 -> R / s1 and
// r1 -> R % s1 leave the kernel with no more IDIV and IMOD nodes, so a
// row-major output loops as one axis.  tinygrad also splits and merges a
// REDUCE's ranges; thvm keeps those.

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

// An output range the kernel reads as RANGE % c, c a proper divisor of its
// extent, with *c_out = c; 0 when there is none.
static Term simplify_ranges_mod_range(Term root, u32 *c_out) {
  UOpWalk w = {0};
  uop_walk_push(&w, root);
  Term found = 0, t;
  while (found == 0 && (t = uop_walk_pop(&w)) != 0) {
    if (term_ext(t) == UOP_IMOD) {
      Term r = heap_read(term_val(t) + 0);
      i64  c = 0;
      if (term_tag(r) == TAG_UOP && term_ext(r) == UOP_RANGE
          && uop_range_axis_type(r) == KAX_LOOP && !kvar_extent_is_var(uop_range_extent(r))
          && uop_iconst_value(heap_read(term_val(t) + 1), &c)
          && c > 1 && (u64)c < uop_range_extent(r) && uop_range_extent(r) % (u64)c == 0) {
        found  = r;
        *c_out = (u32)c;
      }
    }
    u32 ns = uop_src_count(t);
    for (u32 i = 0; i < ns; i++) uop_walk_push(&w, uop_src_at(t, i));
  }
  uop_walk_free(&w);
  return found;
}

// tinygrad's pm_split_ranges (codegen/simplify.py:66-75): that range splits
// into an outer extent / c and an inner c at the next axis id, later ids (a
// REDUCE's too) moving up one, so a stack's selector is an axis of its own.
static Term simplify_ranges_split(Term root) {
  for (;;) {
    u32  c = 0;
    Term r = simplify_ranges_mod_range(root, &c);
    if (r == 0) return root;
    u32  aid   = uop_range_axis_id(r);
    Term outer = uop_range(aid, KAX_LOOP, uop_range_extent(r) / c);
    Term inner = uop_range(aid + 1, KAX_LOOP, c);
    ApplyOptDagSplitCtx ctx;
    ctx.n = 0;
    ctx.entries[ctx.n].key = r;
    ctx.entries[ctx.n].val = uop_int_binary(UOP_IADD,
        uop_int_binary(UOP_IMUL, outer, uop_const(DT_INT32, c)), inner);
    ctx.n++;
    Term ranges[APPLY_OPT_DAG_SPLIT_MAP_CAP];
    u32  n = apply_opt_dag_collect_ranges(root, ranges, APPLY_OPT_DAG_SPLIT_MAP_CAP);
    if (n > APPLY_OPT_DAG_SPLIT_MAP_CAP) return root;
    for (u32 i = 0; i < n && ctx.n < APPLY_OPT_DAG_SPLIT_MAP_CAP; i++) {
      u32 a = uop_range_axis_id(ranges[i]);
      if (ranges[i] == r || a <= aid) continue;
      ctx.entries[ctx.n].key = ranges[i];
      ctx.entries[ctx.n].val = uop_range(a + 1, uop_range_axis_type(ranges[i]),
                                         uop_range_extent(ranges[i]));
      ctx.n++;
    }
    ApplyOptDagSubState st;
    st.memo_n             = 0;
    st.map                = &ctx;
    st.reduce_shift_above = aid;
    st.reduce_swap_a      = 0xFFFFFFFFu;
    st.reduce_swap_b      = 0xFFFFFFFFu;
    st.remap_n            = 0;
    root = apply_opt_dag_substitute(root, &st);
  }
}

fn Term uop_simplify_ranges(Term root) {
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return root;
  root = simplify_ranges_split(root);
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
