// ft.c - constructors, boundary converters, structural
// equality / hash, and pretty-print mirroring atp_pretty_term.
//
// Stage 2 is purely additive: no caller in the rest of the engine
// reads through these yet.  The unit test tests/test_ft.c builds 1000
// random Terms and round-trips them via ft_from_term / ft_to_term,
// also asserting ft_hash agrees with the existing atp_term_struct_hash
// (the latter is the bridge for the Stage-9 memo re-key).
//
// Var encoding: AtpFtCell.sym uses WM's convention (high bit set =
// variable, low 31 bits = var id).  This matches the doc in ft.h:36
// and lets the wmfpa flatterm matcher round-trip Term<->WfNode
// through the same encoding (see WF_VAR_BIT in src/wmfpa/wmfpa.h:54;
// reused here directly via the named macros).
//
// Gated on THVM_ATPFT_CONVERT.  src/atp/_.c pulls this TU in under
// THVM_ATPFT_RULES (default-on); the stage-2 standalone test
// (tests/test_ft.c) also includes it directly.  The
// ATPFT_FT_C_INCLUDED guard makes the second textual inclusion a no-op.

#ifdef THVM_ATPFT_CONVERT
#ifndef ATPFT_FT_C_INCLUDED
#define ATPFT_FT_C_INCLUDED 1

#include "../thvm.h"
#include "ft.h"

// --- Var-bit helpers ---------------------------------------------------
//
// Reuse the existing WF_VAR_BIT macros (src/wmfpa/wmfpa.h:54-57) rather
// than minting a new ATPFT_VAR_BIT: the cell-layout doc in ft.h:36
// mandates exactly that encoding ("High bit set marks a variable") and
// the flat WM matcher in _.c already round-trips Term<->WF through it.
// Numerically identical (0x80000000), same id mask -- same namespace.
#include "../wmfpa/wmfpa.h"

static inline int  ft_is_var (const AtpFtCell *c) {
  return (c->sym & WF_VAR_BIT) != 0u;
}
static inline u32  ft_var_id (const AtpFtCell *c) {
  return c->sym & ~WF_VAR_BIT;
}
static inline u32  ft_ctr_sym(const AtpFtCell *c) {
  // Only valid when ft_is_var(c) is false.  Caller's responsibility.
  return c->sym;
}

// --- Internal alloc routing -------------------------------------------
//
// scratch=0 -> Arena A (persistent); scratch=1 -> Arena B (bump).  The
// Stage 1 allocators already zero every field on hand-out
// (ft_alloc.c:103-108, 164-169), so ftnew_* only has to fill in the
// fields it owns (next/end/sym/arity + GROUND).  SUBST_FRESH and
// LPO_RANK_VALID stay clear by construction.
//
// THVM_ATP_FT_RAW_ALLOC (default OFF): route to the raw (non-zeroing)
// allocator -- the zeroing is redundant because ftnew_* overwrite every
// read field and `_pad` is never read (ft_alloc.c).  This is the first
// increment of the in-place rewrite's allocation phase (native SV_Alloc
// hands raw memory); default-OFF, same-binary kill switch, byte-identical
// trajectory (pins gate it).  THVM_ATP_FT_RAW_ALLOC_CHECK forces the raw
// route AND poisons the scalar fields so a constructor that fails to
// overwrite a read field trips ft_raw_check below -- proven to fire on a
// deliberate constructor-field skip.  See docs/plans/atp_inplace_rewrite.md.
static int g_ft_raw_alloc       = -1;   // -1 unresolved, 0 off, 1 on
static int g_ft_raw_alloc_check = -1;
static void ft_raw_alloc_resolve(void) {
  const char *e = getenv("THVM_ATP_FT_RAW_ALLOC");
  g_ft_raw_alloc = (e != NULL && e[0] != '\0' && e[0] != '0') ? 1 : 0;
  const char *c = getenv("THVM_ATP_FT_RAW_ALLOC_CHECK");
  g_ft_raw_alloc_check = (c != NULL && c[0] != '\0' && c[0] != '0') ? 1 : 0;
  if (g_ft_raw_alloc_check) g_ft_raw_alloc = 1;   // CHECK forces the raw route
}
#define FT_RAW_POISON_SYM   0xFFFFFFFFu
#define FT_RAW_POISON_ARITY 0xFFFFu
#define FT_RAW_POISON_FLAGS 0xFFu

static inline AtpFtCell *ft_alloc_one(AtpFt *a, int scratch) {
  if (g_ft_raw_alloc < 0) ft_raw_alloc_resolve();
  if (g_ft_raw_alloc) {
    AtpFtCell *c = scratch ? ft_alloc_scratch_raw(a) : ft_alloc_persistent_raw(a);
    if (g_ft_raw_alloc_check) {
      c->sym   = FT_RAW_POISON_SYM;
      c->arity = FT_RAW_POISON_ARITY;
      c->flags = FT_RAW_POISON_FLAGS;
      c->_pad  = 0xFFu;
    }
    return c;
  }
  return scratch ? ft_alloc_scratch(a) : ft_alloc_persistent(a);
}

// Certifier: assert the constructor overwrote every READ scalar field
// (sym/arity/flags).  `_pad` is exempt -- it is never read (static audit),
// and next/end are pointers the three constructors always set (verified in
// ft_alloc.c's raw-alloc note).  A no-op unless CHECK is engaged.
static inline void ft_raw_check(const AtpFtCell *c) {
  if (g_ft_raw_alloc_check) {
    if (c->sym == FT_RAW_POISON_SYM || c->arity == FT_RAW_POISON_ARITY
        || c->flags == FT_RAW_POISON_FLAGS) {
      fprintf(stderr,
              "ft_raw_alloc: constructor left a READ field poisoned "
              "(sym=%u arity=%u flags=%u)\n",
              c->sym, (u32)c->arity, (u32)c->flags);
      exit(1);
    }
  }
}

