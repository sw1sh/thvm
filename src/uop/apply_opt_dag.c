// uop/apply_opt_dag.c -- DAG-side apply_opt, the Phase E counterpart
// to apply_opt.c's legacy KpSchedule.applied_opts[] mutation.  Each
// helper takes a UOp DAG root + opt parameters and returns a new
// root with the equivalent transformation applied.  Operates purely
// on the UOp DAG -- no KpSchedule, no tile_uops side-channel.
//
// Initial slice covers the two opts that matter for the matmul
// autotune loop: KOP_TC (tile-size selection for the simdgroup_matrix
// template) and KOP_GLOBAL (axis-type swap to drop the
// `if(sgi==0u && tg==0u)` guard in render_uop's TC emission).
// KOP_UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP/SWAP land in subsequent
// slices.
//
// Wired into kernel_apply_opt's outer dispatcher: when
// `ke->cached_lift.store_root != 0` we take the DAG path here;
// otherwise we fall through to the legacy schedule path.

// ---------- KOP_TC ---------------------------------------------------
// Wrap (or update) the OPT(_, TC, factor) marker around the inner
// REDUCE that lives in STORE.value.  Three layouts to handle:
//   (a) STORE(buf, addr, REDUCE(...))        -- bare matmul, no OPT.
//                                               Wrap REDUCE with OPT.
//   (b) STORE(buf, addr, OPT(REDUCE, TC, _)) -- already TC-marked.
//                                               Replace factor.
//   (c) anything else                        -- not a matmul we
//                                               recognise; bail.
//
// Returns the new STORE root, or 0 on bail.

fn Term uop_dag_apply_tc(Term root, u32 factor) {
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;
  u64 sloc = term_val(root);
  Term buf   = heap_read(sloc + 0);
  Term addr  = heap_read(sloc + 1);
  Term value = heap_read(sloc + 2);

  Term new_value = 0;
  if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_REDUCE) {
    // Layout (a): bare REDUCE.
    new_value = uop_opt(value, UOP_OPT_TC, factor);
  } else if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT
             && uop_opt_kind(value) == UOP_OPT_TC) {
    // Layout (b): already wrapped; rebuild with new factor.
    Term inner = uop_opt_target(value);
    if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_REDUCE) return 0;
    new_value = uop_opt(inner, UOP_OPT_TC, factor);
  } else if (ru_fuse_epilogue_on()) {
    // Layout (c): THVM_FUSE_MATMUL_EPILOGUE -- the store value is an elementwise
    // epilogue chain over a SINGLE matmul REDUCE (the scheduler un-realized the
    // matmul into the consuming gate/modulate).  Wrap that nested REDUCE in
    // OPT_TC in place, keeping the chain, so hand-opts (Section 1) treats the
    // gate kernel as a matmul -- KOP_TC + M/N GLOBAL on the un-split DAG, no
    // elementwise UPCAST tile-split (which would 3-range-decompose the operands
    // and lose the TILED path).  rec_tc_find_epilogue_reduce rejects any
    // movement/second-reduce, so the wrapped form is exactly what the codegen
    // epilogue (render_uop.c rmu_detect_matmul_tc) walks.  rec_tc_wrap_epilogue_-
    // reduce returns the rebuilt STORE (or `root` if nothing changed); honour the
    // factor by re-wrapping with the chosen tile size.
    Term red = rec_tc_find_epilogue_reduce(value);
    if (red == 0) return 0;
    RecTcSubState st = { red, uop_opt(red, UOP_OPT_TC, factor), {0}, {0}, 0 };
    new_value = rec_tc_sub(&st, value, 0);
  } else {
    return 0;
  }

  if (new_value == value) return root;
  return uop_store(buf, addr, new_value);
}

// ---------- KOP_GLOBAL ----------------------------------------------
// Find every UOP_RANGE leaf with `axis_id == target_axis_id` and
// replace its axis_type with `KAX_GLOBAL` (5).  Hash-cons makes the
// new range a fresh term distinct from the old one; uop_graph_rewrite
// then walks the DAG bottom-up, rebuilding parents whose children
// changed (memoised so each parent rebuilds at most once).
//
// Validity gates:
//   - target axis must currently be axis_type == KAX_LOOP (0).  Don't
//     overwrite REDUCE/UPCAST/UNROLL/LOCAL/GROUP/etc. -- those carry
//     semantic meaning and a GLOBAL re-stamp would break the kernel.
//   - The renderer's TC emission with both m & n bound to KAX_GLOBAL
//     drops the sgi==0 guard (see render_uop.c rmu_emit_matmul_tc
//     parallel_tc branch); applying GLOBAL to ONE axis emits the
//     half-bound form (one for-loop survives).
//
// Returns the new STORE root, or `root` unchanged if no RANGE matched.

typedef struct {
  u32 target_axis_id;
} ApplyOptDagGlobalCtx;

static Term apply_opt_dag_global_rewrite(Term t, void *user) {
  ApplyOptDagGlobalCtx const *ctx = (ApplyOptDagGlobalCtx const *)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) return t;
  u64 loc = term_val(t);
  u32 axis_id   = (u32)term_val(heap_read(loc + 0));
  u32 axis_type = (u32)term_val(heap_read(loc + 1));
  u32 extent    = (u32)term_val(heap_read(loc + 2));
  if (axis_id != ctx->target_axis_id) return t;
  if (axis_type != KAX_LOOP) return t;
  return uop_range(axis_id, KAX_GLOBAL, extent);
}

fn Term uop_dag_apply_global(Term root, u32 axis_id) {
  ApplyOptDagGlobalCtx ctx = { axis_id };
  UOpGraphRewriteRule rules[] = {
    { "apply-opt-dag-global", apply_opt_dag_global_rewrite },
  };
  return uop_graph_rewrite(root, rules, 1, &ctx);
}

