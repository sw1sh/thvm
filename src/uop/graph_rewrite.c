// uop/graph_rewrite.c - named bottom-up rewrite pass over UOp DAGs.
//
// This is the UOp-level counterpart to the realize-map rule harness:
// it walks a graph bottom-up, rebuilds changed nodes through the
// canonical UOp constructors, then runs a table of named replacement
// callbacks.  It is intentionally smaller than tinygrad's UPat system;
// the first goal is to provide one reusable home for symbolic,
// movement, and schedule-IR rewrite rules as they are ported.

#define UOP_GRAPH_REWRITE_STATS_CAP 128
#define UOP_GRAPH_REWRITE_MEMO_MAX  (1u << 20)

// Past either budget the pass fails through thvm_fatal with a diagnostic,
// as tinygrad raises RuntimeError (uop/ops.py:1804 stack limit, :1815
// fixed_point_rewrite cycle): stopping early would return a graph rewritten
// only above the cut.  thvm_fatal returns an armed LibraryLink entry to WL
// as an error instead of killing the kernel.
//
// MAX_DEPTH bounds structural recursion (child and STACK-lane descent), the
// only part of the pass that grows the C stack.  Rewrites run on the calling
// thread, at least the 8 MB main-thread stack (no render runs on a worker
// pool).  A level costs one 176-byte frame on arm64 at -O2 (plus 128 bytes
// at a STACK), so the budget stays under 1.5 MB in practice and 2.5 MB at
// worst, while sitting 4x above the deepest chain tested end to end (2000
// levels in py/tests/test_deep_chain_depth.py).
//
// MAX_CHAIN bounds successive rule hits on one node.  A chain runs
// iteratively and costs no stack; a node that reappears on its own chain
// aborts at once.
#define UOP_GRAPH_REWRITE_MAX_DEPTH 8192u
#define UOP_GRAPH_REWRITE_MAX_CHAIN 1024u

typedef struct {
  char const *name;
  u32         hits;
} UOpGraphRewriteStat;

typedef struct {
  Term key;
  Term value;
} UOpGraphRewriteMemoSlot;

// One rule hit on an open chain: the node the rule fired on, and the
// replacement to memoize once the chain settles (0 when it was already
// memoized or is not a UOp node).
typedef struct {
  Term node;
  Term key;
  u32  rule;
} UOpGraphRewriteStep;

