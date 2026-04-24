// thvm.c — single-translation-unit hub.
//
// Each .c included below contributes one function (or a tiny family
// of helpers). The order matters: term packing first, then heap, then
// interactions, then the WNF stack machine that drives them.

#include "thvm.h"

// ─── Globals ────────────────────────────────────────────────────────────────

Term *HEAP      = NULL;
u64   HEAP_NEXT = 0;

Term *WNF_STACK = NULL;
u32   WNF_S_POS = 0;

u64   ITRS      = 0;

// ─── term/ ──────────────────────────────────────────────────────────────────
#include "term/new.c"
#include "term/tag.c"
#include "term/ext.c"
#include "term/val.c"
#include "term/sub/get.c"
#include "term/sub/set.c"

// ─── heap/ ──────────────────────────────────────────────────────────────────
#include "heap/alloc.c"
#include "heap/read.c"
#include "heap/set.c"
#include "heap/take.c"
#include "heap/subst_var.c"

// ─── interact/ ──────────────────────────────────────────────────────────────
#include "interact/app_lam.c"

// ─── wnf/ ───────────────────────────────────────────────────────────────────
#include "wnf/_.c"

// ─── runtime lifecycle ──────────────────────────────────────────────────────

void thvm_init(void) {
  HEAP      = (Term *)calloc(HEAP_CAP, sizeof(Term));
  WNF_STACK = (Term *)calloc(WNF_CAP,  sizeof(Term));
  HEAP_NEXT = 0;
  WNF_S_POS = 0;
  ITRS      = 0;
}

void thvm_free(void) {
  free(HEAP);
  free(WNF_STACK);
  HEAP      = NULL;
  WNF_STACK = NULL;
  HEAP_NEXT = 0;
  WNF_S_POS = 0;
}
