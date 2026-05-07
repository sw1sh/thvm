// backend/metal/shaders/aot_ic_def_run.metal -- iter Z+2 step 4.
//
// Generic per-def runner.  Replaces the per-def metal_emit.c source
// dump (which inlined every cell construction and produced ~10K-line
// MSL for V>=4 Church-bool formulas, hanging xcrun metallib) with a
// single static shader that takes the def's book-heap root Term as a
// constant.  No per-def MSL emit / xcrun roundtrip; one PSO across
// all defs.
//
// Buffer-binding convention (set by thvm_aot_metal_ic_def_run in _.m):
//   buffer(0)  : device   Term         *heap       -- BOOK_HEAP, mutable
//   buffer(1)  : device   Term         *args       -- caller args (n_args Terms)
//   buffer(2)  : device   Term         *result     -- single cell, root Term out
//   buffer(3)  : device   atomic_uint  *book_next  -- mutable allocator
//   buffer(4)  : constant ulong        *root_term  -- the def's book root (full Term)
//   buffer(5)  : constant uint         *n_args     -- how many args to apply
//
// Single-thread (grid_size=1, only tid=0 runs).  Same wnf state
// machine + IC interaction inlines as aot_ic_collapse.metal -- copies
// here for self-containment and to add the BJ -> DP unfold cases that
// iter Z's per-def emit handled at compile time but the shared
// shader didn't (book templates carry BJ tags after Phase C's
// clone_to_book DP->BJ rewrite).
//
// Args are applied as APP(prev, args[i]) chains: the def is left
// curried in book, the kernel builds the call site in fresh book
// cells.  argc determines how many APPs we wrap.

#include <metal_stdlib>
using namespace metal;

#define TAG_SHIFT  56
#define EXT_SHIFT  38
#define TAG_MASK   0x7FUL
#define EXT_MASK   0x3FFFFUL
#define VAL_MASK   0x3FFFFFFFFFUL
#define TAG_APP    0u
#define TAG_LAM    1u
#define TAG_VAR    2u
#define TAG_ERA    3u
#define TAG_DP0    4u
#define TAG_DP1    5u
#define TAG_SUP    6u
#define TAG_NUM    10u
#define TAG_REF    11u
#define TAG_BJ0    33u
#define TAG_BJ1    34u
#define SUB_SHIFT  63
#define SUB_BIT    (1ULL << 63)
#define LAM_ERA_MASK   (1u << 17)

typedef ulong Term;

static inline ulong msl_term_val(Term t) { return t & VAL_MASK; }
static inline uint  msl_term_ext(Term t) {
  return uint((t >> EXT_SHIFT) & EXT_MASK);
}
static inline uint  msl_term_tag(Term t) {
  return uint((t >> TAG_SHIFT) & TAG_MASK);
}
static inline uint  msl_sub_get(Term t) {
  return uint((t >> SUB_SHIFT) & 1u);
}
static inline Term  msl_sub_clr(Term t) { return t & ~SUB_BIT; }
static inline Term  msl_term_new(uint tag, uint ext, ulong val) {
  return ((ulong(tag) & TAG_MASK) << TAG_SHIFT)
       | ((ulong(ext) & EXT_MASK) << EXT_SHIFT)
       | ( val        & VAL_MASK);
}

static inline Term aot_heap_load(device Term *heap, ulong loc) {
  return heap[loc];
}
static inline void aot_heap_store(device Term *heap, ulong loc, Term v) {
  heap[loc] = v;
}
static inline ulong aot_book_alloc(device atomic_uint *book_next, uint n) {
  return ulong(atomic_fetch_add_explicit(book_next, n, memory_order_relaxed));
}
static inline void aot_subst_var(device Term *heap, ulong loc, Term v) {
  aot_heap_store(heap, loc, v | SUB_BIT);
}
static inline Term aot_subst_cop(uint side, ulong loc,
    Term r0, Term r1, device Term *heap) {
  if (side == 0u) { aot_subst_var(heap, loc, r1); return r0; }
  aot_subst_var(heap, loc, r0); return r1;
}

