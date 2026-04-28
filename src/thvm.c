// thvm.c - single-translation-unit hub.
//
// Each .c included below contributes one function (or a tiny family
// of helpers). The order matters: term packing first, then heap, then
// view + backend + tensor lifecycle, then UOp constructors + the
// schedule pipeline, then interactions (which depend on the schedule
// pipeline through uop_kernel), and finally the WNF stack machine
// that drives them.

#include "thvm.h"

// === Multi-context storage ===
//
// Slot 0 holds the default singleton runtime so legacy code (which
// doesn't mention TContext at all) keeps using a stable home.  The
// static storage is BSS so its ~few KB inline arrays cost nothing
// until first write (zero-fill on demand).  CURRENT_CTX always
// points at one of the slots; macros from thvm.h (HEAP, BOOK_HEAP,
// DEFS, ...) dereference through it.

static TContext DEFAULT_CTX_STORAGE;
TContext *CONTEXTS[CONTEXTS_CAP] = {&DEFAULT_CTX_STORAGE};
TContext *CURRENT_CTX            = &DEFAULT_CTX_STORAGE;

// One-time scalar init for slot 0.  thvm_init runs the rest (calloc
// of the heap-allocated arrays + backend setup).  Called via
// init_default_ctx_scalars from thvm_init.
static void init_default_ctx_scalars(TContext *ctx) {
    ctx->heap_next        = 0;
    ctx->wnf_s_pos        = 0;
    ctx->wnf_last_stack_len = 0;
    ctx->itrs             = 0;
    ctx->tens_next        = 1;   // 0 reserved
    ctx->kernels_next     = 1;   // 0 reserved
    ctx->book_next        = 1;   // 0 reserved
    ctx->alo_states_next  = 1;   // 0 reserved
    ctx->cpu_bufs_next    = 1;   // 0 reserved
    ctx->cpu_freelist_len = 0;
}

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
#include "term/new_eql.c"
#include "term/new_and.c"
#include "term/new_or.c"
#include "term/new_any.c"
#include "term/new_inc.c"
#include "term/new_ctr.c"
#include "term/new_when.c"
#include "term/new_fvr.c"
#include "term/new_bri.c"
#include "term/new_ann.c"
#include "term/new_pri.c"

// === heap/ ===
#include "heap/alloc.c"
#include "heap/read.c"
#include "heap/set.c"
#include "heap/take.c"
#include "heap/subst_var.c"
#include "heap/subst_cop.c"

// === lam/ ===
// Side tables tied to LAM heap locs (shape annotation, future:
// arity hints).  No reduction logic; pure storage.
#include "lam/shape.c"

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
#include "view/strided_index.c"

// === backend/cpu/ ===
// Order: init defines CPU_BUFS + CPU_BUFS_NEXT first, then the buf_*
// helpers reference them, then per-op files, the interpreter, and
// finally _.c assembles the Backend vtable.
#include "backend/cpu/init.c"
#include "backend/cpu/buf_freelist.c"   // needed by buf_alloc.c
#include "backend/cpu/buf_alloc.c"
#include "backend/cpu/buf_free.c"
#include "backend/cpu/buf_incref.c"
#include "backend/cpu/buf_decref.c"
#include "backend/cpu/buf_read.c"
#include "backend/cpu/buf_write.c"
#include "backend/cpu/buf_pool.c"
#include "backend/cpu/op/const.c"
#include "backend/cpu/op/add.c"
#include "backend/cpu/op/mul.c"
#include "backend/cpu/op/neg.c"
#include "backend/cpu/op/recip.c"
#include "backend/cpu/op/sqrt.c"
#include "backend/cpu/op/exp2.c"
#include "backend/cpu/op/log2.c"
#include "backend/cpu/op/cmplt.c"
#include "backend/cpu/op/cmpeq.c"
#include "backend/cpu/op/reduce.c"
#include "backend/cpu/op/expand.c"
#include "backend/cpu/op/reshape.c"
#include "backend/cpu/op/load.c"
#include "backend/cpu/op/flip.c"
#include "backend/cpu/op/pad.c"
#include "backend/cpu/op/shrink.c"
#include "backend/cpu/op/permute.c"
#include "backend/cpu/interpret.c"
#include "backend/cpu/_.c"

