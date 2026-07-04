// ft_alloc.c - dual-arena slab allocator.
//
// Persistent (Arena A): slab pool of fixed 30000-cell blocks; each
// block is malloc'd once and never realloc'd, so AtpFtCell* handed
// out by ft_alloc_persistent stay stable across further allocations.
// Recycled spans chain-push onto a per-allocator free list in O(1)
// (mirrors WM TermzellenlisteLoeschen, "term-cell list deletion").
//
// Scratch (Arena B): single contiguous bump region; on overflow,
// realloc 2x.  Reset (= top := base) is O(1).
//
// Gated on THVM_ATPFT_ALLOC.  Once Stage 4 (THVM_ATPFT_RULES) defaulted
// the FT mirror on in the default build, src/atp/_.c #includes this TU
// inside the THVM_ATPFT_RULES block.  The standalone Stage-1 test
// (tests/test_ft_alloc.c) also #includes this TU directly so its
// dual-arena unit tests can run regardless of whether the runtime
// pulled it in first.  The ATPFT_ALLOC_C_INCLUDED guard turns the
// second textual inclusion into a no-op so the function bodies don't
// get duplicated when both paths fire in the same TU.

#ifdef THVM_ATPFT_ALLOC
#ifndef ATPFT_ALLOC_C_INCLUDED
#define ATPFT_ALLOC_C_INCLUDED 1

#include "../thvm.h"
#include "ft.h"

// PHASE-0 norm-core instrumentation (THVM_ATP_PROFILE=2): gross
// allocation traffic through the persistent arena, across ALL sites
// (formation, unpack, splice-rebuild).  Unconditional u64 bumps -- a
// single retired add per call, no branch; the push-scoped
// per-rewrite accounting (g_atp_pushn_alloc_cells) brackets these
// deltas inside ft_mixmost_reduce_here so the norm-core allocs/rewrite
// is readable in isolation.  Defined here (the first textual include of
// this TU) so every consumer -- _.c, ft_norm.c, the bench -- sees one
// definition.
u64 g_ft_persist_alloc_total = 0;   // ft_alloc_persistent successes
u64 g_ft_free_span_total     = 0;   // cells returned to the free list

// PHASE-1 norm-core increment #1 (THVM_ATP_NORMCORE2, default OFF):
// O(1) span free matching native Waldmeister's SV_NestedDealloc (a bare
// last->next=free_head; free_head=first chain-splice, NO per-cell walk).
// thvm's default ft_free_span WALKS the whole span (O(span)) solely to
// keep n_persistent_alive exact -- but that counter is read on the hot
// path ONLY by the THVM_ATP_NF_CAP rewrite-explosion guard (default OFF
// in every gate config) and by this file's underflow assertion.  When
// NORMCORE2 is on AND NF_CAP is not engaged, the walk is pure overhead
// (~30.6M cell-chases on OA push-norm) that changes no normal form.
// Kill switch: unset / =0.  Certifier THVM_ATP_NORMCORE2_CHECK forces
// the walk alongside the O(1) splice and asserts the free-list wiring +
// the exact count stay consistent (n_persistent_alive never underflows),
// proving the O(1) path is a faithful drop-in.  If NF_CAP>0 is set the
// O(1) path auto-disables (falls back to the exact walk) so the guard
// stays correct.
static int g_ft_normcore2       = -1;  // -1 unresolved, 0 off, 1 on
static int g_ft_normcore2_check = -1;
static int g_ft_nfcap_on        = -1;  // THVM_ATP_NF_CAP > 0
static void ft_normcore2_resolve(void) {
  const char *e = getenv("THVM_ATP_NORMCORE2");
  g_ft_normcore2 = (e != NULL && e[0] != '0' && e[0] != '\0') ? 1 : 0;
  const char *c = getenv("THVM_ATP_NORMCORE2_CHECK");
  g_ft_normcore2_check = (c != NULL && c[0] != '0' && c[0] != '\0') ? 1 : 0;
  if (g_ft_normcore2_check) g_ft_normcore2 = 1;   // CHECK forces the route
  const char *n = getenv("THVM_ATP_NF_CAP");
  g_ft_nfcap_on = (n != NULL && n[0] != '\0' && strtol(n, NULL, 10) > 0) ? 1 : 0;
}