// IC interaction inlines (same shape as aot_ic_collapse.metal).
static Term aot_app_lam(Term lam, Term arg, device Term *heap) {
  uint  lam_ext = msl_term_ext(lam);
  ulong loc     = msl_term_val(lam);
  Term  body    = aot_heap_load(heap, loc);
  if ((lam_ext & LAM_ERA_MASK) == 0u) aot_subst_var(heap, loc, arg);
  return body;
}
static Term aot_app_sup(Term sup, Term arg,
    device Term *heap, device atomic_uint *book_next) {
  ulong sup_loc = msl_term_val(sup);
  uint  lab     = msl_term_ext(sup);
  Term f = aot_heap_load(heap, sup_loc + 0);
  Term g = aot_heap_load(heap, sup_loc + 1);
  ulong c = aot_book_alloc(book_next, 7u);
  aot_heap_store(heap, c + 0, arg);
  aot_heap_store(heap, c + 1, f);
  aot_heap_store(heap, c + 2, msl_term_new(TAG_DP0, lab, c + 0));
  aot_heap_store(heap, c + 3, g);
  aot_heap_store(heap, c + 4, msl_term_new(TAG_DP1, lab, c + 0));
  aot_heap_store(heap, c + 5, msl_term_new(TAG_APP, 0u, c + 1));
  aot_heap_store(heap, c + 6, msl_term_new(TAG_APP, 0u, c + 3));
  return msl_term_new(TAG_SUP, lab, c + 5);
}
static Term aot_dup_sup(uint lab, ulong loc, uint side, Term sup,
    device Term *heap, device atomic_uint *book_next) {
  ulong sup_loc = msl_term_val(sup);
  uint  sup_lab = msl_term_ext(sup);
  if (lab == sup_lab) {
    Term tm0 = aot_heap_load(heap, sup_loc + 0);
    Term tm1 = aot_heap_load(heap, sup_loc + 1);
    return aot_subst_cop(side, loc, tm0, tm1, heap);
  }
  Term a = aot_heap_load(heap, sup_loc + 0);
  Term b = aot_heap_load(heap, sup_loc + 1);
  ulong c = aot_book_alloc(book_next, 6u);
  aot_heap_store(heap, c + 0, a);
  aot_heap_store(heap, c + 1, b);
  aot_heap_store(heap, c + 2, msl_term_new(TAG_DP0, lab, c + 0));
  aot_heap_store(heap, c + 3, msl_term_new(TAG_DP0, lab, c + 1));
  aot_heap_store(heap, c + 4, msl_term_new(TAG_DP1, lab, c + 0));
  aot_heap_store(heap, c + 5, msl_term_new(TAG_DP1, lab, c + 1));
  Term x0 = msl_term_new(TAG_SUP, sup_lab, c + 2);
  Term x1 = msl_term_new(TAG_SUP, sup_lab, c + 4);
  return aot_subst_cop(side, loc, x0, x1, heap);
}
static Term aot_dup_lam(uint lab, ulong loc, uint side, Term lam,
    device Term *heap, device atomic_uint *book_next) {
  uint  lam_ext = msl_term_ext(lam);
  ulong lam_loc = msl_term_val(lam);
  Term  body    = aot_heap_load(heap, lam_loc);
  ulong a = aot_book_alloc(book_next, 5u);
  aot_heap_store(heap, a + 4, body);
  aot_heap_store(heap, a + 0, msl_term_new(TAG_DP0, lab, a + 4));
  aot_heap_store(heap, a + 1, msl_term_new(TAG_DP1, lab, a + 4));
  aot_heap_store(heap, a + 2, msl_term_new(TAG_VAR, 0u, a + 0));
  aot_heap_store(heap, a + 3, msl_term_new(TAG_VAR, 0u, a + 1));
  Term sup = msl_term_new(TAG_SUP, lab,     a + 2);
  Term l0  = msl_term_new(TAG_LAM, lam_ext, a + 0);
  Term l1  = msl_term_new(TAG_LAM, lam_ext, a + 1);
  if ((lam_ext & LAM_ERA_MASK) == 0u) aot_subst_var(heap, lam_loc, sup);
  return aot_subst_cop(side, loc, l0, l1, heap);
}
static Term aot_dup_num(uint side, ulong loc, Term num, device Term *heap) {
  return aot_subst_cop(side, loc, num, num, heap);
}
static Term aot_dup_era(uint side, ulong loc, Term era, device Term *heap) {
  return aot_subst_cop(side, loc, era, era, heap);
}

constant uint AOT_DEF_RUN_STACK_CAP = 4096u;
constant uint AOT_DEF_RUN_ITER_CAP  = (1u << 16);   // 65k -- caps GPU hangs;
                                                     // bump after correctness landed

