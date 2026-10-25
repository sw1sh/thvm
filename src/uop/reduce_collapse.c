// tinygrad's pm_reduce_collapse bound rule (codegen/simplify.py:104-111): the
// arange / cumsum / one-hot mask, a single-axis SUM over IWHERE(ILT(affine(r)),
// val, 0), folds to val times the count of r the guard admits.

// A mismatch returns 0 and keeps the reduce loop, so a miss costs only speed;
// reduce_unparented is rangeify's (ru_reduce_unparented).

static int rc_reduce_binds(Term red, u32 aid) {
  u32 n_axes = uop_reduce_n_axes(red);
  for (u32 i = 0; i < n_axes; i++) {
    if (uop_reduce_axis(red, i) == aid) {
      return 1;
    }
  }
  return 0;
}

// 1 when `n` is RANGE(aid); otherwise queues the sources a free RANGE(aid)
// could sit under.  A REDUCE that binds aid closes it for its body.
static int rc_axis_visit(UOpWalk *w, Term n, u32 aid) {
  u64 loc = term_val(n);
  switch (term_ext(n)) {
    case UOP_RANGE:
      return (u32)term_val(heap_read(loc + 0)) == aid;
    case UOP_BUFFER: case UOP_BUFFERIZE: case UOP_KERNEL:
      return 0;
    case UOP_REDUCE:
      if (rc_reduce_binds(n, aid)) {
        return 0;
      }
      break;
    default:
      break;
  }
  u8 ar = uop_arity(term_ext(n));
  for (u8 i = 0; i < ar; i++) {
    uop_walk_push(w, heap_read(loc + i));
  }
  return 0;
}

// 1 iff a free RANGE(aid) sits under `t`, exact at any depth: a false "unused"
// would fold a `val` that varies with the axis.  tinygrad's no_range asks the
// same over the full backward_slice (codegen/simplify.py:79).
static int rc_uses_axis(Term t, u32 aid) {
  UOpWalk w = {0};
  uop_walk_push(&w, t);
  int  found = 0;
  Term n;
  while (!found && (n = uop_walk_pop(&w)) != 0) {
    found = rc_axis_visit(&w, n, aid);
  }
  uop_walk_free(&w);
  return found;
}

// First RANGE(aid) under `body`, bound or free (only its extent is read);
// 0 when there is none.
static Term rc_find_range(Term body, u32 aid) {
  UOpWalk w = {0};
  uop_walk_push(&w, body);
  Term found = 0;
  Term n;
  while (found == 0 && (n = uop_walk_pop(&w)) != 0) {
    u64 loc = term_val(n);
    switch (term_ext(n)) {
      case UOP_RANGE:
        if ((u32)term_val(heap_read(loc + 0)) == aid) {
          found = n;
        }
        break;
      case UOP_BUFFER: case UOP_BUFFERIZE: case UOP_KERNEL:
        break;
      default: {
        u8 ar = uop_arity(term_ext(n));
        for (u8 i = 0; i < ar; i++) {
          uop_walk_push(&w, heap_read(loc + i));
        }
        break;
      }
    }
  }
  uop_walk_free(&w);
  return found;
}

static int rc_const_i32(Term t, i32 *out) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  u64 loc = term_val(t);
  Term num = heap_read(loc + 0);
  if (term_tag(num) != TAG_NUM) return 0;
  *out = (i32)term_val(num);
  return 1;
}

static int rc_is_int32_const(Term t) {
  return term_tag(t) == TAG_UOP && term_ext(t) == UOP_CONST
      && term_ext(heap_read(term_val(t) + 0)) == DT_INT32;
}