// ---------- KOP_UPCAST/UNROLL/LOCAL/GROUP/GROUPTOP (split) ----------
// (KOP_SWAP defined at end of file; reuses split's substitution-map
//  scaffolding.)
// Split the UOP_RANGE leaf at `target_axis` into outer (axis=target,
// LOOP, extent=E/k) + inner (axis=target+1, inner_kax, extent=k).
// All leaves at axis_id > target shift right by 1.  Substitute the
// linear index `IADD(IMUL(outer, k), inner_wrapped)` for any reference
// to the pre-split leaf via Term identity (hash-cons dedups all
// references to the same leaf).
//
// inner_kax is one of KAX_UPCAST/UNROLL/LOCAL/GROUP_REDUCE.
// opt_kind   is UOP_OPT_UPCAST/UNROLL/GROUP_REDUCE (or 0xFF for LOCAL,
// which doesn't wrap inner in UOP_OPT).
//
// Validity: `target_axis` must currently be a single bare UOP_RANGE
// leaf in the DAG with extent % k == 0.  Returns 0 on bail.

// Substitution map: keys are pre-existing Term IDs, values are their
// post-rewrite replacements.  The map only contains ORIGINAL terms;
// the rule never re-fires on its own outputs because the outputs are
// not keys.  Cap is comfortable for typical kernels (one origin per
// axis + one shift per existing axis).
#define APPLY_OPT_DAG_SPLIT_MAP_CAP 64

typedef struct {
  Term key;
  Term val;
} ApplyOptDagSplitMapEntry;

typedef struct {
  ApplyOptDagSplitMapEntry entries[APPLY_OPT_DAG_SPLIT_MAP_CAP];
  u32 n;
} ApplyOptDagSplitCtx;

// Manual single-pass substitution walker.  Avoids uop_graph_rewrite's
// recurse-on-rule-output semantics which causes infinite recursion when
// substitution targets are bidirectional (e.g. SWAP swaps A and B,
// where mapped(A) == B and mapped(B) == A; the rewriter would loop).
//
// Each pre-existing Term is visited at most once via a Term→Term memo.
// A mapped value is returned as-is (NOT recursed into) since by
// construction the values are pre-existing Terms unrelated to the
// substitution shape.
#define APPLY_OPT_DAG_SUB_MEMO_CAP 1024

typedef struct {
  Term key;
  Term val;
} ApplyOptDagSubMemoEntry;

typedef struct {
  ApplyOptDagSubMemoEntry memo[APPLY_OPT_DAG_SUB_MEMO_CAP];
  u32 memo_n;
  ApplyOptDagSplitCtx const *map;
  // UOP_REDUCE.axis fix-up: the substitution map only rewrites
  // UOP_RANGE *leaves*, but a REDUCE node names its reduce axis by a
  // bare integer in heap slot +2.  When an axis split shifts every
  // axis_id > target right by one (KOP_UPCAST/UNROLL/LOCAL/GROUP), the
  // REDUCE's axis must follow suit or the renderer's reduce-range
  // matcher (rmu_split_reduce) stops finding it and falls back to the
  // generic-store path -> the UPCAST'd inner axis gets mistaken for
  // the reduce loop -> MSL "use of undeclared identifier".
  //   reduce_shift_above == 0xFFFFFFFFu  -> no shift (SWAP / standalone)
  //   else                               -> red_axis > thr ? red_axis+1
  u32 reduce_shift_above;
  // SWAP: also re-map a REDUCE whose axis equals one of the swapped
  // positions (sentinel 0xFFFFFFFFu when unused).
  u32 reduce_swap_a;
  u32 reduce_swap_b;
  // Dense renumber (uop_dag_renumber_axes): a full old_id -> new_id
  // table applied to REDUCE axis fields (RANGE leaves are remapped via
  // the `map` entries).  remap_n == 0 disables (shift/swap path used
  // instead).  When active, reduce_shift_above / reduce_swap_* are
  // sentinels and only the table applies.
  u32 const *remap_old;
  u32 const *remap_new;
  u32        remap_n;
} ApplyOptDagSubState;

static int apply_opt_dag_sub_memo_lookup(ApplyOptDagSubState *st, Term t,
                                         Term *out) {
  for (u32 i = 0; i < st->memo_n; i++) {
    if (st->memo[i].key == t) { *out = st->memo[i].val; return 1; }
  }
  return 0;
}

static void apply_opt_dag_sub_memo_insert(ApplyOptDagSubState *st, Term k,
                                          Term v) {
  if (st->memo_n < APPLY_OPT_DAG_SUB_MEMO_CAP) {
    st->memo[st->memo_n].key = k;
    st->memo[st->memo_n].val = v;
    st->memo_n++;
  }
}

static Term apply_opt_dag_substitute(Term t, ApplyOptDagSubState *st);

