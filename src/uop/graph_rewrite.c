// uop/graph_rewrite.c - named bottom-up rewrite pass over UOp DAGs.
//
// This is the UOp-level counterpart to the realize-map rule harness:
// it walks a graph bottom-up, rebuilds changed nodes through the
// canonical UOp constructors, then runs a table of named replacement
// callbacks.  It is intentionally smaller than tinygrad's UPat system;
// the first goal is to provide one reusable home for symbolic,
// movement, and schedule-IR rewrite rules as they are ported.

#define UOP_GRAPH_REWRITE_STATS_CAP    128
#define UOP_GRAPH_REWRITE_MAX_RESTARTS 32
#define UOP_GRAPH_REWRITE_MEMO_MAX     (1u << 20)

typedef struct {
  char const *name;
  u32         hits;
} UOpGraphRewriteStat;

typedef struct {
  Term key;
  Term value;
} UOpGraphRewriteMemoSlot;

typedef struct {
  UOpGraphRewriteMemoSlot *memo;
  u32                      memo_cap;
  u32                      memo_used;
  UOpGraphRewriteRule const *rules;
  u32                      n_rules;
  void                    *user;
} UOpGraphRewriteState;

static UOpGraphRewriteStat UOP_GRAPH_REWRITE_STATS[UOP_GRAPH_REWRITE_STATS_CAP];
static u32                 UOP_GRAPH_REWRITE_STATS_LEN = 0;

fn void uop_graph_rewrite_stats_clear(void) {
  UOP_GRAPH_REWRITE_STATS_LEN = 0;
}

static void uop_graph_rewrite_stats_record(char const *name, u32 hits) {
  if (name == NULL) {
    return;
  }
  for (u32 i = 0; i < UOP_GRAPH_REWRITE_STATS_LEN; i++) {
    if (strcmp(UOP_GRAPH_REWRITE_STATS[i].name, name) == 0) {
      UOP_GRAPH_REWRITE_STATS[i].hits += hits;
      return;
    }
  }
  if (UOP_GRAPH_REWRITE_STATS_LEN >= UOP_GRAPH_REWRITE_STATS_CAP) {
    return;
  }
  UOP_GRAPH_REWRITE_STATS[UOP_GRAPH_REWRITE_STATS_LEN].name = name;
  UOP_GRAPH_REWRITE_STATS[UOP_GRAPH_REWRITE_STATS_LEN].hits = hits;
  UOP_GRAPH_REWRITE_STATS_LEN++;
}

fn u32 uop_graph_rewrite_stat_hits(char const *name) {
  if (name == NULL) {
    return 0;
  }
  for (u32 i = 0; i < UOP_GRAPH_REWRITE_STATS_LEN; i++) {
    if (strcmp(UOP_GRAPH_REWRITE_STATS[i].name, name) == 0) {
      return UOP_GRAPH_REWRITE_STATS[i].hits;
    }
  }
  return 0;
}

static u32 uop_graph_rewrite_hash(Term t, u32 cap) {
  u64 x = t;
  x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
  x ^= x >> 33;
  return (u32)x & (cap - 1);
}

static u32 uop_graph_rewrite_memo_cap(void) {
  u64 need = HEAP_NEXT > 0 ? HEAP_NEXT * 2 : 1024;
  u32 cap = 1024;
  while ((u64)cap < need && cap < UOP_GRAPH_REWRITE_MEMO_MAX) {
    cap <<= 1;
  }
  return cap;
}

static int uop_graph_rewrite_memo_lookup(UOpGraphRewriteState *st,
                                         Term key,
                                         Term *out) {
  if (st->memo == NULL || st->memo_cap == 0) {
    return 0;
  }
  u32 h = uop_graph_rewrite_hash(key, st->memo_cap);
  for (u32 p = 0; p < st->memo_cap; p++) {
    u32 i = (h + p) & (st->memo_cap - 1);
    if (st->memo[i].key == 0) {
      return 0;
    }
    if (st->memo[i].key == key) {
      *out = st->memo[i].value;
      return 1;
    }
  }
  return 0;
}

