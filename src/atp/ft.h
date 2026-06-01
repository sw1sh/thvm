// ft.h - AtpFt flatterm cell + dual-arena allocator state.
//
// Parallel native-flatterm representation modeled on Waldmeister's
// TermzellenT.  See docs/atp/engineering.md for the design.  This
// header pins the cell layout and the allocator state; operations
// live in ft_alloc.c, ft.c, ft_match.c, ft_splice.c, ft_norm.c,
// ft_ri.c, ft_cpq.c.
//
// Layout notes:
//   - AtpFtCell is 24 bytes, 8-byte aligned.  The two pointer fields
//     `next` and `end` are at the head of the struct so the free list
//     can overlay a `struct AtpFtCell *` onto `next` directly -- the
//     same trick WM uses for TermzellenlisteLoeschen ("term-cell list
//     deletion", chain-push of a contiguous span back to the free
//     list, O(1) regardless of span length).
//   - Field order is also chosen so `cell->end->next` IS the standard
//     "next-sibling-of-this-subterm" jump that the existing
//     KboFlatNode walker already consumes (Stage 3 wires LPO/KBO to
//     read AtpFtCell* directly with zero re-encode).

#ifndef THVM_ATP_FT_H
#define THVM_ATP_FT_H

#include "../thvm.h"

// --- Cell --------------------------------------------------------------
//
// `next`  = next cell in the pre-order walk (WM `Nachf`).  When the
//           cell is on the persistent free list, `next` is reused as
//           the free-list link -- safe because a free cell is never
//           dereferenced through any other field.
// `end`   = last cell of THIS subterm (WM `Ende`); `end->next` is the
//           next sibling of the current subterm.
// `sym`   = symbol id.  High bit set marks a variable (var id in the
//           low 31 bits); a constructor's symbol fits in 31 bits too.
// `arity` = cached arity, 0..REWRITE_MAX_ARITY.  Cached on the cell so
//           per-visit walks (LPO/KBO/match) avoid a heap_read per node
//           -- the largest single reason today's FLAT cp-gen lags
//           TREE.
// `flags` = bit0 subst_fresh (WM substFlag, innermost-rewrite skip),
//           bit1 ground (OR-folded at construction by ftnew_ctr -- gives
//           LPO an O(1) ground test it lacks today),
//           bit2 lpo_rank_valid (Stage 3+).
// `_pad`  = pad to keep `sym/arity/flags/_pad` an 8-byte tail.
typedef struct AtpFtCell {
  struct AtpFtCell *next;   // 8B
  struct AtpFtCell *end;    // 8B
  u32               sym;    // 4B
  u16               arity;  // 2B
  u8                flags;  // 1B
  u8                _pad;   // 1B
} AtpFtCell;

// Compile-time guarantee: cells stay 24 bytes / 8-byte aligned.  The
// slab arithmetic in ft_alloc.c (and the chain-push free idiom) both
// assume this -- if a future field bumps the struct, the slab block
// math must be re-validated.
_Static_assert(sizeof(AtpFtCell) == 24, "AtpFtCell must be 24 bytes");
_Static_assert(_Alignof(AtpFtCell) == 8, "AtpFtCell must be 8-byte aligned");

// --- Flag bits ---------------------------------------------------------
#define ATPFT_FLAG_SUBST_FRESH    0x01u
#define ATPFT_FLAG_GROUND         0x02u
#define ATPFT_FLAG_LPO_RANK_VALID 0x04u

// --- Slab parameters ---------------------------------------------------
//
// 30000 cells per block matches WM (and works out to 720 KB per slab
// at 24 B/cell, the same order as the existing rule-index pool).
// Blocks are malloc'd once and never returned to the OS -- on
// ft_destroy the entire list is freed in one pass.
#define ATPFT_CELLS_PER_BLOCK 30000u

// Scratch arena starts at 256 KB (~10923 cells).  On bump overflow the
// region is realloc'd 2x; pointers handed out before the realloc are
// invalidated, so scratch cells are short-lived by construction (per
// the plan: the scratch arena is reset at the top of every push-norm
// / cp-gen call).
#define ATPFT_SCRATCH_INIT_BYTES (256u * 1024u)

// --- Allocator state ---------------------------------------------------
//
// `blocks`            = head of singly-linked slab list (each block is
//                       malloc'd as one AtpFtBlock + ATPFT_CELLS_PER_BLOCK
//                       cells, see ft_alloc.c).
// `free_head`         = head of per-allocator free list, threaded
//                       through `cell->next`.  Push/pop is O(1) at the
//                       head; ft_free_span chain-pushes a pre-linked
//                       span without walking it.
// `scratch_base/top/end`
//                     = bump pointer over a single contiguous region.
//                       `top` is the next allocation; `end` is one past
//                       the last cell.  Reset = `top = base`.
// `n_persistent_alive`= live count in Arena A.
// `n_scratch_alive`   = high-water for the current scratch region (reset
//                       to 0 on ft_scratch_reset).
// `n_blocks`          = number of persistent slabs allocated.

typedef struct AtpFtBlock AtpFtBlock;  // defined in ft_alloc.c

typedef struct AtpFt {
  AtpFtBlock *blocks;       // persistent slab list head
  AtpFtCell  *free_head;    // persistent free-list head (NULL = empty)
  AtpFtCell  *scratch_base; // scratch arena: bump region [base, end)
  AtpFtCell  *scratch_top;  // next bump slot
  AtpFtCell  *scratch_end;  // one past last cell in the region
  u64         n_persistent_alive;
  u64         n_scratch_alive;
  u32         n_blocks;
  u32         _pad;
} AtpFt;

// --- API ---------------------------------------------------------------
//
// All entry points are plain C function declarations -- the body in
// ft_alloc.c is `static` (file-local under the single-TU build) and
// declared here for callers in the same TU.

void       ft_init           (AtpFt *a);
void       ft_destroy        (AtpFt *a);

AtpFtCell *ft_alloc_persistent(AtpFt *a);
AtpFtCell *ft_alloc_scratch   (AtpFt *a);

// Push a pre-linked span [first..last] back onto the persistent free
// list.  Cells must already be threaded via `next` (so the caller
// either keeps the original next pointers or stitches a span on the
// fly).  `last->next` is set to the current free-list head by this
// call -- callers do NOT need to pre-nul-terminate.  O(1).
void       ft_free_span       (AtpFt *a, AtpFtCell *first, AtpFtCell *last);

// Reset the scratch bump pointer.  All AtpFtCell* previously handed
// out by ft_alloc_scratch are invalid after this call.  O(1).
void       ft_scratch_reset   (AtpFt *a);

// Visit every CURRENTLY-ALLOCATED persistent cell exactly once.  The
// walk identifies "allocated" as "not on the free list", so the
// caller must not have re-purposed a cell's `next` field between
// allocation and walk (the persistent invariant: while a cell is
// allocated, its `next` points to the next cell in the term, not into
// the free list).  Used by the Stage 4 GC root scan; Stage 1 only
// exercises it from the test.
typedef void (*AtpFtVisitFn)(AtpFtCell *cell, void *ctx);
void       ft_walk_persistent(AtpFt *a, AtpFtVisitFn visit, void *ctx);

#endif // THVM_ATP_FT_H