static Term apply_opt_dag_sub_uncached(Term t, ApplyOptDagSubState *st) {
  // Direct map hit: return mapped value AS-IS (no recursion).
  for (u32 i = 0; i < st->map->n; i++) {
    if (st->map->entries[i].key == t) return st->map->entries[i].val;
  }
  if (term_tag(t) != TAG_UOP) return t;
  u32 op = term_ext(t);
  // Leaf opcodes: no children to recurse into.
  if (op == UOP_RANGE || op == UOP_BUFFER || op == UOP_CONST
      || op == UOP_INVALID) return t;
  if (op == UOP_OPT) {
    Term tgt = uop_opt_target(t);
    Term new_tgt = apply_opt_dag_substitute(tgt, st);
    if (new_tgt == tgt) return t;
    return uop_opt(new_tgt, uop_opt_kind(t), uop_opt_factor(t));
  }
  if (op == UOP_REDUCE) {
    // Rebuild with the (possibly remapped) reduce axes -- see the
    // long comment on ApplyOptDagSubState.reduce_shift_above.
    // Multi-axis: shift/swap each axis independently.
    u32 kind   = uop_reduce_kind(t);
    u32 n_axes = uop_reduce_n_axes(t);
    u32 new_axes[MAX_DIM];
    int axes_changed = 0;
    for (u32 i = 0; i < n_axes; i++) {
      u32 ax = uop_reduce_axis(t, i);
      u32 mapped = ax;
      if (st->remap_n > 0) {
        // Dense-renumber table remap (takes precedence; shift/swap are
        // sentinels in this mode).  Unlisted ids keep their value
        // (defensive: a well-formed renumber lists every axis).
        for (u32 m = 0; m < st->remap_n; m++) {
          if (st->remap_old[m] == ax) { mapped = st->remap_new[m]; break; }
        }
      } else {
        if (st->reduce_shift_above != 0xFFFFFFFFu
            && ax > st->reduce_shift_above) {
          mapped = ax + 1;
        }
        if (st->reduce_swap_a != 0xFFFFFFFFu) {
          if (ax == st->reduce_swap_a)      mapped = st->reduce_swap_b;
          else if (ax == st->reduce_swap_b) mapped = st->reduce_swap_a;
        }
      }
      new_axes[i] = mapped;
      if (mapped != ax) axes_changed = 1;
    }
    Term old_src = uop_reduce_src(t);
    Term new_src = (term_tag(old_src) == TAG_UOP)
                   ? apply_opt_dag_substitute(old_src, st)
                   : old_src;
    if (new_src == old_src && !axes_changed) return t;
    return uop_reduce_multi(kind, n_axes, new_axes, new_src);
  }
  u8 ar = uop_arity(op);
  if (ar == 0) return t;
  Term srcs[MAX_UOP_SRC];
  int changed = 0;
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old = heap_read(loc + i);
    Term ne  = (term_tag(old) == TAG_UOP)
               ? apply_opt_dag_substitute(old, st)
               : old;
    srcs[i] = ne;
    if (ne != old) changed = 1;
  }
  if (!changed) return t;
  return uop_graph_rebuild_with_srcs(t, srcs);
}

static Term apply_opt_dag_substitute(Term t, ApplyOptDagSubState *st) {
  Term hit;
  if (apply_opt_dag_sub_memo_lookup(st, t, &hit)) return hit;
  Term out = apply_opt_dag_sub_uncached(t, st);
  apply_opt_dag_sub_memo_insert(st, t, out);
  return out;
}

// Walk DAG, find the unique UOP_RANGE leaf at target_axis.  Returns 0
// if none found or if multiple distinct leaves at the same axis_id
// exist (the lifter normally produces one).
static Term apply_opt_dag_find_range(Term root, u32 target_axis) {
  if (root == 0) return 0;
  Term stack[256];
  u32  sp = 0;
  stack[sp++] = root;
  Term found = 0;
  while (sp > 0) {
    Term t = stack[--sp];
    if (term_tag(t) != TAG_UOP) continue;
    u32 op = term_ext(t);
    if (op == UOP_RANGE) {
      if (uop_range_axis_id(t) == target_axis) {
        if (found != 0 && found != t) return 0;  // multiple distinct
        found = t;
      }
      continue;
    }
    if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID) continue;
    if (op == UOP_OPT) {
      Term tgt = uop_opt_target(t);
      if (term_tag(tgt) == TAG_UOP && sp < 256) stack[sp++] = tgt;
      continue;
    }
    u8 ar = uop_arity(op);
    u64 loc = term_val(t);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC && sp < 256; i++) {
      Term child = heap_read(loc + i);
      if (term_tag(child) == TAG_UOP) stack[sp++] = child;
    }
  }
  return found;
}

static u8 apply_opt_dag_inner_kax(u8 op) {
  if (op == KOP_UPCAST)   return (u8)KAX_UPCAST;
  if (op == KOP_UNROLL)   return (u8)KAX_UNROLL;
  if (op == KOP_LOCAL)    return (u8)KAX_LOCAL;
  return (u8)KAX_LOOP;
}

static u32 apply_opt_dag_inner_opt_kind(u8 op) {
  if (op == KOP_UPCAST)                              return UOP_OPT_UPCAST;
  if (op == KOP_UNROLL)                              return UOP_OPT_UNROLL;
  return 0xFFu;  // KOP_LOCAL: no UOP_OPT wrap
}

// Collect every UOP_RANGE leaf reachable from root into out[] (deduped
// by Term identity).  Returns the count.
static u32 apply_opt_dag_collect_ranges(Term root, Term *out, u32 cap) {
  if (root == 0) return 0;
  Term stack[256];
  u32  sp = 0;
  stack[sp++] = root;
  u32 n = 0;
  while (sp > 0) {
    Term t = stack[--sp];
    if (term_tag(t) != TAG_UOP) continue;
    u32 op = term_ext(t);
    if (op == UOP_RANGE) {
      // dedupe
      int seen = 0;
      for (u32 i = 0; i < n; i++) { if (out[i] == t) { seen = 1; break; } }
      if (!seen && n < cap) out[n++] = t;
      continue;
    }
    if (op == UOP_BUFFER || op == UOP_CONST || op == UOP_INVALID) continue;
    if (op == UOP_OPT) {
      Term tgt = uop_opt_target(t);
      if (term_tag(tgt) == TAG_UOP && sp < 256) stack[sp++] = tgt;
      continue;
    }
    u8 ar = uop_arity(op);
    u64 loc = term_val(t);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC && sp < 256; i++) {
      Term child = heap_read(loc + i);
      if (term_tag(child) == TAG_UOP) stack[sp++] = child;
    }
  }
  return n;
}