static void uop_graph_rewrite_memo_insert(UOpGraphRewriteState *st,
                                          Term key,
                                          Term value) {
  if (st->memo == NULL || st->memo_cap == 0) {
    return;
  }
  if (st->memo_used * 2 >= st->memo_cap) {
    return;
  }
  u32 h = uop_graph_rewrite_hash(key, st->memo_cap);
  for (u32 p = 0; p < st->memo_cap; p++) {
    u32 i = (h + p) & (st->memo_cap - 1);
    if (st->memo[i].key == key) {
      st->memo[i].value = value;
      return;
    }
    if (st->memo[i].key == 0) {
      st->memo[i].key   = key;
      st->memo[i].value = value;
      st->memo_used++;
      return;
    }
  }
}

static Term uop_graph_rebuild_with_srcs(Term t, const Term *srcs) {
  u8  op  = term_ext(t);
  u64 loc = term_val(t);
  switch (op) {
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2: case UOP_SQRT:
      return uop_unary(op, srcs[0]);

    case UOP_ADD: case UOP_MUL: case UOP_CMPLT:
    case UOP_CMPEQ: case UOP_ASSIGN:
      return uop_binary(op, srcs[0], srcs[1]);

    case UOP_REDUCE: {
      // Multi-axis REDUCE: copy the full axes array; n_axes==1 case stays
      // single-axis identical to the old reduce(kind, axis, src).
      u32 kind   = uop_reduce_kind(t);
      u32 n_axes = uop_reduce_n_axes(t);
      u32 axes[MAX_DIM];
      for (u32 i = 0; i < n_axes; i++) axes[i] = uop_reduce_axis(t, i);
      return uop_reduce_multi(kind, n_axes, axes, srcs[0]);
    }

    case UOP_RESHAPE: {
      u32 ndim = (u32)term_val(heap_read(loc + 1));
      if (ndim > MAX_DIM) {
        return t;
      }
      u32 dims[MAX_DIM];
      for (u32 i = 0; i < ndim; i++) {
        dims[i] = (u32)term_val(heap_read(loc + 2 + i));
      }
      return uop_reshape(srcs[0], ndim, dims);
    }

    case UOP_PERMUTE: {
      u32 ndim = (u32)term_val(heap_read(loc + 1));
      if (ndim > MAX_DIM) {
        return t;
      }
      u32 perm[MAX_DIM];
      for (u32 i = 0; i < ndim; i++) {
        perm[i] = (u32)term_val(heap_read(loc + 2 + i));
      }
      return uop_permute(srcs[0], ndim, perm);
    }

    case UOP_EXPAND: {
      u32 ndim = (u32)term_val(heap_read(loc + 1));
      if (ndim > MAX_DIM) {
        return t;
      }
      u32 dims[MAX_DIM];
      for (u32 i = 0; i < ndim; i++) {
        dims[i] = (u32)term_val(heap_read(loc + 2 + i));
      }
      return uop_expand(srcs[0], ndim, dims);
    }

    case UOP_PAD: {
      u32 ndim = (u32)term_val(heap_read(loc + 1));
      if (ndim > MAX_DIM) {
        return t;
      }
      u32 widths[2 * MAX_DIM];
      for (u32 i = 0; i < 2 * ndim; i++) {
        widths[i] = (u32)term_val(heap_read(loc + 2 + i));
      }
      return uop_pad(srcs[0], ndim, widths);
    }

    case UOP_SHRINK: {
      u32 ndim = (u32)term_val(heap_read(loc + 1));
      if (ndim > MAX_DIM) {
        return t;
      }
      u32 bounds[2 * MAX_DIM];
      for (u32 i = 0; i < 2 * ndim; i++) {
        bounds[i] = (u32)term_val(heap_read(loc + 2 + i));
      }
      return uop_shrink(srcs[0], ndim, bounds);
    }

    case UOP_FLIP: {
      u32 axes = (u32)term_val(heap_read(loc + 1));
      return uop_flip(srcs[0], axes);
    }

    case UOP_LOAD:
      return uop_load(srcs[0]);

    case UOP_CAST: {
      u32 dtype = (u32)term_val(heap_read(loc + 1));
      return uop_cast(srcs[0], dtype);
    }

    case UOP_BITCAST: {
      u32 dtype = (u32)term_val(heap_read(loc + 1));
      return uop_bitcast(srcs[0], dtype);
    }

    // === Symbolic INDEX layer ===
    // Production lifter output nests UOP_RANGE leaves inside
    // UOP_INDEX_E.addr (a tree of UOP_IADD/IMUL/...).  Without
    // these rebuild cases, uop_pattern_rewrite couldn't descend
    // into address arithmetic to reach the UOP_RANGE leaves an
    // axis_type-stamping rule needs to rewrite.

    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR: case UOP_IXOR:
      return uop_int_binary(op, srcs[0], srcs[1]);

    case UOP_IWHERE:
      return uop_iwhere(srcs[0], srcs[1], srcs[2]);

    case UOP_INDEX_E:
      return uop_index_e(srcs[0], srcs[1]);

    // UOP_RANGE has arity 0; the canonical-reconstruct path is
    // unreachable in practice (no children means changed=0), but
    // wiring it through uop_range hash-cons keeps the rebuild
    // total over every opcode the rewriter visits.
    case UOP_RANGE: {
      u32 axis_id   = (u32)term_val(heap_read(loc + 0));
      u32 axis_type = (u32)term_val(heap_read(loc + 1));
      u32 extent    = (u32)term_val(heap_read(loc + 2));
      return uop_range(axis_id, axis_type, extent);
    }

    // UOP_INVALID is a singleton via the mov-cache.  Reuse the
    // canonical constructor; rebuild is identity in practice.
    case UOP_INVALID:
      return uop_invalid();

    case UOP_OPT: {
      u32 kind   = (u32)term_val(heap_read(loc + 1));
      u32 factor = (u32)term_val(heap_read(loc + 2));
      return uop_opt(srcs[0], kind, factor);
    }

    case UOP_STORE:
      return uop_store(srcs[0], srcs[1], srcs[2]);

    // === Expander structural opcodes (src/uop/expander.c) ===
    // Arity 1: only src[0] is a recursable child.  The tail NUM cells
    // (n_args + axis_id/factor pairs, or n_idx + indices) describe the
    // op's static arg shape and don't change under graph rewrite.
    case UOP_UNROLL: {
      u32 n = (u32)term_val(heap_read(loc + 1));
      if (n > 8) return t;
      u32 axes[8];
      u32 facs[8];
      for (u32 i = 0; i < n; i++) {
        axes[i] = (u32)term_val(heap_read(loc + 2 + 2 * i + 0));
        facs[i] = (u32)term_val(heap_read(loc + 2 + 2 * i + 1));
      }
      return uop_unroll(srcs[0], n, axes, facs);
    }
    case UOP_CONTRACT: {
      u32 n = (u32)term_val(heap_read(loc + 1));
      if (n > 8) return t;
      u32 axes[8];
      u32 facs[8];
      for (u32 i = 0; i < n; i++) {
        axes[i] = (u32)term_val(heap_read(loc + 2 + 2 * i + 0));
        facs[i] = (u32)term_val(heap_read(loc + 2 + 2 * i + 1));
      }
      return uop_contract(srcs[0], n, axes, facs);
    }
    case UOP_GEP: {
      u32 n = (u32)term_val(heap_read(loc + 1));
      if (n > 256) return t;
      u32 idxs[256];
      for (u32 i = 0; i < n; i++) {
        idxs[i] = (u32)term_val(heap_read(loc + 2 + i));
      }
      return uop_gep(srcs[0], n, idxs);
    }
    // UOP_VCONST / UOP_PLACEHOLDER: arity 0, no rebuild path needed.
    // UOP_STACK / UOP_END: variadic Term payload, arity-0 in the
    // uop_meta table; nodes are constructed at devectorizer-pass output
    // and rewrites do not need to descend into them.  If a future pass
    // needs to recurse, add the rebuild explicitly here.

    default:
      return t;
  }
}

