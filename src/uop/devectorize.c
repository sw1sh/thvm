// uop/devectorize.c - port of tinygrad codegen/late/devectorizer.py to
// the thvm UOp graph rewrite framework.
//
// Builds on uop/expander.c (commit fc371fe3): the expander produces a
// DAG carrying vector-typed UOPs wrapped in UOP_UNROLL with VCONST /
// CONTRACT / GEP swizzles.  This file lowers that DAG into a shape the
// renderer's existing emit walk can consume:
//
//   1. uop_devectorize_graph(root) runs three sub-passes:
//      (a) pm_reduce      -- REDUCE(body, *axes) ->
//                             PLACEHOLDER(acc_id)
//                             + STORE(acc.after(*live), 0, identity)
//                             + STORE(acc.after(init), 0, acc op body)
//                             + END(update, *axes)
//                             + LOAD(acc.after(END))
//          Mirrors codegen/__init__.py:197-204 (reduce_ranges_to_acc).
//      (b) pm_devectorize -- vector-typed ALU / CAST / BITCAST split
//                            into a STACK of N scalar ALUs threading
//                            GEPs into the source operands.  Mirrors
//                            devectorizer.py:235-273.
//      (c) pm_render      -- VCONST -> STACK of scalar CONSTs;
//                            GEP(STACK(...), (i,)) -> STACK[i];
//                            GEP of a single index over a scalar src
//                            -> src; STACK with single src -> src.
//                            Mirrors devectorizer.py:275-295.
//
//   2. uop_load_store_fold_graph(root) is a SEPARATE pass implementing
//      the load-store folding from devectorizer.py:81-117 / 136-149:
//      F adjacent scalar LOADs whose addresses differ by 0..F-1 along
//      a single stride collapse to one wide LOAD + GEPs.  Gated on
//      the backend's `supports_float4`.  Currently implements the
//      detection scaffolding -- the actual fold fires when the input
//      shape matches (driven by tests for the structural case).
//
// NOT WIRED INTO render_uop.c.  The renderer still walks the RANGE-leaf
// representation directly; teaching it to emit PLACEHOLDER / END / STACK
// / GEP-folded wide LOADs is a separate change (the "wire" step from
// the task description's stage (f)).  This file lands the graph rewrite
// infrastructure + tests; wiring + the V100 wall measurement is a
// follow-up commit once the renderer is taught the new shape.
//
// References:
//   tinygrad/codegen/late/devectorizer.py:81-149   load_store_folding
//   tinygrad/codegen/late/devectorizer.py:235-273  devectorize
//   tinygrad/codegen/late/devectorizer.py:275-295  pm_render
//   tinygrad/codegen/late/devectorizer.py:311-328  reduce_to_acc
//   tinygrad/codegen/late/devectorizer.py:350-357  pm_reduce
//   tinygrad/codegen/__init__.py:60-78             pipeline order

// === Constructors ========================================================

// UOP_STACK: heap = [NUM(n), src_0, ..., src_{n-1}].  Variadic over Term
// children; we treat it as arity-0 in uop_meta so the generic walker
// stops short of the variadic payload.  Hash-cons via uop_mov_cache.
fn Term uop_stack(u32 n, Term const *srcs) {
  if (n == 0) {
    // Zero-element STACK is the empty vector marker (tinygrad uses it
    // for SINK accumulator init; we keep the slot for symmetry).
    u32 key_buf[1] = { 0 };
    u64 key = uop_mov_hash(UOP_STACK, 0, key_buf, 1);
    Term hit = uop_mov_lookup(key);
    if (hit != 0) return hit;
    u64 loc = heap_alloc(1);
    heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, 0));
    Term t = term_new(0, TAG_UOP, UOP_STACK, loc);
    uop_mov_insert(key, t);
    return t;
  }
  // Single-element STACK degenerates to its src (mirrors pm_render
  // rule devectorizer.py:282).  Callers can rely on this so the
  // expand+devec pipeline doesn't accumulate trivial STACKs.
  if (n == 1) return srcs[0];
  enum { STACK_KEY_MAX = 1 + 256 };
  if (1u + n > STACK_KEY_MAX) {
    u64 loc = heap_alloc(1 + n);
    heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, n));
    for (u32 i = 0; i < n; i++) heap_set(loc + 1 + i, srcs[i]);
    return term_new(0, TAG_UOP, UOP_STACK, loc);
  }
  u32 key_buf[STACK_KEY_MAX];
  key_buf[0] = n;
  // Hash the src Term values directly; each Term is u64 but the
  // mov-cache hash takes u32; xor the high/low halves so two
  // distinct Terms with same low 32 bits don't collide spuriously.
  for (u32 i = 0; i < n; i++) {
    key_buf[1 + i] = (u32)(((u64)srcs[i]) ^ (((u64)srcs[i]) >> 32));
  }
  u64 key = uop_mov_hash(UOP_STACK, 0, key_buf, 1 + n);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) {
    // Sanity-check the hit (low probability of cross-key collision).
    u64 loc = term_val(hit);
    u32 hit_n = (u32)term_val(heap_read(loc + 0));
    if (hit_n == n) {
      int eq = 1;
      for (u32 i = 0; i < n && eq; i++) {
        if (heap_read(loc + 1 + i) != srcs[i]) eq = 0;
      }
      if (eq) return hit;
    }
  }
  u64 loc = heap_alloc(1 + n);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, n));
  for (u32 i = 0; i < n; i++) heap_set(loc + 1 + i, srcs[i]);
  Term t = term_new(0, TAG_UOP, UOP_STACK, loc);
  uop_mov_insert(key, t);
  return t;
}

fn u32 uop_stack_n(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STACK) return 0;
  return (u32)term_val(heap_read(term_val(t) + 0));
}
fn Term uop_stack_src(Term t, u32 i) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STACK) return 0;
  return heap_read(term_val(t) + 1 + i);
}

// UOP_PLACEHOLDER: heap = [NUM(dtype), NUM(acc_id)].  Atom-like (arity 0).
// Two PLACEHOLDERs with the same (dtype, acc_id) hash-cons to the same
// Term -- the renderer should emit exactly one `<dtype> _acc<acc_id>;`
// declaration regardless of how many graph nodes reference it.
fn Term uop_placeholder(u32 dtype, u32 acc_id) {
  u32 key_buf[2] = { dtype, acc_id };
  u64 key = uop_mov_hash(UOP_PLACEHOLDER, 0, key_buf, 2);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, dtype));
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, acc_id));
  Term t = term_new(0, TAG_UOP, UOP_PLACEHOLDER, loc);
  uop_mov_insert(key, t);
  return t;
}
fn u32 uop_placeholder_dtype(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_PLACEHOLDER) return 0;
  return (u32)term_val(heap_read(term_val(t) + 0));
}
fn u32 uop_placeholder_acc_id(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_PLACEHOLDER) return 0;
  return (u32)term_val(heap_read(term_val(t) + 1));
}