// Splits an IADD chain holding exactly one RANGE(aid) into that range (*rng)
// and the aid-free rest (*rest, 0 for a bare range); 0 for any other shape.
// An ADD of an int32 CONST (an arange's start - step) wraps as IADD does.
static int rc_split_affine(Term t, u32 aid, Term *rng, Term *rest) {
  if (term_tag(t) == TAG_UOP && term_ext(t) == UOP_RANGE
      && (u32)term_val(heap_read(term_val(t) + 0)) == aid) {
    *rng = t; *rest = 0; return 1;
  }
  if (term_tag(t) == TAG_UOP && (term_ext(t) == UOP_IADD || term_ext(t) == UOP_ADD)) {
    Term a = heap_read(term_val(t) + 0);
    Term b = heap_read(term_val(t) + 1);
    int a_has = rc_uses_axis(a, aid);
    int b_has = rc_uses_axis(b, aid);
    if (term_ext(t) == UOP_ADD && !rc_is_int32_const(a_has ? b : a)) return 0;
    if (a_has && !b_has) {
      Term ar, arest;
      if (!rc_split_affine(a, aid, &ar, &arest)) return 0;
      *rng = ar;
      *rest = (arest == 0) ? b : uop_int_binary(UOP_IADD, arest, b);
      return 1;
    }
    if (b_has && !a_has) {
      Term br, brest;
      if (!rc_split_affine(b, aid, &br, &brest)) return 0;
      *rng = br;
      *rest = (brest == 0) ? a : uop_int_binary(UOP_IADD, brest, a);
      return 1;
    }
  }
  return 0;
}

static Term rc_clamp_0_hi(Term x, u32 hi) {
  Term hic = uop_const(DT_INT32, hi);
  Term lt_hi = uop_int_binary(UOP_ILT, x, hic);
  Term minned = uop_iwhere(lt_hi, x, hic);
  Term zero = uop_const(DT_INT32, 0);
  Term lt_0 = uop_int_binary(UOP_ILT, minned, zero);
  return uop_iwhere(lt_0, zero, minned);
}

// The folded form of `red`, or 0 when it does not match.
static Term rc_collapse_one(Term red) {
  if (term_tag(red) != TAG_UOP || term_ext(red) != UOP_REDUCE) return 0;
  if (uop_reduce_kind(red) != REDUCE_SUM) return 0;
  if (uop_reduce_n_axes(red) != 1) return 0;
  u32 aid = uop_reduce_axis(red, 0);
  Term body = uop_reduce_src(red);
  // Only an IWHERE body can fold: decline before the walks below, each a pass
  // over the body.
  if (term_tag(body) != TAG_UOP || term_ext(body) != UOP_IWHERE) return 0;

  Term range_leaf = rc_find_range(body, aid);

  if (range_leaf == 0) {
    // The body never reads aid, so there is no RANGE leaf to take the extent
    // from; rangeify's reduce_unparented owns this case.
    return 0;
  }
  u32 ext = uop_range_extent(range_leaf);
  if (ext == 0 || ext > (1u << 24)) return 0;          // guard absurd extents

  u64 bloc = term_val(body);
  Term cond = heap_read(bloc + 0);
  Term then_v = heap_read(bloc + 1);
  Term else_v = heap_read(bloc + 2);
  if (rc_uses_axis(then_v, aid)) return 0;
  // else must be 0 / INVALID (both render as 0 and are the SUM identity).
  int else_zero = 0;
  if (term_tag(else_v) == TAG_UOP) {
    if (term_ext(else_v) == UOP_INVALID) else_zero = 1;
    else { i32 ev; if (rc_const_i32(else_v, &ev) && ev == 0) else_zero = 1; }
  }
  if (!else_zero) return 0;
  if (term_tag(cond) != TAG_UOP || term_ext(cond) != UOP_ILT) return 0;
  Term lhs = heap_read(term_val(cond) + 0);
  Term rhs = heap_read(term_val(cond) + 1);
  int lhs_has = rc_uses_axis(lhs, aid);
  int rhs_has = rc_uses_axis(rhs, aid);
  if (lhs_has == rhs_has) return 0;

  // Count of r in [0, ext) satisfying the guard, times `then_v`.
  Term count = 0;
  if (rhs_has) {
    // C < r + rest admits r >= C - rest + 1:
    // count = ext - clamp(C - rest + 1, 0, ext).
    Term rng, rest;
    if (!rc_split_affine(rhs, aid, &rng, &rest)) return 0;
    (void)rng;
    Term thr = (rest == 0) ? lhs : uop_int_binary(UOP_ISUB, lhs, rest);
    thr = uop_int_binary(UOP_IADD, thr, uop_const(DT_INT32, 1));
    Term clamped = rc_clamp_0_hi(thr, ext);
    count = uop_int_binary(UOP_ISUB, uop_const(DT_INT32, ext), clamped);
  } else {
    // r + rest < C admits r < C - rest: count = clamp(C - rest, 0, ext).
    Term rng, rest;
    if (!rc_split_affine(lhs, aid, &rng, &rest)) return 0;
    (void)rng;
    Term thr = (rest == 0) ? rhs : uop_int_binary(UOP_ISUB, rhs, rest);
    count = rc_clamp_0_hi(thr, ext);
  }
  if (count == 0) return 0;

  // val multiplies in its own dtype: IMUL is int32 index math, rendered
  // (int)val * (int)count, so a running sum of 0.1 read 0 and an int64 ramp
  // past 2^31 or a uint32 one wrapped.
  u32 vdt;
  if (term_dtype_in(then_v, 0, &vdt) && vdt != DT_INT32) {
    return uop_binary(UOP_MUL, then_v, uop_cast(count, vdt));
  }
  return uop_int_binary(UOP_IMUL, then_v, count);
}