static Term uop_graph_rewrite_rec(UOpGraphRewriteState *st,
                                  Term t,
                                  u32 depth) {
  if (depth > 256) {
    return t;
  }
  Term resolved = term_resolve(t);
  if (term_tag(resolved) != TAG_UOP) {
    return resolved;
  }
  if (term_ext(resolved) == UOP_KERNEL) {
    return resolved;
  }

  Term memo_hit = 0;
  if (uop_graph_rewrite_memo_lookup(st, resolved, &memo_hit)) {
    return memo_hit;
  }

  u8  op  = term_ext(resolved);
  u8  ar  = uop_arity(op);
  u64 loc = term_val(resolved);
  Term srcs[MAX_UOP_SRC] = {0};
  int changed = 0;
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old_child = heap_read(loc + i);
    Term new_child = uop_graph_rewrite_rec(st, old_child, depth + 1);
    srcs[i] = new_child;
    if (new_child != old_child) {
      changed = 1;
    }
  }

  Term cur = changed ? uop_graph_rebuild_with_srcs(resolved, srcs)
                     : resolved;

  // Variadic STACK descent.  uop_arity returns 0 for STACK so the
  // generic fixed-arity walk above leaves the lanes untouched -- but
  // the sym pass (and any future graph rewrite running over post-
  // devectorize lanes) needs to recurse into each src and rebuild the
  // STACK if any lane changed.  The lane reads + rebuild use the
  // canonical uop_stack constructor, which hash-conses on the new
  // lane Terms and collapses singletons.
  if (term_tag(cur) == TAG_UOP && term_ext(cur) == UOP_STACK) {
    u32 n = uop_stack_n(cur);
    if (n > 0 && n <= 256) {
      Term lanes[256];
      int stack_changed = 0;
      for (u32 i = 0; i < n; i++) {
        Term old_l = uop_stack_src(cur, i);
        Term new_l = uop_graph_rewrite_rec(st, old_l, depth + 1);
        lanes[i] = new_l;
        if (new_l != old_l) stack_changed = 1;
      }
      if (stack_changed) {
        cur = uop_stack(n, lanes);
      }
    }
  }

  for (u32 restart = 0; restart < UOP_GRAPH_REWRITE_MAX_RESTARTS; restart++) {
    int hit = 0;
    for (u32 i = 0; i < st->n_rules; i++) {
      if (st->rules[i].apply == NULL) {
        continue;
      }
      Term next = st->rules[i].apply(cur, st->user);
      if (next == 0 || next == cur) {
        continue;
      }
      uop_graph_rewrite_stats_record(st->rules[i].name, 1);
      cur = uop_graph_rewrite_rec(st, next, depth + 1);
      hit = 1;
      break;
    }
    if (!hit) {
      break;
    }
  }

  uop_graph_rewrite_memo_insert(st, resolved, cur);
  return cur;
}