// UOP_END: heap = [NUM(n), range_0, ..., range_{n-1}, body].  tinygrad's
// Ops.END(body, *ranges): the loops over the ranges close after `body`,
// the store that runs inside them, so body is a src and orders the END
// after it.  Each range_i is a UOP_RANGE Term.  We don't hash-cons
// identical END nodes -- they carry a positional meaning (close THIS loop
// body) and identical END markers in two different reduces are
// semantically distinct.
fn Term uop_end(Term body, u32 n, Term const *ranges) {
  if (n == 0) {
    return 0;
  }
  u64 loc = heap_alloc(2 + n);
  heap_set(loc + 0, term_new(0, TAG_NUM, DT_INT32, n));
  for (u32 i = 0; i < n; i++) {
    heap_set(loc + 1 + i, ranges[i]);
  }
  heap_set(loc + 1 + n, body);
  return term_new(0, TAG_UOP, UOP_END, loc);
}
fn u32 uop_end_n(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_END) return 0;
  return (u32)term_val(heap_read(term_val(t) + 0));
}
fn Term uop_end_range(Term t, u32 i) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_END) return 0;
  return heap_read(term_val(t) + 1 + i);
}
fn Term uop_end_body(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_END) {
    return 0;
  }
  return heap_read(term_val(t) + 1 + uop_end_n(t));
}

// === Helpers ===========================================================

// The CONST payload of a reduce kind's identity element, tinygrad's
// identity_element() (uop/ops.py:61): 0 for SUM, dt.min for MAX.
// dt.min (dtype.py:70-72) is -2^(bits-1) for a signed int, 0 for an
// unsigned int, False for bool, and -dt.max for a float: -inf, except
// for e4m3, which has no inf (dtype.py:74-78).  A -inf e4m3 CONST
// truncates to NaN, which the MAX combine below never replaces.  An
// int payload is the value's low 32 bits, so a signed dt.min
// sign-extends.  Returns 0 when dt.min does not fit that payload (i64),
// else 1 with the payload in *bits.
static int devec_reduce_identity_bits(u32 kind, u32 dtype, u32 *bits) {
  *bits = 0u;
  if (kind != REDUCE_MAX || dtype_is_bool(dtype)) {
    return 1;
  }
  if (dtype_is_int(dtype)) {
    if (!dtype_is_signed(dtype)) {
      return 1;
    }
    u32 width = dtype_info(dtype)->bits;
    if (width > 32) {
      return 0;
    }
    *bits = (u32)-(i64)((u64)1 << (width - 1));
    return 1;
  }
  f32 v = (dtype == DT_FP8E4M3) ? -fp8e4m3_max() : -INFINITY;
  memcpy(bits, &v, sizeof *bits);
  return 1;
}

// === Cone walk ==========================================================
//
// Visits every node reachable from a root exactly once: the uop_arity srcs
// plus the children uop_arity reports as 0 (both AFTER operands, every
// STACK lane, every END range and the END's body).  Iterative with a
// visited set, so it is linear on hash-consed DAGs and has no depth
// budget: a budgeted walk silently returns a partial answer past its
// budget (the depth-guard bug class fixed in materialize.c).  Returns 0
// only if it cannot allocate.

typedef void (*DevecVisit)(Term t, void *user);

typedef struct {
  Term *set;        // open-addressed visited set; 0 marks an empty slot
  u32   set_cap;    // power of two
  u32   set_used;
  Term *stk;
  u32   stk_n;
  u32   stk_cap;
} DevecWalk;

static u32 devec_walk_slot(Term t, u32 cap) {
  u64 x = (u64)t;
  x ^= x >> 33;
  x *= 0xff51afd7ed558ccdULL;
  x ^= x >> 33;
  return (u32)x & (cap - 1);
}

static int devec_walk_rehash(DevecWalk *w, u32 cap) {
  Term *set = (Term *)calloc(cap, sizeof(Term));
  if (set == NULL) {
    return 0;
  }
  for (u32 i = 0; i < w->set_cap; i++) {
    Term t = w->set[i];
    if (t == 0) {
      continue;
    }
    u32 j = devec_walk_slot(t, cap);
    while (set[j] != 0) {
      j = (j + 1) & (cap - 1);
    }
    set[j] = t;
  }
  free(w->set);
  w->set     = set;
  w->set_cap = cap;
  return 1;
}

// 1 if `t` is visited for the first time, 0 if seen before, -1 if the set
// cannot grow.
static int devec_walk_mark(DevecWalk *w, Term t) {
  if ((w->set_used + 1) * 2 > w->set_cap) {
    u32 cap = (w->set_cap != 0) ? w->set_cap * 2 : 256;
    if (!devec_walk_rehash(w, cap)) {
      return -1;
    }
  }
  u32 i = devec_walk_slot(t, w->set_cap);
  while (w->set[i] != 0) {
    if (w->set[i] == t) {
      return 0;
    }
    i = (i + 1) & (w->set_cap - 1);
  }
  w->set[i] = t;
  w->set_used++;
  return 1;
}

static int devec_walk_push(DevecWalk *w, Term t) {
  if (w->stk_n == w->stk_cap) {
    u32 cap = (w->stk_cap != 0) ? w->stk_cap * 2 : 256;
    Term *stk = (Term *)realloc(w->stk, cap * sizeof(Term));
    if (stk == NULL) {
      return 0;
    }
    w->stk     = stk;
    w->stk_cap = cap;
  }
  w->stk[w->stk_n++] = t;
  return 1;
}

static int devec_walk_push_children(DevecWalk *w, Term t) {
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  u8  ar  = uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    if (!devec_walk_push(w, heap_read(loc + i))) {
      return 0;
    }
  }
  switch (op) {
    case UOP_AFTER:
      return devec_walk_push(w, heap_read(loc + 0))
          && devec_walk_push(w, heap_read(loc + 1));
    case UOP_STACK: {
      u32 n = uop_stack_n(t);
      for (u32 i = 0; i < n; i++) {
        if (!devec_walk_push(w, uop_stack_src(t, i))) {
          return 0;
        }
      }
      return 1;
    }
    case UOP_END: {
      u32 n = uop_end_n(t);
      for (u32 i = 0; i < n; i++) {
        if (!devec_walk_push(w, uop_end_range(t, i))) {
          return 0;
        }
      }
      return devec_walk_push(w, uop_end_body(t));
    }
    default:
      return 1;
  }
}

static int devec_cone_walk(Term root, DevecVisit visit, void *user) {
  DevecWalk w;
  memset(&w, 0, sizeof(w));
  int ok = devec_walk_push(&w, root);
  while (ok && w.stk_n > 0) {
    Term t = w.stk[--w.stk_n];
    if (term_tag(t) != TAG_UOP) {
      continue;
    }
    int fresh = devec_walk_mark(&w, t);
    if (fresh < 0) {
      ok = 0;
    } else if (fresh > 0) {
      visit(t, user);
      ok = devec_walk_push_children(&w, t);
    }
  }
  free(w.set);
  free(w.stk);
  return ok;
}

// === pm_reduce / reduce_to_acc (devectorizer.py:311-328) ===============
//
// The one per-kernel scratch-slot counter, threaded through every rewrite
// of the pass: tinygrad's `slots` (codegen/__init__.py:310-316), which
// "remove reduces" and "add local buffers" share.  It starts one past the
// largest acc_id already in the DAG so a lowered reduce never hash-conses
// onto a live accumulator.  Any future LOCAL/REG scratch producer on the
// linearized route (a pm_add_local_buffers or group-for-reduce port) must
// draw from this counter too: its buffers render with the same `_acc<N>`
// names as these accumulators (render_linearized.c).
typedef struct {
  u32 acc_num;
} DevecReduceCtx;