typedef struct {
  UOpGraphRewriteMemoSlot   *memo;
  u32                        memo_cap;
  u32                        memo_used;
  UOpGraphRewriteRule const *rules;
  u32                        n_rules;
  void                      *user;
  UOpGraphRewriteStep       *chain;      // every open chain, innermost last
  u32                        chain_len;
  u32                        chain_cap;
  u32                        max_depth;  // high-water marks, DUMP_UOP_REWRITE
  u32                        max_chain;
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

__attribute__((noinline, noreturn))
static void uop_graph_rebuild_fail(Term t, char const *why) {
  char msg[160];
  snprintf(msg, sizeof(msg), "uop_graph_rewrite: cannot rebuild op %u: %s",
           (u32)term_ext(t), why);
  thvm_fatal(msg);
}

static u32 uop_graph_rebuild_ndim(Term t) {
  u32 ndim = (u32)term_val(heap_read(term_val(t) + 1));
  if (ndim > MAX_DIM) {
    uop_graph_rebuild_fail(t, "ndim exceeds MAX_DIM");
  }
  return ndim;
}

// `small` when `n` values fit in it, else a heap buffer the caller frees.
static u32 *uop_graph_rebuild_buf(Term t, u32 *small, u32 small_n, u32 n) {
  if (n <= small_n) {
    return small;
  }
  u32 *buf = (u32 *)malloc((size_t)n * sizeof(u32));
  if (buf == NULL) {
    uop_graph_rebuild_fail(t, "out of memory");
  }
  return buf;
}

// UNROLL / CONTRACT: [src, NUM(n), (NUM(axis), NUM(factor)) * n].
static Term uop_graph_rebuild_axis_args(Term t, Term src) {
  u64  loc = term_val(t);
  u32  n   = (u32)term_val(heap_read(loc + 1));
  u32  small[16];
  u32 *buf  = uop_graph_rebuild_buf(t, small, 16, 2 * n);
  u32 *axes = buf;
  u32 *facs = buf + n;
  for (u32 i = 0; i < n; i++) {
    axes[i] = (u32)term_val(heap_read(loc + 2 + 2 * i + 0));
    facs[i] = (u32)term_val(heap_read(loc + 2 + 2 * i + 1));
  }
  Term out = term_ext(t) == UOP_UNROLL ? uop_unroll(src, n, axes, facs)
                                       : uop_contract(src, n, axes, facs);
  if (buf != small) {
    free(buf);
  }
  return out;
}

// Kept out of line: its argument buffers must not land in the recursive
// frame of uop_graph_rewrite_rec.
__attribute__((noinline))
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
      u32 ndim = uop_graph_rebuild_ndim(t);
      u32 dims[MAX_DIM];
      for (u32 i = 0; i < ndim; i++) {
        dims[i] = (u32)term_val(heap_read(loc + 2 + i));
      }
      return uop_reshape(srcs[0], ndim, dims);
    }

    case UOP_PERMUTE: {
      u32 ndim = uop_graph_rebuild_ndim(t);
      u32 perm[MAX_DIM];
      for (u32 i = 0; i < ndim; i++) {
        perm[i] = (u32)term_val(heap_read(loc + 2 + i));
      }
      return uop_permute(srcs[0], ndim, perm);
    }

    case UOP_EXPAND: {
      u32 ndim = uop_graph_rebuild_ndim(t);
      u32 dims[MAX_DIM];
      for (u32 i = 0; i < ndim; i++) {
        dims[i] = (u32)term_val(heap_read(loc + 2 + i));
      }
      return uop_expand(srcs[0], ndim, dims);
    }

    case UOP_PAD: {
      u32 ndim = uop_graph_rebuild_ndim(t);
      u32 widths[2 * MAX_DIM];
      for (u32 i = 0; i < 2 * ndim; i++) {
        widths[i] = (u32)term_val(heap_read(loc + 2 + i));
      }
      return uop_pad(srcs[0], ndim, widths);
    }

    case UOP_SHRINK: {
      u32 ndim = uop_graph_rebuild_ndim(t);
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
    case UOP_ISHR:
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
    case UOP_UNROLL:
    case UOP_CONTRACT:
      return uop_graph_rebuild_axis_args(t, srcs[0]);

    case UOP_GEP: {
      u32  n = (u32)term_val(heap_read(loc + 1));
      u32  small[64];
      u32 *idxs = uop_graph_rebuild_buf(t, small, 64, n);
      for (u32 i = 0; i < n; i++) {
        idxs[i] = (u32)term_val(heap_read(loc + 2 + i));
      }
      Term out = uop_gep(srcs[0], n, idxs);
      if (idxs != small) {
        free(idxs);
      }
      return out;
    }
    // UOP_VCONST / UOP_PLACEHOLDER: arity 0, no rebuild path needed.
    // UOP_STACK: arity 0 here; uop_graph_rewrite_stack_lanes rewrites and
    // rebuilds its variadic lanes.  UOP_END: arity 0, lanes are RANGEs.

    default:
      return t;
  }
}

static char const *uop_graph_rewrite_rule_name(UOpGraphRewriteState const *st,
                                               u32 rule) {
  char const *name = st->rules[rule].name;
  return name != NULL ? name : "?";
}

// Prints the rule table and the newest steps of the open chains from `base`
// up, then fails with `why` and the op of `node`.
__attribute__((noinline, noreturn))
static void uop_graph_rewrite_fail(UOpGraphRewriteState const *st,
                                   char const *why, Term node, u32 base) {
  fprintf(stderr, "uop_graph_rewrite: rules:");
  for (u32 i = 0; i < st->n_rules; i++) {
    fprintf(stderr, " %s", uop_graph_rewrite_rule_name(st, i));
  }
  fprintf(stderr, "\n");
  u32 from = st->chain_len - base > 16 ? st->chain_len - 16 : base;
  if (from < st->chain_len) {
    fprintf(stderr, "uop_graph_rewrite: chain (%u hits, newest %u):\n",
            st->chain_len - base, st->chain_len - from);
  }
  for (u32 k = from; k < st->chain_len; k++) {
    UOpGraphRewriteStep const *s = &st->chain[k];
    fprintf(stderr, "    op %u -> %s\n", (u32)term_ext(s->node),
            uop_graph_rewrite_rule_name(st, s->rule));
  }
  char msg[192];
  snprintf(msg, sizeof(msg), "uop_graph_rewrite: %s at op %u", why,
           (u32)term_ext(node));
  thvm_fatal(msg);
}