fn Term uop_dag_apply_split(Term root, u8 op, u32 target_axis, u32 k) {
  if (k == 0) return 0;
  Term old_leaf = apply_opt_dag_find_range(root, target_axis);
  if (old_leaf == 0) {
    if (getenv("THVM_HANDOPT_TRACE")) {
      // Distinguish "no range with this id" from "multiple distinct".
      u32 n_match = 0;
      Term rs[APPLY_OPT_DAG_SPLIT_MAP_CAP];
      u32 nr = apply_opt_dag_collect_ranges(root, rs, APPLY_OPT_DAG_SPLIT_MAP_CAP);
      for (u32 i = 0; i < nr; i++) if (uop_range_axis_id(rs[i]) == target_axis) n_match++;
      fprintf(stderr, "[split] BAIL op=%u axis=%u k=%u: find_range=0 (%u matching ranges of %u total)\n",
              op, target_axis, k, n_match, nr);
      fflush(stderr);
    }
    return 0;
  }
  u32 extent = uop_range_extent(old_leaf);
  // A symbolic (kvar) axis packs its id (0x80000000|id) into the extent
  // slot; `extent % k` / `extent / k` here would shred the packed value
  // (0x80000004 / 4 = 0x20000001) into a bogus literal stride/loop bound.
  // tinygrad excludes symbolic dims from upcastable/unrollable_dims, so a
  // well-formed opt never targets one -- bail defensively if one slips in.
  if (kvar_extent_is_var(extent)) {
    if (getenv("THVM_HANDOPT_TRACE"))
      fprintf(stderr, "[split] BAIL op=%u axis=%u k=%u: symbolic kvar extent (id=%u)\n",
              op, target_axis, k, kvar_extent_var_id(extent));
    return 0;
  }
  if (extent % k != 0) {
    if (getenv("THVM_HANDOPT_TRACE"))
      fprintf(stderr, "[split] BAIL op=%u axis=%u k=%u: extent=%u %% k != 0\n",
              op, target_axis, k, extent);
    return 0;
  }
  u32 axis_type_outer = uop_range_axis_type(old_leaf);
  u8  inner_kax       = apply_opt_dag_inner_kax(op);
  u32 opt_kind        = apply_opt_dag_inner_opt_kind(op);

  Term outer = uop_range(target_axis,     axis_type_outer, extent / k);
  Term inner = uop_range(target_axis + 1, inner_kax,       k);
  Term inner_wrapped = (opt_kind != 0xFFu)
                       ? uop_opt(inner, opt_kind, k)
                       : inner;
  Term scaled = uop_int_binary(UOP_IMUL, outer, uop_const(DT_INT32, k));
  Term linear = uop_int_binary(UOP_IADD, scaled, inner_wrapped);

  // Build substitution map: pre-existing leaves → replacement.
  // - old_leaf at target_axis → linear (the IADD/IMUL composition)
  // - any leaf at axis_id > target_axis → same range with axis_id+1
  // The rule lookup is by Term identity; outputs are NOT keys, so the
  // rewriter's recursion on rule outputs naturally fixpoints.
  ApplyOptDagSplitCtx ctx;
  ctx.n = 0;
  ctx.entries[ctx.n].key = old_leaf;
  ctx.entries[ctx.n].val = linear;
  ctx.n++;

  Term ranges[APPLY_OPT_DAG_SPLIT_MAP_CAP];
  u32 n_ranges = apply_opt_dag_collect_ranges(root, ranges,
                                              APPLY_OPT_DAG_SPLIT_MAP_CAP);
  for (u32 i = 0; i < n_ranges && ctx.n < APPLY_OPT_DAG_SPLIT_MAP_CAP; i++) {
    Term r = ranges[i];
    if (r == old_leaf) continue;
    u32 a_id = uop_range_axis_id(r);
    if (a_id <= target_axis) continue;
    u32 a_type = uop_range_axis_type(r);
    u32 a_ext  = uop_range_extent(r);
    ctx.entries[ctx.n].key = r;
    ctx.entries[ctx.n].val = uop_range(a_id + 1, a_type, a_ext);
    ctx.n++;
  }

  ApplyOptDagSubState st;
  st.memo_n            = 0;
  st.map               = &ctx;
  st.reduce_shift_above = target_axis;     // red_axis > target -> +1
  st.reduce_swap_a     = 0xFFFFFFFFu;
  st.reduce_swap_b     = 0xFFFFFFFFu;
  st.remap_n = 0;
  return apply_opt_dag_substitute(root, &st);
}

// ---------- KOP_GROUP / KOP_GROUPTOP --------------------------------
// Stamp the target UOP_RANGE leaf (which must be a KAX_REDUCE axis) as
// KAX_GROUP_REDUCE and wrap every reference in OPT(_, GROUP_REDUCE, k)
// so the renderer's rmu_emit_store_reduce sees the OPT annotation on
// the reduce axis and dispatches to rmu_emit_group_reduce (cooperative
// shared-memory accumulator + barrier + final per-thread fold).
//
// Unlike UPCAST/UNROLL/LOCAL this is NOT an axis-id split: the existing
// REDUCE node keeps naming the same axis_id, the cooperative `k`
// threads stride through the full reduce extent.  This matches the
// thvm rmu_emit_group_reduce template which iterates the full
// red_extent in strides of `k`.
//
// Validity: target axis_id must currently be a KAX_REDUCE leaf with
// extent % k == 0.  Returns 0 on bail.

fn Term uop_dag_apply_group_reduce(Term root, u32 target_axis, u32 k) {
  if (k == 0) return 0;
  Term old_leaf = apply_opt_dag_find_range(root, target_axis);
  if (old_leaf == 0) return 0;
  if (uop_range_axis_type(old_leaf) != KAX_REDUCE) return 0;
  u32 extent = uop_range_extent(old_leaf);
  if (extent == 0 || extent % k != 0) return 0;

  Term new_leaf = uop_range(target_axis, KAX_GROUP_REDUCE, extent);
  Term wrapped  = uop_opt(new_leaf, UOP_OPT_GROUP_REDUCE, k);

  ApplyOptDagSplitCtx ctx;
  ctx.n = 1;
  ctx.entries[0].key = old_leaf;
  ctx.entries[0].val = wrapped;

  ApplyOptDagSubState st;
  st.memo_n             = 0;
  st.map                = &ctx;
  st.reduce_shift_above = 0xFFFFFFFFu;  // no axis-id shift (single-axis stamp)
  st.reduce_swap_a      = 0xFFFFFFFFu;
  st.reduce_swap_b      = 0xFFFFFFFFu;
  st.remap_n = 0;
  return apply_opt_dag_substitute(root, &st);
}