static void devec_visit_acc_max(Term t, void *user) {
  if (term_ext(t) != UOP_PLACEHOLDER) {
    return;
  }
  u32 *next = (u32 *)user;
  u32 id = uop_placeholder_acc_id(t);
  if (id + 1 > *next) {
    *next = id + 1;
  }
}

// Combine an accumulator load with a body lane via the REDUCE's kind.
// Returns 0 for an unknown kind.
static Term devec_reduce_combine(u32 kind, Term acc_val, Term body) {
  if (kind == REDUCE_SUM) {
    return uop_binary(UOP_ADD, acc_val, body);
  }
  if (kind == REDUCE_MAX) {
    // No UOP_MAX in the value layer: max(acc, body) ->
    // WHERE(CMPLT(acc, body), body, acc).
    Term lt = uop_binary(UOP_CMPLT, acc_val, body);
    return uop_iwhere(lt, body, acc_val);
  }
  return 0;
}

// === Reduce ranges (codegen/__init__.py:197-204) ========================
//
// tinygrad's reduce_ranges_to_acc makes the accumulator init depend on the
// ranges the reduce input lives in, minus the reduce's own ranges:
//   input_ranges = tuple(x for x in r.src[0].ranges if x not in r.src[1:])
//   acc_init     = acc.after(*input_ranges).store(identity)
// so the init runs once per iteration of every enclosing loop.  thvm's
// REDUCE lists axis ids rather than RANGE Terms, and an inner reduce that
// is already lowered keeps its RANGE leaves reachable through its AFTER
// chain, so the scan collects the RANGE leaves of the body's cone and
// sorts them: the reduce's own axes are the loops its END closes; the rest
// are live unless an END in the body already closes them (an inner
// reduce) or they are UPCAST/UNROLL lanes rather than loops.

#define DEVEC_SCAN_CAP 32

typedef struct {
  Term red;
  Term live[DEVEC_SCAN_CAP];
  u32  n_live;
  Term own[DEVEC_SCAN_CAP];
  u32  n_own;
  u32  ended[DEVEC_SCAN_CAP];
  u32  n_ended;
  int  overflow;
} DevecRangeScan;

static int devec_reduce_has_axis(Term red, u32 axis_id) {
  u32 n = uop_reduce_n_axes(red);
  for (u32 i = 0; i < n; i++) {
    if (uop_reduce_axis(red, i) == axis_id) {
      return 1;
    }
  }
  return 0;
}

static void devec_scan_push(DevecRangeScan *s, Term *arr, u32 *n, Term t) {
  if (*n >= DEVEC_SCAN_CAP) {
    s->overflow = 1;
    return;
  }
  arr[(*n)++] = t;
}

static void devec_visit_ranges(Term t, void *user) {
  DevecRangeScan *s = (DevecRangeScan *)user;
  switch (term_ext(t)) {
    case UOP_RANGE: {
      u32 at = uop_range_axis_type(t);
      if (devec_reduce_has_axis(s->red, uop_range_axis_id(t))) {
        devec_scan_push(s, s->own, &s->n_own, t);
      } else if (at != KAX_UPCAST && at != KAX_UNROLL) {
        devec_scan_push(s, s->live, &s->n_live, t);
      }
      return;
    }
    case UOP_END: {
      u32 n = uop_end_n(t);
      for (u32 i = 0; i < n; i++) {
        if (s->n_ended >= DEVEC_SCAN_CAP) {
          s->overflow = 1;
          return;
        }
        s->ended[s->n_ended++] = uop_range_axis_id(uop_end_range(t, i));
      }
      return;
    }
    default:
      return;
  }
}

// Sort RANGE Terms by axis id so the AFTER chain and the END (and so the
// rendered source and its JIT cache key) do not depend on walk order.
static void devec_sort_ranges(Term *r, u32 n) {
  for (u32 i = 1; i < n; i++) {
    Term cur = r[i];
    u32 j = i;
    while (j > 0 && uop_range_axis_id(r[j - 1]) > uop_range_axis_id(cur)) {
      r[j] = r[j - 1];
      j--;
    }
    r[j] = cur;
  }
}

// Scan `red`'s body.  Returns 0 if the scan overflowed or cannot allocate,
// or if a reduce axis does not map to exactly one RANGE leaf in the body
// (none: there is no loop for its END to close; several: one axis would
// run as several loops); the caller then leaves the REDUCE unlowered.
static int devec_reduce_scan(Term red, DevecRangeScan *s) {
  memset(s, 0, sizeof(*s));
  s->red = red;
  if (!devec_cone_walk(uop_reduce_src(red), devec_visit_ranges, s)) {
    return 0;
  }
  if (s->overflow) {
    return 0;
  }
  u32 kept = 0;
  for (u32 i = 0; i < s->n_live; i++) {
    u32 axis_id = uop_range_axis_id(s->live[i]);
    int ended = 0;
    for (u32 j = 0; j < s->n_ended; j++) {
      if (s->ended[j] == axis_id) {
        ended = 1;
        break;
      }
    }
    if (!ended) {
      s->live[kept++] = s->live[i];
    }
  }
  s->n_live = kept;
  u32 n_axes = uop_reduce_n_axes(red);
  for (u32 i = 0; i < n_axes; i++) {
    u32 axis_id = uop_reduce_axis(red, i);
    u32 found = 0;
    for (u32 j = 0; j < s->n_own; j++) {
      if (uop_range_axis_id(s->own[j]) == axis_id) {
        found++;
      }
    }
    if (found != 1) {
      return 0;
    }
  }
  devec_sort_ranges(s->live, s->n_live);
  devec_sort_ranges(s->own, s->n_own);
  return 1;
}

// acc.after(*ranges) as nested binary AFTERs.
static Term devec_after_ranges(Term acc, Term const *ranges, u32 n) {
  Term t = acc;
  for (u32 i = 0; i < n; i++) {
    t = uop_after(t, ranges[i]);
  }
  return t;
}