// === backend/metal/ ===
// On non-Apple builds (and Apple builds that DON'T compile the .m
// glue separately), include the C stub so METAL_BACKEND still
// resolves -- THVM_BACKEND=metal selects it but every compute call
// returns an error.  Apple builds that link build/backend_metal.o
// (the dual-TU Metal path) define THVM_HAS_METAL to skip this
// include and use the .m-defined symbols instead.
#ifndef THVM_HAS_METAL
#include "backend/metal/_.c"
#endif

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
#include "uop/mov_cache.c"
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
#include "uop/load.c"

// === schedule/ ===
// Materialize pipeline: schedule + kernelize + linearize + splice.
// Produces the scheduled DAG of UOP_KERNEL terms that
// interact_kernel fires bottom-up.
#include "schedule/kernel_alloc.c"
#include "schedule/kernel_program_cache.c"
#include "schedule/uop_meta.c"
#include "schedule/consumer_count.c"
#include "schedule/realize_classify.c"
#include "schedule/materialize.c"

// === interact/ ===
// Interaction rules.  uop_kernel.c needs the schedule pipeline above
// (KERNELS table, CPU dispatch vtable), so it's loaded last.
#include "interact/app_lam.c"
#include "interact/app_era.c"
#include "interact/dup_sup.c"
#include "interact/dup_era.c"
#include "interact/dup_lam.c"
#include "interact/dup_num.c"
#include "interact/dup_any.c"
#include "interact/dup_ten.c"
#include "interact/dup_uop.c"
#include "interact/app_bri.c"
#include "interact/app_pri.c"
#include "interact/app_sup.c"
#include "interact/ann_lam.c"
#include "interact/ann_bri.c"
#include "interact/dup_bri.c"
#include "interact/uop_grad.c"
#include "interact/uop_kernel.c"
#include "interact/uop_assign.c"

// === wnf/ ===
// The reducer dispatches to the interactions and to materialize,
// so every file it calls must be defined above.
#include "wnf/_.c"
#include "wnf/redex.c"
#include "wnf/nf.c"

// === term/prims_core.c ===
// Core THVM_PRIM_* primitives (SEQ + LOG).  Has to live below wnf/
// because the prim functions call wnf() to drive their first arg.
#include "term/prims_core.c"

// === collapse/ ===
// Depends on wnf().  No dependants in the runtime itself; called by
// the WL bridge and tests.
#include "collapse/_.c"
#include "collapse/ordered.c"

// === kbo/ ===
// Knuth-Bendix ordering as a C function.  No reduction or heap
// allocation; reads CTR/FVR terms and a caller-supplied KboConfig.
#include "kbo/_.c"
#include "lpo/_.c"

// === rewrite/ ===
// Equational rewriter (one-shot rule application + iterative
// normalize).  Reuses kbo_eq from src/kbo/_.c, so must be included
// after it.
#include "rewrite/_.c"

// === unify/ ===
// Most-general-unifier (stage 4).  Reuses kbo_eq + RewriteSubst.
#include "unify/_.c"

// === cp/ ===
// Critical-pair enumeration.  Depends on thvm_unify and
// thvm_rename_vars; included last in the IC-as-ATP block.
#include "cp/_.c"

// === atp/ ===
// Saturation-loop state: AtpState struct + init / free /
// add_equation / set_goal.  Step + run drivers land in 5.2.
#include "atp/_.c"

// === wald/ ===
// Waldmeister .pr spec parser (stage 6.3).  Data model only at
// 6.3a; lexer / section drivers / term parser / equations land
// in 6.3b..g.
#include "wald/_.c"

// hrp1: heap-rooted preserve walk -- alternative to
// realize.c's mark_preserved_chain.  Standalone helper; hrp2
// wires it into thvm_realize.
#include "schedule/heap_rooted_preserve.c"