// ---------- KOP_SWAP ------------------------------------------------
// Swap axis_ids of every UOP_RANGE leaf at positions (a, b).  Both
// positions must already exist in the DAG (otherwise no-op).  Reuses
// the split-machinery's substitution-map idempotence: outputs are NOT
// keys, so the rewriter's recursion on rule outputs naturally fixpoints.
//
// Returns the new root, or `root` unchanged if no RANGE matched.

fn Term uop_dag_apply_swap(Term root, u32 axis_a, u32 axis_b) {
  if (axis_a == axis_b) return root;
  Term ranges[APPLY_OPT_DAG_SPLIT_MAP_CAP];
  u32 n_ranges = apply_opt_dag_collect_ranges(root, ranges,
                                              APPLY_OPT_DAG_SPLIT_MAP_CAP);
  ApplyOptDagSplitCtx ctx;
  ctx.n = 0;
  for (u32 i = 0; i < n_ranges && ctx.n < APPLY_OPT_DAG_SPLIT_MAP_CAP; i++) {
    Term r = ranges[i];
    u32 a_id = uop_range_axis_id(r);
    if (a_id != axis_a && a_id != axis_b) continue;
    u32 new_id = (a_id == axis_a) ? axis_b : axis_a;
    u32 a_type = uop_range_axis_type(r);
    u32 a_ext  = uop_range_extent(r);
    ctx.entries[ctx.n].key = r;
    ctx.entries[ctx.n].val = uop_range(new_id, a_type, a_ext);
    ctx.n++;
  }
  if (ctx.n == 0) return root;
  ApplyOptDagSubState st;
  st.memo_n            = 0;
  st.map               = &ctx;
  st.reduce_shift_above = 0xFFFFFFFFu;     // no +1 shift on swap
  st.reduce_swap_a     = axis_a;           // but follow the swap
  st.reduce_swap_b     = axis_b;
  st.remap_n = 0;
  return apply_opt_dag_substitute(root, &st);
}

// ---------- KOP_FAST_MATH -------------------------------------------
// Wrap every UOP_EXP2/LOG2/SQRT leaf with OPT(_, FAST_MATH, 0).  The
// renderer peels the wrapper at emit time and substitutes Apple's
// `fast::exp2 / fast::log2 / fast::sqrt` for the precise variants
// (~5-15% throughput on softmax/layernorm/attention at ~2 ULP cost).
//
// Manual walker (not uop_graph_rewrite) because uop_graph_rewrite
// recurses on rule outputs: a rule that turns EXP2 -> OPT(EXP2, FM, 0)
// would re-visit the inner EXP2 and re-wrap it, infinite loop.  The
// substitution-map memo from apply_opt_dag_substitute is reused for
// identity-keyed memoisation and idempotency: an already-wrapped EXP2
// reaches us as OPT(EXP2, FM, 0) and falls through unchanged.

#define APPLY_OPT_DAG_FM_MEMO_CAP 1024

typedef struct {
  Term key;
  Term val;
} ApplyOptDagFastMathMemoEntry;

typedef struct {
  ApplyOptDagFastMathMemoEntry memo[APPLY_OPT_DAG_FM_MEMO_CAP];
  u32 memo_n;
  int changed;
} ApplyOptDagFastMathState;

static int apply_opt_dag_fm_memo_lookup(ApplyOptDagFastMathState *st,
                                        Term k, Term *out) {
  for (u32 i = 0; i < st->memo_n; i++) {
    if (st->memo[i].key == k) { *out = st->memo[i].val; return 1; }
  }
  return 0;
}

static void apply_opt_dag_fm_memo_insert(ApplyOptDagFastMathState *st,
                                         Term k, Term v) {
  if (st->memo_n < APPLY_OPT_DAG_FM_MEMO_CAP) {
    st->memo[st->memo_n].key = k;
    st->memo[st->memo_n].val = v;
    st->memo_n++;
  }
}

static Term apply_opt_dag_fm_walk(Term t, ApplyOptDagFastMathState *st);

static Term apply_opt_dag_fm_walk_uncached(Term t,
                                           ApplyOptDagFastMathState *st) {
  if (term_tag(t) != TAG_UOP) return t;
  u32 op = term_ext(t);
  if (op == UOP_RANGE || op == UOP_BUFFER || op == UOP_CONST
      || op == UOP_INVALID) return t;
  // Idempotency: an already-wrapped unary reaches us as
  // OPT(<unary>, FAST_MATH, 0).  Don't re-recurse into the inner unary
  // (which would re-wrap it via the `op == UOP_EXP2/LOG2/SQRT` arm
  // below) -- that would produce OPT(OPT(EXP2, FM), FM).  Instead
  // descend into the wrapped unary's body and re-wrap the rebuilt
  // unary in the same OPT shape.  Hash-cons makes the no-change case
  // bottom out at the original term so r2 == r1.
  if (op == UOP_OPT && uop_opt_kind(t) == UOP_OPT_FAST_MATH) {
    Term inner = uop_opt_target(t);
    if (term_tag(inner) == TAG_UOP) {
      u32 in_op = term_ext(inner);
      if (in_op == UOP_EXP2 || in_op == UOP_LOG2 || in_op == UOP_SQRT) {
        u8 ar_in = uop_arity(in_op);
        Term in_srcs[MAX_UOP_SRC];
        int in_changed = 0;
        u64 in_loc = term_val(inner);
        for (u8 i = 0; i < ar_in && i < MAX_UOP_SRC; i++) {
          Term old = heap_read(in_loc + i);
          Term ne  = (term_tag(old) == TAG_UOP)
                     ? apply_opt_dag_fm_walk(old, st)
                     : old;
          in_srcs[i] = ne;
          if (ne != old) in_changed = 1;
        }
        Term rebuilt_inner = in_changed
                             ? uop_graph_rebuild_with_srcs(inner, in_srcs)
                             : inner;
        if (rebuilt_inner == inner) return t;
        return uop_opt(rebuilt_inner, UOP_OPT_FAST_MATH, 0);
      }
    }
  }
  // Recurse into children first.
  u8 ar = uop_arity(op);
  Term srcs[MAX_UOP_SRC];
  int child_changed = 0;
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old = heap_read(loc + i);
    Term ne  = (term_tag(old) == TAG_UOP)
               ? apply_opt_dag_fm_walk(old, st)
               : old;
    srcs[i] = ne;
    if (ne != old) child_changed = 1;
  }
  Term rebuilt = child_changed ? uop_graph_rebuild_with_srcs(t, srcs) : t;
  // Wrap unary ops here.  An OPT(EXP2, FAST_MATH, 0) wrapper is itself
  // a UOP_OPT (not UOP_EXP2) so the test below doesn't re-fire on the
  // already-wrapped child.
  if (op == UOP_EXP2 || op == UOP_LOG2 || op == UOP_SQRT) {
    st->changed = 1;
    return uop_opt(rebuilt, UOP_OPT_FAST_MATH, 0);
  }
  return rebuilt;
}