// 1 iff a REDUCE sits under `t`, through UOp sources and not past a buffer.
static int rc_has_reduce(Term t) {
  UOpWalk w = {0};
  uop_walk_push(&w, t);
  int found = 0;
  Term n;
  while (!found && (n = uop_walk_pop(&w)) != 0) {
    u8 op = term_ext(n);
    if (op == UOP_REDUCE) {
      found = 1;
    } else if (op != UOP_BUFFER && op != UOP_BUFFERIZE && op != UOP_KERNEL) {
      for (u8 i = 0; i < uop_arity(op); i++) {
        uop_walk_push(&w, heap_read(term_val(n) + i));
      }
    }
  }
  uop_walk_free(&w);
  return found;
}

static Term rc_peel_casts(Term t) {
  while (term_tag(t) == TAG_UOP && term_ext(t) == UOP_CAST) {
    t = heap_read(term_val(t) + 0);
  }
  return t;
}

// tinygrad's gated-load rule (codegen/simplify.py:125-127): a one-hot SUM over
// r of IWHERE(r + rest == idx, val, 0) reads val at r = idx - rest, gated to
// [0, extent), and the reduce goes.  The gather address is clamped in range too,
// so a renderer that hoists the load never reads past the buffer.  A 64-bit idx
// stays a reduce: as int32 index math it would wrap back into range.  So does an
// idx a REDUCE computes in the kernel (masked_select's cumsum): its accumulator
// renders as float, which cannot subscript a load.
static Term rc_load_collapse_one(Term red) {
  if (uop_reduce_kind(red) != REDUCE_SUM || uop_reduce_n_axes(red) != 1) return 0;
  u32 aid = uop_reduce_axis(red, 0);
  Term body = uop_reduce_src(red);
  if (term_tag(body) != TAG_UOP || term_ext(body) != UOP_IWHERE) return 0;
  u64 bloc = term_val(body);
  Term cond = rc_peel_casts(heap_read(bloc + 0));
  Term val = heap_read(bloc + 1);
  Term zero = heap_read(bloc + 2);
  i32 zv;
  if (!(term_tag(zero) == TAG_UOP && term_ext(zero) == UOP_INVALID) && !(rc_const_i32(zero, &zv) && zv == 0)) {
    return 0;
  }
  if (term_tag(cond) != TAG_UOP || term_ext(cond) != UOP_CMPEQ) return 0;
  Term x = heap_read(term_val(cond) + 0);
  Term y = heap_read(term_val(cond) + 1);
  if (rc_uses_axis(y, aid)) {
    Term s = x; x = y; y = s;
  }
  u32 ydt;
  if (rc_uses_axis(y, aid) || !term_dtype_in(y, 0, &ydt) || !dtype_is_int(ydt)
      || ydt == DT_INT64 || ydt == DT_UINT64 || rc_has_reduce(y)) {
    return 0;
  }
  Term rng, rest;
  if (!rc_split_affine(x, aid, &rng, &rest)) return 0;
  u32 ext = uop_range_extent(rng);
  if (ext == 0 || ext > (1u << 24)) return 0;
  Term idx = rest == 0 ? y : uop_int_binary(UOP_ISUB, y, rest);
  Term safe = rc_clamp_0_hi(idx, ext - 1);
  Term read = uop_substitute(val, &rng, &safe, 1);
  if (rc_uses_axis(read, aid)) return 0;
  Term in_hi = uop_where(uop_int_binary(UOP_ILT, idx, uop_const(DT_INT32, ext)), read, zero);
  return uop_where(uop_int_binary(UOP_ILT, idx, uop_const(DT_INT32, 0)), zero, in_hi);
}

