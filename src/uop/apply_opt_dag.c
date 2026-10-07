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
// leaf in the DAG with extent % k == 0, its type a split target of `op`
// (kop_split_source_ok), and an UNROLL target an axis some REDUCE reduces
// (tinygrad codegen/opt/postrange.py:128-130).  Returns 0 on bail.

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
  // matching stops finding it and falls back to the
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
  // An AFTER kernel root (arity 0) rewrites every store, so all stores
  // share one axis-id space.
  if (op == UOP_AFTER) {
    Term node  = uop_after_node(t);
    Term after = uop_after_after_node(t);
    Term nn = apply_opt_dag_substitute(node, st);
    Term na = apply_opt_dag_substitute(after, st);
    if (nn == node && na == after) return t;
    return uop_after(nn, na);
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
// by Term identity), through every store of an AFTER root.  Returns the
// count.
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
    u8 ar = (op == UOP_AFTER) ? 2 : uop_arity(op);
    u64 loc = term_val(t);
    for (u8 i = 0; i < ar && i < MAX_UOP_SRC && sp < 256; i++) {
      Term child = heap_read(loc + i);
      if (term_tag(child) == TAG_UOP) stack[sp++] = child;
    }
  }
  return n;
}

// tinygrad split_targets (codegen/opt/postrange.py:14-15), checked in
// shift_to (:80), which every split goes through.  thvm keeps tinygrad's
// WEAK output axes as KAX_LOOP and the reduce-typed LOCAL a group leaves
// as KAX_GROUP_REDUCE (tinygrad's GROUP_REDUCE before #18388), so the
// group split of a REDUCE axis into a local is KOP_GROUP/GROUPTOP, not
// KOP_LOCAL.
fn int kop_split_source_ok(u8 op, u32 axis_type) {
  switch (op) {
    case KOP_UPCAST:
      return axis_type == KAX_LOOP || axis_type == KAX_GLOBAL
          || axis_type == KAX_LOCAL;
    case KOP_UNROLL:
      return axis_type == KAX_REDUCE || axis_type == KAX_GROUP_REDUCE;
    case KOP_LOCAL:
      return axis_type == KAX_LOOP || axis_type == KAX_GLOBAL;
    case KOP_GROUP:
    case KOP_GROUPTOP:
      return axis_type == KAX_REDUCE;
    default:
      return 0;
  }
}

static int apply_opt_dag_reduce_names(Term red, u32 axis_id) {
  u32 n = uop_reduce_n_axes(red);
  for (u32 i = 0; i < n; i++) {
    if (uop_reduce_axis(red, i) == axis_id) {
      return 1;
    }
  }
  return 0;
}

static void apply_opt_dag_push_srcs(UOpWalk *w, Term t) {
  u8  ar  = uop_arity((u8)term_ext(t));
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    uop_walk_push(w, heap_read(loc + i));
  }
}

// First UOP_REDUCE reachable from `root` that reduces `axis_id`, or 0.
static Term apply_opt_dag_reduce_of(Term root, u32 axis_id) {
  UOpWalk w = {0};
  uop_walk_push(&w, root);
  Term found = 0;
  for (Term t = uop_walk_pop(&w); t != 0; t = uop_walk_pop(&w)) {
    if (term_ext(t) == UOP_REDUCE && apply_opt_dag_reduce_names(t, axis_id)) {
      found = t;
      break;
    }
    apply_opt_dag_push_srcs(&w, t);
  }
  uop_walk_free(&w);
  return found;
}

fn int uop_dag_reduces_axis(Term root, u32 axis_id) {
  return apply_opt_dag_reduce_of(root, axis_id) != 0;
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
  if (!kop_split_source_ok(op, axis_type_outer)) {
    if (getenv("THVM_HANDOPT_TRACE")) {
      fprintf(stderr, "[split] BAIL op=%u axis=%u k=%u: axis type %u is no "
              "split target\n", op, target_axis, k, axis_type_outer);
    }
    return 0;
  }
  if (op == KOP_UNROLL && !uop_dag_reduces_axis(root, target_axis)) {
    if (getenv("THVM_HANDOPT_TRACE")) {
      fprintf(stderr, "[split] BAIL op=%u axis=%u k=%u: no REDUCE reduces "
              "the axis\n", op, target_axis, k);
    }
    return 0;
  }
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

// ---------- split ownership -----------------------------------------
// A split of reduce axis T leaves REDUCE.axes naming only `outer`, while
// every former reference to T reads IADD(IMUL(outer, k), OPT(inner, _, k)).
// tinygrad's shift_to substitutes that composition into REDUCE.src[1:], so
// the REDUCE owns BOTH halves (codegen/opt/postrange.py:79-87).  thvm reads
// the owner of `inner` back off the composition: it is the REDUCE naming a
// range of the `outer` operand.  Body membership cannot answer this once a
// kernel nests reduces -- `inner` then sits inside every enclosing reduce's
// body, and a reduce that folds another reduce's inner half re-sums it.
//
// Both walks below reach every node at any graph size.  A walk that stopped
// early would answer "not owned": the renderer and the CPU walker would fold
// the inner half in every reduce around it, and the group check would count
// it as an open range and refuse a valid group.

static u32 apply_opt_dag_opt_range_aid(Term t) {
  while (term_tag(t) == TAG_UOP && term_ext(t) == UOP_OPT) {
    t = uop_opt_target(t);
  }
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_RANGE) {
    return 0xFFFFFFFFu;
  }
  return uop_range_axis_id(t);
}

