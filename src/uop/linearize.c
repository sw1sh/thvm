// uop/linearize.c - port of tinygrad codegen/late/linearizer.py to
// thvm's UOp graph layer.
//
// Stage (a) of the architectural port arc started in src/uop/expander.c
// + src/uop/devectorize.c.  Those passes rewrite a heuristic-tagged
// UOp DAG into the post-late-pass shape (PLACEHOLDER acc + STORE-back
// + END + STACK + scalar ALU + folded wide LOADs).  The linearizer
// flattens that DAG into an ordered list of UOp Terms in emission
// order.  A subsequent render_linearized.c walks the list and writes
// target source.
//
// The algorithm mirrors tinygrad's `linearize()`
// (tinygrad/codegen/late/linearizer.py:7-52).  It is a priority-
// guided topological sort:
//
//   1. Toposort the DAG once.
//   2. For each node, compute (run_count, priority, extra) where
//      run_count = product of enclosing range extents and priority
//      is a small bias by opcode (LOAD early, STORE late, RANGE later,
//      END earlier).
//   3. Number nodes in "ideal" order (sort by priority tuple).
//   4. Pop nodes in toposorted order but using the ideal-order number
//      as the priority key on a min-heap, producing the final list.
//
// We use a straightforward array-backed min-heap (no STL) and a
// closed-addressing hash for the out_degree / nkey lookups.  The
// linearizer makes no allocations on the main heap; everything lives
// in a transient LinKernel struct the caller owns.
//
// References:
//   tinygrad/codegen/late/linearizer.py:7-52  linearize()
//   tinygrad/codegen/__init__.py:126-129       do_linearize wrapper

// === Public types ========================================================
//
// LinKernel + LIN_KERNEL_CAP live in src/thvm.h so callers (renderer,
// tests) can include them without pulling in the implementation here.
// LinKernel.uops is a fixed-cap arena suitable for kernel-sized DAGs
// (observed worst-case ~1600 UOps at BS=128 conv).
//
// === Internal: priority tuple ===========================================
//
// (run_count, priority, extra) packed into a single u64 so we can use
// it as a min-heap key directly:
//
//   bits 63..40  run_count (24 bits -- product of range extents; capped)
//   bits 39..16  priority  (24 bits, signed-biased)
//   bits 15..0   extra     (16 bits -- placeholder; we don't currently
//                           differentiate PARAM/DEFINE_VAR slots like
//                           tinygrad does for its asm backend, so this
//                           stays 0)
//
// Smaller values sort earlier.  The priority bias adds 0x800000 so the
// "negative" values from tinygrad's table become non-negative u24.

#define LIN_PRIO_BIAS  0x800000u

static u64 lin_priority_pack(u32 run_count, i32 priority, u32 extra) {
  if (run_count > 0xFFFFFFu) run_count = 0xFFFFFFu;
  u32 pri = (u32)((i64)priority + (i64)LIN_PRIO_BIAS);
  if (pri > 0xFFFFFFu) pri = 0xFFFFFFu;
  if (extra > 0xFFFFu) extra = 0xFFFFu;
  return ((u64)run_count << 40) | ((u64)pri << 16) | (u64)extra;
}

// Opcode -> priority bias.  Matches linearizer.py:23-33.  Lower priority
// values sort earlier in the ideal-order key.
static i32 lin_opcode_priority(u32 op) {
  switch (op) {
    // tinygrad PARAM/DEFINE_VAR/DEFINE_LOCAL/DEFINE_REG have no direct
    // thvm analog -- our buffers are bound by the kernel signature
    // and accumulator registers are PLACEHOLDER nodes.  We give
    // PLACEHOLDER the equivalent "very early" priority so accumulator
    // decls float to the top of the body.
    case UOP_PLACEHOLDER: return -17;
    case UOP_BUFFER:      return -18;
    case UOP_LOAD:        return -1;
    case UOP_STORE:       return  1;
    case UOP_RANGE:       return  5;
    case UOP_END:         return -5;
    default:              return  0;
  }
}

// === Internal: visited-set + out-degree table ==========================
//
// We need: (i) toposort discovery, (ii) per-node out-degree decrement.
// Both keyed on Term value (u64).  A small open-addressed hash on a
// stack-allocated array suffices for the kernel sizes we see.

#define LIN_HT_CAP  16384