// Lower one REDUCE to an accumulator (tinygrad reduce_ranges_to_acc,
// codegen/__init__.py:197-204), with `live` and `own` from
// devec_reduce_scan:
//   acc         = PLACEHOLDER(dtype, next slot)
//   init        = STORE(acc.after(*live), 0, identity)
//   acc_initted = acc.after(init, *own)
//   update      = STORE(acc_initted, 0, LOAD(acc_initted) <op> body)
//   result      = LOAD(acc.after(END(update, *own)))
// The init runs once per iteration of the live loops, the update once per
// iteration of the reduce loops.  The AFTERs order init -> update -> END
// -> result by dependency, and the linearizer nests the reduce loops
// inside the ranges the END lives in, so the init also precedes them.
// The accumulator is a 1-slot REG buffer, so the address is the CONST 0.
//
// PARALLEL ACCUMULATORS: a STACK body of F lanes is F independent
// reductions (an UPCAST'd output that reached the reduce as F lanes, not
// F distinct REDUCEs -- conv's W[cout,:] . X differs per cout), so it gets
// F accumulators, one END over all F updates, and the STACK of the F lane
// loads as its result.  Mirrors tinygrad giving the DEFINE_REG the body's
// vec(F) dtype and no_vectorized splitting it into F lanes.
static Term devec_reduce_to_acc(DevecReduceCtx *ctx, Term red) {
  if (term_tag(red) != TAG_UOP || term_ext(red) != UOP_REDUCE) {
    return 0;
  }
  u32 kind = uop_reduce_kind(red);
  if (kind != REDUCE_SUM && kind != REDUCE_MAX) {
    return 0;
  }
  Term body  = uop_reduce_src(red);
  u32  lanes = (term_tag(body) == TAG_UOP && term_ext(body) == UOP_STACK)
             ? uop_stack_n(body) : 1;
  if (lanes == 0 || lanes > 256) {
    return 0;
  }
  u32 dtype = DT_FP32;
  (void)term_dtype_in(body, 0, &dtype);
  // An i64 MAX identity does not fit the CONST payload: leave the REDUCE
  // unlowered, which the linearized renderers decline.
  u32 identity_bits;
  if (!devec_reduce_identity_bits(kind, dtype, &identity_bits)) {
    return 0;
  }
  DevecRangeScan scan;
  if (!devec_reduce_scan(red, &scan)) {
    return 0;
  }
  Term identity  = uop_const(dtype, identity_bits);
  Term zero_addr = uop_const(DT_INT32, 0);
  // A bool SUM or MAX is an OR (tinygrad uop/symbolic.py:163-166), which
  // on {0, 1} is the MAX combine; an ADD would count past 1.
  u32 combine_kind = dtype_is_bool(dtype) ? REDUCE_MAX : kind;

  Term accs[256];
  Term updates = 0;
  for (u32 i = 0; i < lanes; i++) {
    Term acc  = uop_placeholder(dtype, ctx->acc_num++);
    Term init = uop_store(devec_after_ranges(acc, scan.live, scan.n_live),
                          zero_addr, identity);
    Term initted = devec_after_ranges(uop_after(acc, init),
                                      scan.own, scan.n_own);
    Term lane = (lanes > 1) ? uop_stack_src(body, i) : body;
    Term combined = devec_reduce_combine(combine_kind, uop_load(initted), lane);
    Term update = uop_store(initted, zero_addr, combined);
    updates = (updates == 0) ? update : uop_after(update, updates);
    accs[i] = acc;
  }
  Term done = (scan.n_own > 0) ? uop_end(updates, scan.n_own, scan.own)
                               : updates;
  Term loads[256];
  for (u32 i = 0; i < lanes; i++) {
    loads[i] = uop_load(uop_after(accs[i], done));
  }
  return (lanes > 1) ? uop_stack(lanes, loads) : loads[0];
}

static Term devec_pm_reduce(Term t, void *user) {
  DevecReduceCtx *ctx = (DevecReduceCtx *)user;
  return devec_reduce_to_acc(ctx, t);
}

// === devectorize: vector ALU -> STACK of scalar ALU =====================
//
// Detects when a UOP_ADD / UOP_MUL / UOP_NEG / etc. has vector dtype
// (carried via UOP_UNROLL wrapping) and rewrites to:
//   STACK(N, [op(gep(src0, i), gep(src1, i), ...) for i in range(N)])
// Mirrors devectorizer.py:235-239 (no_vectorized_alu).
//
// We detect "vector" by structural shape: the node is wrapped in
// UOP_UNROLL.  The factor product gives the lane count.  We pull the
// inner (vector) ALU out, replicate it scalar-wise.

static int devec_alu_op_arity(u32 op) {
  switch (op) {
    case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
    case UOP_LOG2: case UOP_SQRT:
    case UOP_CAST: case UOP_BITCAST:
      return 1;
    case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
    case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
    case UOP_IMOD: case UOP_ILT: case UOP_IAND: case UOP_IOR:
    case UOP_IXOR: case UOP_ISHR:
      return 2;
    case UOP_IWHERE:
      return 3;
    default:
      return 0;
  }
}

// Build STACK(N, [op(gep(src0, i), gep(src1, i), ...) for i in N]).
// `srcs` is an array of N_SRCS Term operands (each potentially a vector
// or a scalar to be broadcast); for non-vector srcs we don't wrap in
// GEP since they're already scalar.  Returns 0 if no rewrite applies.
static Term devec_no_vec_alu(Term node) {
  if (term_tag(node) != TAG_UOP) return 0;
  u32 op = term_ext(node);
  int ar = devec_alu_op_arity(op);
  if (ar == 0) return 0;
  // Detect the vector width: look at the topmost UNROLL wrapping (or
  // STACK below already-lowered srcs).  We accept any src that's a
  // STACK to derive N from.  No vector context -> no rewrite.
  u64 loc = term_val(node);
  Term srcs[3];
  for (int i = 0; i < ar; i++) srcs[i] = heap_read(loc + i);
  u32 width = 0;
  for (int i = 0; i < ar; i++) {
    if (term_tag(srcs[i]) == TAG_UOP && term_ext(srcs[i]) == UOP_STACK) {
      u32 w = uop_stack_n(srcs[i]);
      if (width == 0) width = w;
      else if (width != w) return 0;  // mismatched widths -> bail
    }
  }
  if (width == 0 || width == 1) return 0;
  if (width > 256) return 0;
  Term out[256];
  for (u32 i = 0; i < width; i++) {
    Term lane_srcs[3];
    for (int j = 0; j < ar; j++) {
      Term s = srcs[j];
      if (term_tag(s) == TAG_UOP && term_ext(s) == UOP_STACK) {
        lane_srcs[j] = uop_stack_src(s, i);
      } else {
        // Scalar broadcast: same src on every lane.
        lane_srcs[j] = s;
      }
    }
    Term lane_node = 0;
    switch (op) {
      case UOP_NEG: case UOP_RECIP: case UOP_EXP2:
      case UOP_LOG2: case UOP_SQRT:
        lane_node = uop_unary(op, lane_srcs[0]);
        break;
      case UOP_ADD: case UOP_MUL: case UOP_CMPLT: case UOP_CMPEQ:
        lane_node = uop_binary(op, lane_srcs[0], lane_srcs[1]);
        break;
      case UOP_IADD: case UOP_ISUB: case UOP_IMUL: case UOP_IDIV:
      case UOP_IMOD: case UOP_ILT:  case UOP_IAND: case UOP_IOR:
      case UOP_IXOR: case UOP_ISHR:
        lane_node = uop_int_binary(op, lane_srcs[0], lane_srcs[1]);
        break;
      case UOP_IWHERE:
        lane_node = uop_iwhere(lane_srcs[0], lane_srcs[1], lane_srcs[2]);
        break;
      case UOP_CAST: {
        u32 dt = (u32)term_val(heap_read(loc + 1));
        lane_node = uop_cast(lane_srcs[0], dt);
        break;
      }
      case UOP_BITCAST: {
        u32 dt = (u32)term_val(heap_read(loc + 1));
        lane_node = uop_bitcast(lane_srcs[0], dt);
        break;
      }
      default:
        return 0;
    }
    out[i] = lane_node;
  }
  return uop_stack(width, out);
}

static Term devec_pm_devectorize(Term t, void *user) {
  (void)user;
  return devec_no_vec_alu(t);
}