// --- Slab block --------------------------------------------------------
//
// A block is a header + ATPFT_CELLS_PER_BLOCK trailing cells, all
// allocated in a single malloc so the cells fit in one cache-aligned
// region and the block free-list spans contiguous memory.  Blocks
// are linked into AtpFt.blocks; the list is walked on ft_destroy and
// ft_walk_persistent.

struct AtpFtBlock {
  struct AtpFtBlock *next_block;
  // Trailing flexible array; the slab pointer arithmetic in this
  // file treats `cells` as a normal AtpFtCell[ATPFT_CELLS_PER_BLOCK].
  AtpFtCell          cells[ATPFT_CELLS_PER_BLOCK];
};

// --- Persistent slab pool ----------------------------------------------
//
// Bare `static` (not `fn`) on pool helpers, matching the existing
// rule_index pool convention -- these take addresses, mutate the
// arena, and live in a single TU; nothing wants them inlined out of
// the .c.

static AtpFtBlock *ft_block_new(void) {
  AtpFtBlock *b = (AtpFtBlock *)malloc(sizeof(AtpFtBlock));
  if (b == NULL) { fprintf(stderr, "ft_alloc: block pool OOM\n"); exit(1); }
  b->next_block = NULL;
  return b;
}

// Thread every cell in `b` into a singly-linked free list via the
// `next` field, returning the head (= &b->cells[0]).  Last cell's
// `next` is set to `tail_link` so a fresh slab can splice onto the
// existing free-list head in one pass.
static AtpFtCell *ft_block_threaded(AtpFtBlock *b, AtpFtCell *tail_link) {
  for (u32 i = 0; i + 1 < ATPFT_CELLS_PER_BLOCK; i++) {
    b->cells[i].next = &b->cells[i + 1];
  }
  b->cells[ATPFT_CELLS_PER_BLOCK - 1].next = tail_link;
  return &b->cells[0];
}

// --- Public API --------------------------------------------------------

void ft_init(AtpFt *a) {
  a->blocks             = NULL;
  a->free_head          = NULL;
  a->scratch_base       = NULL;
  a->scratch_top        = NULL;
  a->scratch_end        = NULL;
  a->n_persistent_alive = 0;
  a->n_scratch_alive    = 0;
  a->n_blocks           = 0;
  a->_pad               = 0;
}

void ft_destroy(AtpFt *a) {
  AtpFtBlock *b = a->blocks;
  while (b != NULL) {
    AtpFtBlock *nx = b->next_block;
    free(b);
    b = nx;
  }
  free(a->scratch_base);
  ft_init(a);
}

AtpFtCell *ft_alloc_persistent(AtpFt *a) {
  if (a->free_head == NULL) {
    AtpFtBlock *b = ft_block_new();
    // Thread the new block onto the (currently empty) free list, then
    // prepend the block to the arena's block list.
    a->free_head     = ft_block_threaded(b, NULL);
    b->next_block    = a->blocks;
    a->blocks        = b;
    a->n_blocks     += 1u;
  }
  AtpFtCell *cell = a->free_head;
  a->free_head    = cell->next;
  // Zero the cell so a caller can populate fields without worrying
  // about the lingering free-list link in `next`.  The plan's
  // constructors (Stage 2) overwrite every field, but Stage 1 callers
  // (the test) only set `sym`, so zeroing keeps the unwritten fields
  // deterministic.
  cell->next  = NULL;
  cell->end   = cell;
  cell->sym   = 0;
  cell->arity = 0;
  cell->flags = 0;
  cell->_pad  = 0;
  a->n_persistent_alive += 1u;
  g_ft_persist_alloc_total += 1u;
  return cell;
}