// 1 iff a RANGE leaf of index expression `t` is one of `red`'s axes.
static int apply_opt_dag_names_leaf_of(Term red, Term t) {
  UOpWalk w = {0};
  uop_walk_push(&w, t);
  int named = 0;
  for (Term n = uop_walk_pop(&w); n != 0; n = uop_walk_pop(&w)) {
    if (term_ext(n) == UOP_RANGE
        && apply_opt_dag_reduce_names(red, uop_range_axis_id(n))) {
      named = 1;
      break;
    }
    apply_opt_dag_push_srcs(&w, n);
  }
  uop_walk_free(&w);
  return named;
}

// 1 iff IADD `t` adds `inner_aid` to an operand holding one of `red`'s
// axes: the composition a split of that axis leaves.
static int apply_opt_dag_composes_split(Term red, Term t, u32 inner_aid) {
  Term a = heap_read(term_val(t) + 0);
  Term b = heap_read(term_val(t) + 1);
  return (apply_opt_dag_opt_range_aid(b) == inner_aid
          && apply_opt_dag_names_leaf_of(red, a))
      || (apply_opt_dag_opt_range_aid(a) == inner_aid
          && apply_opt_dag_names_leaf_of(red, b));
}

fn int uop_reduce_owns_split_axis(Term red, u32 inner_aid) {
  if (term_tag(red) != TAG_UOP || term_ext(red) != UOP_REDUCE) {
    return 0;
  }
  UOpWalk w = {0};
  uop_walk_push(&w, uop_reduce_src(red));
  int owns = 0;
  for (Term t = uop_walk_pop(&w); t != 0; t = uop_walk_pop(&w)) {
    if (term_ext(t) == UOP_IADD
        && apply_opt_dag_composes_split(red, t, inner_aid)) {
      owns = 1;
      break;
    }
    apply_opt_dag_push_srcs(&w, t);
  }
  uop_walk_free(&w);
  return owns;
}

// ---------- no group inside another reduce --------------------------
// tinygrad codegen/opt/postrange.py:131-134 refuses a group whose REDUCE
// sits inside another reduce.  It reads reduces[0].ranges: the ranges still
// open at the REDUCE, i.e. those of its body minus the ones it or a reduce
// in the body closes (uop/ops.py:469-497).  A REDUCE closes the axes it
// names plus the inner halves of their splits, which tinygrad keeps in
// REDUCE.src[1:] and thvm reads off the split composition.

static int apply_opt_dag_reduce_closes(Term red, u32 aid, u32 kax) {
  if (apply_opt_dag_reduce_names(red, aid)) {
    return 1;
  }
  return kax == KAX_UNROLL && uop_reduce_owns_split_axis(red, aid);
}

// 1 iff `red` or a REDUCE in its body closes the range (aid, kax).
static int apply_opt_dag_closed_in(Term red, u32 aid, u32 kax) {
  if (apply_opt_dag_reduce_closes(red, aid, kax)) {
    return 1;
  }
  UOpWalk w = {0};
  uop_walk_push(&w, uop_reduce_src(red));
  int closed = 0;
  for (Term t = uop_walk_pop(&w); t != 0; t = uop_walk_pop(&w)) {
    if (term_ext(t) == UOP_REDUCE && apply_opt_dag_reduce_closes(t, aid, kax)) {
      closed = 1;
      break;
    }
    apply_opt_dag_push_srcs(&w, t);
  }
  uop_walk_free(&w);
  return closed;
}