static void uop_graph_rewrite_chain_push(UOpGraphRewriteState *st, Term node,
                                         u32 rule) {
  if (st->chain_len == st->chain_cap) {
    u32 cap = st->chain_cap != 0 ? st->chain_cap * 2 : 64;
    UOpGraphRewriteStep *grown = (UOpGraphRewriteStep *)realloc(
        st->chain, (size_t)cap * sizeof(UOpGraphRewriteStep));
    if (grown == NULL) {
      uop_graph_rewrite_fail(st, "out of memory growing the chain", node,
                             st->chain_len);
    }
    st->chain     = grown;
    st->chain_cap = cap;
  }
  st->chain[st->chain_len].node = node;
  st->chain[st->chain_len].key  = 0;
  st->chain[st->chain_len].rule = rule;
  st->chain_len++;
}

// First rule that rewrites `t`: its index, with the replacement in *out.
// n_rules when none fires.
static u32 uop_graph_rewrite_match(UOpGraphRewriteState *st, Term t,
                                   Term *out) {
  for (u32 i = 0; i < st->n_rules; i++) {
    if (st->rules[i].apply == NULL) {
      continue;
    }
    Term next = st->rules[i].apply(t, st->user);
    if (next != 0 && next != t) {
      *out = next;
      return i;
    }
  }
  return st->n_rules;
}

static Term uop_graph_rewrite_rec(UOpGraphRewriteState *st, Term t, u32 depth);

// Variadic STACK: uop_arity reports 0, so the fixed-arity descent skips the
// lanes.  Rewrites each lane and rebuilds through the canonical uop_stack
// constructor (hash-conses, collapses singletons) when any lane changed.
// Out of line so the lane buffer never sits in the recursive frame.
__attribute__((noinline))
static Term uop_graph_rewrite_stack_lanes(UOpGraphRewriteState *st,
                                          Term stack, u32 depth) {
  u32   n     = uop_stack_n(stack);
  Term *lanes = NULL;
  for (u32 i = 0; i < n; i++) {
    Term old_l = uop_stack_src(stack, i);
    Term new_l = uop_graph_rewrite_rec(st, old_l, depth);
    if (new_l != old_l && lanes == NULL) {
      lanes = (Term *)malloc((size_t)n * sizeof(Term));
      if (lanes == NULL) {
        uop_graph_rewrite_fail(st, "out of memory rebuilding a STACK", stack,
                               st->chain_len);
      }
      for (u32 j = 0; j < i; j++) {
        lanes[j] = uop_stack_src(stack, j);
      }
    }
    if (lanes != NULL) {
      lanes[i] = new_l;
    }
  }
  if (lanes == NULL) {
    return stack;
  }
  Term out = uop_stack(n, lanes);
  free(lanes);
  return out;
}

// Rewrites the sources of UOp node `t` at `depth` and rebuilds it when any
// changed.
static Term uop_graph_rewrite_descend(UOpGraphRewriteState *st, Term t,
                                      u32 depth) {
  u8   op  = term_ext(t);
  u8   ar  = uop_arity(op);
  u64  loc = term_val(t);
  Term srcs[MAX_UOP_SRC] = {0};
  int  changed = 0;
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old_child = heap_read(loc + i);
    Term new_child = uop_graph_rewrite_rec(st, old_child, depth);
    srcs[i] = new_child;
    if (new_child != old_child) {
      changed = 1;
    }
  }
  Term cur = changed ? uop_graph_rebuild_with_srcs(t, srcs) : t;
  if (term_tag(cur) == TAG_UOP && term_ext(cur) == UOP_STACK) {
    cur = uop_graph_rewrite_stack_lanes(st, cur, depth);
  }
  return cur;
}