// --- Construction ------------------------------------------------------
//
// A variable cell is a single cell: end == self, no children, arity 0.
// Variables are NEVER ground (a ground term has no FVR leaves), so the
// GROUND flag stays clear.

AtpFtCell *ftnew_var(AtpFt *a, u32 var_id, int scratch) {
  AtpFtCell *c = ft_alloc_one(a, scratch);
  c->next  = NULL;        // caller stitches the next-sibling link when
                          // inserting into a parent CTR.
  c->end   = c;
  c->sym   = WF_VAR_BIT | (var_id & ~WF_VAR_BIT);
  c->arity = 0;
  c->flags = 0;           // SUBST_FRESH=0, GROUND=0, LPO_RANK_VALID=0.
  ft_raw_check(c);
  return c;
}

// A 0-arity constructor (a "constant") is a single cell, ground by
// construction (no children that could carry an FVR).
AtpFtCell *ftnew_const(AtpFt *a, u32 sym, int scratch) {
  AtpFtCell *c = ft_alloc_one(a, scratch);
  c->next  = NULL;
  c->end   = c;
  // sym MUST NOT have the high bit set (that would alias the var-flag
  // namespace).  Mask defensively -- the caller almost certainly
  // passes a low-bit constructor id; the mask only kicks in if a
  // misuse drives a sym into the var range.
  c->sym   = sym & ~WF_VAR_BIT;
  c->arity = 0;
  c->flags = ATPFT_FLAG_GROUND;
  ft_raw_check(c);
  return c;
}

// A CTR cell: allocate one cell for the parent, then stitch the
// `kids` subterm spans into a pre-order chain.  Layout invariants
// (from ft.h):
//
//   parent->next == first cell of kid[0] (== kid[0] itself).
//   for i in 0..arity-2:
//     kid[i]->end->next == first cell of kid[i+1]
//   parent->end == last cell of kid[arity-1]'s subterm (== kid[n-1]->end)
//   parent->end->next == NULL initially (the caller stitches the
//                        next-sibling link if this parent is itself a
//                        child of an outer CTR; for a top-level term
//                        the trailing NULL is fine).
//
// Each kid was previously built by ftnew_var / ftnew_const / ftnew_ctr
// and currently has kid->end->next == NULL (top-level), so the stitch
// is purely overwriting that trailing NULL with the next sibling's
// head.  This matches how a multi-kid persistent term threads in
// pre-order under WM's TermzellenT.
//
// GROUND is OR-folded: a CTR is ground iff every child is ground.
AtpFtCell *ftnew_ctr(AtpFt *a, u32 sym, u16 arity,
                     AtpFtCell *const *kids, int scratch) {
  AtpFtCell *c = ft_alloc_one(a, scratch);
  c->sym   = sym & ~WF_VAR_BIT;
  c->arity = arity;
  c->flags = ATPFT_FLAG_GROUND;     // tentatively; OR-folded below
  if (arity == 0u) {
    c->next = NULL;
    c->end  = c;
    ft_raw_check(c);
    return c;
  }
  // Stitch first child.
  AtpFtCell *first = kids[0];
  c->next = first;
  AtpFtCell *prev_end = first->end;
  if ((first->flags & ATPFT_FLAG_GROUND) == 0u) c->flags = 0;
  for (u16 i = 1; i < arity; i++) {
    AtpFtCell *k = kids[i];
    prev_end->next = k;
    prev_end       = k->end;
    if ((k->flags & ATPFT_FLAG_GROUND) == 0u) c->flags = 0;
  }
  c->end           = prev_end;
  prev_end->next   = NULL;
  ft_raw_check(c);
  return c;
}

// --- Boundary: Term -> AtpFt -----------------------------------------
//
// Walk over a heap-cell Term.  TAG_FVR -> ftnew_var, TAG_CTR ->
// ftnew_ctr over the converted children, everything else falls back
// to a constant cell keyed by tag (the only other thing the ATP layer
// ever puts into a term position is TAG_NUM, which we encode as a
// 0-arity constructor with sym = (TAG_NUM << 24) | dtype<<16 |
// raw_lo16 -- chosen so two distinct NUM Terms hash differently and
// the round-trip back to Term reproduces the original).  The narrow
// boundary is exactly TAG_FVR + TAG_CTR; the fallback exists so a
// stray sibling-tagged Term doesn't crash the converter during
// bring-up.

// Pack a TAG_NUM into a constructor sym + a child carrying the raw
// bits.  TAG_NUM has no children in heap-cell land (atom; val = raw
// u32/f32 bits, ext = dtype -- thvm.h:118), but we still need to
// round-trip both fields.  Encode as:
//   sym = (TAG_NUM_MARKER) for the outer CTR cell
//   children = [dtype-const-cell, rawbits-const-cell]
// where each child is a 0-arity ftnew_const carrying the field in its
// sym.  ft_to_term inverts.  Marker value picks a sym range that
// won't collide with any constructor label the ATP layer actually
// uses (labels in the harvested wolfram.pr / sheffer.pr live in
// [0, 256) -- this marker lives at 0x40000000, well below
// WF_VAR_BIT and well above any plausible label).
#define ATPFT_NUM_MARKER 0x40000000u

// Every walk below keeps its pending frames on an explicit stack
// (FT_WALK_INL entries on the C stack, then the heap via
// term_stack_grow), so a deep term costs heap, never C stack.  Each
// walk visits nodes in the order the recursive form did and allocates
// cells / heap words in the same order, so outputs and arena / heap
// layout are those of the recursion.
enum { FT_WALK_INL = 64 };

// A Term CTR whose children are being converted: child i of the
// layout cell at `base` is heap_read(base + 1 + i).
typedef struct {
  u64 base;
  u32 i;
  u32 n;
  u32 label;
} FtFromFrame;