typedef struct {
  Term  key;     // 0 = empty slot
  u32   val;     // generic payload (idx, out_degree, nkey)
} LinHashEntry;

typedef struct {
  LinHashEntry e[LIN_HT_CAP];
} LinHash;

static void lin_hash_reset(LinHash *h) {
  memset(h, 0, sizeof(*h));
}

static u32 lin_hash_probe(Term key) {
  u64 k = (u64)key;
  k ^= k >> 33; k *= 0xff51afd7ed558ccdULL;
  k ^= k >> 33; k *= 0xc4ceb9fe1a85ec53ULL;
  k ^= k >> 33;
  return (u32)k & (LIN_HT_CAP - 1);
}

// Insert or update.  Returns 1 on insert, 0 on update.
static int lin_hash_put(LinHash *h, Term key, u32 val) {
  u32 i = lin_hash_probe(key);
  for (u32 probe = 0; probe < LIN_HT_CAP; probe++) {
    LinHashEntry *e = &h->e[(i + probe) & (LIN_HT_CAP - 1)];
    if (e->key == 0)        { e->key = key; e->val = val; return 1; }
    if (e->key == key)      { e->val = val; return 0; }
  }
  return 0;
}

// Lookup.  Returns 1 + val if found (val mapped through +1 so the
// caller can use 0 as "not present"); 0 otherwise.
static u32 lin_hash_get_default(LinHash const *h, Term key, u32 def) {
  u32 i = lin_hash_probe(key);
  for (u32 probe = 0; probe < LIN_HT_CAP; probe++) {
    LinHashEntry const *e = &h->e[(i + probe) & (LIN_HT_CAP - 1)];
    if (e->key == 0)   return def;
    if (e->key == key) return e->val;
  }
  return def;
}

// === Internal: src enumerator ===========================================
//
// Different opcodes have variadic or fixed sources.  We enumerate the
// recursable Term children for each opcode, matching the contract that
// uop_arity() advertises (plus the variadic payload for STACK / END /
// AFTER that uop_arity treats as 0 for the generic rewriter).

#define LIN_SRC_CAP 16   // STACK can hit 8; END a handful; everything else <=3

static u32 lin_collect_srcs(Term t, Term out[LIN_SRC_CAP]) {
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  u32 n = 0;
  // Generic fixed-arity walker for the bulk of the opcodes.
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar && n < LIN_SRC_CAP; i++) {
    Term c = heap_read(loc + i);
    if (c != 0) out[n++] = c;
  }
  // Variadic / out-of-table cases that uop_arity reports as 0.
  switch (op) {
    case UOP_AFTER: {
      // Heap = [node, after_node].
      Term a = heap_read(loc + 0);
      Term b = heap_read(loc + 1);
      if (a != 0 && n < LIN_SRC_CAP) out[n++] = a;
      if (b != 0 && n < LIN_SRC_CAP) out[n++] = b;
      break;
    }
    case UOP_STACK: {
      u32 sn = uop_stack_n(t);
      for (u32 i = 0; i < sn && n < LIN_SRC_CAP; i++) {
        Term c = uop_stack_src(t, i);
        if (c != 0) out[n++] = c;
      }
      break;
    }
    case UOP_END: {
      u32 en = uop_end_n(t);
      for (u32 i = 0; i < en && n < LIN_SRC_CAP; i++) {
        Term r = uop_end_range(t, i);
        if (r != 0) out[n++] = r;
      }
      Term body = uop_end_body(t);
      if (body != 0 && n < LIN_SRC_CAP) {
        out[n++] = body;
      }
      break;
    }
    default:
      break;
  }
  return n;
}

// === Internal: loop nesting + run_count ==================================
//
// tinygrad's run_count is the product of the extents of u.ranges: the
// RANGEs reachable through u's srcs, minus the ones an END on the way has
// already closed (ops.py:470-499).  Before linearizing, pm_add_control_flow
// (linearizer.py:52-85) gives each loop's RANGE a src on the loop it is
// nested in, so a nested RANGE's run_count includes every enclosing extent
// and the heap opens it after whatever runs once per enclosing iteration
// (an accumulator init).  thvm's output loops have no END -- they close at
// the end of the kernel -- so the only nesting is a reduce loop inside the
// ranges its END lives in: each RANGE an END closes gets those ranges as
// extra srcs.  A RANGE closed by two ENDs has no single nesting (tinygrad
// merges or clones such ranges first, merge_reduce_ends), so uop_linearize
// declines it, as it does a cyclic nesting.