// Bottom-up: rewrite the sources, then apply the first matching rule and
// fully rewrite its replacement, repeating until no rule fires.  The
// replacement chain is a loop, not recursion, so `depth` counts structural
// descent only.  Every node on a chain maps to the chain's fixpoint.
static Term uop_graph_rewrite_rec(UOpGraphRewriteState *st, Term t, u32 depth) {
  Term resolved = term_resolve(t);
  if (term_tag(resolved) != TAG_UOP || term_ext(resolved) == UOP_KERNEL) {
    return resolved;
  }
  Term memo_hit = 0;
  if (uop_graph_rewrite_memo_lookup(st, resolved, &memo_hit)) {
    return memo_hit;
  }
  if (depth >= UOP_GRAPH_REWRITE_MAX_DEPTH) {
    uop_graph_rewrite_fail(st, "structural depth budget exceeded (graph too "
                           "deep, or a replacement contains the node being "
                           "rewritten)", resolved, 0);
  }
  if (depth > st->max_depth) {
    st->max_depth = depth;
  }

  Term cur   = resolved;
  int  fresh = 1;                // cur's sources are not rewritten yet
  u32  base  = st->chain_len;
  for (;;) {
    if (fresh) {
      cur = uop_graph_rewrite_descend(st, cur, depth + 1);
    }
    for (u32 k = base; k < st->chain_len; k++) {
      if (st->chain[k].node == cur) {
        uop_graph_rewrite_fail(st, "rewrite chain revisits a node (infinite "
                               "loop in fixed_point_rewrite)", resolved, base);
      }
    }
    Term next = 0;
    u32  rule = uop_graph_rewrite_match(st, cur, &next);
    if (rule == st->n_rules) {
      break;
    }
    uop_graph_rewrite_stats_record(st->rules[rule].name, 1);
    uop_graph_rewrite_chain_push(st, cur, rule);
    if (st->chain_len - base > UOP_GRAPH_REWRITE_MAX_CHAIN) {
      uop_graph_rewrite_fail(st, "rewrite chain budget exceeded (a rule "
                             "keeps producing new nodes)", resolved, base);
    }
    Term key = term_resolve(next);
    fresh = 0;
    if (term_tag(key) != TAG_UOP || term_ext(key) == UOP_KERNEL) {
      cur = key;
    } else if (!uop_graph_rewrite_memo_lookup(st, key, &cur)) {
      st->chain[st->chain_len - 1].key = key;
      cur   = key;
      fresh = 1;
    }
  }

  u32 hits = st->chain_len - base;
  if (hits > st->max_chain) {
    st->max_chain = hits;
  }
  for (u32 k = base; k < st->chain_len; k++) {
    if (st->chain[k].key != 0) {
      uop_graph_rewrite_memo_insert(st, st->chain[k].key, cur);
    }
  }
  st->chain_len = base;
  uop_graph_rewrite_memo_insert(st, resolved, cur);
  return cur;
}

static int uop_graph_rewrite_dump_enabled(void) {
  char const *e = getenv("DUMP_UOP_REWRITE");
  return e != NULL && e[0] == '1';
}

static void uop_graph_rewrite_stats_dump(UOpGraphRewriteState const *st) {
  if (!uop_graph_rewrite_dump_enabled()) {
    return;
  }
  fprintf(stderr, "uop_graph_rewrite_summary rules=%u max_depth=%u "
          "max_chain=%u\n", UOP_GRAPH_REWRITE_STATS_LEN, st->max_depth,
          st->max_chain);
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
  free(st.chain);
  uop_graph_rewrite_stats_dump(&st);
  return out;
}