// External-caller pin table.  Tracks every Term that a foreign
// caller is holding so gc_collect_roots can keep them live.
#include "schedule/extern_pin.c"

// gc1: collect the dyn-heap GC root set.  Standalone helper;
// gc2/gc3 build the recursive mark + integrate.
#include "schedule/gc_roots.c"

// gc2: recursive mark-from-root.  Standalone helper; gc3
// composes gc1 + gc2 into the thvm_realize integration.
#include "schedule/gc_mark.c"

// thvm_realize: materialize + wnf + per-step buffer pool boundary
// (sub-item b of the per-step buffer pool arc).  Lives here
// because it depends on wnf + thvm_materialize + cpu pool helpers.
#include "schedule/realize.c"

// === runtime lifecycle ===
//
// thvm_init / thvm_free / thvm_context_* all operate on whichever
// context CURRENT_CTX points at.  Macros from thvm.h dereference
// transparently so the per-field assignments below look identical
// to the pre-context version.

// init_ctx_arrays + init_ctx_backends are the per-slot worker for
// thvm_init AND thvm_context_create.  Picks default_device by name
// ("cpu" / "metal" / NULL).
static void init_ctx_arrays(TContext *ctx) {
    ctx->heap           = (Term *)calloc(HEAP_CAP,     sizeof(Term));
    ctx->wnf_stack      = (Term *)calloc(WNF_CAP,      sizeof(Term));
    ctx->wnf_last_stack = (Term *)calloc(WNF_CAP,      sizeof(Term));
    ctx->tens           = (TenDesc *)calloc(TENS_CAP,  sizeof(TenDesc));
    ctx->kernels        = (KernelEntry *)calloc(KERNELS_CAP, sizeof(KernelEntry));
    ctx->book_heap      = (Term *)calloc(BOOK_CAP,     sizeof(Term));
    ctx->alo_states     = (AloState *)calloc(ALO_STATE_CAP, sizeof(AloState));
    ctx->cpu_bufs       = (CpuBuf *)calloc(CPU_BUFS_CAP, sizeof(CpuBuf));
    init_default_ctx_scalars(ctx);
    memset(ctx->defs,             0, sizeof(ctx->defs));
    memset(ctx->book_ref_visited, 0, sizeof(ctx->book_ref_visited));
}

static void install_ctx_backends(TContext *ctx, const char *want) {
    for (u32 i = 0; i < THVM_MAX_BACKENDS; i++) ctx->backends[i] = NULL;
    ctx->backends[THVM_DEV_CPU] = &CPU_BACKEND;
    ctx->n_backends             = 1;
    ctx->default_device         = THVM_DEV_CPU;
    if (want && strcmp(want, "metal") == 0) {
        ctx->backends[THVM_DEV_METAL] = &METAL_BACKEND;
        ctx->n_backends               = 2;
        ctx->default_device           = THVM_DEV_METAL;
    }
}

void thvm_init(void) {
  init_ctx_arrays(CURRENT_CTX);
  uop_const_cache_reset();   // CONST cache keyed by raw bits + dtype;
                             // stale entries point into a freed heap.
  uop_mov_cache_reset();     // movement-op cache, same lifecycle.
  kernel_program_cache_reset();   // KProgOp[] hash-cons; stale
                             // entries would alias freed kernels.
  lam_shape_reset();         // LAM-bound-var shape table; stale
                             // entries reference invalid lam_loc.
  extern_pin_clear();   // drop any leftover pins from a prior session
  // Backend selection: THVM_BACKEND=metal picks Metal as the default
  // device for newly allocated tensors.  Per-tensor backends are still
  // stored on TenDesc.backend, so tensors created in a future session
  // could in principle live on a different backend than the default.
  install_ctx_backends(CURRENT_CTX, getenv("THVM_BACKEND"));
  DEFAULT_BACKEND->init();
  // Register core PRI primitives (SEQ + LOG) -- idempotent overwrite,
  // safe across re-init.  ATP registers its own block separately on
  // first thvm_atp_init.
  thvm_register_core_prims();
}

