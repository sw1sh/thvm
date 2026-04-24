// thvm.c - single-translation-unit hub.
//
// Each .c included below contributes one function (or a tiny family
// of helpers). The order matters: term packing first, then heap, then
// interactions, then the WNF stack machine that drives them.

#include "thvm.h"

// === Globals ===
Term *HEAP      = NULL;
u64   HEAP_NEXT = 0;

Term *WNF_STACK = NULL;
u32   WNF_S_POS = 0;

u64   ITRS      = 0;

TenDesc *TENS         = NULL;
u32      TENS_NEXT    = 1;            // 0 reserved for "no tensor"

Backend *CURRENT_BACKEND = NULL;

// === term/ ===
#include "term/new.c"
#include "term/tag.c"
#include "term/ext.c"
#include "term/val.c"
#include "term/sub/get.c"
#include "term/sub/set.c"

// === heap/ ===
#include "heap/alloc.c"
#include "heap/read.c"
#include "heap/set.c"
#include "heap/take.c"
#include "heap/subst_var.c"
#include "heap/subst_cop.c"

// === interact/ ===
#include "interact/app_lam.c"
#include "interact/app_era.c"
#include "interact/dup_sup.c"
#include "interact/dup_era.c"
#include "interact/dup_lam.c"

// === wnf/ ===
#include "wnf/_.c"

// === view/ ===
#include "view/create.c"

// === backend/cpu/ ===
// Order: init defines CPU_BUFS + CPU_BUFS_NEXT first, then the buf_*
// helpers reference them, then _.c assembles the Backend vtable.
#include "backend/cpu/init.c"
#include "backend/cpu/buf_alloc.c"
#include "backend/cpu/buf_free.c"
#include "backend/cpu/buf_incref.c"
#include "backend/cpu/buf_decref.c"
#include "backend/cpu/buf_read.c"
#include "backend/cpu/buf_write.c"
#include "backend/cpu/_.c"

// === tensor/ ===
#include "tensor/alloc.c"
#include "tensor/incref.c"
#include "tensor/decref.c"
#include "tensor/release.c"
#include "tensor/view_of.c"

// === runtime lifecycle ===
void thvm_init(void) {
  HEAP      = (Term *)calloc(HEAP_CAP, sizeof(Term));
  WNF_STACK = (Term *)calloc(WNF_CAP,  sizeof(Term));
  TENS      = (TenDesc *)calloc(TENS_CAP, sizeof(TenDesc));
  HEAP_NEXT = 0;
  WNF_S_POS = 0;
  ITRS      = 0;
  TENS_NEXT = 1;
  CURRENT_BACKEND = &CPU_BACKEND;
  CPU_BACKEND.init();
}

void thvm_free(void) {
  if (CURRENT_BACKEND) CURRENT_BACKEND->shutdown();
  free(HEAP);
  free(WNF_STACK);
  free(TENS);
  HEAP      = NULL;
  WNF_STACK = NULL;
  TENS      = NULL;
  HEAP_NEXT = 0;
  WNF_S_POS = 0;
  TENS_NEXT = 1;
  CURRENT_BACKEND = NULL;
}
