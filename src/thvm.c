// thvm.c - single-translation-unit hub.
//
// Each .c included below contributes one function (or a tiny family
// of helpers). The order matters: term packing first, then heap, then
// view + backend + tensor lifecycle, then UOp constructors + the
// schedule pipeline, then interactions (which depend on the schedule
// pipeline through uop_kernel), and finally the WNF stack machine
// that drives them.

#include "thvm.h"

// === Globals ===
Term *HEAP      = NULL;
u64   HEAP_NEXT = 0;

Term *WNF_STACK = NULL;
u32   WNF_S_POS = 0;

u64   ITRS      = 0;

TenDesc     *TENS         = NULL;
u32          TENS_NEXT    = 1;   // 0 reserved for "no tensor"

KernelEntry *KERNELS      = NULL;
u32          KERNELS_NEXT = 1;   // 0 reserved for "no kernel"

Backend     *CURRENT_BACKEND = NULL;

Term *BOOK_HEAP = NULL;
u64   BOOK_NEXT = 1;   // 0 reserved as "no template"
Term  DEFS[DEFS_CAP] = {0};

AloState *ALO_STATES      = NULL;
u32       ALO_STATES_NEXT = 1;   // 0 reserved as "empty chain"

// === term/ ===
#include "term/new.c"
#include "term/tag.c"
#include "term/ext.c"
#include "term/val.c"
#include "term/sub/get.c"
#include "term/sub/set.c"
#include "term/new_ref.c"
#include "term/new_alo.c"
#include "term/new_op2.c"
#include "term/new_mat.c"

// === heap/ ===
#include "heap/alloc.c"
#include "heap/read.c"
#include "heap/set.c"
#include "heap/take.c"
#include "heap/subst_var.c"
#include "heap/subst_cop.c"

// === book/ ===
// from_dynamic depends on heap/, book/alloc, book/set; included after them.
#include "book/alloc.c"
#include "book/read.c"
#include "book/set.c"
#include "book/from_dynamic.c"

// === alo/ ===
// realize uses heap_alloc + book_read + state_push/lookup + term_new_alo;
// force calls alo_realize (regular fn, prototype in thvm.h).
#include "alo/state.c"
#include "alo/realize.c"
#include "alo/force.c"

// === term/resolve.c ===
// Lazy outermost-layer walker (VAR-SUB chain + ALO force).  Comes
// after alo/ since it calls alo_force.  Used by interact_grad and
// materialize_expr to surface the outermost structure without
// firing materialize / kernel / grad.
#include "term/resolve.c"

// === view/ ===
#include "view/shape_numel.c"
#include "view/create.c"

// === backend/cpu/ ===
// Order: init defines CPU_BUFS + CPU_BUFS_NEXT first, then the buf_*
// helpers reference them, then per-op files, the interpreter, and
// finally _.c assembles the Backend vtable.
#include "backend/cpu/init.c"
#include "backend/cpu/buf_alloc.c"
#include "backend/cpu/buf_free.c"
#include "backend/cpu/buf_incref.c"
#include "backend/cpu/buf_decref.c"
#include "backend/cpu/buf_read.c"
#include "backend/cpu/buf_write.c"
#include "backend/cpu/op/const.c"
#include "backend/cpu/op/add.c"
#include "backend/cpu/op/mul.c"
#include "backend/cpu/op/neg.c"
#include "backend/cpu/op/recip.c"
#include "backend/cpu/op/sqrt.c"
#include "backend/cpu/op/exp2.c"
#include "backend/cpu/op/log2.c"
#include "backend/cpu/op/cmplt.c"
#include "backend/cpu/op/reduce.c"
#include "backend/cpu/op/expand.c"
#include "backend/cpu/op/reshape.c"
#include "backend/cpu/op/conv2d.c"
#include "backend/cpu/interpret.c"
#include "backend/cpu/_.c"

// === tensor/ ===
#include "tensor/alloc.c"
#include "tensor/incref.c"
#include "tensor/decref.c"
#include "tensor/release.c"
#include "tensor/view_of.c"

// === uop/ ===
// Constructors for raw UOp graph nodes.  Each helper allocates the
// heap cells for one opcode and returns a TAG_UOP term; nothing reduces.
#include "uop/const.c"
#include "uop/unary.c"
#include "uop/binary.c"
#include "uop/reduce.c"
#include "uop/reshape.c"
#include "uop/permute.c"
#include "uop/expand.c"
#include "uop/pad.c"
#include "uop/shrink.c"
#include "uop/flip.c"
#include "uop/grad.c"
#include "uop/conv2d.c"

// === schedule/ ===
// Materialize pipeline: schedule + kernelize + linearize + splice.
// Produces the scheduled DAG of UOP_KERNEL terms that
// interact_kernel fires bottom-up.
#include "schedule/kernel_alloc.c"
#include "schedule/shape_env.c"
#include "schedule/materialize.c"
#include "schedule/materialize_in_env.c"
#include "schedule/walk.c"

// === interact/ ===
// Interaction rules.  uop_kernel.c needs the schedule pipeline above
// (KERNELS table, CPU dispatch vtable), so it's loaded last.
#include "interact/app_lam.c"
#include "interact/app_era.c"
#include "interact/dup_sup.c"
#include "interact/dup_era.c"
#include "interact/dup_lam.c"
#include "interact/uop_grad.c"
#include "interact/uop_kernel.c"

// === wnf/ ===
// The reducer dispatches to the interactions and to materialize,
// so every file it calls must be defined above.
#include "wnf/_.c"
#include "wnf/redex.c"

// === runtime lifecycle ===
void thvm_init(void) {
  HEAP       = (Term *)calloc(HEAP_CAP,     sizeof(Term));
  WNF_STACK  = (Term *)calloc(WNF_CAP,      sizeof(Term));
  TENS       = (TenDesc *)calloc(TENS_CAP,  sizeof(TenDesc));
  KERNELS    = (KernelEntry *)calloc(KERNELS_CAP, sizeof(KernelEntry));
  BOOK_HEAP  = (Term *)calloc(BOOK_CAP,     sizeof(Term));
  ALO_STATES = (AloState *)calloc(ALO_STATE_CAP, sizeof(AloState));
  HEAP_NEXT  = 0;
  WNF_S_POS  = 0;
  ITRS       = 0;
  TENS_NEXT  = 1;
  KERNELS_NEXT = 1;
  BOOK_NEXT  = 1;
  ALO_STATES_NEXT = 1;
  for (u32 i = 0; i < DEFS_CAP; i++) DEFS[i] = 0;
  CURRENT_BACKEND = &CPU_BACKEND;
  CPU_BACKEND.init();
}

void thvm_free(void) {
  if (CURRENT_BACKEND) CURRENT_BACKEND->shutdown();
  free(HEAP);
  free(WNF_STACK);
  free(TENS);
  free(KERNELS);
  free(BOOK_HEAP);
  free(ALO_STATES);
  HEAP       = NULL;
  WNF_STACK  = NULL;
  TENS       = NULL;
  KERNELS    = NULL;
  BOOK_HEAP  = NULL;
  ALO_STATES = NULL;
  HEAP_NEXT  = 0;
  WNF_S_POS  = 0;
  TENS_NEXT  = 1;
  KERNELS_NEXT = 1;
  BOOK_NEXT  = 1;
  ALO_STATES_NEXT = 1;
  for (u32 i = 0; i < DEFS_CAP; i++) DEFS[i] = 0;
  CURRENT_BACKEND = NULL;
}