// A REDUCE, UNROLL or GROUP_REDUCE range still open at `red`, or 0: the
// check's (REDUCE, UNROLL) plus the GROUP_REDUCE tinygrad listed before
// #18388 folded it into LOCAL.
static Term apply_opt_dag_open_reduce_range(Term red) {
  UOpWalk w = {0};
  uop_walk_push(&w, uop_reduce_src(red));
  Term open = 0;
  for (Term t = uop_walk_pop(&w); t != 0; t = uop_walk_pop(&w)) {
    if (term_ext(t) != UOP_RANGE) {
      apply_opt_dag_push_srcs(&w, t);
      continue;
    }
    u32 kax = uop_range_axis_type(t);
    if ((kax == KAX_REDUCE || kax == KAX_UNROLL || kax == KAX_GROUP_REDUCE)
        && !apply_opt_dag_closed_in(red, uop_range_axis_id(t), kax)) {
      open = t;
      break;
    }
  }
  uop_walk_free(&w);
  return open;
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
// Validity: target axis_id must name KAX_REDUCE leaf(s) with extent % k
// == 0.  An im2col-fused conv (the VAE decode) indexes its operands
// (x and w) with the SAME contraction axis_id from SEPARATE address
// subtrees -- the lifter emits the axis as MULTIPLE distinct UOP_RANGE
// terms sharing one axis_id.  A single-leaf find_range bails on that
// (its "multiple distinct" guard returns 0), which silently killed the
// conv GROUP candidate.  Collect EVERY matching leaf and substitute each
// with its GROUP_REDUCE-wrapped form; the rewriter recursion fixpoints
// since the wrapped outputs are not themselves rule keys.  A REDUCE must
// reduce the axis and must not sit inside another reduce (tinygrad
// codegen/opt/postrange.py:128-134).  Returns 0 on bail (no match, a
// non-REDUCE leaf, extent not divisible, no such REDUCE, or a nested one).

fn Term uop_dag_apply_group_reduce(Term root, u32 target_axis, u32 k) {
  int gr_tr = getenv("THVM_GROUP_REDUCE_TRACE") != NULL;
  if (k == 0) return 0;
  Term ranges[APPLY_OPT_DAG_SPLIT_MAP_CAP];
  u32 n_ranges = apply_opt_dag_collect_ranges(root, ranges,
                                              APPLY_OPT_DAG_SPLIT_MAP_CAP);
  ApplyOptDagSplitCtx ctx;
  ctx.n = 0;
  u32 extent = 0;
  for (u32 i = 0; i < n_ranges; i++) {
    Term r = ranges[i];
    if (uop_range_axis_id(r) != target_axis) continue;
    if (!kop_split_source_ok(KOP_GROUP, uop_range_axis_type(r))) {
      if (gr_tr) fprintf(stderr, "[gr] axis=%u k=%u BAIL: leaf kax=%u != REDUCE\n",
                         target_axis, k, uop_range_axis_type(r));
      return 0;
    }
    u32 e = uop_range_extent(r);
    if (e == 0 || e % k != 0) {
      if (gr_tr) fprintf(stderr, "[gr] axis=%u k=%u BAIL: extent=%u not div\n",
                         target_axis, k, e);
      return 0;
    }
    if (extent == 0) extent = e;
    else if (e != extent) {
      // Same axis_id, different extent -> malformed; refuse to GROUP.
      if (gr_tr) fprintf(stderr, "[gr] axis=%u k=%u BAIL: mixed extents %u/%u\n",
                         target_axis, k, extent, e);
      return 0;
    }
    if (ctx.n >= APPLY_OPT_DAG_SPLIT_MAP_CAP) return 0;
    Term new_leaf = uop_range(target_axis, KAX_GROUP_REDUCE, e);
    ctx.entries[ctx.n].key = r;
    ctx.entries[ctx.n].val = uop_opt(new_leaf, UOP_OPT_GROUP_REDUCE, k);
    ctx.n++;
  }
  if (ctx.n == 0) {
    if (gr_tr) {
      fprintf(stderr, "[gr] axis=%u k=%u BAIL: no REDUCE leaf matched; "
              "collected %u/%u ranges:", target_axis, k, n_ranges,
              APPLY_OPT_DAG_SPLIT_MAP_CAP);
      for (u32 i = 0; i < n_ranges; i++) {
        fprintf(stderr, " a%u(t=%u,e=%u)", uop_range_axis_id(ranges[i]),
                uop_range_axis_type(ranges[i]), uop_range_extent(ranges[i]));
      }
      fputc('\n', stderr);
    }
    return 0;
  }
  Term red = apply_opt_dag_reduce_of(root, target_axis);
  if (red == 0) {
    if (gr_tr) {
      fprintf(stderr, "[gr] axis=%u k=%u BAIL: no REDUCE reduces the axis\n",
              target_axis, k);
    }
    return 0;
  }
  Term open = apply_opt_dag_open_reduce_range(red);
  if (open != 0) {
    if (gr_tr) {
      fprintf(stderr, "[gr] axis=%u k=%u BAIL: group inside another reduce "
              "(range a%u kax=%u open at its REDUCE)\n", target_axis, k,
              uop_range_axis_id(open), uop_range_axis_type(open));
    }
    return 0;
  }

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
    default:
      return 0;  // unsupported (PADTO, NOLOCALS reserved)
  }
}