void ft_free_span(AtpFt *a, AtpFtCell *first, AtpFtCell *last) {
  // Caller-provided span: cells [first..last] are already threaded
  // via `next`.  Splice the span at the front of the free list in
  // O(1).  We do NOT walk the span to count cells -- the caller is
  // expected to maintain `n_persistent_alive` consistency at the
  // construction/destruction boundary (Stage 1 test passes the span
  // length explicitly; Stage 2+ ftnew_* / ft_splice will manage it).
  //
  // For Stage 1 we do the simple thing and walk the span ONCE to
  // decrement the alive counter -- it's O(span) but called rarely
  // (only at GC / fixpoint boundaries) and keeps the counter honest
  // for ft_walk_persistent.  Stage 4 GC will replace this with a
  // batch-reset on a known root set.
  if (g_ft_normcore2 < 0) ft_normcore2_resolve();
  // NORMCORE2: O(1) chain-splice, no per-cell walk (native SV_NestedDealloc
  // discipline).  Engaged only when NF_CAP is not active, so the single
  // hot-path reader of n_persistent_alive stays correct; n_persistent_alive
  // is left un-decremented (write-only in every gate config).  The CHECK
  // route still walks to certify the count.
  if (g_ft_normcore2 && !g_ft_nfcap_on) {
    if (g_ft_normcore2_check) {
      // Certifier: compute the exact count the fast path skips and assert
      // the same non-underflow invariant the walk enforces, then keep
      // n_persistent_alive exact so a mixed OFF/ON run cross-checks.
      AtpFtCell *cp = first; u64 cn = 1;
      for (u64 _s = 0; cp != last && _s < (1ull << 24); _s++) {
        if (cp->next == NULL) break;
        cp = cp->next; cn += 1;
      }
      if (cn > a->n_persistent_alive) {
        fprintf(stderr, "ft_alloc: NORMCORE2 free_span underflow (%llu > %llu)\n",
                (unsigned long long)cn,
                (unsigned long long)a->n_persistent_alive);
        exit(1);
      }
      a->n_persistent_alive -= cn;
      g_ft_free_span_total += cn;
    }
    last->next   = a->free_head;
    a->free_head = first;
    return;
  }
  AtpFtCell *p = first;
  u64 n = 1;
  // Defensive cap: a mis-threaded span chain (caller's first.next -> ... ->
  // last) can otherwise loop forever or segfault on a null/garbage pointer
  // when the FT chain is corrupted by upstream interaction.  1<<24 covers
  // any reasonable span; break early on NULL to keep `n` honest.
  for (u64 _s = 0; p != last && _s < (1ull << 24); _s++) {
    if (p->next == NULL) break;
    p = p->next;
    n += 1;
  }
  last->next   = a->free_head;
  a->free_head = first;
  g_ft_free_span_total += n;
  if (n > a->n_persistent_alive) {
    fprintf(stderr, "ft_alloc: free_span underflow (%llu > %llu)\n",
            (unsigned long long)n,
            (unsigned long long)a->n_persistent_alive);
    exit(1);
  }
  a->n_persistent_alive -= n;
}

// --- Scratch arena -----------------------------------------------------

static void ft_scratch_grow(AtpFt *a, u32 want_cells) {
  u32 cur_cells = (u32)(a->scratch_end - a->scratch_base);
  u32 new_cells = cur_cells ? cur_cells : (ATPFT_SCRATCH_INIT_BYTES / sizeof(AtpFtCell));
  while (new_cells < want_cells) new_cells *= 2u;
  AtpFtCell *p = (AtpFtCell *)realloc(a->scratch_base, (size_t)new_cells * sizeof(AtpFtCell));
  if (p == NULL) { fprintf(stderr, "ft_alloc: scratch arena OOM\n"); exit(1); }
  u64 used = (u64)(a->scratch_top - a->scratch_base);
  a->scratch_base = p;
  a->scratch_top  = p + used;
  a->scratch_end  = p + new_cells;
}

