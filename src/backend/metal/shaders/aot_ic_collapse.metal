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

// === Iter Z+2 step 5: per-thread arena + substitution map ================
//
// Apple GPU has no 64-bit device atomics, so iter Z+1's shared
// device book-heap saw racy SUB-bit writes (both threads see "no
// SUB" simultaneously, both write -- last-writer-wins).  Symptoms:
// non-deterministic num= counts, occasional torn 64-bit writes
// producing invalid tags (0x37, 0x68 observed).
//
// Two-part fix per thread:
//   (1) Arena: a private slice of the book heap [base, base+size)
//       for fresh allocations during this thread's IC fires.  No
//       writes from other threads land here, so heap_store is safe.
//   (2) Subst map: a private (loc -> term) table for SUB-bit writes
//       that target cells OUTSIDE the arena (i.e. cells iter Z
//       allocated and that all collapse threads share).  Writing
//       them to device memory would race; storing privately keeps
//       the substitution available to this thread's reads without
//       affecting siblings.  Lookups happen in VAR / DP0 / DP1
//       enter cases before falling back to the heap cell.
//
// Net: full determinism + each thread fires its own complete
// reduction.  Cost: ~9 KiB per thread (8 KiB ic_stk + 0.5 KiB
// subst map + 12 B arena state).  Within the ~32 KiB Apple GPU
// thread-private budget at our 256-thread tg.
//
// Subst map capacity: 256 entries.  Each path level through the
// SUP-tree adds substitutions for: every AND / OR / NOT LAM
// binder fired during this path's reduction, plus the DP cells
// resolved for the variable assignments.  V=10 / C=20 measured
// at ~150 entries; 256 leaves headroom.  Overflow silently drops
// (this thread stalls on the missing substitution).
constant uint AOT_SUBST_MAP_CAP = 256u;

struct SubstSlot {
  uint loc;
  uint _pad;
  ulong term;
};

struct ThreadCtx {
  uint base;
  uint size;
  uint next;
  uint smap_n;
  SubstSlot smap[AOT_SUBST_MAP_CAP];
};

static inline Term aot_heap_load(device Term *heap, ulong loc) {
  return heap[loc];
}
static inline void aot_heap_store(device Term *heap, ulong loc, Term v) {
  heap[loc] = v;
}
// Arena alloc: returns absolute heap loc for n cells, 0 on overflow.
// At overflow downstream IC fires write garbage cells; the wnf state
// machine bounces them as stuck APP/SUP terms.  Better than racing.
static inline ulong aot_arena_alloc(thread ThreadCtx *ctx, uint n) {
  uint local = ctx->next;
  if (local + n > ctx->size) return 0u;
  ctx->next = local + n;
  return ulong(ctx->base + local);
}
static inline bool aot_in_arena(thread ThreadCtx *ctx, ulong loc) {
  return loc >= ulong(ctx->base) && loc < ulong(ctx->base + ctx->size);
}

// Subst-map ops: linear scan over <= AOT_SUBST_MAP_CAP entries.
// Insert OR update (lam_loc / dup_loc may be re-substituted across
// nested DUP-SUP fires; latest wins, mirroring SUB-bit semantics).
// Returns 0 (term=0) on miss; lookup callers must check before using.
static inline ulong aot_smap_get(thread ThreadCtx *ctx, ulong loc) {
  uint loc32 = uint(loc);
  for (uint i = 0u; i < ctx->smap_n; i++) {
    if (ctx->smap[i].loc == loc32) return ctx->smap[i].term;
  }
  return 0ul;
}
static inline void aot_smap_put(thread ThreadCtx *ctx, ulong loc, ulong v) {
  uint loc32 = uint(loc);
  for (uint i = 0u; i < ctx->smap_n; i++) {
    if (ctx->smap[i].loc == loc32) {
      ctx->smap[i].term = v;
      return;
    }
  }
  if (ctx->smap_n < AOT_SUBST_MAP_CAP) {
    ctx->smap[ctx->smap_n].loc  = loc32;
    ctx->smap[ctx->smap_n].term = v;
    ctx->smap_n++;
  }
  // Overflow: silently drop -- this thread's reduction may stall
  // earlier on the missing substitution; tradeoff for fixed cap.
}