void thvm_free(void) {
  if (DEFAULT_BACKEND) DEFAULT_BACKEND->shutdown();
  free(HEAP);            HEAP            = NULL;
  free(WNF_STACK);       WNF_STACK       = NULL;
  free(WNF_LAST_STACK);  WNF_LAST_STACK  = NULL;
  free(TENS);            TENS            = NULL;
  // Free per-kernel heap arrays before freeing the KERNELS table
  // itself (each KernelEntry now owns input_*[] and program[] on
  // the heap; calloc-zeroed entries have NULL pointers, free is
  // NULL-safe).
  if (KERNELS) {
    for (u32 i = 0; i < KERNELS_NEXT; i++) kernel_free_arrays(&KERNELS[i]);
  }
  free(KERNELS);         KERNELS         = NULL;
  free(BOOK_HEAP);       BOOK_HEAP       = NULL;
  free(ALO_STATES);      ALO_STATES      = NULL;
  free(CPU_BUFS);        CPU_BUFS        = NULL;
  HEAP_NEXT       = 0;
  WNF_S_POS       = 0;
  WNF_LAST_STACK_LEN = 0;
  ITRS            = 0;
  TENS_NEXT       = 1;
  KERNELS_NEXT    = 1;
  BOOK_NEXT       = 1;
  ALO_STATES_NEXT = 1;
  CPU_BUFS_NEXT   = 1;
  CPU_FREELIST_LEN = 0;
  memset(DEFS,             0, sizeof(((TContext *)0)->defs));
  memset(BOOK_REF_VISITED, 0, sizeof(((TContext *)0)->book_ref_visited));
  extern_pin_clear();
  for (u32 i = 0; i < THVM_MAX_BACKENDS; i++) CURRENT_CTX->backends[i] = NULL;
  CURRENT_CTX->n_backends     = 0;
  CURRENT_CTX->default_device = 0;
}

// === Multi-context API ===

u32 thvm_context_create(const char *default_device) {
    for (u32 slot = 1; slot < CONTEXTS_CAP; slot++) {
        if (CONTEXTS[slot] != NULL) continue;
        TContext *ctx = (TContext *)calloc(1, sizeof(TContext));
        if (!ctx) return 0;
        CONTEXTS[slot] = ctx;
        TContext *prev = CURRENT_CTX;
        CURRENT_CTX = ctx;
        init_ctx_arrays(ctx);
        install_ctx_backends(ctx, default_device);
        DEFAULT_BACKEND->init();
        CURRENT_CTX = prev;
        return slot;
    }
    return 0;
}

u32 thvm_context_select(u32 slot) {
    u32 prev = thvm_context_current();
    if (slot < CONTEXTS_CAP && CONTEXTS[slot] != NULL) {
        CURRENT_CTX = CONTEXTS[slot];
    }
    return prev;
}

u32 thvm_context_current(void) {
    for (u32 slot = 0; slot < CONTEXTS_CAP; slot++) {
        if (CONTEXTS[slot] == CURRENT_CTX) return slot;
    }
    return 0;
}

void thvm_context_destroy(u32 slot) {
    if (slot == 0)              return;       // default slot owns DEFAULT_CTX_STORAGE
    if (slot >= CONTEXTS_CAP)   return;
    if (CONTEXTS[slot] == NULL) return;
    TContext *ctx = CONTEXTS[slot];
    TContext *prev = CURRENT_CTX;
    CURRENT_CTX = ctx;
    if (DEFAULT_BACKEND) DEFAULT_BACKEND->shutdown();
    free(ctx->heap);
    free(ctx->wnf_stack);
    free(ctx->wnf_last_stack);
    free(ctx->tens);
    free(ctx->kernels);
    free(ctx->book_heap);
    free(ctx->alo_states);
    free(ctx->cpu_bufs);
    CURRENT_CTX = prev;
    if (CURRENT_CTX == ctx) CURRENT_CTX = CONTEXTS[0];
    free(ctx);
    CONTEXTS[slot] = NULL;
}