// === devec_pm_do_contract (mirrors tinygrad expander.py:77-86 do_contract)
//
// The expander's expander_pm_do_contract folds the well-formed case
// CONTRACT(UNROLL(x, eargs), cargs) but bails when src is not an
// UNROLL.  After expander_broadcast wraps scalar leaves (BUFFER /
// CONST / RANGE) in CONTRACT-without-UNROLL markers (the "broadcast"
// shape, tinygrad expander.py:80), those wrappers reach devectorize
// untouched.  We finish the fold here so the post-devectorize graph
// is CONTRACT-free.
//
// Cases:
//   Case A (broadcast): CONTRACT(non-UNROLL src, args) ->
//                       STACK(src, src, ..., src)  [N = prod(factors)]
//   Case B (defensive copy of expander's do_contract): in case the
//                       expander couldn't see this CONTRACT (e.g. it
//                       was synthesized by a later rewrite) we still
//                       apply the GEP-swizzle fold.  Identical logic
//                       to expander_pm_do_contract.
//
// Reference: tinygrad/codegen/late/expander.py:77-86 do_contract.

static Term devec_pm_do_contract(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_CONTRACT) return 0;
  Term src = heap_read(term_val(t) + 0);
  u32 c_n = uop_contract_n_args(t);
  if (c_n == 0) return src;
  u32 c_axes[MAX_DIM];
  u32 c_factors[MAX_DIM];
  for (u32 i = 0; i < c_n && i < MAX_DIM; i++) {
    c_axes[i]    = uop_contract_axis_id(t, i);
    c_factors[i] = uop_contract_factor(t, i);
  }
  // Case A: CONTRACT(non-UNROLL src) -> STACK(src * total_factor).
  // Mirrors expander.py:80.
  //
  // GUARD: do NOT fold if src is a STORE *OR* a CONTRACT that wraps
  // a STORE.  Two reasons:
  //
  //   1. Semantics: CONTRACT(STORE, (ax, F)) means "execute this STORE
  //      F times with the contracted axis taking values 0..F-1"; the
  //      F lanes have DIFFERENT addresses (Cout substituted in addr).
  //      Emitting STACK(STORE * F) of F identical STORE Terms would
  //      silently collapse F distinct stores into one (they all share
  //      the unsubstituted addr).  Tinygrad routes this through STORE
  //      in do_expand's eligible-ops list -- thvm's expander.c skips
  //      STORE, so leave CONTRACT(STORE) intact and let the renderer's
  //      CONTRACT-bail path handle it (render_linearized.c lz_emit_body).
  //
  //   2. Termination: expander_fix_store_unroll has a known
  //      self-recursive pattern that leaves a deep chain of nested
  //      CONTRACT(STORE) wrappers (one per recursive depth-cap hit).
  //      If Case A folded the inner-of-2 CONTRACT-without-UNROLL nodes
  //      to STACK(CONTRACT(STORE) * N), the surrounding CONTRACT
  //      would then fold STACK over STACK ad infinitum, exploding
  //      the graph + hanging the linearizer's range-product walker.
  //
  // The fold IS sound + necessary for the LOAD path (case 8) where
  // identical LOAD Terms share the same source / address.
  if (term_tag(src) != TAG_UOP || term_ext(src) != UOP_UNROLL) {
    if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_STORE) return 0;
    // Walk through any chain of nested CONTRACT wrappers (a known
    // by-product of expander_fix_store_unroll's self-recursive
    // depth-cap behavior).  If the chain bottoms out at a STORE,
    // bail; otherwise fold normally.
    if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_CONTRACT) {
      Term probe = src;
      for (u32 d = 0; d < 8; d++) {
        if (term_tag(probe) != TAG_UOP || term_ext(probe) != UOP_CONTRACT) break;
        probe = heap_read(term_val(probe) + 0);
      }
      if (term_tag(probe) == TAG_UOP && term_ext(probe) == UOP_STORE) return 0;
      // Still inside CONTRACT chain after 8 layers -- treat as
      // pathological + leave it for the renderer to bail on.
      if (term_tag(probe) == TAG_UOP && term_ext(probe) == UOP_CONTRACT) return 0;
    }
    u32 total = 1;
    for (u32 i = 0; i < c_n; i++) total *= c_factors[i];
    if (total == 0 || total > 256) return 0;
    Term elts[256];
    for (u32 i = 0; i < total; i++) elts[i] = src;
    return uop_stack(total, elts);
  }
  // Case B: CONTRACT(UNROLL(x, eargs), cargs) -> UNROLL(GEP(x, sw),
  // eargs \ cargs).  Identical to expander_pm_do_contract.
  u32 e_n = uop_unroll_n_args(src);
  u32 e_axes[MAX_DIM];
  u32 e_factors[MAX_DIM];
  for (u32 i = 0; i < e_n && i < MAX_DIM; i++) {
    e_axes[i]    = uop_unroll_axis_id(src, i);
    e_factors[i] = uop_unroll_factor(src, i);
  }
  u32 r_axes[MAX_DIM];
  u32 r_factors[MAX_DIM];
  u32 r_n = 0;
  for (u32 i = 0; i < e_n; i++) {
    int in_c = 0;
    for (u32 j = 0; j < c_n; j++) {
      if (e_axes[i] == c_axes[j]) { in_c = 1; break; }
    }
    if (!in_c) {
      r_axes[r_n]    = e_axes[i];
      r_factors[r_n] = e_factors[i];
      r_n++;
    }
  }
  Term inner = heap_read(term_val(src) + 0);
  u32 total_r = 1, total_c = 1;
  for (u32 i = 0; i < r_n; i++) total_r *= r_factors[i];
  for (u32 i = 0; i < c_n; i++) total_c *= c_factors[i];
  u32 total = total_r * total_c;
  if (total == 0 || total > 256) return 0;
  u32 idx_buf[256];
  u32 written = 0;
  for (u32 ri = 0; ri < total_r; ri++) {
    u32 r_vals[MAX_DIM] = {0};
    {
      u32 rem = ri;
      for (u32 rj = 0; rj < r_n; rj++) {
        u32 k = r_n - 1 - rj;
        r_vals[k] = rem % r_factors[k];
        rem /= r_factors[k];
      }
    }
    for (u32 ci = 0; ci < total_c; ci++) {
      u32 c_vals[MAX_DIM] = {0};
      {
        u32 rem = ci;
        for (u32 cj = 0; cj < c_n; cj++) {
          u32 k = c_n - 1 - cj;
          c_vals[k] = rem % c_factors[k];
          rem /= c_factors[k];
        }
      }
      u32 e_vals[MAX_DIM] = {0};
      for (u32 ei = 0; ei < e_n; ei++) {
        u32 ax = e_axes[ei];
        int found = 0;
        for (u32 j = 0; j < r_n; j++) {
          if (r_axes[j] == ax) { e_vals[ei] = r_vals[j]; found = 1; break; }
        }
        if (!found) {
          for (u32 j = 0; j < c_n; j++) {
            if (c_axes[j] == ax) { e_vals[ei] = c_vals[j]; break; }
          }
        }
      }
      // _expand_arg_to_idx (expander.py:8-13): idx = sum(rpk[ax]*mul);
      // mul running right-to-left.
      u32 idx = 0, mul = 1;
      for (u32 ei = e_n; ei-- > 0; ) {
        idx += e_vals[ei] * mul;
        mul *= e_factors[ei];
      }
      idx_buf[written++] = idx;
    }
  }
  Term gepped = uop_gep(inner, written, idx_buf);
  if (r_n == 0) return gepped;
  return uop_unroll(gepped, r_n, r_axes, r_factors);
}