// wnf state machine.  ENTER pushes APP / DP / BJ frames and descends;
// APPLY dispatches DUP-XXX based on body's WHNF tag.  BJ tags are
// treated as DPs so the def-template's BJ-rewritten cells get the same
// fire-DUP-XXX dispatch (mirrors iter Z's emit-time BJ -> DP unfold).
static Term aot_def_wnf(Term term,
    device Term *heap, device atomic_uint *book_next)
{
  thread Term ic_stk[AOT_DEF_RUN_STACK_CAP];
  uint ic_spos  = 0u;
  Term next     = term;
  Term whnf     = 0u;
  uint state    = 0u;
  uint ic_iters = 0u;
  while (state != 2u && ic_iters < AOT_DEF_RUN_ITER_CAP) {
    ic_iters++;
    if (state == 0u) {
      uint t = msl_term_tag(next);
      if (t == TAG_VAR) {
        ulong loc  = msl_term_val(next);
        Term  cell = aot_heap_load(heap, loc);
        if (msl_sub_get(cell)) { next = msl_sub_clr(cell); continue; }
        whnf = next; state = 1u; continue;
      }
      if (t == TAG_DP0 || t == TAG_DP1 || t == TAG_BJ0 || t == TAG_BJ1) {
        ulong loc  = msl_term_val(next);
        Term  cell = aot_heap_load(heap, loc);
        if (msl_sub_get(cell)) { next = msl_sub_clr(cell); continue; }
        if (ic_spos >= AOT_DEF_RUN_STACK_CAP) {
          return msl_term_new(TAG_ERA, 0xFFFFFu, 0u);
        }
        // Normalize BJ -> DP on the stack frame so APPLY can dispatch
        // identically.  BJ projections have the same val/ext layout
        // as DP, just different tags; rewrite at frame-push time.
        if (t == TAG_BJ0) {
          next = msl_term_new(TAG_DP0, msl_term_ext(next), loc);
        } else if (t == TAG_BJ1) {
          next = msl_term_new(TAG_DP1, msl_term_ext(next), loc);
        }
        ic_stk[ic_spos++] = next;
        next = cell; continue;
      }
      if (t == TAG_APP) {
        ulong loc = msl_term_val(next);
        if (ic_spos >= AOT_DEF_RUN_STACK_CAP) {
          return msl_term_new(TAG_ERA, 0xFFFFFu, 0u);
        }
        ic_stk[ic_spos++] = next;
        next = aot_heap_load(heap, loc); continue;
      }
      whnf = next; state = 1u; continue;
    } else {
      if (ic_spos == 0u) { state = 2u; continue; }
      Term frame = ic_stk[--ic_spos];
      uint ft    = msl_term_tag(frame);
      if (ft == TAG_APP) {
        ulong app_loc = msl_term_val(frame);
        Term  arg     = aot_heap_load(heap, app_loc + 1);
        uint  wt      = msl_term_tag(whnf);
        if (wt == TAG_LAM) {
          next = aot_app_lam(whnf, arg, heap);
          state = 0u; continue;
        }
        if (wt == TAG_SUP) {
          next = aot_app_sup(whnf, arg, heap, book_next);
          state = 0u; continue;
        }
        if (wt == TAG_ERA) {
          whnf = msl_term_new(TAG_ERA, 0u, 0u); continue;
        }
        aot_heap_store(heap, app_loc + 0, whnf);
        whnf = frame; continue;
      }
      if (ft == TAG_DP0 || ft == TAG_DP1) {
        ulong loc  = msl_term_val(frame);
        uint  lab  = msl_term_ext(frame);
        uint  side = (ft == TAG_DP0) ? 0u : 1u;
        uint  wt   = msl_term_tag(whnf);
        if (wt == TAG_SUP) {
          next = aot_dup_sup(lab, loc, side, whnf, heap, book_next);
          state = 0u; continue;
        }
        if (wt == TAG_LAM) {
          next = aot_dup_lam(lab, loc, side, whnf, heap, book_next);
          state = 0u; continue;
        }
        if (wt == TAG_NUM) {
          whnf = aot_dup_num(side, loc, whnf, heap); continue;
        }
        if (wt == TAG_ERA) {
          whnf = aot_dup_era(side, loc, whnf, heap); continue;
        }
        aot_heap_store(heap, loc, whnf);
        whnf = frame; continue;
      }
      whnf = frame; continue;
    }
  }
  if (ic_iters >= AOT_DEF_RUN_ITER_CAP) {
    return msl_term_new(TAG_ERA, 0xFFFFEu, 0u);
  }
  return whnf;
}

// Generic def runner.  Single thread; builds APP(... APP(root, args[0]),
// args[n-1]) chain in book heap, then drives wnf.
kernel void aot_ic_def_run(
    device Term         *heap      [[buffer(0)]],
    device Term         *args      [[buffer(1)]],
    device Term         *result    [[buffer(2)]],
    device atomic_uint  *book_next [[buffer(3)]],
    constant ulong      *root_term [[buffer(4)]],
    constant uint       *n_args    [[buffer(5)]],
    uint                 tid        [[thread_position_in_grid]])
{
  if (tid != 0u) return;
  Term cur = (Term)*root_term;
  uint n = *n_args;
  for (uint i = 0u; i < n; i++) {
    ulong loc = aot_book_alloc(book_next, 2u);
    aot_heap_store(heap, loc + 0, cur);
    aot_heap_store(heap, loc + 1, args[i]);
    cur = msl_term_new(TAG_APP, 0u, loc);
  }
  result[0] = aot_def_wnf(cur, heap, book_next);
}
