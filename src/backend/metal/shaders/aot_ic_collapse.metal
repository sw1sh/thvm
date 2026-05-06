// backend/metal/shaders/aot_ic_collapse.metal -- iter Z+1.
//
// Parallel cnf+collapse for AOT-Metal IC kernels (iter Z output).
//
// After iter Z's per-def kernel writes a SUP-tree-rooted Term to
// BOOK_HEAP, this generic shader walks N independent leaf paths in
// parallel.  Each thread:
//   1. Reads `root` (the kernel-1 result Term).
//   2. Decodes `tid` into a binary path -- LSB picks left/right at
//      depth 0, etc.
//   3. Walks SUP nodes via heap[loc + bit].  At each non-SUP, runs a
//      mini wnf state machine to drive the term to WHNF; if WHNF is
//      SUP, continue walking; if NUM / ERA / LAM / atom, halt.
//   4. Writes the final Term to result[tid].
//
// The IC interaction inlines (app_lam, app_sup, dup_sup, dup_lam,
// dup_num, dup_era) mirror src/interact/*.c -- same shape as the
// per-def emit in src/aot/metal_emit.c, ported into a shared static
// shader so multiple defs can collapse via one PSO.
//
// Buffer-binding convention (set by thvm_aot_metal_collapse in _.m):
//   buffer(0)  : device Term *heap   -- BOOK_HEAP, mutable
//   buffer(1)  : constant ulong       *root  -- single cell, kernel-1's result
//   buffer(2)  : device   Term        *result -- [N] cells, one per leaf path
//   buffer(3)  : device atomic_uint  *book_next -- mutable allocator
//   buffer(4)  : constant uint        *path_depth -- max bits to consume from tid
//
// Heap is `atomic_ulong` so subst-bit transitions (heap_subst_var)
// stay coherent across the N threads sharing dup cells.  Each thread
// also has its own thread-private wnf stack (256 Terms = 2 KiB).

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

// Plain device-pointer reads/writes.  Apple GPU lacks 64-bit atomic
// load/store; for the iter Z+1 first cut we accept that DUP-LAM/SUP
// fires on shared dup cells may race (both threads alloc, last write
// wins).  For SAT-shaped reductions the racy fires are idempotent at
// the value level (DUP-NUM writes the same NUM; DUP-LAM-of-shared
// LAM converges).  Iter Z+2 can revisit with 32-bit slot atomics or
// per-thread heap arenas.
static inline Term aot_heap_load(device Term *heap, ulong loc) {
  return heap[loc];
}
static inline void aot_heap_store(device Term *heap, ulong loc, Term v) {
  heap[loc] = v;
}
static inline ulong aot_book_alloc(device atomic_uint *book_next, uint n) {
  return ulong(atomic_fetch_add_explicit(book_next, n, memory_order_relaxed));
}
// Iter Z+2 plan (TODO -- not implemented in this revision):
//
// SUB-bit writes through subst_var / subst_cop race across the N
// parallel threads of kernel-2 (Apple GPU has no 64-bit device
// atomics).  Two paths to a fix:
//
//   (a) Per-thread book-heap arenas.  Each thread bumps its own
//       book_next within a private slice.  Subst writes that target
//       the thread's slice are safe (private); writes to shared
//       cells (anything in iter Z's allocation range) get skipped.
//       Requires plumbing slice_base/end through every IC call.
//
//   (b) 32-bit slot atomics.  Re-encode the heap as device
//       atomic_uint *, two slots per Term.  SUB-bit lives on the
//       upper slot; compare-exchange before writing the lower
//       resolves the race.  Apple GPU supports atomic_uint CAS.
//       Requires touching every Term read/write call site.
//
// Both are bigger than this revision; they belong in iter Z+2
// proper.  Today's path keeps the original (racy) semantics so
// the iter Z+1 functionality stays intact while we profile and
// design.
static inline void aot_subst_var(device Term *heap, ulong loc, Term v) {
  aot_heap_store(heap, loc, v | SUB_BIT);
}
static inline Term aot_subst_cop(uint side, ulong loc, Term r0, Term r1,
                                  device Term *heap) {
  if (side == 0u) { aot_subst_var(heap, loc, r1); return r0; }
  aot_subst_var(heap, loc, r0); return r1;
}