// Bottom-up: a CTR's children are built (left to right, each subtree
// whole) before ftnew_ctr stitches them under the parent.
static AtpFtCell *ft_from_term_up(AtpFt *a, Term t, int scratch) {
  extern unsigned char g_atp_phase_detail;
  FtFromFrame  fr_inl[FT_WALK_INL];
  AtpFtCell   *val_inl[FT_WALK_INL];
  FtFromFrame *fr   = fr_inl;
  AtpFtCell  **val  = val_inl;
  u32          nfr  = 0u;
  u32          cfr  = FT_WALK_INL;
  u32          nval = 0u;
  u32          cval = FT_WALK_INL;
  for (;;) {
    if (g_atp_phase_detail) { extern u64 g_ft_from_nodes; g_ft_from_nodes++; }
    AtpFtCell *done = NULL;
    u8 tag = term_tag(t);
    if (tag == TAG_FVR) {
      done = ftnew_var(a, term_ext(t), scratch);
    } else if (tag == TAG_CTR) {
      u32 n = term_ctr_n(t);
      if (n == 0u) {
        done = ftnew_const(a, term_ext(t), scratch);
      } else {
        if (nfr == cfr) {
          fr = (FtFromFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].base  = term_val(t);
        fr[nfr].i     = 0u;
        fr[nfr].n     = n;
        fr[nfr].label = term_ext(t);
        nfr++;
      }
    } else if (tag == TAG_NUM) {
      u32 dtype = term_ext(t);
      u32 raw   = (u32)term_val(t);
      AtpFtCell *dt_cell  = ftnew_const(a, dtype & ~WF_VAR_BIT, scratch);
      AtpFtCell *raw_cell = ftnew_const(a, raw   & ~WF_VAR_BIT, scratch);
      AtpFtCell *kids[2]  = { dt_cell, raw_cell };
      done = ftnew_ctr(a, ATPFT_NUM_MARKER, 2u, kids, scratch);
    } else {
      done = ftnew_const(a, 0x20000000u | (u32)tag, scratch);
    }
    if (done != NULL) {
      if (nval == cval) {
        val = (AtpFtCell **)term_stack_grow(val, val_inl, &cval, sizeof *val);
      }
      val[nval++] = done;
    }
    while (nfr > 0u && fr[nfr - 1u].i == fr[nfr - 1u].n) {
      FtFromFrame *f = &fr[nfr - 1u];
      nval -= f->n;
      val[nval] = ftnew_ctr(a, f->label, (u16)f->n, &val[nval], scratch);
      nval++;
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    FtFromFrame *f = &fr[nfr - 1u];
    t = heap_read(f->base + 1u + (u64)f->i);
    f->i++;
  }
  AtpFtCell *out = val[0];
  term_stack_release(fr, fr_inl);
  term_stack_release(val, val_inl);
  return out;
}

// Preorder-streaming term builder (FT-representation P1): allocate the
// PARENT cell BEFORE its children so the pre-order chain (parent->next
// == child0, child->end->next == next sibling) threads through
// PHYSICALLY-CONSECUTIVE cells on the bump/adjacency-preserving arena --
// Waldmeister's SigmaRGanzNeu discipline (MatchOperationen.c:1397),
// where a term is materialised straight down the free list so a
// pre-order walk is a near-sequential memory scan.  ft_from_term_up
// (the default) builds BOTTOM-UP (children before parent), so a
// pre-order walk hops backward in memory and every ->next is a
// dependent random load; this walk removes that on freshly-built terms.
//
// Byte-identical RESULT to ft_from_term_up: same sym/arity/flags/next/
// end on every cell (the GROUND flag is OR-folded identically -- cleared
// as soon as a non-ground child is seen).  Only the cell ADDRESSES
// differ, and no normal form / hash / KBO verdict observes an address
// (ftnfm is content-keyed over the pre-order SYMBOL sequence, not cell
// pointers), so the emitted RULEADD/proof stream is bit-for-bit
// unchanged.  Gated by THVM_ATP_FT_PREORDER (resolved once); default OFF
// = the bottom-up path, physically unchanged.
int g_ft_preorder = -1;
static inline int ft_preorder_on(void) {
  if (g_ft_preorder < 0) g_ft_preorder = getenv("THVM_ATP_FT_PREORDER") ? 1 : 0;
  return g_ft_preorder;
}

// A parent cell (already allocated) whose children are being built;
// `prev_end` is the last cell of the child stitched last.
typedef struct {
  u64        base;
  u32        i;
  u32        n;
  AtpFtCell *c;
  AtpFtCell *prev_end;
} FtPreFrame;

static AtpFtCell *ft_from_term_pre(AtpFt *a, Term t, int scratch) {
  extern unsigned char g_atp_phase_detail;
  FtPreFrame  fr_inl[FT_WALK_INL];
  FtPreFrame *fr  = fr_inl;
  u32         nfr = 0u;
  u32         cfr = FT_WALK_INL;
  AtpFtCell  *done;
  for (;;) {
    if (g_atp_phase_detail) { extern u64 g_ft_from_nodes; g_ft_from_nodes++; }
    done = NULL;
    u8 tag = term_tag(t);
    if (tag == TAG_FVR) {
      AtpFtCell *c = ft_alloc_one(a, scratch);
      c->next  = NULL;
      c->end   = c;
      c->sym   = WF_VAR_BIT | (term_ext(t) & ~WF_VAR_BIT);
      c->arity = 0;
      c->flags = 0;
      ft_raw_check(c);
      done = c;
    } else if (tag == TAG_CTR) {
      u32 label = term_ext(t);
      u32 n     = term_ctr_n(t);
      AtpFtCell *c = ft_alloc_one(a, scratch);   // PARENT FIRST (pre-order)
      c->sym   = label & ~WF_VAR_BIT;
      c->arity = (u16)n;
      c->flags = ATPFT_FLAG_GROUND;              // OR-folded below
      if (n == 0u) {
        c->next = NULL;
        c->end  = c;
        ft_raw_check(c);
        done = c;
      } else {
        if (nfr == cfr) {
          fr = (FtPreFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].base     = term_val(t);
        fr[nfr].i        = 0u;
        fr[nfr].n        = n;
        fr[nfr].c        = c;
        fr[nfr].prev_end = NULL;
        nfr++;
      }
    } else if (tag == TAG_NUM) {
      // Marker parent, then the two field constants, pre-order.
      u32 dtype = term_ext(t);
      u32 raw   = (u32)term_val(t);
      AtpFtCell *c = ft_alloc_one(a, scratch);
      c->sym   = ATPFT_NUM_MARKER;
      c->arity = 2u;
      c->flags = ATPFT_FLAG_GROUND;
      AtpFtCell *dt_cell = ft_alloc_one(a, scratch);
      dt_cell->next = NULL; dt_cell->end = dt_cell;
      dt_cell->sym = dtype & ~WF_VAR_BIT; dt_cell->arity = 0;
      dt_cell->flags = ATPFT_FLAG_GROUND;
      ft_raw_check(dt_cell);
      AtpFtCell *raw_cell = ft_alloc_one(a, scratch);
      raw_cell->next = NULL; raw_cell->end = raw_cell;
      raw_cell->sym = raw & ~WF_VAR_BIT; raw_cell->arity = 0;
      raw_cell->flags = ATPFT_FLAG_GROUND;
      ft_raw_check(raw_cell);
      c->next        = dt_cell;
      dt_cell->next  = raw_cell;
      c->end         = raw_cell;
      raw_cell->next = NULL;
      ft_raw_check(c);
      done = c;
    } else {
      AtpFtCell *c = ft_alloc_one(a, scratch);
      c->next  = NULL;
      c->end   = c;
      c->sym   = 0x20000000u | (u32)tag;
      c->arity = 0;
      c->flags = ATPFT_FLAG_GROUND;
      ft_raw_check(c);
      done = c;
    }
    // Stitch the finished span under its parent; a parent whose last
    // child is stitched is finished in turn.
    while (done != NULL && nfr > 0u) {
      FtPreFrame *f = &fr[nfr - 1u];
      if (f->i == 0u) f->c->next = done;         // done is the physically next cell
      else            f->prev_end->next = done;
      if ((done->flags & ATPFT_FLAG_GROUND) == 0u) {
        f->c->flags = 0;
      }
      f->prev_end = done->end;
      f->i++;
      if (f->i < f->n) {
        done = NULL;
        break;
      }
      AtpFtCell *c = f->c;
      c->end       = f->prev_end;
      c->end->next = NULL;
      ft_raw_check(c);
      done = c;
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    t = heap_read(fr[nfr - 1u].base + 1u + (u64)fr[nfr - 1u].i);
  }
  term_stack_release(fr, fr_inl);
  return done;
}

__attribute__((noinline)) void ft_region2_to_hook(void) {
  // lldb anchor: region-2 ft_to_term (interchange residue hunt).
  __asm__ volatile("");
}
__attribute__((noinline)) void ft_region2_from_hook(void) {
  // lldb anchor: region-2 ft_from_term (interchange residue hunt).
  __asm__ volatile("");
}
__attribute__((noinline)) void ft_region0_from_hook(void) {
  // lldb anchor: region-0 (untagged) ft_from_term.
  __asm__ volatile("");
}
__attribute__((noinline)) void ft_region0_to_hook(void) {
  // lldb anchor: region-0 (untagged) ft_to_term.
  __asm__ volatile("");
}
AtpFtCell *ft_from_term(AtpFt *a, Term t, int scratch) {
  extern unsigned char g_atp_phase_detail;
  if (g_atp_phase_detail) {
    extern u64 g_ft_from_calls; g_ft_from_calls++;
    extern u8 g_ft_region; extern u64 g_ft_from_by_region[8];
    g_ft_from_by_region[g_ft_region & 7u]++;
    if (g_ft_region == 2u) ft_region2_from_hook();
    if (g_ft_region == 0u) ft_region0_from_hook();
    if (getenv("THVM_ATP_FT2DBG") != NULL) {
      extern const char *g_thvm_phase;
      fprintf(stderr, "FT%uFROM phase=%s tag=%u ext=%u n=%u\n",
              (u32)g_ft_region, g_thvm_phase, (u32)term_tag(t),
              (u32)term_ext(t),
              term_tag(t) == TAG_CTR ? term_ctr_n(t) : 0u);
    }
  }
  if (ft_preorder_on()) return ft_from_term_pre(a, t, scratch);
  return ft_from_term_up(a, t, scratch);
}

// A span CTR cell whose children are being visited: `child` is the
// next child's head cell, `left` the children not yet visited.
typedef struct {
  const AtpFtCell *child;
  u32              left;
  u32              sym;
  u32              arity;
} FtSpanFrame;

// Bottom-up span copy through the ftnew_* constructors with every var
// id shifted by `offset` (0 = plain clone): children are built before
// ftnew_ctr stitches them, as ft_from_term_up does.
static AtpFtCell *ft_copy_span(AtpFt *a, const AtpFtCell *x, u32 offset,
                               int scratch) {
  FtSpanFrame  fr_inl[FT_WALK_INL];
  AtpFtCell   *val_inl[FT_WALK_INL];
  FtSpanFrame *fr   = fr_inl;
  AtpFtCell  **val  = val_inl;
  u32          nfr  = 0u;
  u32          cfr  = FT_WALK_INL;
  u32          nval = 0u;
  u32          cval = FT_WALK_INL;
  for (;;) {
    AtpFtCell *done = NULL;
    if (ft_is_var(x)) {
      done = ftnew_var(a, ft_var_id(x) + offset, scratch);
    } else if (x->arity == 0u) {
      done = ftnew_const(a, ft_ctr_sym(x), scratch);
    } else {
      if (nfr == cfr) {
        fr = (FtSpanFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
      }
      fr[nfr].child = x->next;
      fr[nfr].left  = x->arity;
      fr[nfr].sym   = ft_ctr_sym(x);
      fr[nfr].arity = x->arity;
      nfr++;
    }
    if (done != NULL) {
      if (nval == cval) {
        val = (AtpFtCell **)term_stack_grow(val, val_inl, &cval, sizeof *val);
      }
      val[nval++] = done;
    }
    while (nfr > 0u && fr[nfr - 1u].left == 0u) {
      FtSpanFrame *f = &fr[nfr - 1u];
      nval -= f->arity;
      val[nval] = ftnew_ctr(a, f->sym, (u16)f->arity, &val[nval], scratch);
      nval++;
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    FtSpanFrame *f = &fr[nfr - 1u];
    x        = f->child;
    f->child = x->end->next;
    f->left--;
  }
  AtpFtCell *out = val[0];
  term_stack_release(fr, fr_inl);
  term_stack_release(val, val_inl);
  return out;
}

// Structural clone of a cell span through the same ftnew_* constructors
// ft_from_term_up uses -- the output is bit-identical to
// ft_from_term(a, ft_to_term(x), scratch) with both Term hops removed
// (sym / arity / next / end threading and the GROUND fold all come from
// ftnew_ctr, exactly as a fresh conversion would).  Runtime-only flag
// bits the source may carry (SUBST_FRESH, LPO_RANK_VALID) are NOT
// copied, matching what a fresh conversion produces.  Used to hand a
// DESTRUCTIVE normalize a private copy of a live mirror span (rule or
// goal) without re-encoding the Term side.
AtpFtCell *ft_clone_span(AtpFt *a, const AtpFtCell *x, int scratch) {
  return ft_copy_span(a, x, 0u, scratch);
}

// Subterm addressing on a span: the cell at child-index path
// p[0..p_len) -- the FT sibling of cp_subterm_at.  Each step descends
// to the first child (parent->next) and advances end->next per index.
// Returns NULL on an out-of-range index (the Term walker's 0), and the
// addressed cell is the subterm's span head (its own end delimits it).
const AtpFtCell *ft_subterm_at(const AtpFtCell *c, const u32 *p,
                               u32 p_len) {
  for (u32 d = 0; d < p_len; d++) {
    if (c == NULL || ft_is_var(c) || c->arity == 0u) return NULL;
    u32 idx = p[d];
    if (idx >= (u32)c->arity) return NULL;
    const AtpFtCell *k = c->next;
    for (u32 i = 0; i < idx; i++) k = k->end->next;
    c = k;
  }
  return c;
}

// Rename-clone: a fresh span with every var id shifted by `offset` --
// the FT sibling of thvm_rename_vars(t, offset).  Built through the
// same ftnew_* constructors, so the output is bit-identical to
// ft_from_term(a, thvm_rename_vars(ft_to_term(x), offset)) with the
// Term hops removed.
AtpFtCell *ft_rename_span(AtpFt *a, const AtpFtCell *x, u32 offset,
                          int scratch) {
  return ft_copy_span(a, x, offset, scratch);
}

// --- Boundary: AtpFt -> Term -----------------------------------------
//
// Mirror of the above: a var cell becomes term_new_fvr; a 0-arity
// CTR cell becomes term_new_ctr with no children; an arity>0 CTR
// becomes term_new_ctr over its converted children (walked via
// cell->end->next sibling jumps), allocated after its last child as
// the recursion did.  ATPFT_NUM_MARKER round-trips back to a TAG_NUM
// via term_new(0, TAG_NUM, dtype, raw).
Term ft_to_term(const AtpFtCell *x) {
  extern unsigned char g_atp_phase_detail;
  if (g_atp_phase_detail) {
    extern u64 g_ft_to_calls; g_ft_to_calls++;
    extern u8 g_ft_region; extern u64 g_ft_to_by_region[8];
    g_ft_to_by_region[g_ft_region & 7u]++;
    if (getenv("THVM_ATP_FT2DBG") != NULL) {
      extern const char *g_thvm_phase;
      fprintf(stderr, "FT%uTO phase=%s sym=%u arity=%u\n",
              (u32)g_ft_region, g_thvm_phase, x->sym, (u32)x->arity);
    }
    if (g_ft_region == 2u) ft_region2_to_hook();
    if (g_ft_region == 0u) ft_region0_to_hook();
  }
  FtSpanFrame  fr_inl[FT_WALK_INL];
  Term         val_inl[FT_WALK_INL];
  FtSpanFrame *fr   = fr_inl;
  Term        *val  = val_inl;
  u32          nfr  = 0u;
  u32          cfr  = FT_WALK_INL;
  u32          nval = 0u;
  u32          cval = FT_WALK_INL;
  for (;;) {
    if (g_atp_phase_detail) { extern u64 g_ft_to_nodes; g_ft_to_nodes++; }
    u8   leaf = 1u;
    Term v    = 0;
    if (ft_is_var(x)) {
      v = term_new_fvr(ft_var_id(x));
    } else {
      u32 sym   = ft_ctr_sym(x);
      u16 arity = x->arity;
      if (sym == ATPFT_NUM_MARKER && arity == 2u) {
        // Recover (dtype, raw) from the two child constant cells.
        const AtpFtCell *dt_cell  = x->next;
        const AtpFtCell *raw_cell = dt_cell->end->next;
        v = term_new(0, TAG_NUM, ft_ctr_sym(dt_cell), ft_ctr_sym(raw_cell));
      } else if (arity == 0u) {
        v = term_new_ctr(sym, NULL, 0u);
      } else {
        if (nfr == cfr) {
          fr = (FtSpanFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].child = x->next;
        fr[nfr].left  = arity;
        fr[nfr].sym   = sym;
        fr[nfr].arity = arity;
        nfr++;
        leaf = 0u;
      }
    }
    if (leaf) {
      if (nval == cval) {
        val = (Term *)term_stack_grow(val, val_inl, &cval, sizeof *val);
      }
      val[nval++] = v;
    }
    while (nfr > 0u && fr[nfr - 1u].left == 0u) {
      FtSpanFrame *f = &fr[nfr - 1u];
      nval -= f->arity;
      val[nval] = term_new_ctr(f->sym, &val[nval], f->arity);
      nval++;
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    FtSpanFrame *f = &fr[nfr - 1u];
    x        = f->child;
    f->child = x->end->next;
    f->left--;
  }
  Term out = val[0];
  term_stack_release(fr, fr_inl);
  term_stack_release(val, val_inl);
  return out;
}

// A span cell walked in lockstep with a Term CTR: `kid` is the next
// span child, child i of the Term is heap_read(base + 1 + i).
typedef struct {
  const AtpFtCell *kid;
  u64              base;
  u32              i;
  u32              n;
} FtTermFrame;

// Exact FT-vs-Term structural compare -- 1 iff `c` is the span
// ft_from_term(a, t) would build (same encoding contract: FVR <-> var
// cell by id, CTR <-> ctr cell by sym/arity/children, TAG_NUM <-> the
// ATPFT_NUM_MARKER pair).  Read-only, allocation-free, early-exit; the
// adoption gates use it to prove a candidate span IS a Term's image
// before adopting it in place of a fresh conversion.
int ft_eq_term(const AtpFtCell *c, Term t) {
  FtTermFrame  fr_inl[FT_WALK_INL];
  FtTermFrame *fr  = fr_inl;
  u32          nfr = 0u;
  u32          cfr = FT_WALK_INL;
  int          eq  = 1;
  for (;;) {
    u8 tag = term_tag(t);
    if (tag == TAG_FVR) {
      if (!ft_is_var(c) || ft_var_id(c) != (term_ext(t) & ~WF_VAR_BIT)) {
        eq = 0;
        break;
      }
    } else if (ft_is_var(c)) {
      eq = 0;
      break;
    } else if (tag == TAG_CTR) {
      if (ft_ctr_sym(c) != (term_ext(t) & ~WF_VAR_BIT)) {
        eq = 0;
        break;
      }
      u32 n = term_ctr_n(t);
      if ((u32)c->arity != n) {
        eq = 0;
        break;
      }
      if (n > 0u) {
        if (nfr == cfr) {
          fr = (FtTermFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].kid  = c->next;
        fr[nfr].base = term_val(t);
        fr[nfr].i    = 0u;
        fr[nfr].n    = n;
        nfr++;
      }
    } else if (tag == TAG_NUM) {
      if (ft_ctr_sym(c) != ATPFT_NUM_MARKER || c->arity != 2u) {
        eq = 0;
        break;
      }
      const AtpFtCell *dt_cell  = c->next;
      const AtpFtCell *raw_cell = dt_cell->end->next;
      if (ft_ctr_sym(dt_cell)  != (term_ext(t) & ~WF_VAR_BIT) ||
          ft_ctr_sym(raw_cell) != (((u32)term_val(t)) & ~WF_VAR_BIT)) {
        eq = 0;
        break;
      }
    } else {
      eq = 0;
      break;
    }
    while (nfr > 0u && fr[nfr - 1u].i == fr[nfr - 1u].n) {
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    FtTermFrame *f = &fr[nfr - 1u];
    c      = f->kid;
    f->kid = c->end->next;
    t      = heap_read(f->base + 1u + (u64)f->i);
    f->i++;
  }
  term_stack_release(fr, fr_inl);
  return eq;
}

// Span-pattern one-way match against a HEAP-TERM subject -- the exact
// sibling of thvm_match with the pattern read off mirror cells:
// pattern var binds the subject SUBTREE (repeated var re-checks with
// kbo_eq), constructor cells compare label + arity + children, and a
// TAG_NUM-marker pattern never matches (thvm_match's default arm).
// Bindings land in the same RewriteSubst (Term-typed), so the caller's
// substitution application is unchanged.  Children are matched left to
// right, depth first, so a failed match leaves the same partial
// bindings the recursion did.
int ft_match_term(const AtpFtCell *pat, Term term, RewriteSubst *subst) {
  FtTermFrame  fr_inl[FT_WALK_INL];
  FtTermFrame *fr  = fr_inl;
  u32          nfr = 0u;
  u32          cfr = FT_WALK_INL;
  int          ok  = 1;
  for (;;) {
    if (ft_is_var(pat)) {
      u32 id = ft_var_id(pat);
      if (id >= REWRITE_MAX_VAR) {
        ok = 0;
        break;
      }
      if (subst->bindings[id] == 0) {
        subst->bindings[id] = term;
      } else if (!kbo_eq(subst->bindings[id], term)) {
        ok = 0;
        break;
      }
    } else {
      u32 sym = ft_ctr_sym(pat);
      if (sym == ATPFT_NUM_MARKER || term_tag(term) != TAG_CTR ||
          sym != term_ext(term)) {
        ok = 0;
        break;
      }
      u32 n = (u32)pat->arity;
      if (term_ctr_n(term) != n) {
        ok = 0;
        break;
      }
      if (n > 0u) {
        if (nfr == cfr) {
          fr = (FtTermFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].kid  = pat->next;
        fr[nfr].base = term_val(term);
        fr[nfr].i    = 0u;
        fr[nfr].n    = n;
        nfr++;
      }
    }
    while (nfr > 0u && fr[nfr - 1u].i == fr[nfr - 1u].n) {
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    FtTermFrame *f = &fr[nfr - 1u];
    pat    = f->kid;
    f->kid = pat->end->next;
    term   = heap_read(f->base + 1u + (u64)f->i);
    f->i++;
  }
  term_stack_release(fr, fr_inl);
  return ok;
}

// Instantiate a span TEMPLATE under a Term-typed substitution,
// producing a fresh heap Term -- the exact sibling of
// thvm_subst_apply(template_term, subst) minus the hash-cons reuse of
// unchanged subtrees (there is no original Term to reuse; downstream
// reads are content-based).  Unbound vars pass through as fresh FVRs;
// the TAG_NUM marker round-trips to its NUM; a node wider than
// REWRITE_MAX_ARITY yields 0 in its parent's child slot.
Term ft_subst_apply_term(const AtpFtCell *tmpl, const RewriteSubst *subst) {
  FtSpanFrame  fr_inl[FT_WALK_INL];
  Term         val_inl[FT_WALK_INL];
  FtSpanFrame *fr   = fr_inl;
  Term        *val  = val_inl;
  u32          nfr  = 0u;
  u32          cfr  = FT_WALK_INL;
  u32          nval = 0u;
  u32          cval = FT_WALK_INL;
  for (;;) {
    u8   leaf = 1u;
    Term v    = 0;
    if (ft_is_var(tmpl)) {
      u32 id = ft_var_id(tmpl);
      if (id < REWRITE_MAX_VAR && subst->bindings[id] != 0) {
        v = subst->bindings[id];
      } else {
        v = term_new_fvr(id);
      }
    } else {
      u32 sym = ft_ctr_sym(tmpl);
      u32 n   = (u32)tmpl->arity;
      if (sym == ATPFT_NUM_MARKER && n == 2u) {
        const AtpFtCell *dt_cell  = tmpl->next;
        const AtpFtCell *raw_cell = dt_cell->end->next;
        v = term_new(0, TAG_NUM, ft_ctr_sym(dt_cell), ft_ctr_sym(raw_cell));
      } else if (n > REWRITE_MAX_ARITY) {
        v = 0;
      } else if (n == 0u) {
        v = term_new_ctr(sym, NULL, 0u);
      } else {
        if (nfr == cfr) {
          fr = (FtSpanFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].child = tmpl->next;
        fr[nfr].left  = n;
        fr[nfr].sym   = sym;
        fr[nfr].arity = n;
        nfr++;
        leaf = 0u;
      }
    }
    if (leaf) {
      if (nval == cval) {
        val = (Term *)term_stack_grow(val, val_inl, &cval, sizeof *val);
      }
      val[nval++] = v;
    }
    while (nfr > 0u && fr[nfr - 1u].left == 0u) {
      FtSpanFrame *f = &fr[nfr - 1u];
      nval -= f->arity;
      val[nval] = term_new_ctr(f->sym, &val[nval], f->arity);
      nval++;
      nfr--;
    }
    if (nfr == 0u) {
      break;
    }
    FtSpanFrame *f = &fr[nfr - 1u];
    tmpl     = f->child;
    f->child = tmpl->end->next;
    f->left--;
  }
  Term out = val[0];
  term_stack_release(fr, fr_inl);
  term_stack_release(val, val_inl);
  return out;
}

// --- Structural equality ----------------------------------------------
//
// Same shape, same sym at every cell (which also captures
// var-id identity, since the high-bit-set sym carries the id).
// One linear scan of the two preorders (see below).

int ft_eq(const AtpFtCell *x, const AtpFtCell *y) {
  if (x == y) return 1;
  if (x == NULL || y == NULL) return 0;
  // Iterative lockstep walk of the two flatterm preorders.  Two terms
  // are structurally equal iff their preorders agree cell-for-cell on
  // (sym, arity): arity fully determines the shape, so matching every
  // aligned (sym, arity) pair forces identical trees of identical
  // length -- y exhausts its subterm exactly when x reaches x_end.
  // Collapses the recursion to a single linear scan (no call frame).
  const AtpFtCell *x_end = (x->end != NULL) ? x->end->next : NULL;
  const AtpFtCell *px = x, *py = y;
  while (px != x_end) {
    if (py == NULL) return 0;
    if (px->sym != py->sym || px->arity != py->arity) return 0;
    px = px->next;
    py = py->next;
  }
  return 1;
}

// --- Structural hash --------------------------------------------------
//
// Mirrors atp_term_struct_hash (_.c:326-342) exactly so a Term and
// its ft_from_term image produce the SAME u64.  This is the bridge
// that lets Stage 9 re-key atp_unf_memo / atp_norm_memo to ft_hash
// without invalidating any in-flight entries.
//
// Recurrence summary:
//   CTR(sym, c0..c_{n-1})
//     h := FNV_OFFSET ^ (sym * FNV_PRIME)
//     for each child:  h := (h ^ rec(child)) * FNV_PRIME
//     return h ^ (n * MIX)
//   FVR(id):  return FVR_TAG ^ (id * FNV_PRIME)
//   (other tags fall back to TAG_TAG ^ (tag * FNV_PRIME), but the
//    ATP layer only ever stores CTR + FVR + the NUM-marker
//    encoding above, which is a CTR in AtpFt-land.)
//
// For the FVR case in AtpFt-land, the "id" is the bare var id (low
// 31 bits of sym), NOT the sym with the high bit set -- this matches
// what atp_term_struct_hash sees in Term-land (term_ext(FVR) = bare
// var id, no flag bit).
#define ATPFT_HASH_FNV_OFFSET   0xcbf29ce484222325ull
#define ATPFT_HASH_FNV_PRIME    0x100000001b3ull
#define ATPFT_HASH_ARITY_MIX    0x9e3779b97f4a7c15ull
#define ATPFT_HASH_FVR_TAG      0xfacefacefaceull

// A CTR cell whose child hashes are being folded into `h`.
typedef struct {
  const AtpFtCell *child;
  u64              h;
  u32              left;
  u32              arity;
} FtHashFrame;

u64 ft_hash(const AtpFtCell *x) {
  FtHashFrame  fr_inl[FT_WALK_INL];
  FtHashFrame *fr  = fr_inl;
  u32          nfr = 0u;
  u32          cfr = FT_WALK_INL;
  u64          v   = 0u;
  for (;;) {
    u8 leaf = 1u;
    if (x == NULL) {
      // Match atp_term_struct_hash's default branch for "no real
      // term": 0xdeadbeefcafebabe ^ (0 * FNV_PRIME) == 0xdeadbeef..be.
      v = 0xdeadbeefcafebabeull;
    } else if (ft_is_var(x)) {
      v = ATPFT_HASH_FVR_TAG ^ ((u64)ft_var_id(x) * ATPFT_HASH_FNV_PRIME);
    } else {
      u64 h = ATPFT_HASH_FNV_OFFSET ^ ((u64)ft_ctr_sym(x) * ATPFT_HASH_FNV_PRIME);
      if (x->arity == 0u) {
        v = h;
      } else {
        if (nfr == cfr) {
          fr = (FtHashFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
        }
        fr[nfr].child = x->next;
        fr[nfr].h     = h;
        fr[nfr].left  = x->arity;
        fr[nfr].arity = x->arity;
        nfr++;
        leaf = 0u;
      }
    }
    if (leaf) {
      // Fold the finished child hash into its parent; a parent whose
      // last child is folded finishes in turn.
      while (nfr > 0u) {
        FtHashFrame *f = &fr[nfr - 1u];
        f->h ^= v;
        f->h *= ATPFT_HASH_FNV_PRIME;
        f->left--;
        if (f->left > 0u) {
          break;
        }
        v = f->h ^ ((u64)f->arity * ATPFT_HASH_ARITY_MIX);
        nfr--;
      }
      if (nfr == 0u) {
        break;
      }
    }
    FtHashFrame *f = &fr[nfr - 1u];
    x        = f->child;
    f->child = x->end->next;
  }
  term_stack_release(fr, fr_inl);
  return v;
}

// --- Pretty print -----------------------------------------------------
//
// Byte-identical mirror of atp_pretty_term (_.c:7028-7058): "x_<id>"
// for FVR, "C<lab>" or "C<lab>(c0, c1, ...)" for CTR, "#<val>" for
// the NUM marker, with the ", " (comma+space) separator on CTR
// children.  Buffer-aware, truncates silently.  Each open CTR level
// keeps the locals the recursive form had: its own buffer window
// (buf, cap), written length w and child cursor; a finished child's
// length is added to its parent's w exactly as the recursive return.
typedef struct {
  const AtpFtCell *child;
  char            *buf;
  u32              cap;
  u32              w;
  u16              i;
  u16              n;
} FtPrettyFrame;

u32 atp_pretty_ft(const AtpFtCell *x, char *buf, u32 cap) {
  FtPrettyFrame  fr_inl[FT_WALK_INL];
  FtPrettyFrame *fr  = fr_inl;
  u32            nfr = 0u;
  u32            cfr = FT_WALK_INL;
  u32            ret = 0u;
  for (;;) {
    u8 opened = 0u;
    if (cap == 0u) {
      ret = 0u;
    } else if (x == NULL) {
      ret = (u32)snprintf(buf, cap, "?T0");
    } else if (ft_is_var(x)) {
      ret = (u32)snprintf(buf, cap, "x_%u", ft_var_id(x));
    } else if (cap <= 1u) {
      ret = 0u;
    } else {
      u32 sym = ft_ctr_sym(x);
      u16 n   = x->arity;
      // Detect the NUM-marker shape so the pretty print round-trips back
      // to the "#<val>" format atp_pretty_term emits for TAG_NUM.
      if (sym == ATPFT_NUM_MARKER && n == 2u) {
        const AtpFtCell *raw_cell = x->next->end->next;
        int n_w = snprintf(buf, cap, "#%u", ft_ctr_sym(raw_cell));
        if (n_w < 0) {
          ret = 0u;
        } else {
          ret = ((u32)n_w >= cap) ? cap - 1u : (u32)n_w;
        }
      } else {
        int n_w = snprintf(buf, cap, "C%u", sym);
        u32 w   = (u32)n_w;
        if (n_w < 0) {
          ret = 0u;
        } else if (w >= cap) {
          ret = cap - 1u;
        } else if (n == 0u || w + 1u >= cap) {
          ret = w;
        } else {
          w += (u32)snprintf(buf + w, cap - w, "(");
          if (nfr == cfr) {
            fr = (FtPrettyFrame *)term_stack_grow(fr, fr_inl, &cfr, sizeof *fr);
          }
          fr[nfr].child = x->next;
          fr[nfr].buf   = buf;
          fr[nfr].cap   = cap;
          fr[nfr].w     = w;
          fr[nfr].i     = 0u;
          fr[nfr].n     = n;
          nfr++;
          opened = 1u;
        }
      }
    }
    // Run open levels until one starts a child.  `have` = a child just
    // returned `ret` to the innermost level.
    u8 have = (u8)!opened;
    for (;;) {
      if (nfr == 0u) {
        term_stack_release(fr, fr_inl);
        return ret;
      }
      FtPrettyFrame *f = &fr[nfr - 1u];
      if (have) {
        have = 0u;
        f->w += ret;
        if (f->w >= f->cap) {
          ret = f->cap - 1u;
          nfr--;
          have = 1u;
          continue;
        }
        f->child = f->child->end->next;
        f->i++;
      }
      if (f->i < f->n && f->w + 2u < f->cap) {
        if (f->i > 0u) {
          f->w += (u32)snprintf(f->buf + f->w, f->cap - f->w, ", ");
        }
        if (f->w >= f->cap) {
          ret = f->cap - 1u;
          nfr--;
          have = 1u;
          continue;
        }
        x   = f->child;
        buf = f->buf + f->w;
        cap = f->cap - f->w;
        break;
      }
      if (f->w + 1u < f->cap) {
        f->w += (u32)snprintf(f->buf + f->w, f->cap - f->w, ")");
      }
      ret = f->w;
      nfr--;
      have = 1u;
    }
  }
}

#endif // !ATPFT_FT_C_INCLUDED
#endif // THVM_ATPFT_CONVERT
