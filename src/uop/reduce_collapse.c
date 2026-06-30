// uop/reduce_collapse.c - closed-form collapse of REDUCE(SUM) over a
// pure RANGE.  Faithful port of tinygrad's arange/reduce-collapse
// (tinygrad/codegen/simplify.py:78-150 pm_reduce_simplify ->
// reduce_unparented + the pm_reduce_collapse "bound" rules), specialised
// to the post-rangeify thvm form where a UOP_REDUCE carries a numeric
// reduce axis and its body references the matching UOP_RANGE leaf.
//
// The arange / cumsum / one-hot / RoPE-table construction lowers to a
// triangular-mask reduce that tinygrad collapses to a const ramp BEFORE
// kernel-split, emitting ZERO kernels.  thvm (this rule absent) iterated
// the reduce axis as a real loop, materialising an N-element array + a
// reduce loop where tinygrad folds.  This rule reproduces tinygrad's
// closed form so the same construction renders without the inner
// reduce-axis loop.
//
// Two faithful rules, mirroring tinygrad simplify.py:
//
//  (1) reduce_unparented (simplify.py:78-88): a REDUCE(SUM) over an axis
//      whose RANGE the body never references is `body * extent`.
//
//  (2) reduce bound-collapse (simplify.py:103-111): a REDUCE(SUM, r) of
//      IWHERE(cmp(affine(r)), val, 0) where `val` does not reference `r`.
//      The sum counts the iters of r where the guard holds, times val.
//      thvm's PAD valid-mask renders the guard as ILT(a, b); we recognise
//      the two affine-in-r shapes:
//        ILT(C, r + rest)  (r > C-rest)  -> upper-tail count
//        ILT(r + rest, C)  (r < C-rest)  -> lower-tail count
//      and fold the iter count to a clamped affine of the non-r terms.
//
// Both are exact integer identities.  When the pattern doesn't match the
// rule returns 0 and the caller keeps the literal reduce loop, so this is
// a pure narrowing optimisation with no correctness exposure.

// True iff `t`'s DAG references a free UOP_RANGE leaf with axis_id == aid.
// Stops at nested UOP_REDUCE that bind `aid` (none here, but faithful).
static int rc_uses_axis(Term t, u32 aid, u32 depth) {
  if (depth > 256) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) return (u32)term_val(heap_read(loc + 0)) == aid;
  if (op == UOP_BUFFER || op == UOP_BUFFERIZE || op == UOP_KERNEL) return 0;
  if (op == UOP_REDUCE) {
    Term tred = term_new(0, TAG_UOP, UOP_REDUCE, loc);
    u32 n_axes = uop_reduce_n_axes(tred);
    for (u32 i = 0; i < n_axes; i++)
      if (uop_reduce_axis(tred, i) == aid) return 0;   // bound by inner reduce
    return rc_uses_axis(uop_reduce_src(tred), aid, depth + 1);
  }
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar; i++)
    if (rc_uses_axis(heap_read(loc + i), aid, depth + 1)) return 1;
  return 0;
}

// Read a UOP_CONST integer payload.  Returns 1 + sets *out on success.
static int rc_const_i32(Term t, i32 *out) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONST) return 0;
  u64 loc = term_val(t);
  Term num = heap_read(loc + 0);
  if (term_tag(num) != TAG_NUM) return 0;
  *out = (i32)term_val(num);
  return 1;
}