#define LIN_RANGE_CAP 256
#define LIN_MASK_WORDS (LIN_RANGE_CAP / 64)

typedef struct {
  u32  n;                                    // distinct RANGE Terms
  u32  words;                                // u64 words per range mask
  Term range  [LIN_RANGE_CAP];               // index -> RANGE Term
  u64  parent [LIN_RANGE_CAP * LIN_MASK_WORDS];
  u64  closure[LIN_RANGE_CAP * LIN_MASK_WORDS];
  u64  live   [LIN_KERNEL_CAP * LIN_MASK_WORDS];
} LinNest;

static void lin_mask_set(u64 *m, u32 k) {
  m[k >> 6] |= 1ULL << (k & 63);
}

static void lin_mask_clear(u64 *m, u32 k) {
  m[k >> 6] &= ~(1ULL << (k & 63));
}

static int lin_mask_has(u64 const *m, u32 k) {
  return (int)((m[k >> 6] >> (k & 63)) & 1ULL);
}

static void lin_mask_or(u64 *dst, u64 const *src, u32 words) {
  for (u32 w = 0; w < words; w++) {
    dst[w] |= src[w];
  }
}

// u.ranges for every toposort node: the union over its srcs, minus the
// ranges an END closes.  With `nested`, a RANGE also lives in every range
// it is nested in (the pm_add_control_flow src).
static void lin_nest_live(LinKernel const *topo, LinHash const *seen,
                          LinHash const *index, LinNest *nest, int nested) {
  u32 W = nest->words;
  for (u32 i = 0; i < topo->n; i++) {
    Term t  = topo->uops[i];
    u64 *m  = nest->live + (u64)i * W;
    u32  op = term_ext(t);
    memset(m, 0, (size_t)W * sizeof(u64));
    if (op == UOP_RANGE) {
      u32 k = lin_hash_get_default(index, t, 0);
      if (nested) {
        memcpy(m, nest->closure + (u64)k * W, (size_t)W * sizeof(u64));
      }
      lin_mask_set(m, k);
      continue;
    }
    Term srcs[LIN_SRC_CAP];
    u32 ns = lin_collect_srcs(t, srcs);
    for (u32 j = 0; j < ns; j++) {
      u32 p = lin_hash_get_default(seen, srcs[j], 0xFFFFFFFFu);
      if (p < topo->n) {
        lin_mask_or(m, nest->live + (u64)p * W, W);
      }
    }
    if (op == UOP_END) {
      u32 en = uop_end_n(t);
      for (u32 j = 0; j < en; j++) {
        lin_mask_clear(m, lin_hash_get_default(index, uop_end_range(t, j), 0));
      }
    }
  }
}

// Index the RANGEs, find each END-closed RANGE's parents, close them
// transitively, and fill nest->live with the nested u.ranges.  Returns 0
// if the ranges overflow LIN_RANGE_CAP, an END closes a non-RANGE, a
// RANGE is closed by two ENDs, or the nesting is cyclic.
static int lin_nest_build(LinKernel const *topo, LinHash const *seen,
                          LinHash *index, LinNest *nest) {
  lin_hash_reset(index);
  nest->n = 0;
  for (u32 i = 0; i < topo->n; i++) {
    Term t = topo->uops[i];
    if (term_ext(t) != UOP_RANGE) {
      continue;
    }
    if (nest->n >= LIN_RANGE_CAP) {
      return 0;
    }
    lin_hash_put(index, t, nest->n);
    nest->range[nest->n++] = t;
  }
  u32 W = (nest->n + 63) / 64;
  nest->words = (W == 0) ? 1 : W;
  W = nest->words;
  memset(nest->parent, 0, (size_t)nest->n * W * sizeof(u64));
  lin_nest_live(topo, seen, index, nest, 0);
  static u8 closed[LIN_RANGE_CAP];
  memset(closed, 0, sizeof(closed));
  for (u32 i = 0; i < topo->n; i++) {
    Term t = topo->uops[i];
    if (term_ext(t) != UOP_END) {
      continue;
    }
    u32 en = uop_end_n(t);
    for (u32 j = 0; j < en; j++) {
      Term r = uop_end_range(t, j);
      if (term_tag(r) != TAG_UOP || term_ext(r) != UOP_RANGE) {
        return 0;
      }
      u32 k = lin_hash_get_default(index, r, 0);
      if (closed[k]) {
        return 0;
      }
      closed[k] = 1;
      lin_mask_or(nest->parent + (u64)k * W, nest->live + (u64)i * W, W);
    }
  }
  memcpy(nest->closure, nest->parent, (size_t)nest->n * W * sizeof(u64));
  for (int changed = 1; changed; ) {
    changed = 0;
    for (u32 k = 0; k < nest->n; k++) {
      u64 *c = nest->closure + (u64)k * W;
      for (u32 p = 0; p < nest->n; p++) {
        if (!lin_mask_has(c, p)) {
          continue;
        }
        u64 const *cp = nest->closure + (u64)p * W;
        for (u32 w = 0; w < W; w++) {
          if ((c[w] | cp[w]) != c[w]) {
            c[w] |= cp[w];
            changed = 1;
          }
        }
      }
    }
  }
  for (u32 k = 0; k < nest->n; k++) {
    if (lin_mask_has(nest->closure + (u64)k * W, k)) {
      return 0;
    }
  }
  lin_nest_live(topo, seen, index, nest, 1);
  return 1;
}