// === Push UNROLL through scalar wrappers (LOAD / INDEX_E) ==============
//
// LOAD is not in the expander's op_is_expandable() list, so when a
// LOAD wraps a vector-indexed INDEX_E the UNROLL marker stays INSIDE
// the LOAD.  Mirror tinygrad's do_expand by pushing the UNROLL up:
//   LOAD(UNROLL(idx, args))  ->  UNROLL(LOAD(idx), args)
// This is sound because LOAD over a vector index distributes per-lane.
//
// Reference: tinygrad/codegen/late/expander.py:74-75 (do_expand wraps
// in outer UNROLL after rebuilding the root op).

static Term devec_pm_push_unroll_load(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_LOAD) return 0;
  Term inner = heap_read(term_val(t) + 0);
  if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_UNROLL) return 0;
  u32 n = uop_unroll_n_args(inner);
  if (n == 0 || n > MAX_DIM) return 0;
  u32 axes[MAX_DIM];
  u32 factors[MAX_DIM];
  for (u32 i = 0; i < n; i++) {
    axes[i]    = uop_unroll_axis_id(inner, i);
    factors[i] = uop_unroll_factor(inner, i);
  }
  Term idx = heap_read(term_val(inner) + 0);
  Term new_load = uop_load(idx);
  return uop_unroll(new_load, n, axes, factors);
}

// === UNROLL(STACK(...)) collapse ========================================
//
// After CONTRACT-broadcast + INDEX_E/LOAD devec turn the body into a
// STACK of N lanes, the surrounding UNROLL becomes a redundant marker
// (the lanes are already enumerated).  Drop the UNROLL when its child
// is a STACK whose width matches prod(factors).
//
// This is the "structural sink" mirroring tinygrad's pm_render rule
// devectorizer.py:282-284 (STACK at the leaf of a UNROLL of matching
// width collapses).

static Term devec_pm_unroll_stack(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_UNROLL) return 0;
  Term inner = heap_read(term_val(t) + 0);
  if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_STACK) return 0;
  u32 expect = 1;
  u32 na = uop_unroll_n_args(t);
  for (u32 i = 0; i < na; i++) expect *= uop_unroll_factor(t, i);
  if (uop_stack_n(inner) != expect) return 0;
  return inner;
}

// UNROLL(STORE) -> STORE.  An UPCAST'd output store expanded by
// do_expand is UNROLL(STORE(buf, addr_lanes, value_lanes)); once the
// addr/value lanes are STACKs the UNROLL marker is redundant (the F-ness
// lives in the STACKs), and the renderer lowers the STORE itself into F
// scalar stores.  Mirrors devec_pm_unroll_stack for the void-typed STORE.
static Term devec_pm_unroll_store(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_UNROLL) return 0;
  Term inner = heap_read(term_val(t) + 0);
  if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_STORE) return 0;
  return inner;
}

// === devec INDEX_E + LOAD on STACK srcs ================================
//
// After CONTRACT(non-UNROLL) folds to STACK(src x N), INDEX_E whose
// buf or addr is a STACK becomes a vector op.  Split it per-lane
// (mirror of devec_no_vec_alu but specialized for these two ops,
// which the ALU dispatcher above didn't cover because their dtype /
// constructor doesn't fit the generic ALU shape).

static Term devec_no_vec_index_e(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_INDEX_E) return 0;
  u64 loc = term_val(t);
  Term buf = heap_read(loc + 0);
  Term addr = heap_read(loc + 1);
  int buf_v = (term_tag(buf) == TAG_UOP && term_ext(buf) == UOP_STACK);
  int addr_v = (term_tag(addr) == TAG_UOP && term_ext(addr) == UOP_STACK);
  if (!buf_v && !addr_v) return 0;
  u32 width = buf_v ? uop_stack_n(buf) : uop_stack_n(addr);
  if (width == 0 || width == 1) return 0;
  if (buf_v && addr_v && uop_stack_n(buf) != uop_stack_n(addr)) return 0;
  if (width > 256) return 0;
  Term lanes[256];
  for (u32 i = 0; i < width; i++) {
    Term b = buf_v ? uop_stack_src(buf, i) : buf;
    Term a = addr_v ? uop_stack_src(addr, i) : addr;
    lanes[i] = uop_index_e(b, a);
  }
  return uop_stack(width, lanes);
}

static Term devec_no_vec_load(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_LOAD) return 0;
  Term inner = heap_read(term_val(t) + 0);
  if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_STACK) return 0;
  u32 width = uop_stack_n(inner);
  if (width == 0 || width == 1) return 0;
  if (width > 256) return 0;
  Term lanes[256];
  for (u32 i = 0; i < width; i++) {
    lanes[i] = uop_load(uop_stack_src(inner, i));
  }
  return uop_stack(width, lanes);
}

static Term devec_pm_devectorize_extra(Term t, void *user) {
  (void)user;
  Term r;
  if ((r = devec_no_vec_index_e(t)) != 0) return r;
  if ((r = devec_no_vec_load(t)) != 0) return r;
  return 0;
}

// === pm_render lowering rules (devectorizer.py:275-295) ================
//
// 1. VCONST -> STACK of scalar CONSTs.
// 2. GEP(STACK(...), (i,)) -> STACK src[i] (when single index over a
//    STACK).
// 3. GEP(scalar_src, (0,)) -> scalar_src (when source's vcount==1).
// 4. STACK with single src -> the src itself.

static Term devec_pm_vconst_to_stack(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_VCONST) return 0;
  u32 dtype = uop_vconst_dtype(t);
  u32 n = uop_vconst_n(t);
  if (n <= 1 || n > 256) return 0;
  Term elts[256];
  for (u32 i = 0; i < n; i++) {
    elts[i] = uop_const(dtype, uop_vconst_bits(t, i));
  }
  return uop_stack(n, elts);
}

static Term devec_pm_gep_unwrap(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_GEP) return 0;
  u32 n = uop_gep_n_idx(t);
  Term src = heap_read(term_val(t) + 0);
  // Case: GEP over a STACK -> pull out the lanes.
  if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_STACK) {
    u32 src_n = uop_stack_n(src);
    if (n == 1) {
      u32 idx = uop_gep_idx(t, 0);
      if (idx < src_n) return uop_stack_src(src, idx);
    }
    if (n > 1 && n <= 256) {
      // GEP(STACK(...), (i_0, ..., i_{n-1})) -> STACK(src[i_0], ..., src[i_{n-1}])
      Term out[256];
      for (u32 i = 0; i < n; i++) {
        u32 idx = uop_gep_idx(t, i);
        if (idx >= src_n) return 0;
        out[i] = uop_stack_src(src, idx);
      }
      return uop_stack(n, out);
    }
  }
  // Case: GEP(scalar, (0,)) -> scalar.
  if (n == 1 && uop_gep_idx(t, 0) == 0) {
    // Heuristic: if src is not a STACK / UNROLL / VCONST, it's already
    // scalar, so the GEP is a no-op.
    if (term_tag(src) == TAG_UOP) {
      u32 sop = term_ext(src);
      if (sop != UOP_STACK && sop != UOP_UNROLL && sop != UOP_VCONST) {
        return src;
      }
    } else {
      return src;
    }
  }
  return 0;
}

// uop_stack already collapses n==1 in the constructor, but a STACK that
// was built before its only-element status was visible can still be
// rewritten by this rule fired bottom-up.
static Term devec_pm_stack_singleton(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STACK) return 0;
  if (uop_stack_n(t) != 1) return 0;
  return uop_stack_src(t, 0);
}