// Find the UOP_RANGE leaf with axis_id == aid inside an affine sum tree
// `t` of the form (RANGE(aid) [+ rest...]).  Splits `t` into the RANGE
// term (returned in *rng) and the residual `rest` (everything else,
// must not reference aid).  Only handles a single top-level IADD chain
// containing exactly one RANGE(aid) leaf and an aid-free remainder.
// Returns 1 on success.  `rest` is 0 (sentinel "no residual") when the
// affine is the bare range.
static int rc_split_affine(Term t, u32 aid, Term *rng, Term *rest) {
  // Bare RANGE leaf.
  if (term_tag(t) == TAG_UOP && term_ext(t) == UOP_RANGE
      && (u32)term_val(heap_read(term_val(t) + 0)) == aid) {
    *rng = t; *rest = 0; return 1;
  }
  // IADD(a, b): exactly one side carries the range, the other is aid-free.
  if (term_tag(t) == TAG_UOP && term_ext(t) == UOP_IADD) {
    Term a = heap_read(term_val(t) + 0);
    Term b = heap_read(term_val(t) + 1);
    int a_has = rc_uses_axis(a, aid, 0);
    int b_has = rc_uses_axis(b, aid, 0);
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

// clamp(x, 0, hi) == max(0, min(x, hi)) on integer Terms.
static Term rc_clamp_0_hi(Term x, u32 hi) {
  Term hic = uop_const(DT_INT32, hi);
  // min(x, hi) = (x < hi) ? x : hi
  Term lt_hi = uop_int_binary(UOP_ILT, x, hic);
  Term minned = uop_iwhere(lt_hi, x, hic);
  // max(0, minned) = (minned < 0) ? 0 : minned
  Term zero = uop_const(DT_INT32, 0);
  Term lt_0 = uop_int_binary(UOP_ILT, minned, zero);
  return uop_iwhere(lt_0, zero, minned);
}

// Core: try to collapse `red` (a UOP_REDUCE SUM with a single axis) into
// a closed form.  Returns the folded Term, or 0 if no rule matched.
static Term rc_collapse_one(Term red) {
  if (term_tag(red) != TAG_UOP || term_ext(red) != UOP_REDUCE) return 0;
  if (uop_reduce_kind(red) != REDUCE_SUM) return 0;
  if (uop_reduce_n_axes(red) != 1) return 0;          // single-axis only
  u32 aid = uop_reduce_axis(red, 0);
  Term body = uop_reduce_src(red);

  // Locate the reduce axis's RANGE leaf to read its static extent.  The
  // axis_id is the numeric reduce axis; scan the body for RANGE(aid).
  // (Used only for the extent; the affine split re-finds it.)
  Term range_leaf = 0;
  {
    // Small DFS for the first RANGE(aid).
    Term stack[256]; u32 sp = 0; stack[sp++] = body;
    while (sp > 0 && range_leaf == 0) {
      Term t = stack[--sp];
      if (term_tag(t) != TAG_UOP) continue;
      u32 op = term_ext(t); u64 loc = term_val(t);
      if (op == UOP_RANGE) {
        if ((u32)term_val(heap_read(loc + 0)) == aid) range_leaf = t;
        continue;
      }
      if (op == UOP_BUFFER || op == UOP_BUFFERIZE || op == UOP_KERNEL) continue;
      u8 ar = uop_arity(op);
      for (u8 i = 0; i < ar && sp < 256; i++) stack[sp++] = heap_read(loc + i);
    }
  }

  // Rule (1): reduce_unparented -- body never references aid.
  if (range_leaf == 0) {
    // No RANGE(aid) in body: sum of a constant body over `ext` iters.
    // We can only fold when we know the static extent; without the leaf
    // we cannot recover it here.  Decline (the loop is already trivial).
    return 0;
  }
  u32 ext = uop_range_extent(range_leaf);
  if (ext == 0 || ext > (1u << 24)) return 0;          // guard absurd extents

  // Rule (2): bound-collapse of IWHERE(cmp(affine(r)), val, 0).
  if (term_ext(body) != UOP_IWHERE) return 0;
  u64 bloc = term_val(body);
  Term cond = heap_read(bloc + 0);
  Term then_v = heap_read(bloc + 1);
  Term else_v = heap_read(bloc + 2);
  // `val` must NOT reference the reduce axis.
  if (rc_uses_axis(then_v, aid, 0)) return 0;
  // else must be 0 / INVALID (both render as 0 and are the SUM identity).
  int else_zero = 0;
  if (term_tag(else_v) == TAG_UOP) {
    if (term_ext(else_v) == UOP_INVALID) else_zero = 1;
    else { i32 ev; if (rc_const_i32(else_v, &ev) && ev == 0) else_zero = 1; }
  }
  if (!else_zero) return 0;
  // Guard must be ILT.
  if (term_tag(cond) != TAG_UOP || term_ext(cond) != UOP_ILT) return 0;
  Term lhs = heap_read(term_val(cond) + 0);
  Term rhs = heap_read(term_val(cond) + 1);
  int lhs_has = rc_uses_axis(lhs, aid, 0);
  int rhs_has = rc_uses_axis(rhs, aid, 0);
  if (lhs_has == rhs_has) return 0;                    // need exactly one side

  // Count of r in [0, ext) satisfying the guard, times `then_v`.
  Term count = 0;
  if (rhs_has) {
    // ILT(C_side, r + rest)  <=>  r > C_side - rest  <=>  r >= (C - rest) + 1
    // upper-tail count = ext - clamp((C - rest) + 1, 0, ext)
    Term rng, rest;
    if (!rc_split_affine(rhs, aid, &rng, &rest)) return 0;
    (void)rng;
    // threshold = lhs - rest + 1   (first r that is included)
    Term thr = (rest == 0) ? lhs : uop_int_binary(UOP_ISUB, lhs, rest);
    thr = uop_int_binary(UOP_IADD, thr, uop_const(DT_INT32, 1));
    Term clamped = rc_clamp_0_hi(thr, ext);
    count = uop_int_binary(UOP_ISUB, uop_const(DT_INT32, ext), clamped);
  } else {
    // ILT(r + rest, C_side)  <=>  r < C_side - rest
    // lower-tail count = clamp(C - rest, 0, ext)
    Term rng, rest;
    if (!rc_split_affine(lhs, aid, &rng, &rest)) return 0;
    (void)rng;
    Term thr = (rest == 0) ? rhs : uop_int_binary(UOP_ISUB, rhs, rest);
    count = rc_clamp_0_hi(thr, ext);
  }
  if (count == 0) return 0;

  // result = then_v * count.  then_v is aid-free; for the canonical
  // arange it is iconst(1) so the MUL folds to `count`.
  return uop_int_binary(UOP_IMUL, then_v, count);
}

// Bottom-up rewrite: collapse every foldable single-axis SUM-reduce in
// the DAG rooted at `root`.  Rebuilds parents around folded children via
// the simplifying constructors (which hash-cons).  Returns the rewritten
// root (== root when nothing folded).  Bounded recursion; a small memo
// avoids re-walking shared sub-DAGs.
// Open-addressed memo (Term key -> rewritten Term).  Was a LINEAR-scan array:
// rc_memo_get scanned all `n` entries per node, so visiting K nodes cost O(K^2)
// -- the dominant deep-graph JIT-capture cost (per-kernel store_root of O(depth)
// nodes, rewritten for every kernel: 0.98 ms @ 4 blocks -> 7.5 ms @ 8 blocks in
// profiling, ~O(N^2.7)).  A hash makes each lookup O(1).  Key 0 = empty (the
// caller never memoizes a 0 Term: rc_rewrite_rec returns early on non-UOP and
// guards root==0).  Power-of-2 capacity; mask-index open addressing.  At load
// the table just stops memoizing (correct, slow -- the prior >CAP behavior).
// Open-addressed memo (Term key -> rewritten Term), generation-stamped so the
// per-call reset is O(1) (bump `cur_gen`) instead of memset'ing the whole
// table.  A slot is "live" iff slot_gen[i] == m->cur_gen.  This replaces the
// old LINEAR-scan array whose rc_memo_get scanned all `n` entries per node
// (O(K^2) over a K-node store_root; the dominant deep-graph JIT-capture cost:
// 0.98 ms @ 4 blocks -> 7.5 ms @ 8 blocks before this).  Key 0 never memoized
// (caller guards root==0; non-UOP returns early).  Power-of-2 capacity.
#define RC_MEMO_CAP 16384            /* power of 2 */
typedef struct {
  u64 key[RC_MEMO_CAP];
  Term val[RC_MEMO_CAP];
  u32 slot_gen[RC_MEMO_CAP];
  u32 cur_gen;
  u32 n;
} RcMemo;
static inline u32 rc_memo_hash(Term t) {
  u64 v = (u64)t;
  v ^= v >> 33; v *= 0xff51afd7ed558ccdULL;
  v ^= v >> 33; v *= 0xc4ceb9fe1a85ec53ULL;
  v ^= v >> 33;
  return (u32)v & (RC_MEMO_CAP - 1);
}
static Term rc_memo_get(RcMemo *m, Term t, int *hit) {
  u32 h = rc_memo_hash(t);
  for (u32 probe = 0; probe < RC_MEMO_CAP; probe++) {
    u32 i = (h + probe) & (RC_MEMO_CAP - 1);
    if (m->slot_gen[i] != m->cur_gen) { *hit = 0; return 0; }  // empty (this gen)
    if (m->key[i] == (u64)t) { *hit = 1; return m->val[i]; }
  }
  *hit = 0; return 0;
}
static void rc_memo_put(RcMemo *m, Term t, Term v) {
  if (m->n * 2 >= RC_MEMO_CAP) return;                 // keep load < 0.5: stop memoizing
  u32 h = rc_memo_hash(t);
  for (u32 probe = 0; probe < RC_MEMO_CAP; probe++) {
    u32 i = (h + probe) & (RC_MEMO_CAP - 1);
    if (m->slot_gen[i] != m->cur_gen) {                // empty (this gen): claim
      m->key[i] = (u64)t; m->val[i] = v;
      m->slot_gen[i] = m->cur_gen; m->n++; return;
    }
    if (m->key[i] == (u64)t) { m->val[i] = v; return; }  // update
  }
}

static Term rc_rewrite_rec(Term t, RcMemo *m, u32 depth) {
  if (depth > 512) return t;
  if (term_tag(t) != TAG_UOP) return t;
  int hit; Term cached = rc_memo_get(m, t, &hit);
  if (hit) return cached;
  u32 op = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_BUFFER || op == UOP_KERNEL) { rc_memo_put(m, t, t); return t; }

  // Try to collapse this node if it's a reduce; recurse into the result
  // (a folded reduce becomes pure index arithmetic with no further
  // reduce, but the recursion keeps the contract uniform).
  if (op == UOP_REDUCE) {
    Term folded = rc_collapse_one(t);
    if (folded != 0 && folded != t) {
      Term r = rc_rewrite_rec(folded, m, depth + 1);
      rc_memo_put(m, t, r);
      return r;
    }
  }

  // Generic: rebuild children, then re-apply the right constructor so the
  // hash-cons / simplifying folds settle.  We only need to rebuild the
  // op families that can carry a reduce in their subtree.
  u8 ar = uop_arity(op);
  if (ar == 0) { rc_memo_put(m, t, t); return t; }
  Term children[8];
  int changed = 0;
  for (u8 i = 0; i < ar && i < 8; i++) {
    Term c = heap_read(loc + i);
    Term nc = rc_rewrite_rec(c, m, depth + 1);
    children[i] = nc;
    if (nc != c) changed = 1;
  }
  if (!changed) { rc_memo_put(m, t, t); return t; }

  Term out = t;
  switch (op) {
    case UOP_STORE:
      out = uop_store(children[0], children[1], children[2]);
      break;
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
      out = uop_int_binary(op, children[0], children[1]);
      break;
    case UOP_IWHERE:
      out = uop_iwhere(children[0], children[1], children[2]);
      break;
    case UOP_REDUCE: {
      u32 n_axes = uop_reduce_n_axes(t);
      u32 axes[MAX_DIM];
      for (u32 i = 0; i < n_axes && i < MAX_DIM; i++) axes[i] = uop_reduce_axis(t, i);
      out = uop_reduce_multi(uop_reduce_kind(t), n_axes, axes, children[0]);
      break;
    }
    case UOP_CAST: {
      // CAST heap layout: [src, NUM(dst_dtype)].
      u32 dst_dtype = (u32)term_val(heap_read(loc + 1));
      out = uop_cast(children[0], dst_dtype);
      break;
    }
    default:
      // Op we don't rebuild (movement / INDEX_E / etc.): keep original.
      // A reduce buried under one of these still folds on its own walk
      // because rc_rewrite_rec recursed into the child above; only the
      // wrapper Term identity is preserved here.
      out = t;
      break;
  }
  rc_memo_put(m, t, out);
  return out;
}

// Public entry: collapse arange/reduce store values.  `root` is the
// kernel store_root (UOP_STORE(...)) from kernel_lift; returns a
// rewritten root with foldable single-axis SUM-reduces replaced by their
// closed forms.  Gated OFF by THVM_NO_REDUCE_COLLAPSE=1 for A/B.
fn Term uop_reduce_arange_collapse(Term root) {
  static int known = 0, enabled = 1;
  if (!known) {
    char const *e = getenv("THVM_NO_REDUCE_COLLAPSE");
    enabled = !(e != NULL && e[0] == '1');
    known = 1;
  }
  if (!enabled) return root;
  if (root == 0) return root;
  // File-scope static (single-threaded per realize; ~384 KB, too big for the
  // stack).  Zero-initialized once at load (slot_gen all 0, cur_gen 0); each
  // call bumps cur_gen so all prior stamps read as empty -- an O(1) reset.
  static RcMemo memo;
  memo.n = 0;
  if (++memo.cur_gen == 0) {            // wrap (astronomically rare): hard-clear
    memset(memo.slot_gen, 0, sizeof(memo.slot_gen));
    memo.cur_gen = 1;
  }
  return rc_rewrite_rec(root, &memo, 0);
}