static Term apply_opt_dag_fm_walk(Term t, ApplyOptDagFastMathState *st) {
  Term hit;
  if (apply_opt_dag_fm_memo_lookup(st, t, &hit)) return hit;
  Term out = apply_opt_dag_fm_walk_uncached(t, st);
  apply_opt_dag_fm_memo_insert(st, t, out);
  return out;
}

fn Term uop_dag_apply_fast_math(Term root) {
  ApplyOptDagFastMathState st;
  st.memo_n = 0;
  st.changed = 0;
  Term out = apply_opt_dag_fm_walk(root, &st);
  return st.changed ? out : root;
}

// ---------- KOP_SIMD_REDUCE -----------------------------------------
// Wrap every UOP_REDUCE node with OPT(_, SIMD_REDUCE, 0).  The renderer
// peels the wrapper at emit time and emits Apple's `simd_sum` /
// `simd_max` simdgroup-collective reduce (1 instruction across all 32
// lanes) instead of the scalar for-loop accumulator.
//
// Reuses the FAST_MATH walker shape: manual recursion (not
// uop_graph_rewrite) so the wrapper itself doesn't re-visit the inner
// REDUCE.  Idempotent because a REDUCE that's already wrapped reaches
// us as OPT(REDUCE, SIMD_REDUCE, 0) and falls through unchanged.

#define APPLY_OPT_DAG_SR_MEMO_CAP 1024

typedef struct {
  Term key;
  Term val;
} ApplyOptDagSimdReduceMemoEntry;

typedef struct {
  ApplyOptDagSimdReduceMemoEntry memo[APPLY_OPT_DAG_SR_MEMO_CAP];
  u32 memo_n;
  int changed;
} ApplyOptDagSimdReduceState;

static int apply_opt_dag_sr_memo_lookup(ApplyOptDagSimdReduceState *st,
                                        Term k, Term *out) {
  for (u32 i = 0; i < st->memo_n; i++) {
    if (st->memo[i].key == k) { *out = st->memo[i].val; return 1; }
  }
  return 0;
}

static void apply_opt_dag_sr_memo_insert(ApplyOptDagSimdReduceState *st,
                                         Term k, Term v) {
  if (st->memo_n < APPLY_OPT_DAG_SR_MEMO_CAP) {
    st->memo[st->memo_n].key = k;
    st->memo[st->memo_n].val = v;
    st->memo_n++;
  }
}

static Term apply_opt_dag_sr_walk(Term t, ApplyOptDagSimdReduceState *st);

static Term apply_opt_dag_sr_walk_uncached(Term t,
                                           ApplyOptDagSimdReduceState *st) {
  if (term_tag(t) != TAG_UOP) return t;
  u32 op = term_ext(t);
  if (op == UOP_RANGE || op == UOP_BUFFER || op == UOP_CONST
      || op == UOP_INVALID) return t;
  // Idempotency: an already-wrapped REDUCE reaches us as
  // OPT(REDUCE, SIMD_REDUCE, _).  Don't re-wrap the inner REDUCE; just
  // recurse into the REDUCE's body and re-wrap the (possibly-rebuilt)
  // REDUCE in the same OPT shape.  Hash-cons makes the no-change case
  // bottom out at the original term.
  if (op == UOP_OPT && uop_opt_kind(t) == UOP_OPT_SIMD_REDUCE) {
    Term inner = uop_opt_target(t);
    if (term_tag(inner) == TAG_UOP && term_ext(inner) == UOP_REDUCE) {
      u8 ar_in = uop_arity(UOP_REDUCE);
      Term in_srcs[MAX_UOP_SRC];
      int in_changed = 0;
      u64 in_loc = term_val(inner);
      for (u8 i = 0; i < ar_in && i < MAX_UOP_SRC; i++) {
        Term old = heap_read(in_loc + i);
        Term ne  = (term_tag(old) == TAG_UOP)
                   ? apply_opt_dag_sr_walk(old, st)
                   : old;
        in_srcs[i] = ne;
        if (ne != old) in_changed = 1;
      }
      Term rebuilt_inner = in_changed
                           ? uop_graph_rebuild_with_srcs(inner, in_srcs)
                           : inner;
      if (rebuilt_inner == inner) return t;
      return uop_opt(rebuilt_inner, UOP_OPT_SIMD_REDUCE, 0);
    }
  }
  u8 ar = uop_arity(op);
  Term srcs[MAX_UOP_SRC];
  int child_changed = 0;
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old = heap_read(loc + i);
    Term ne  = (term_tag(old) == TAG_UOP)
               ? apply_opt_dag_sr_walk(old, st)
               : old;
    srcs[i] = ne;
    if (ne != old) child_changed = 1;
  }
  Term rebuilt = child_changed ? uop_graph_rebuild_with_srcs(t, srcs) : t;
  if (op == UOP_REDUCE) {
    st->changed = 1;
    return uop_opt(rebuilt, UOP_OPT_SIMD_REDUCE, 0);
  }
  return rebuilt;
}