// === Entry point: uop_devectorize_graph ================================

fn Term uop_devectorize_graph(Term root) {
  // The pipeline runs THREE rewrite groups in this order:
  //
  //   1. CONTRACT folding + UNROLL push + scalar ALU devec.  This
  //      runs FIRST (before pm_reduce wraps the body in UOP_AFTER)
  //      because the graph rewriter does not descend into AFTER's
  //      variadic children -- once reduce_to_acc nests the body
  //      under AFTER, no further per-node rewrites reach it.
  //
  //   2. pm_reduce (reduce_to_acc): REDUCE -> PLACEHOLDER + STOREs +
  //      END + LOAD chain.  Mirrors codegen/__init__.py:197-204.
  //
  //   3. A final cleanup pm_render pass over the post-reduce graph
  //      (catches GEP/STACK singletons synthesized while rebuilding).
  //
  // Reference: tinygrad/codegen/late/expander.py:77-86 do_contract;
  //            tinygrad/codegen/late/expander.py:22-75 do_expand.
  Term t = root;

  // Pass (a1): push UNROLL through scalar wrappers (LOAD).  Must
  // run BEFORE the CONTRACT fold + devectorize so the inner
  // INDEX_E with CONTRACT-broadcasts is exposed.  Mirrors tinygrad
  // expander.py:74-75 do_expand's outer-UNROLL wrap, which thvm's
  // expander.c skips because UOP_LOAD is not in op_is_expandable().
  static UOpGraphRewriteRule const PUSH_RULES[] = {
    { "devec_pm_push_unroll_load", devec_pm_push_unroll_load },
  };
  t = uop_graph_rewrite(t, PUSH_RULES,
                        sizeof(PUSH_RULES) / sizeof(PUSH_RULES[0]),
                        NULL);
  // pm_render: VCONST -> STACK; do_contract (Cases A + B).
  static UOpGraphRewriteRule const RENDER_RULES[] = {
    { "devec_pm_vconst_to_stack",  devec_pm_vconst_to_stack },
    { "devec_pm_do_contract",      devec_pm_do_contract },
    { "devec_pm_gep_unwrap",       devec_pm_gep_unwrap },
    { "devec_pm_stack_singleton",  devec_pm_stack_singleton },
    { "devec_pm_unroll_stack",     devec_pm_unroll_stack },
    { "devec_pm_unroll_store",     devec_pm_unroll_store },
  };
  t = uop_graph_rewrite(t, RENDER_RULES,
                        sizeof(RENDER_RULES) / sizeof(RENDER_RULES[0]),
                        NULL);
  // devectorize: vector ALU / INDEX_E / LOAD -> STACK of scalars.
  static UOpGraphRewriteRule const DEVEC_RULES[] = {
    { "devec_pm_devectorize",       devec_pm_devectorize },
    { "devec_pm_devectorize_extra", devec_pm_devectorize_extra },
  };
  t = uop_graph_rewrite(t, DEVEC_RULES,
                        sizeof(DEVEC_RULES) / sizeof(DEVEC_RULES[0]),
                        NULL);
  // Second sweep of pm_render to clean up STACK/GEP residue from
  // the per-lane rebuilds.
  t = uop_graph_rewrite(t, RENDER_RULES,
                        sizeof(RENDER_RULES) / sizeof(RENDER_RULES[0]),
                        NULL);

  // Pass (b): pm_reduce -- one shared ReduceContext threads acc_num
  // across every REDUCE the rewrite encounters.  If the seed walk cannot
  // allocate, the REDUCEs stay unlowered: every linearized renderer
  // declines a surviving REDUCE, so the kernel takes the legacy emit
  // instead of reusing a live accumulator id.
  static UOpGraphRewriteRule const REDUCE_RULES[] = {
    { "devec_pm_reduce", devec_pm_reduce },
  };
  DevecReduceCtx ctx = { 0 };
  if (!devec_cone_walk(t, devec_visit_acc_max, &ctx.acc_num)) {
    return t;
  }
  t = uop_graph_rewrite(t, REDUCE_RULES,
                        sizeof(REDUCE_RULES) / sizeof(REDUCE_RULES[0]),
                        &ctx);

  // Pass (c) final: STACK-singleton / GEP-singleton cleanup (the
  // reduce-to-acc rebuild may have left singletons on edge cases).
  t = uop_graph_rewrite(t, RENDER_RULES,
                        sizeof(RENDER_RULES) / sizeof(RENDER_RULES[0]),
                        NULL);
  return t;
}

// === load_store_folding (devectorizer.py:81-149) =======================
//
// fold_expanded_index: F adjacent scalar LOADs whose addresses differ by
// 0, 1, ..., F-1 along a single stride collapse to one wide LOAD + GEPs.
// This is the path #3 effect: the post-devectorize graph carries STACKs
// of scalar LOADs; this pass walks each STACK, checks the address
// pattern, and rewrites to a single LOAD over a vector-typed INDEX_E
// plus per-lane GEPs.
//
// Detection: a STACK(LOAD(INDEX_E(buf, addr_0)), LOAD(INDEX_E(buf, addr_1)), ...)
// where addr_i - addr_0 == i (or a unit-stride pattern in symbolic
// arithmetic).  For this first port we recognise the simplest case:
// addr_i = IADD(base, CONST(i)) where base is shared and CONST(i) is
// literally i.
//
// Backend gate: float / half dtype only; integer types fold differently
// (no SIMD wide-load benefit) and we keep them scalar.

// Walks t down through UOP_LOAD -> UOP_INDEX_E to extract (buf, addr).
// Returns 1 on match.
static int devec_extract_load_index(Term t, Term *out_buf, Term *out_addr) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_LOAD) return 0;
  Term inner = heap_read(term_val(t) + 0);
  if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_INDEX_E) return 0;
  *out_buf = heap_read(term_val(inner) + 0);
  *out_addr = heap_read(term_val(inner) + 1);
  return 1;
}

// For addr_i = IADD(base, CONST(i)) extract base and i.  If addr_i is
// just a CONST (i.e. i == 0 base), accept base == NULL / i = const.
// If addr_i is a bare non-CONST term (e.g. UOP_IADD's identity rule
// folded IADD(base, CONST(0)) -> base), treat that as (base, off=0).
static int devec_extract_unit_stride(Term addr, Term *out_base, u32 *out_off) {
  if (term_tag(addr) == TAG_UOP && term_ext(addr) == UOP_IADD) {
    u64 loc = term_val(addr);
    Term a = heap_read(loc + 0);
    Term b = heap_read(loc + 1);
    if (term_tag(b) == TAG_UOP && term_ext(b) == UOP_CONST) {
      Term cell = heap_read(term_val(b) + 0);
      if (term_tag(cell) == TAG_NUM) {
        *out_base = a;
        *out_off = (u32)term_val(cell);
        return 1;
      }
    }
    if (term_tag(a) == TAG_UOP && term_ext(a) == UOP_CONST) {
      Term cell = heap_read(term_val(a) + 0);
      if (term_tag(cell) == TAG_NUM) {
        *out_base = b;
        *out_off = (u32)term_val(cell);
        return 1;
      }
    }
    return 0;
  }
  if (term_tag(addr) == TAG_UOP && term_ext(addr) == UOP_CONST) {
    Term cell = heap_read(term_val(addr) + 0);
    if (term_tag(cell) == TAG_NUM) {
      *out_base = 0;
      *out_off = (u32)term_val(cell);
      return 1;
    }
  }
  // Bare non-CONST term: identity simplifier collapsed addr = base + 0
  // to just `base`.  Treat it as (base, off=0) so the F=4 lane scan
  // succeeds when the lane-0 address is the shared base.
  *out_base = addr;
  *out_off = 0;
  return 1;
}