static int uop_graph_rewrite_dump_enabled(void) {
  char const *e = getenv("DUMP_UOP_REWRITE");
  return e != NULL && e[0] == '1';
}

static void uop_graph_rewrite_stats_dump(void) {
  if (!uop_graph_rewrite_dump_enabled()) {
    return;
  }
  fprintf(stderr, "uop_graph_rewrite_summary rules=%u\n",
          UOP_GRAPH_REWRITE_STATS_LEN);
  for (u32 i = 0; i < UOP_GRAPH_REWRITE_STATS_LEN; i++) {
    fprintf(stderr, "  %s hits=%u\n",
            UOP_GRAPH_REWRITE_STATS[i].name,
            UOP_GRAPH_REWRITE_STATS[i].hits);
  }
}

fn Term uop_graph_rewrite(Term root,
                          UOpGraphRewriteRule const *rules,
                          u32 n_rules,
                          void *user) {
  uop_graph_rewrite_stats_clear();
  if (rules == NULL || n_rules == 0) {
    return root;
  }

  UOpGraphRewriteState st;
  memset(&st, 0, sizeof(st));
  st.memo_cap = uop_graph_rewrite_memo_cap();
  st.memo = (UOpGraphRewriteMemoSlot *)calloc(st.memo_cap, sizeof(st.memo[0]));
  if (st.memo == NULL) {
    st.memo_cap = 0;
  }
  st.rules   = rules;
  st.n_rules = n_rules;
  st.user    = user;

  Term out = uop_graph_rewrite_rec(&st, root, 0);
  free(st.memo);
  uop_graph_rewrite_stats_dump();
  return out;
}