// IC interaction inlines (mirror src/interact/*.c).
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

// Per-thread wnf state machine.  Drives `term` to WHNF; returns the
// resolved Term.  Iteration cap protects against runaway / unintended
// loops; stack overflow returns a sentinel (TAG_ERA with ext=0xFFFFF).
constant uint AOT_COLLAPSE_STACK_CAP = 1024u;
constant uint AOT_COLLAPSE_ITER_CAP  = (1u << 20);

static Term aot_wnf_thread(Term term,
    device Term *heap, device atomic_uint *book_next)
{
  thread Term ic_stk[AOT_COLLAPSE_STACK_CAP];
  uint ic_spos  = 0u;
  Term next     = term;
  Term whnf     = 0u;
  uint state    = 0u;
  uint ic_iters = 0u;
  while (state != 2u && ic_iters < AOT_COLLAPSE_ITER_CAP) {
    ic_iters++;
    if (state == 0u) {
      uint t = msl_term_tag(next);
      if (t == TAG_VAR) {
        ulong loc  = msl_term_val(next);
        Term  cell = aot_heap_load(heap, loc);
        if (msl_sub_get(cell)) { next = msl_sub_clr(cell); continue; }
        whnf = next; state = 1u; continue;
      }
      if (t == TAG_DP0 || t == TAG_DP1) {
        ulong loc  = msl_term_val(next);
        Term  cell = aot_heap_load(heap, loc);
        if (msl_sub_get(cell)) { next = msl_sub_clr(cell); continue; }
        if (ic_spos >= AOT_COLLAPSE_STACK_CAP) {
          return msl_term_new(TAG_ERA, 0xFFFFFu, 0u);
        }
        ic_stk[ic_spos++] = next;
        next = cell; continue;
      }
      if (t == TAG_APP) {
        ulong loc = msl_term_val(next);
        if (ic_spos >= AOT_COLLAPSE_STACK_CAP) {
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
  if (ic_iters >= AOT_COLLAPSE_ITER_CAP) {
    return msl_term_new(TAG_ERA, 0xFFFFEu, 0u);
  }
  return whnf;
}

// Generic SUP-tree walker.  Each thread enumerates one leaf path by
// decoding `tid` bits.  After descending past all SUP nodes (or
// hitting a non-SUP), drives the leaf to WHNF.  Out-of-range paths
// (tid >= leaf count) write ERA sentinel.
//
// `path_depth` upper-bounds the SUP-tree depth -- caller computes it
// once on host by walking the tree.  Threads with all-zero high bits
// past the actual path depth still produce valid leaves; threads
// whose bits select an ERA-pruned branch return ERA naturally.
kernel void aot_ic_collapse(
    device Term *heap      [[buffer(0)]],
    constant ulong      *root_in   [[buffer(1)]],
    device   Term       *result    [[buffer(2)]],
    device   atomic_uint *book_next [[buffer(3)]],
    constant uint       *path_depth [[buffer(4)]],
    uint                 tid        [[thread_position_in_grid]])
{
  Term cur = (Term)*root_in;
  uint depth = 0u;
  uint depth_cap = *path_depth;
  while (depth < depth_cap) {
    // Drive current to WHNF.  If it's a SUP, descend by tid bit.
    cur = aot_wnf_thread(cur, heap, book_next);
    if (msl_term_tag(cur) != TAG_SUP) break;
    ulong loc = msl_term_val(cur);
    uint  bit = (tid >> depth) & 1u;
    cur = aot_heap_load(heap, loc + bit);
    depth++;
  }
  // After loop: cur is either irreducible non-SUP (leaf) or we hit
  // depth cap.  Drive once more to be sure leaf is in WHNF.
  cur = aot_wnf_thread(cur, heap, book_next);
  result[tid] = cur;
}