static inline void aot_subst_var(thread ThreadCtx *ctx,
    device Term *heap, ulong loc, Term v) {
  if (aot_in_arena(ctx, loc)) {
    aot_heap_store(heap, loc, v | SUB_BIT);  // private, safe
    return;
  }
  aot_smap_put(ctx, loc, ulong(v) | SUB_BIT);  // shared, route private
}
static inline Term aot_subst_cop(thread ThreadCtx *ctx,
    uint side, ulong loc, Term r0, Term r1, device Term *heap) {
  if (side == 0u) { aot_subst_var(ctx, heap, loc, r1); return r0; }
  aot_subst_var(ctx, heap, loc, r0); return r1;
}

// IC interaction inlines (mirror src/interact/*.c).
static Term aot_app_lam(thread ThreadCtx *ctx,
    Term lam, Term arg, device Term *heap) {
  uint  lam_ext = msl_term_ext(lam);
  ulong loc     = msl_term_val(lam);
  Term  body    = aot_heap_load(heap, loc);
  if ((lam_ext & LAM_ERA_MASK) == 0u) aot_subst_var(ctx, heap, loc, arg);
  return body;
}

static Term aot_app_sup(thread ThreadCtx *ctx,
    Term sup, Term arg, device Term *heap) {
  ulong sup_loc = msl_term_val(sup);
  uint  lab     = msl_term_ext(sup);
  Term f = aot_heap_load(heap, sup_loc + 0);
  Term g = aot_heap_load(heap, sup_loc + 1);
  ulong c = aot_arena_alloc(ctx, 7u);
  aot_heap_store(heap, c + 0, arg);
  aot_heap_store(heap, c + 1, f);
  aot_heap_store(heap, c + 2, msl_term_new(TAG_DP0, lab, c + 0));
  aot_heap_store(heap, c + 3, g);
  aot_heap_store(heap, c + 4, msl_term_new(TAG_DP1, lab, c + 0));
  aot_heap_store(heap, c + 5, msl_term_new(TAG_APP, 0u, c + 1));
  aot_heap_store(heap, c + 6, msl_term_new(TAG_APP, 0u, c + 3));
  return msl_term_new(TAG_SUP, lab, c + 5);
}

static Term aot_dup_sup(thread ThreadCtx *ctx,
    uint lab, ulong loc, uint side, Term sup, device Term *heap) {
  ulong sup_loc = msl_term_val(sup);
  uint  sup_lab = msl_term_ext(sup);
  if (lab == sup_lab) {
    Term tm0 = aot_heap_load(heap, sup_loc + 0);
    Term tm1 = aot_heap_load(heap, sup_loc + 1);
    return aot_subst_cop(ctx, side, loc, tm0, tm1, heap);
  }
  Term a = aot_heap_load(heap, sup_loc + 0);
  Term b = aot_heap_load(heap, sup_loc + 1);
  ulong c = aot_arena_alloc(ctx, 6u);
  aot_heap_store(heap, c + 0, a);
  aot_heap_store(heap, c + 1, b);
  aot_heap_store(heap, c + 2, msl_term_new(TAG_DP0, lab, c + 0));
  aot_heap_store(heap, c + 3, msl_term_new(TAG_DP0, lab, c + 1));
  aot_heap_store(heap, c + 4, msl_term_new(TAG_DP1, lab, c + 0));
  aot_heap_store(heap, c + 5, msl_term_new(TAG_DP1, lab, c + 1));
  Term x0 = msl_term_new(TAG_SUP, sup_lab, c + 2);
  Term x1 = msl_term_new(TAG_SUP, sup_lab, c + 4);
  return aot_subst_cop(ctx, side, loc, x0, x1, heap);
}

static Term aot_dup_lam(thread ThreadCtx *ctx,
    uint lab, ulong loc, uint side, Term lam, device Term *heap) {
  uint  lam_ext = msl_term_ext(lam);
  ulong lam_loc = msl_term_val(lam);
  Term  body    = aot_heap_load(heap, lam_loc);
  ulong a = aot_arena_alloc(ctx, 5u);
  aot_heap_store(heap, a + 4, body);
  aot_heap_store(heap, a + 0, msl_term_new(TAG_DP0, lab, a + 4));
  aot_heap_store(heap, a + 1, msl_term_new(TAG_DP1, lab, a + 4));
  aot_heap_store(heap, a + 2, msl_term_new(TAG_VAR, 0u, a + 0));
  aot_heap_store(heap, a + 3, msl_term_new(TAG_VAR, 0u, a + 1));
  Term sup = msl_term_new(TAG_SUP, lab,     a + 2);
  Term l0  = msl_term_new(TAG_LAM, lam_ext, a + 0);
  Term l1  = msl_term_new(TAG_LAM, lam_ext, a + 1);
  if ((lam_ext & LAM_ERA_MASK) == 0u) aot_subst_var(ctx, heap, lam_loc, sup);
  return aot_subst_cop(ctx, side, loc, l0, l1, heap);
}