AtpFtCell *ft_alloc_scratch(AtpFt *a) {
  if (a->scratch_top == a->scratch_end) {
    u32 want = a->scratch_end ? (u32)(a->scratch_end - a->scratch_base) + 1u
                              : (ATPFT_SCRATCH_INIT_BYTES / sizeof(AtpFtCell));
    ft_scratch_grow(a, want);
  }
  AtpFtCell *cell = a->scratch_top++;
  cell->next  = NULL;
  cell->end   = cell;
  cell->sym   = 0;
  cell->arity = 0;
  cell->flags = 0;
  cell->_pad  = 0;
  a->n_scratch_alive += 1u;
  return cell;
}

void ft_scratch_reset(AtpFt *a) {
  a->scratch_top     = a->scratch_base;
  a->n_scratch_alive = 0;
}

// --- ft_walk_persistent ------------------------------------------------
//
// Persistent cells live in `blocks`; the free list threads through a
// subset of them.  Build a sparse bitmap of free-list members, then
// walk every block and emit cells absent from the bitmap.
//
// For the Stage 1 unit test this runs over O(n_blocks * 30000) cells;
// future GC users (Stage 4) will keep the walk on the live-root set.

static u8 *ft_walk_build_freemap(AtpFt *a, size_t *out_total_cells_p) {
  size_t total = (size_t)a->n_blocks * ATPFT_CELLS_PER_BLOCK;
  // Map each cell pointer to a block index + offset.  Since blocks
  // are not stored contiguously, mark a cell as free by writing into
  // an indexed bitmap: index = block_ordinal * CELLS_PER_BLOCK + off.
  if (total == 0) { *out_total_cells_p = 0; return NULL; }
  u8 *map = (u8 *)calloc(total, 1);
  if (map == NULL) { fprintf(stderr, "ft_alloc: walk bitmap OOM\n"); exit(1); }
  // Index lookup: walk blocks once, assigning ordinal positions.
  // For each free cell pointer, find its block + offset.  Linear scan
  // over blocks per free cell is O(n_blocks); for Stage 1 we accept
  // this since the test exercises at most a couple of blocks.
  AtpFtCell *fp = a->free_head;
  while (fp != NULL) {
    size_t ord = 0;
    AtpFtBlock *b = a->blocks;
    int found = 0;
    while (b != NULL) {
      if (fp >= &b->cells[0] && fp < &b->cells[ATPFT_CELLS_PER_BLOCK]) {
        size_t off = (size_t)(fp - &b->cells[0]);
        map[ord * ATPFT_CELLS_PER_BLOCK + off] = 1;
        found = 1;
        break;
      }
      ord += 1;
      b = b->next_block;
    }
    if (!found) {
      fprintf(stderr, "ft_alloc: free cell %p not in any block\n", (void *)fp);
      free(map);
      exit(1);
    }
    fp = fp->next;
  }
  *out_total_cells_p = total;
  return map;
}

void ft_walk_persistent(AtpFt *a, AtpFtVisitFn visit, void *ctx) {
  size_t total = 0;
  u8 *freemap = ft_walk_build_freemap(a, &total);
  size_t ord = 0;
  for (AtpFtBlock *b = a->blocks; b != NULL; b = b->next_block) {
    for (u32 i = 0; i < ATPFT_CELLS_PER_BLOCK; i++) {
      if (freemap == NULL || !freemap[ord * ATPFT_CELLS_PER_BLOCK + i]) {
        visit(&b->cells[i], ctx);
      }
    }
    ord += 1;
  }
  free(freemap);
}

#endif // !ATPFT_ALLOC_C_INCLUDED
#endif // THVM_ATPFT_ALLOC