// prod(extent) over topo node i's nested u.ranges, saturated to the
// 24-bit run_count field of the priority key.
static u32 lin_nest_run_count(LinNest const *nest, u32 i) {
  u64 const *m = nest->live + (u64)i * nest->words;
  u64 prod = 1;
  for (u32 k = 0; k < nest->n; k++) {
    if (!lin_mask_has(m, k)) {
      continue;
    }
    u32 extent = uop_range_extent(nest->range[k]);
    prod *= (extent == 0) ? 1 : extent;
    if (prod > 0xFFFFFFu) {
      return 0xFFFFFFu;
    }
  }
  return (u32)prod;
}

// === Internal: topological sort ========================================
//
// DFS post-order over the DAG rooted at `sink`, deduplicating via a
// visited map (LinHash with val = position-in-output).  Produces the
// classic post-order toposort that linearizer.py:9 calls with
// `list(sink.toposort())`.

static int lin_toposort(Term sink, LinKernel *out, LinHash *seen) {
  // Iterative DFS to avoid stack blowups on deep AFTER chains.
  // Stack entry: (Term, child_idx, n_children, children_cache_start).
  // We expand children on first visit, then revisit when each child
  // returns.  Children cache uses a parallel u32 stack indexing into
  // a sidecar children array.
  enum { STK_CAP = 8192, KIDS_CAP = LIN_KERNEL_CAP * 4 };
  typedef struct { Term t; u32 idx; u32 nk; u32 base; } StkE;
  static StkE  stk [STK_CAP];   // static -> not on the C stack
  static Term  kids[KIDS_CAP];
  u32 sp = 0;
  u32 kp = 0;

  if (term_tag(sink) != TAG_UOP) return 0;
  // Push root.
  if (kp + LIN_SRC_CAP > KIDS_CAP) return 0;
  u32 base0 = kp;
  Term tmp[LIN_SRC_CAP];
  u32 n0 = lin_collect_srcs(sink, tmp);
  for (u32 i = 0; i < n0; i++) { kids[kp++] = tmp[i]; }
  stk[sp++] = (StkE){ sink, 0, n0, base0 };

  while (sp > 0) {
    StkE *top = &stk[sp - 1];
    if (top->idx >= top->nk) {
      // Post-order: emit `top->t` if unseen.
      if (lin_hash_get_default(seen, top->t, 0xFFFFFFFFu) == 0xFFFFFFFFu) {
        if (out->n >= LIN_KERNEL_CAP) return 0;
        lin_hash_put(seen, top->t, out->n);
        out->uops[out->n++] = top->t;
      }
      // Free kids slice we appended (LIFO release).
      kp = top->base;
      sp--;
      continue;
    }
    Term child = kids[top->base + top->idx];
    top->idx++;
    // Skip if already seen.
    if (lin_hash_get_default(seen, child, 0xFFFFFFFFu) != 0xFFFFFFFFu) continue;
    if (term_tag(child) != TAG_UOP) {
      // Non-UOP leaf (TEN/NUM): record in `seen` (so dedup is fine)
      // but DON'T emit -- the linearized list is UOps only.
      lin_hash_put(seen, child, 0xFFFFFFFEu);   // sentinel: seen, not emitted
      continue;
    }
    // Push child.
    if (sp >= STK_CAP) return 0;
    u32 base = kp;
    Term tmp2[LIN_SRC_CAP];
    u32 nn = lin_collect_srcs(child, tmp2);
    if (kp + nn > KIDS_CAP) return 0;
    for (u32 i = 0; i < nn; i++) { kids[kp++] = tmp2[i]; }
    stk[sp++] = (StkE){ child, 0, nn, base };
  }
  return 1;
}