// === Move a compute graph to a device by COPYing every tensor leaf =======
// A lazy graph "lives on device D" iff it is COMPUTED on D, which means each
// op's operands must be on D, recursively down to the leaves.  So TToDevice /
// Tensor.to(D) wraps every TAG_TEN leaf (weights, realized tensors, host
// inputs) in COPY(leaf, D); the routed realize then computes the whole graph
// on D with each leaf uploaded once.  This is what makes an imported forward
// actually run on the GPU -- a single COPY around the root would compute the
// whole thing on CPU and only move the result.  Mirrors tinygrad moving a
// model's parameters with model.to(D), not copying only the output.
//
// A bare realized tensor is the degenerate single-leaf case (one upload).  An
// existing COPY is left intact: it already names its src's device, so we don't
// recurse into it (which would double-wrap its leaf).  Memoized by term so a
// shared leaf yields one COPY (uploaded once).
//
// Past UOP_GRAPH_REWRITE_MAX_DEPTH it fails: returning the subtree unwrapped
// would leave its leaves on the old device, a mixed-device graph.
static void uop_to_device_leaves_check_depth(Term r, u32 depth) {
  if (depth >= UOP_GRAPH_REWRITE_MAX_DEPTH) {
    char msg[96];
    snprintf(msg, sizeof(msg),
             "uop_to_device_leaves: depth budget exceeded at op %u",
             (u32)term_ext(r));
    thvm_fatal(msg);
  }
}

static Term uop_to_device_leaves_rec(Term t, i32 dev,
                                     UOpGraphRewriteMemoSlot *memo,
                                     u32 cap, u32 *used, u32 depth) {
  Term r   = term_resolve(t);
  u8   tag = term_tag(r);
  if (tag == TAG_TEN) return uop_copy_dev(r, dev);     // the leaf upload
  if (tag != TAG_UOP) return r;                        // NUM / VAR: leave as-is
  u8 op = term_ext(r);
  if (op == UOP_KERNEL) return r;                      // opaque
  if (op == UOP_COPY) {
    // An EXPLICIT-device COPY already names where its src lives -- respect it.
    // A GENERIC COPY (device -1, the deferred-upload a TFromNet weight uses via
    // TUOpCopy[TTensorCreateHost[...]]) otherwise resolves its target to
    // CURRENT_BACKEND at materialize time, which is CPU during a routed realize
    // -- so the weight would stay on CPU while leaf-inserted inputs go to `dev`,
    // and the mixed-device kernel computes garbage.  Re-target it to `dev` so
    // every operand lands on the same device.
    if (uop_copy_device(term_val(r)) >= 0) return r;
    uop_to_device_leaves_check_depth(r, depth);
    Term csrc = uop_to_device_leaves_rec(heap_read(term_val(r) + 0),
                                         dev, memo, cap, used, depth + 1);
    return uop_copy_dev(csrc, dev);
  }

  Term memo_hit = 0;
  if (cap != 0) {
    u32 h = uop_graph_rewrite_hash(r, cap);
    for (u32 p = 0; p < cap; p++) {
      u32 i = (h + p) & (cap - 1);
      if (memo[i].key == 0) break;
      if (memo[i].key == r) { memo_hit = memo[i].value; break; }
    }
    if (memo_hit != 0) return memo_hit;
  }
  uop_to_device_leaves_check_depth(r, depth);

  u8  ar  = uop_arity(op);
  u64 loc = term_val(r);
  Term srcs[MAX_UOP_SRC] = {0};
  int changed = 0;
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    Term old_child = heap_read(loc + i);
    Term new_child = uop_to_device_leaves_rec(old_child, dev, memo, cap, used, depth + 1);
    srcs[i] = new_child;
    if (new_child != old_child) changed = 1;
  }
  Term cur = changed ? uop_graph_rebuild_with_srcs(r, srcs) : r;

  if (cap != 0 && *used * 2 < cap) {
    u32 h = uop_graph_rewrite_hash(r, cap);
    for (u32 p = 0; p < cap; p++) {
      u32 i = (h + p) & (cap - 1);
      if (memo[i].key == r)  { memo[i].value = cur; break; }
      if (memo[i].key == 0)  { memo[i].key = r; memo[i].value = cur; (*used)++; break; }
    }
  }
  return cur;
}

fn Term uop_to_device_leaves(Term root, i32 dev) {
  u32 cap = uop_graph_rewrite_memo_cap();
  UOpGraphRewriteMemoSlot *memo =
      (UOpGraphRewriteMemoSlot *)calloc(cap, sizeof(memo[0]));
  if (memo == NULL) cap = 0;     // OOM: still correct, just unshared COPYs
  u32 used = 0;
  Term out = uop_to_device_leaves_rec(root, dev, memo, cap, &used, 0);
  free(memo);
  return out;
}