// Backend gate for wide-load folding.  In the absence of a renderer
// flag plumbed all the way through, allow folding on float / half /
// bfloat dtypes -- the dtype gate matches tinygrad's supports_float4
// check (which is True on Metal + CUDA, False on cpu).
static int devec_load_fold_dtype_ok(u32 dtype) {
  if (dtype == DT_FP32) return 1;
  // dtypes for f16 / bf16 are gated off until the renderer can emit
  // half-width vector loads.
  return 0;
}

// Shared-load CSE: F lanes of a STACK that ALL reference the same
// scalar LOAD via hash-consed INDEX_E (e.g. the X loads in a conv
// chain where the per-K address does not depend on the F-lane
// upcast axis).  Hash-cons already makes them the same Term; this
// rule rewrites the STACK into UNROLL(load, [(axis=0xFE, F)]) +
// per-lane GEPs so the renderer (which lowers UNROLL'd LOADs as a
// single scalar emit + lane broadcasts) emits the LOAD once rather
// than F times.  Mirrors the broadcast effect of tinygrad's
// devectorizer.py:160 fold_expanded_index ALL-same-source case
// (which is implicit there because GEP(VCONST, ...) folds first).
//
// Returns the rewritten STACK, or 0 if no fold applies.
static Term devec_fold_load_stack_shared(Term t) {
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STACK) return 0;
  u32 n = uop_stack_n(t);
  if (!(n == 2 || n == 4 || n == 8)) return 0;
  Term first = uop_stack_src(t, 0);
  // Must be a LOAD over an INDEX_E to qualify (matches the wide-load
  // gate; preserves dtype routing).
  if (term_tag(first) != TAG_UOP || term_ext(first) != UOP_LOAD) return 0;
  Term inner = heap_read(term_val(first) + 0);
  if (term_tag(inner) != TAG_UOP || term_ext(inner) != UOP_INDEX_E) return 0;
  // ALL lanes must be the same LOAD term (hash-cons identity).
  for (u32 i = 1; i < n; i++) {
    if (uop_stack_src(t, i) != first) return 0;
  }
  // Dtype gate -- same supports_float4 check as the adjacent-stride
  // wide-LOAD fold above.
  Term buf = heap_read(term_val(inner) + 0);
  u32 dt = DT_FP32;
  if (!term_dtype_in(buf, 0, &dt) || !devec_load_fold_dtype_ok(dt)) {
    return 0;
  }
  // Emit one shared LOAD wrapped in UNROLL(axis=0xFE, factor=n) so the
  // linearized renderer sees a marker and emits the LOAD once, then
  // broadcasts the scalar across the n lanes through per-lane GEPs.
  // GEP-on-UNROLL is the same shape the adjacent-stride fold produces;
  // the renderer path handles both identically.
  u32 axis_id = 0xFEu;
  u32 factor  = n;
  Term wide_vector = uop_unroll(first, 1, &axis_id, &factor);
  Term out[16];
  for (u32 i = 0; i < n; i++) {
    u32 idx = i;
    out[i] = uop_gep(wide_vector, 1, &idx);
  }
  return uop_stack(n, out);
}

static Term devec_fold_load_stack(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_STACK) return 0;
  u32 n = uop_stack_n(t);
  if (n < 2 || n > 16) return 0;
  // F = 4 / 8 / 16 supported; F=2 also legal (float2).  Restrict to
  // powers of two between 2 and 8 -- the tinygrad backend default.
  if (!(n == 2 || n == 4 || n == 8)) return 0;
  // Shared-load CSE: try the all-identical-lane fold first.  When it
  // matches it short-circuits the adjacent-stride scan below (the
  // lanes have stride 0, not 1, and would fail off-check otherwise).
  Term shared = devec_fold_load_stack_shared(t);
  if (shared != 0) return shared;
  Term shared_buf = 0;
  Term shared_base = 0;
  u32  base_off = 0;
  for (u32 i = 0; i < n; i++) {
    Term lane = uop_stack_src(t, i);
    Term buf, addr;
    if (!devec_extract_load_index(lane, &buf, &addr)) return 0;
    Term base;
    u32 off;
    if (!devec_extract_unit_stride(addr, &base, &off)) return 0;
    if (i == 0) {
      shared_buf = buf;
      shared_base = base;
      base_off = off;
    } else {
      if (buf != shared_buf || base != shared_base) return 0;
      if (off != base_off + i) return 0;
    }
  }
  // Dtype gate.
  u32 dt = DT_FP32;
  if (!term_dtype_in(shared_buf, 0, &dt) || !devec_load_fold_dtype_ok(dt)) {
    return 0;
  }
  // Build one wide LOAD whose INDEX_E address is (base + base_off) and
  // whose result is a vector of width n.  Since thvm's LOAD currently
  // is scalar-valued, we represent the wide load by a single LOAD over
  // INDEX_E(buf, base_addr) wrapped in UNROLL carrying a synthetic
  // (axis=0, n) so the renderer (when wired) can lower it to a
  // float<n> reinterpret_cast.  GEPs per lane recover the original
  // STACK shape; the constructor's STACK(n, [GEP(load, i)]) is the
  // bottom-up rewrite output the renderer consumes.
  Term base_addr;
  if (shared_base == 0) {
    base_addr = uop_const(DT_INT32, base_off);
  } else if (base_off == 0) {
    base_addr = shared_base;
  } else {
    Term off_const = uop_const(DT_INT32, base_off);
    base_addr = uop_int_binary(UOP_IADD, shared_base, off_const);
  }
  Term wide_idx = uop_index_e(shared_buf, base_addr);
  Term wide_load = uop_load(wide_idx);
  u32 axis_id = 0xFEu;          // synthetic "vector-lane" axis id
  u32 factor = n;
  Term wide_vector = uop_unroll(wide_load, 1, &axis_id, &factor);
  Term out[16];
  for (u32 i = 0; i < n; i++) {
    u32 idx = i;
    out[i] = uop_gep(wide_vector, 1, &idx);
  }
  return uop_stack(n, out);
}

fn Term uop_load_store_fold_graph(Term root) {
  // Pre-condition: caller has already run uop_devectorize_graph so
  // adjacent LOADs are present as STACK children.  The fold pass
  // walks STACKs bottom-up; rewriting a STACK of LOADs in place is
  // safe because the GEPs we emit point into the new wide_load --
  // pm_render's GEP-on-STACK rule would re-expand them if run on the
  // result, but we don't run pm_render after fold (the renderer
  // consumes the GEP form to emit per-lane reads).
  static UOpGraphRewriteRule const FOLD_RULES[] = {
    { "devec_fold_load_stack", devec_fold_load_stack },
  };
  return uop_graph_rewrite(root, FOLD_RULES,
                           sizeof(FOLD_RULES) / sizeof(FOLD_RULES[0]),
                           NULL);
}