static Term apply_opt_dag_sr_walk(Term t, ApplyOptDagSimdReduceState *st) {
  Term hit;
  if (apply_opt_dag_sr_memo_lookup(st, t, &hit)) return hit;
  Term out = apply_opt_dag_sr_walk_uncached(t, st);
  apply_opt_dag_sr_memo_insert(st, t, out);
  return out;
}

fn Term uop_dag_apply_simd_reduce(Term root) {
  ApplyOptDagSimdReduceState st;
  st.memo_n = 0;
  st.changed = 0;
  Term out = apply_opt_dag_sr_walk(root, &st);
  return st.changed ? out : root;
}

// ---------- KOP_VEC_LOAD --------------------------------------------
// Wrap each UOP_INDEX_E whose address has a "contiguous-axis" shape
// with OPT(_, VEC_LOAD, width).  The renderer peels the wrapper at
// emit time and produces a (device const floatN*) reinterpret_cast.
//
// Contiguous-address heuristic (matches the typical row-major access
// pattern emitted by movement -> index lowering):
//
//   IADD(IMUL(x, c), y)
//
// where `y` is a single UOP_RANGE leaf (the unit-strided innermost
// axis).  `x` and `c` are arbitrary; we only need y to be a bare RANGE
// for the load span to be contiguous.  Anything else (bare RANGE,
// scalar address, non-IADD shape, IADD with a compound RHS) is left
// scalar.
//
// Manual walker (mirrors uop_dag_apply_fast_math): uop_graph_rewrite
// recurses on rule outputs, so a rule that turns INDEX_E ->
// OPT(INDEX_E, VEC_LOAD, w) would re-fire on the inner INDEX_E and
// stack OPT-of-OPT.  The Term-keyed memo here ensures the wrap fires
// exactly once per unique INDEX_E.  Idempotent: re-applying with the
// same width hash-cons-dedups back to the same wrapped Term.
//
// Width must be 2/4/8/16; other widths bail (caller responsibility).

#define APPLY_OPT_DAG_VEC_MEMO_CAP 1024

typedef struct {
  Term key;
  Term val;
} ApplyOptDagVecLoadMemoEntry;

typedef struct {
  ApplyOptDagVecLoadMemoEntry memo[APPLY_OPT_DAG_VEC_MEMO_CAP];
  u32 memo_n;
  u32 width;
  int changed;
} ApplyOptDagVecLoadState;

static int apply_opt_dag_vec_memo_lookup(ApplyOptDagVecLoadState *st,
                                         Term k, Term *out) {
  for (u32 i = 0; i < st->memo_n; i++) {
    if (st->memo[i].key == k) { *out = st->memo[i].val; return 1; }
  }
  return 0;
}

static void apply_opt_dag_vec_memo_insert(ApplyOptDagVecLoadState *st,
                                          Term k, Term v) {
  if (st->memo_n < APPLY_OPT_DAG_VEC_MEMO_CAP) {
    st->memo[st->memo_n].key = k;
    st->memo[st->memo_n].val = v;
    st->memo_n++;
  }
}

// Address shape: IADD(IMUL(x, c), y) where y is a single UOP_RANGE.
static int apply_opt_dag_vec_addr_contiguous(Term addr) {
  if (term_tag(addr) != TAG_UOP) return 0;
  if (term_ext(addr) != UOP_IADD) return 0;
  u64 aloc = term_val(addr);
  Term lhs = heap_read(aloc + 0);
  Term rhs = heap_read(aloc + 1);
  if (term_tag(rhs) != TAG_UOP || term_ext(rhs) != UOP_RANGE) return 0;
  if (term_tag(lhs) != TAG_UOP || term_ext(lhs) != UOP_IMUL) return 0;
  return 1;
}

static Term apply_opt_dag_vec_walk(Term t, ApplyOptDagVecLoadState *st);

static Term apply_opt_dag_vec_walk_uncached(Term t,
                                            ApplyOptDagVecLoadState *st) {
  if (term_tag(t) != TAG_UOP) return t;
  u32 op = term_ext(t);
  if (op == UOP_RANGE || op == UOP_BUFFER || op == UOP_CONST
      || op == UOP_INVALID) return t;
  // Idempotency stop-leaf: if we hit OPT(_, VEC_LOAD, _) we're walking
  // a previously-wrapped INDEX_E.  Recursing into its INDEX_E child
  // would re-fire the wrap rule and return OPT(INDEX_E, VEC_LOAD, w)
  // -- a different Term than the existing OPT we're inside, causing
  // child_changed and a double-wrap rebuild OPT(OPT, VEC_LOAD, w).
  // Stop early so re-application is a true no-op.
  if (op == UOP_OPT && uop_opt_kind(t) == UOP_OPT_VEC_LOAD) return t;
  // Recurse into children first (rebuild bottom-up so an INDEX_E whose
  // address mentions a child we replaced sees the new shape).
  u8 ar = uop_arity(op);
  Term srcs[MAX_UOP_SRC];
  int child_changed = 0;
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old = heap_read(loc + i);
    Term ne  = (term_tag(old) == TAG_UOP)
               ? apply_opt_dag_vec_walk(old, st)
               : old;
    srcs[i] = ne;
    if (ne != old) child_changed = 1;
  }
  Term rebuilt = child_changed ? uop_graph_rebuild_with_srcs(t, srcs) : t;
  // Wrap INDEX_E with OPT(_, VEC_LOAD, width) when the address shape is
  // contiguous.  An OPT(INDEX_E, ...) wrapper is a UOP_OPT (not
  // UOP_INDEX_E) so the test below doesn't re-fire on already-wrapped
  // children that come back through the rebuild.
  if (op == UOP_INDEX_E) {
    Term addr = heap_read(term_val(rebuilt) + 1);
    if (apply_opt_dag_vec_addr_contiguous(addr)) {
      st->changed = 1;
      return uop_opt(rebuilt, UOP_OPT_VEC_LOAD, st->width);
    }
  }
  return rebuilt;
}

