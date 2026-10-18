// uop/apply_opt_dag.c - apply one KOpt to a kernel's UOp DAG (kernel_apply_opt
// calls uop_dag_apply_kopt).  Each helper returns the rewritten root; 0, or the
// root itself, means the opt does not apply.

// KOP_TC: wrap the store's REDUCE in OPT(_, TC, factor), or re-factor its
// OPT_TC; 0 when there is none.
fn Term uop_dag_apply_tc(Term root, u32 factor) {
  if (term_tag(root) != TAG_UOP || term_ext(root) != UOP_STORE) return 0;
  u64 sloc = term_val(root);
  Term buf   = heap_read(sloc + 0);
  Term addr  = heap_read(sloc + 1);
  Term value = heap_read(sloc + 2);

  Term new_value = 0;
  if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_REDUCE) {
    new_value = uop_opt(value, UOP_OPT_TC, factor);
  } else if (term_tag(value) == TAG_UOP && term_ext(value) == UOP_OPT
             && uop_opt_kind(value) == UOP_OPT_TC) {
    Term inner = uop_opt_target(value);
    if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_REDUCE) return 0;
    new_value = uop_opt(inner, UOP_OPT_TC, factor);
  } else if (ru_fuse_epilogue_on()) {
    // THVM_FUSE_MATMUL_EPILOGUE: wrap the matmul REDUCE under an elementwise
    // chain in place; rmu_detect_matmul_tc accepts the chain around an OPT_TC.
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

// KOP_GLOBAL: stamp the KAX_LOOP RANGE leaves of `axis_id` KAX_GLOBAL; other
// axis types carry meaning and stay.  rmu_emit_matmul_tc binds a GLOBAL M or N
// to threadgroups.  Returns `root` unchanged when no RANGE matched.
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

// Original Term -> replacement, one entry per RANGE leaf an opt rewrites.
#define APPLY_OPT_DAG_SPLIT_MAP_CAP 64

typedef struct {
  Term key;
  Term val;
} ApplyOptDagSplitMapEntry;

typedef struct {
  ApplyOptDagSplitMapEntry entries[APPLY_OPT_DAG_SPLIT_MAP_CAP];
  u32 n;
} ApplyOptDagSplitCtx;

// One-pass substitution, not uop_graph_rewrite: that re-applies rules to their
// outputs, and a two-way map (SWAP sends A to B and B to A) would cycle.  A
// mapped value is returned as is; the memo visits each original Term once.
#define APPLY_OPT_DAG_SUB_MEMO_CAP 1024

typedef struct {
  Term key;
  Term val;
} ApplyOptDagSubMemoEntry;

typedef struct {
  ApplyOptDagSubMemoEntry memo[APPLY_OPT_DAG_SUB_MEMO_CAP];
  u32 memo_n;
  ApplyOptDagSplitCtx const *map;
  // A REDUCE names its axes by number, which the map of RANGE Terms misses: a
  // split moves each reduce axis above reduce_shift_above up by one, a SWAP
  // exchanges reduce_swap_a and reduce_swap_b (0xFFFFFFFFu: unused).
  u32 reduce_shift_above;
  u32 reduce_swap_a;
  u32 reduce_swap_b;
  // uop_dag_renumber_axes: remap_old[i] -> remap_new[i] for the reduce axes,
  // replacing the shift and swap (remap_n 0: unused).
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
  for (u32 i = 0; i < st->map->n; i++) {
    if (st->map->entries[i].key == t) return st->map->entries[i].val;
  }
  if (term_tag(t) != TAG_UOP) return t;
  u32 op = term_ext(t);
  if (op == UOP_RANGE || op == UOP_BUFFER || op == UOP_CONST
      || op == UOP_INVALID) return t;
  if (op == UOP_OPT) {
    Term tgt = uop_opt_target(t);
    Term new_tgt = apply_opt_dag_substitute(tgt, st);
    if (new_tgt == tgt) return t;
    return uop_opt(new_tgt, uop_opt_kind(t), uop_opt_factor(t));
  }
  // An AFTER root (arity 0) rewrites every store: they share one axis-id space.
  if (op == UOP_AFTER) {
    Term node  = uop_after_node(t);
    Term after = uop_after_after_node(t);
    Term nn = apply_opt_dag_substitute(node, st);
    Term na = apply_opt_dag_substitute(after, st);
    if (nn == node && na == after) return t;
    return uop_after(nn, na);
  }
  if (op == UOP_REDUCE) {
    u32 kind   = uop_reduce_kind(t);
    u32 n_axes = uop_reduce_n_axes(t);
    u32 new_axes[MAX_DIM];
    int axes_changed = 0;
    for (u32 i = 0; i < n_axes; i++) {
      u32 ax = uop_reduce_axis(t, i);
      u32 mapped = ax;
      if (st->remap_n > 0) {
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

// The one RANGE leaf of `target_axis`; 0 when there is none or several.
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
        if (found != 0 && found != t) return 0;
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

// The distinct RANGE leaves under `root`, every store of an AFTER root
// included, into out[0..cap); returns the count.
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

// tinygrad split_targets (codegen/opt/postrange.py:14-15), checked in shift_to
// (:80).  thvm's KAX_LOOP is tinygrad's WEAK, and a REDUCE axis splits into a
// local by KOP_GROUP (KAX_GROUP_REDUCE, tinygrad's GROUP_REDUCE before #18388).
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

// 1 iff a UOP_REDUCE reachable from `root` reduces `axis_id`, tinygrad's
// Scheduler.reduce_axes (codegen/opt/postrange.py:92-95).
fn int uop_dag_reduces_axis(Term root, u32 axis_id) {
  return apply_opt_dag_reduce_of(root, axis_id) != 0;
}

// KOP_UPCAST/UNROLL/LOCAL: split RANGE `target_axis` (extent E) into an outer
// E/k and an inner k at id target_axis+1, move later ids up one, and read the
// old leaf as outer*k + inner.  0 unless k divides E and kop_split_source_ok.
fn Term uop_dag_apply_split(Term root, u8 op, u32 target_axis, u32 k) {
  if (k == 0) return 0;
  Term old_leaf = apply_opt_dag_find_range(root, target_axis);
  if (old_leaf == 0) {
    return 0;
  }
  u32 extent = uop_range_extent(old_leaf);
  // A symbolic (kvar) extent packs a var id that `/ k` would turn into a bogus
  // bound; tinygrad's upcastable_dims and unrollable_dims skip symbolic dims.
  if (kvar_extent_is_var(extent)) {
    return 0;
  }
  if (extent % k != 0) {
    return 0;
  }
  u32 axis_type_outer = uop_range_axis_type(old_leaf);
  if (!kop_split_source_ok(op, axis_type_outer)) {
    return 0;
  }
  // tinygrad codegen/opt/postrange.py:128-130: UNROLL only a reduced axis.
  if (op == KOP_UNROLL && !uop_dag_reduces_axis(root, target_axis)) {
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
  st.reduce_shift_above = target_axis;
  st.reduce_swap_a     = 0xFFFFFFFFu;
  st.reduce_swap_b     = 0xFFFFFFFFu;
  st.remap_n = 0;
  return apply_opt_dag_substitute(root, &st);
}

// Split ownership: tinygrad keeps a split's outer*k + inner in REDUCE.src[1:]
// (codegen/opt/postrange.py:79-87).  thvm reads the owner off that IADD, as
// nested reduces all hold `inner`; a walk that missed it would say "not owned".
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

// No group inside another reduce (tinygrad codegen/opt/postrange.py:131-134):
// no reduce range may be open at the REDUCE, in its body yet closed neither by
// it nor by a reduce there (uop/ops.py:469-497).

// A REDUCE closes the axes it names and the inner halves of their splits.
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

// A REDUCE, UNROLL or GROUP_REDUCE range still open at `red`, or 0: the check's
// (REDUCE, UNROLL) plus the GROUP_REDUCE it listed before tinygrad #18388.
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

// KOP_GROUP/GROUPTOP: stamp the REDUCE axis KAX_GROUP_REDUCE and wrap its
// leaves in OPT(_, GROUP_REDUCE, k), rendered as a threadgroup-shared reduce
// (rmu_emit_group_reduce).  No id split: the k threads stride the full extent.
fn Term uop_dag_apply_group_reduce(Term root, u32 target_axis, u32 k) {
  if (k == 0) return 0;
  Term ranges[APPLY_OPT_DAG_SPLIT_MAP_CAP];
  u32 n_ranges = apply_opt_dag_collect_ranges(root, ranges,
                                              APPLY_OPT_DAG_SPLIT_MAP_CAP);
  ApplyOptDagSplitCtx ctx;
  ctx.n = 0;
  u32 extent = 0;
  // Every leaf of the axis, not one: an im2col conv indexes x and w through
  // separate RANGE Terms that share the axis id.
  for (u32 i = 0; i < n_ranges; i++) {
    Term r = ranges[i];
    if (uop_range_axis_id(r) != target_axis) continue;
    if (!kop_split_source_ok(KOP_GROUP, uop_range_axis_type(r))) {
      return 0;
    }
    u32 e = uop_range_extent(r);
    if (e == 0 || e % k != 0) {
      return 0;
    }
    if (extent == 0) extent = e;
    else if (e != extent) {
      return 0;
    }
    if (ctx.n >= APPLY_OPT_DAG_SPLIT_MAP_CAP) return 0;
    Term new_leaf = uop_range(target_axis, KAX_GROUP_REDUCE, e);
    ctx.entries[ctx.n].key = r;
    ctx.entries[ctx.n].val = uop_opt(new_leaf, UOP_OPT_GROUP_REDUCE, k);
    ctx.n++;
  }
  if (ctx.n == 0) {
    return 0;
  }
  // A REDUCE must reduce the axis and not sit inside another reduce (tinygrad
  // codegen/opt/postrange.py:128-134).
  Term red = apply_opt_dag_reduce_of(root, target_axis);
  if (red == 0) {
    return 0;
  }
  Term open = apply_opt_dag_open_reduce_range(red);
  if (open != 0) {
    return 0;
  }

  ApplyOptDagSubState st;
  st.memo_n             = 0;
  st.map                = &ctx;
  st.reduce_shift_above = 0xFFFFFFFFu;
  st.reduce_swap_a      = 0xFFFFFFFFu;
  st.reduce_swap_b      = 0xFFFFFFFFu;
  st.remap_n = 0;
  return apply_opt_dag_substitute(root, &st);
}

// KOP_SWAP: exchange the ids of axes a and b in their RANGE leaves and REDUCE
// axes.  Returns `root` unchanged when neither axis has a leaf.
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
  st.reduce_shift_above = 0xFFFFFFFFu;
  st.reduce_swap_a     = axis_a;
  st.reduce_swap_b     = axis_b;
  st.remap_n = 0;
  return apply_opt_dag_substitute(root, &st);
}

// Renumber a kernel's RANGE ids and REDUCE axes to 0..n-1 in their order, as
// each tinygrad kernel's axes start fresh; identical kernels then render
// byte-identical source.  Returns `root` when the ids are already dense.
fn Term uop_dag_renumber_axes(Term root) {
  if (root == 0 || term_tag(root) != TAG_UOP) return root;
  Term ranges[APPLY_OPT_DAG_SPLIT_MAP_CAP];
  u32 n_ranges = apply_opt_dag_collect_ranges(root, ranges,
                                              APPLY_OPT_DAG_SPLIT_MAP_CAP);
  if (n_ranges == 0) return root;
  u32 ids[APPLY_OPT_DAG_SPLIT_MAP_CAP]; u32 n_ids = 0;
  for (u32 i = 0; i < n_ranges; i++) {
    u32 a = uop_range_axis_id(ranges[i]);
    int seen = 0;
    for (u32 j = 0; j < n_ids; j++) if (ids[j] == a) { seen = 1; break; }
    if (!seen && n_ids < APPLY_OPT_DAG_SPLIT_MAP_CAP) ids[n_ids++] = a;
  }
  for (u32 i = 1; i < n_ids; i++) {
    u32 v = ids[i]; i32 j = (i32)i - 1;
    while (j >= 0 && ids[j] > v) { ids[j + 1] = ids[j]; j--; }
    ids[j + 1] = v;
  }
  int identity = 1;
  for (u32 i = 0; i < n_ids; i++) if (ids[i] != i) { identity = 0; break; }
  if (identity) return root;
  u32 remap_new[APPLY_OPT_DAG_SPLIT_MAP_CAP];
  for (u32 i = 0; i < n_ids; i++) remap_new[i] = i;
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
      return 0;  // PADTO and NOLOCALS are not supported
  }
}