// === Internal: out-degree pass =========================================
//
// For each UOp in the toposort, for each src, increment src's out_degree.
// Mirrors linearizer.py:15-16.

#define LIN_NESTED_SRC_CAP (LIN_SRC_CAP + LIN_RANGE_CAP)

// lin_collect_srcs plus, for a RANGE, the ranges it is nested in.
static u32 lin_collect_nested_srcs(Term t, LinNest const *nest,
                                   LinHash const *index,
                                   Term out[LIN_NESTED_SRC_CAP]) {
  u32 n = lin_collect_srcs(t, out);
  if (term_ext(t) != UOP_RANGE) {
    return n;
  }
  u64 const *pm = nest->parent
                + (u64)lin_hash_get_default(index, t, 0) * nest->words;
  for (u32 p = 0; p < nest->n; p++) {
    if (lin_mask_has(pm, p)) {
      out[n++] = nest->range[p];
    }
  }
  return n;
}

static void lin_build_out_degree(LinKernel const *lst, LinNest const *nest,
                                 LinHash const *index, LinHash *out_deg) {
  lin_hash_reset(out_deg);
  for (u32 i = 0; i < lst->n; i++) {
    Term t = lst->uops[i];
    Term srcs[LIN_NESTED_SRC_CAP];
    u32 ns = lin_collect_nested_srcs(t, nest, index, srcs);
    for (u32 j = 0; j < ns; j++) {
      if (term_tag(srcs[j]) != TAG_UOP) continue;
      u32 cur = lin_hash_get_default(out_deg, srcs[j], 0);
      lin_hash_put(out_deg, srcs[j], cur + 1);
    }
  }
}

// === Internal: ideal-order key (nkey) assignment ========================
//
// Sort the toposort by priority tuple, then assign each Term a u32
// index in that sorted order.  Mirrors linearizer.py:37.

// Sort indices by priority key.
static void lin_sort_by_key(u32 *idx, u32 n, u64 const *keys) {
  // Insertion sort; n is at most LIN_KERNEL_CAP.  For LIN_KERNEL_CAP =
  // 4096 this is O(n^2) worst-case but the partial-order locality of
  // toposorted input keeps the practical cost down.  Replace with a
  // proper sort if profiling shows it.
  for (u32 i = 1; i < n; i++) {
    u32 cur = idx[i];
    u64 kc = keys[cur];
    u32 j = i;
    while (j > 0) {
      u32 prev = idx[j - 1];
      if (keys[prev] <= kc) break;
      idx[j] = prev;
      j--;
    }
    idx[j] = cur;
  }
}

static void lin_assign_nkey(LinKernel const *lst, u64 const *keys,
                            LinHash *nkey_map) {
  // Allocate a sort-index array on the static side (LIN_KERNEL_CAP).
  static u32 idx[LIN_KERNEL_CAP];
  u32 n = lst->n;
  for (u32 i = 0; i < n; i++) idx[i] = i;
  lin_sort_by_key(idx, n, keys);
  lin_hash_reset(nkey_map);
  for (u32 i = 0; i < n; i++) {
    Term t = lst->uops[idx[i]];
    lin_hash_put(nkey_map, t, i);
  }
}

// === Internal: heap-driven re-emission =================================
//
// Pop Terms from a max-heap keyed on -nkey; for each popped Term,
// decrement its sources' out-degree and push them when they hit zero.
// Reverse the result to get the final emission order.  Mirrors
// linearizer.py:40-47.

typedef struct {
  u32  nkey;     // sort key (smaller -> emit earlier in final order;
                 // we negate it for a min-heap, but since u32 doesn't
                 // negate, we store ~nkey).
  Term t;
} LinHeapEntry;