static Term apply_opt_dag_vec_walk(Term t, ApplyOptDagVecLoadState *st) {
  Term hit;
  if (apply_opt_dag_vec_memo_lookup(st, t, &hit)) return hit;
  Term out = apply_opt_dag_vec_walk_uncached(t, st);
  apply_opt_dag_vec_memo_insert(st, t, out);
  return out;
}

fn Term uop_dag_apply_vec_load(Term root, u32 width) {
  if (width != 2 && width != 4 && width != 8 && width != 16) return root;
  ApplyOptDagVecLoadState st;
  st.memo_n = 0;
  st.width = width;
  st.changed = 0;
  Term out = apply_opt_dag_vec_walk(root, &st);
  return st.changed ? out : root;
}

// ---------- top-level dispatcher ------------------------------------

// Dense-renumber every UOP_RANGE axis_id in a kernel DAG to 0..n-1,
// ascending by current axis_id.  Rewrites RANGE leaves (via the
// substitution map, by Term identity) AND REDUCE axis fields (via the
// old->new table in ApplyOptDagSubState).  Makes axis_ids per-kernel-
// dense so downstream consumers (KOpt.axis, render decode arrays, the
// JIT source) never see the global, sparse, possibly-large ids the
// lifter assigns -- the tinygrad model, where each kernel's axes are a
// fresh 0..n.  Returns the new root (or `root` unchanged when already
// dense / no ranges).
//
// Determinism / cache-stability: sort by current axis_id; the lifter
// assigns ids monotonically in DAG-build order, so structurally-
// identical kernels (even with different absolute id bases) map to the
// SAME dense ids -> the rendered source is byte-identical and the JIT
// disk cache hits without a separate post-render canonicalize pass.
//
// Insertion point: after materialize finalizes store_root, before the
// fire-time hand_opts split pass (whose `axis_id+1` inserts then stay
// contiguous on top of a dense base).
fn Term uop_dag_renumber_axes(Term root) {
  if (root == 0 || term_tag(root) != TAG_UOP) return root;
  Term ranges[APPLY_OPT_DAG_SPLIT_MAP_CAP];
  u32 n_ranges = apply_opt_dag_collect_ranges(root, ranges,
                                              APPLY_OPT_DAG_SPLIT_MAP_CAP);
  if (n_ranges == 0) return root;
  // Distinct axis_ids.
  u32 ids[APPLY_OPT_DAG_SPLIT_MAP_CAP]; u32 n_ids = 0;
  for (u32 i = 0; i < n_ranges; i++) {
    u32 a = uop_range_axis_id(ranges[i]);
    int seen = 0;
    for (u32 j = 0; j < n_ids; j++) if (ids[j] == a) { seen = 1; break; }
    if (!seen && n_ids < APPLY_OPT_DAG_SPLIT_MAP_CAP) ids[n_ids++] = a;
  }
  // Ascending insertion sort (n tiny).
  for (u32 i = 1; i < n_ids; i++) {
    u32 v = ids[i]; i32 j = (i32)i - 1;
    while (j >= 0 && ids[j] > v) { ids[j + 1] = ids[j]; j--; }
    ids[j + 1] = v;
  }
  // Already dense 0..n-1?  Then a no-op (identity map).
  int identity = 1;
  for (u32 i = 0; i < n_ids; i++) if (ids[i] != i) { identity = 0; break; }
  if (identity) return root;
  // remap_new[i] = i: ids[i] (old, sorted) -> dense i.
  u32 remap_new[APPLY_OPT_DAG_SPLIT_MAP_CAP];
  for (u32 i = 0; i < n_ids; i++) remap_new[i] = i;
  // RANGE leaf substitution map: old range term -> new range with dense id.
  ApplyOptDagSplitCtx ctx; ctx.n = 0;
  for (u32 i = 0; i < n_ranges && ctx.n < APPLY_OPT_DAG_SPLIT_MAP_CAP; i++) {
    Term r = ranges[i];
    u32 a = uop_range_axis_id(r);
    u32 dense = a;
    for (u32 j = 0; j < n_ids; j++) if (ids[j] == a) { dense = j; break; }
    if (dense == a) continue;
    ctx.entries[ctx.n].key = r;
    ctx.entries[ctx.n].val = uop_range(dense, uop_range_axis_type(r),
                                       uop_range_extent(r));
    ctx.n++;
  }
  ApplyOptDagSubState st;
  st.memo_n             = 0;
  st.map                = &ctx;
  st.reduce_shift_above = 0xFFFFFFFFu;
  st.reduce_swap_a      = 0xFFFFFFFFu;
  st.reduce_swap_b      = 0xFFFFFFFFu;
  st.remap_old          = ids;
  st.remap_new          = remap_new;
  st.remap_n            = n_ids;
  return apply_opt_dag_substitute(root, &st);
}

fn Term uop_dag_apply_kopt(Term root, KOpt opt) {
  switch (opt.op) {
    case KOP_TC:
      return uop_dag_apply_tc(root, opt.arg);
    case KOP_GLOBAL:
      return uop_dag_apply_global(root, opt.axis);
    case KOP_SWAP:
      return uop_dag_apply_swap(root, opt.axis, opt.arg);
    case KOP_UPCAST:
    case KOP_UNROLL:
    case KOP_LOCAL:
      return uop_dag_apply_split(root, opt.op, opt.axis, opt.arg);
    case KOP_GROUP:
    case KOP_GROUPTOP:
      return uop_dag_apply_group_reduce(root, opt.axis, opt.arg);
    case KOP_FAST_MATH:
      return uop_dag_apply_fast_math(root);
    case KOP_SIMD_REDUCE:
      return uop_dag_apply_simd_reduce(root);
    case KOP_VEC_LOAD:
      return uop_dag_apply_vec_load(root, opt.arg);
    default:
      return 0;  // unsupported (PADTO, NOLOCALS reserved)
  }
}