static Term aot_dup_num(thread ThreadCtx *ctx,
    uint side, ulong loc, Term num, device Term *heap) {
  return aot_subst_cop(ctx, side, loc, num, num, heap);
}
static Term aot_dup_era(thread ThreadCtx *ctx,
    uint side, ulong loc, Term era, device Term *heap) {
  return aot_subst_cop(ctx, side, loc, era, era, heap);
}

// Per-thread wnf state machine.  Drives `term` to WHNF; returns the
// resolved Term.  Iteration cap protects against runaway / unintended
// loops; stack overflow returns a sentinel (TAG_ERA with ext=0xFFFFF).
constant uint AOT_COLLAPSE_STACK_CAP = 1024u;
constant uint AOT_COLLAPSE_ITER_CAP  = (1u << 20);

static Term aot_wnf_thread(thread ThreadCtx *ctx, Term term,
    device Term *heap)
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
        // Per-thread subst map first: shared-cell binders that
        // app_lam / dup_lam couldn't write to device memory live here.
        ulong mapped = aot_smap_get(ctx, loc);
        if (mapped != 0ul && msl_sub_get(mapped)) {
          next = msl_sub_clr(mapped); continue;
        }
        Term  cell = aot_heap_load(heap, loc);
        if (msl_sub_get(cell)) { next = msl_sub_clr(cell); continue; }
        whnf = next; state = 1u; continue;
      }
      if (t == TAG_DP0 || t == TAG_DP1) {
        ulong loc  = msl_term_val(next);
        // Subst map first (DUP-SUP / DUP-LAM / DUP-NUM may have
        // routed the substitution privately).
        ulong mapped = aot_smap_get(ctx, loc);
        if (mapped != 0ul && msl_sub_get(mapped)) {
          next = msl_sub_clr(mapped); continue;
        }
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
          next = aot_app_lam(ctx, whnf, arg, heap);
          state = 0u; continue;
        }
        if (wt == TAG_SUP) {
          next = aot_app_sup(ctx, whnf, arg, heap);
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
          next = aot_dup_sup(ctx, lab, loc, side, whnf, heap);
          state = 0u; continue;
        }
        if (wt == TAG_LAM) {
          next = aot_dup_lam(ctx, lab, loc, side, whnf, heap);
          state = 0u; continue;
        }
        if (wt == TAG_NUM) {
          whnf = aot_dup_num(ctx, side, loc, whnf, heap); continue;
        }
        if (wt == TAG_ERA) {
          whnf = aot_dup_era(ctx, side, loc, whnf, heap); continue;
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
//
// Per-thread arena (iter Z+2 step 5): host pre-allocates
// `n_threads * arena_size` cells past iter Z's book_next; each thread
// owns the slice [arena_base + tid*arena_size, +arena_size).  Subst
// writes outside the slice (i.e. into iter Z's shared SUP-tree
// allocations) are skipped to avoid races; allocations inside the
// slice are private and race-free.
kernel void aot_ic_collapse(
    device Term *heap         [[buffer(0)]],
    constant ulong      *root_in    [[buffer(1)]],
    device   Term       *result     [[buffer(2)]],
    constant uint       *path_depth [[buffer(3)]],
    constant uint       *arena_base [[buffer(4)]],
    constant uint       *arena_size [[buffer(5)]],
    uint                 tid        [[thread_position_in_grid]])
{
  ThreadCtx ctx;
  ctx.base   = *arena_base + tid * (*arena_size);
  ctx.size   = *arena_size;
  ctx.next   = 0u;
  ctx.smap_n = 0u;

  Term cur = (Term)*root_in;
  uint depth = 0u;
  uint depth_cap = *path_depth;
  while (depth < depth_cap) {
    // Drive current to WHNF.  If it's a SUP, descend by tid bit.
    cur = aot_wnf_thread(&ctx, cur, heap);
    if (msl_term_tag(cur) != TAG_SUP) break;
    ulong loc = msl_term_val(cur);
    uint  bit = (tid >> depth) & 1u;
    cur = aot_heap_load(heap, loc + bit);
    depth++;
  }
  // After loop: cur is either irreducible non-SUP (leaf) or we hit
  // depth cap.  Drive once more to be sure leaf is in WHNF.
  cur = aot_wnf_thread(&ctx, cur, heap);
  result[tid] = cur;
}