static void lin_heap_push(LinHeapEntry *h, u32 *n, u32 nkey, Term t) {
  u32 i = (*n)++;
  h[i].nkey = ~nkey;
  h[i].t    = t;
  while (i > 0) {
    u32 p = (i - 1) >> 1;
    if (h[p].nkey <= h[i].nkey) break;
    LinHeapEntry tmp = h[p]; h[p] = h[i]; h[i] = tmp;
    i = p;
  }
}

static Term lin_heap_pop(LinHeapEntry *h, u32 *n) {
  Term root = h[0].t;
  (*n)--;
  if (*n == 0) return root;
  h[0] = h[*n];
  u32 i = 0;
  while (1) {
    u32 l = 2 * i + 1, r = 2 * i + 2, m = i;
    if (l < *n && h[l].nkey < h[m].nkey) m = l;
    if (r < *n && h[r].nkey < h[m].nkey) m = r;
    if (m == i) break;
    LinHeapEntry tmp = h[m]; h[m] = h[i]; h[i] = tmp;
    i = m;
  }
  return root;
}

// === Entry: uop_linearize(sink) -> LinKernel ===========================
//
// Returns 1 on success, 0 on capacity-overflow or shape error.  The
// caller passes an uninitialized LinKernel; on success out->n is the
// number of UOps + out->uops[0..n-1] holds them in emission order.

fn int uop_linearize(Term sink, LinKernel *out) {
  out->n = 0;
  if (term_tag(sink) != TAG_UOP) return 0;
  // Stage 1: toposort.
  static LinHash seen;
  lin_hash_reset(&seen);
  static LinKernel topo;
  topo.n = 0;
  if (!lin_toposort(sink, &topo, &seen)) return 0;

  // Stage 2: loop nesting (the pm_add_control_flow srcs) + out-degree.
  static LinHash range_index;
  static LinNest nest;
  if (!lin_nest_build(&topo, &seen, &range_index, &nest)) {
    return 0;
  }
  static LinHash out_deg;
  lin_build_out_degree(&topo, &nest, &range_index, &out_deg);

  // Stage 3: compute priority key per Term.
  static u64 keys[LIN_KERNEL_CAP];
  for (u32 i = 0; i < topo.n; i++) {
    i32 pri = lin_opcode_priority(term_ext(topo.uops[i]));
    keys[i] = lin_priority_pack(lin_nest_run_count(&nest, i), pri, 0);
  }

  // Stage 4: assign nkey by sorted-key index.
  static LinHash nkey_map;
  lin_assign_nkey(&topo, keys, &nkey_map);

  // Stage 5: heap-driven re-emission.
  static LinHeapEntry heap[LIN_KERNEL_CAP];
  u32 hn = 0;
  // Push sink first.
  u32 sink_nkey = lin_hash_get_default(&nkey_map, sink, 0);
  lin_heap_push(heap, &hn, sink_nkey, sink);
  // Buffer the new (reverse) list.
  static Term newlst[LIN_KERNEL_CAP];
  u32 newn = 0;
  while (hn > 0) {
    if (newn >= LIN_KERNEL_CAP) return 0;
    Term u = lin_heap_pop(heap, &hn);
    newlst[newn++] = u;
    Term srcs[LIN_NESTED_SRC_CAP];
    u32 ns = lin_collect_nested_srcs(u, &nest, &range_index, srcs);
    for (u32 j = 0; j < ns; j++) {
      Term v = srcs[j];
      if (term_tag(v) != TAG_UOP) continue;
      u32 cur = lin_hash_get_default(&out_deg, v, 0);
      if (cur == 0) continue;
      cur--;
      lin_hash_put(&out_deg, v, cur);
      if (cur == 0) {
        u32 vk = lin_hash_get_default(&nkey_map, v, 0);
        lin_heap_push(heap, &hn, vk, v);
      }
    }
  }
  if (newn != topo.n) {
    return 0;
  }
  // Reverse newlst into out.
  out->n = newn;
  for (u32 i = 0; i < newn; i++) {
    out->uops[i] = newlst[newn - 1 - i];
  }
  return 1;
}

// === Accessors for tests + the renderer ================================

fn u32  lin_kernel_size(LinKernel const *k)         { return k->n; }
fn Term lin_kernel_at  (LinKernel const *k, u32 i)  { return (i < k->n) ? k->uops[i] : 0; }