static Term rc_fold(Term red, int load);

// tinygrad's REDUCE on ADD (codegen/simplify.py:112-114) inside reduce_collapse,
// which keeps the split only when no range is left (simplify.py:141): a SUM
// over a + b folds when both halves do.
static Term rc_split_add(Term red, int load) {
  if (uop_reduce_kind(red) != REDUCE_SUM) return 0;
  Term body = uop_reduce_src(red);
  if (term_tag(body) != TAG_UOP || term_ext(body) != UOP_ADD) return 0;
  Term half[2];
  for (u32 i = 0; i < 2; i++) {
    Term src = heap_read(term_val(body) + i);
    half[i] = rc_fold(uop_substitute(red, &body, &src, 1), load);
    if (half[i] == 0) return 0;
  }
  return uop_binary(UOP_ADD, half[0], half[1]);
}

// The folded form of `red`, or 0 when no rule folds it; `load` adds the
// gated-load rule.
static Term rc_fold(Term red, int load) {
  if (term_tag(red) != TAG_UOP || term_ext(red) != UOP_REDUCE) return 0;
  Term folded = rc_collapse_one(red);
  if (folded == 0 && load) folded = rc_load_collapse_one(red);
  if (folded == 0) folded = rc_split_add(red, load);
  return folded;
}

static Term rc_collapse_rule(Term t, void *load) {
  Term folded = rc_fold(t, load != NULL);
  return folded != 0 ? folded : t;
}

static Term rc_collapse_store(Term store, void *load) {
  UOpGraphRewriteRule rules[] = {
    { "reduce-collapse", rc_collapse_rule },
  };
  return uop_graph_rewrite(store, rules, 1, load);
}

// `root` is a kernel's store_root (a STORE, or an AFTER chain of them); returns
// it with every foldable REDUCE in its stores collapsed: tinygrad's
// pm_reduce_simplify (codegen/simplify.py:147-150, applied in
// schedule/rangeify.py:606).
fn Term uop_reduce_arange_collapse(Term root) {
  if (root == 0) return root;
  return uop_root_map_stores(root, rc_collapse_store, NULL);
}

// The same plus pm_load_collapse (codegen/simplify.py:153-157), which tinygrad
// runs in codegen: it rewrites load addresses, so it waits for the walker
// rewrite, whose per-axis table is keyed by the INDEX_E nodes rangeify built.
fn Term uop_reduce_load_collapse(Term root) {
  static int load = 1;
  if (root == 0) return root;
  return uop_root_map_stores(root, rc_collapse_store, &load);
}
