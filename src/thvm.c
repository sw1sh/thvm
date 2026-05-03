// thvm.c - single-translation-unit hub.
//
// Each .c contributes one function (or a tiny family of helpers).
// Include order is the dependency order: term -> dtype -> heap ->
// view -> backend -> tensor -> uop -> schedule -> interact -> wnf.
// Interactions depend on the schedule pipeline (uop_kernel reaches
// into KERNELS), and wnf drives the interactions.

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

// === dtype/ ===
// dtype info table + element-size primitives + integer-kernel macros.
// Must come before any TU that calls dtype_itemsize / dtype_storage_bytes
// (tensor/alloc.c, schedule/materialize.c, backend/cpu/op/*.c,
// jit/capture.c).  fp_convert + lane primitives ride alongside;
// they're consumed by backend/cpu/op/_promote.c (loaded a bit later)
// for the f16 / bf16 / fp8 promote-to-f32 ALU path.
#include "dtype/info.c"
#include "dtype/int_kernels.h"
#include "dtype/fp_convert.c"
#include "dtype/fp8.c"
#include "dtype/nibble.c"
#include "dtype/lane.c"

// === heap/ ===
#include "heap/alloc.c"
#include "heap/read.c"
#include "heap/set.c"
#include "heap/take.c"
#include "heap/subst_var.c"
#include "heap/subst_cop.c"

// === instrument/ ===
// Process-global hot-path counters for WL-side debugging.  Single
// file-scope statics; downstream consumers `HOT_*++` directly.
#include "instrument/hot_counters.c"

// === lam/ ===
// Side tables tied to LAM heap locs (shape annotation, future:
// arity hints).  No reduction logic; pure storage.
// body_uses_var.c follows shape.c so alo_realize and
// clone_to_book_rec (loaded next) can call it from their LAM
// construction sites to set LAM_ERA_MASK on unused-binder lambdas.
#include "lam/shape.c"
#include "lam/body_uses_var.c"
#include "lam/auto_dup.c"

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
#include "backend/cpu/buf_copy.c"
#include "backend/cpu/buf_pool.c"
// Promote-to-fp32 helper for narrow-float / fp8 elementwise.
#include "backend/cpu/op/_promote.c"
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
#include "backend/cpu/op/cast.c"
#include "backend/cpu/op/bitcast.c"
// codegen/ is backend-agnostic.  axis.c + apply_opt.c land first
// so renderer + cg passes see the KernelAxes scheduling structure.
// so they can call cg_profile_record / cg_now_us / cg_kernel_flops
// + reference KDispatchKind enum constants.
#include "codegen/axis.c"
#include "codegen/apply_opt.c"
#include "codegen/propose.c"
#include "codegen/cg.c"
#include "codegen/profile.c"
#include "codegen/render_c.c"
#include "codegen/render_c_scalar.c"
#include "codegen/render_metal.c"
// CPU dispatch: interpreter + BLAS pattern dispatch + clang-JIT.
// cpu_dispatch_kernel composes the three (BLAS first, then JIT, then
// interpreter); each records its route via cg_profile_record.
#include "backend/cpu/interpret.c"
#include "backend/cpu/blas.c"
#include "backend/cpu/jit.c"
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
#include "backend/dispatch/begin_all.c"
#include "backend/dispatch/flush_all.c"
#include "backend/dispatch/end_all.c"

// === tensor/ ===
#include "tensor/alloc.c"
#include "tensor/incref.c"
#include "tensor/decref.c"
#include "tensor/release.c"
#include "tensor/mark_buf_preserved.c"
#include "tensor/view_of.c"

// === uop/ ===
// Constructors for raw UOp graph nodes.  Each helper allocates the
// heap cells for one opcode and returns a TAG_UOP term; nothing reduces.
#include "uop/const.c"
#include "uop/mov_cache.c"
#include "uop/rewrite.c"   // constant fold + algebraic identities;
                            // called by binary / unary constructors.
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
#include "uop/leaf_tids.c"
#include "uop/load.c"
#include "uop/cast.c"
#include "uop/bitcast.c"
#include "uop/view.c"
#include "uop/graph_rewrite.c"
#include "uop/graph_simplify.c"

// === schedule/ ===
// Materialize pipeline: schedule + kernelize + linearize + splice.
// Produces the scheduled DAG of UOP_KERNEL terms that
// interact_kernel fires bottom-up.
#include "schedule/rangeify.c"
#include "schedule/tile.c"
#include "schedule/kernel_alloc.c"
#include "schedule/kernel_program_cache.c"
#include "schedule/uop_meta.c"
#include "schedule/consumer_count.c"
#include "schedule/realize_rewrite.c"
#include "schedule/bufferize.c"
#include "schedule/realize_classify.c"
#include "schedule/materialize.c"

// === jit/ ===
// Capture+replay of a kernel-dispatch sequence (Phase 7 of the
// tinygrad-parity arc).  Loaded BEFORE interact/uop_kernel.c so
// kernel_fire_by_id can call the capture hook.
#include "jit/capture.c"

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
#include "interact/dup_ctr.c"
#include "interact/dup_app.c"
#include "interact/dup_op2.c"
#include "interact/dup_mat.c"
#include "interact/app_bri.c"
#include "interact/app_pri.c"
#include "interact/app_sup.c"
#include "interact/ann_lam.c"
#include "interact/ann_bri.c"
#include "interact/dup_bri.c"
#include "interact/uop_grad.c"
#include "interact/uop_kernel.c"
#include "interact/uop_assign.c"

// codegen/autotune.c needs kernel_fire_by_id (uop_kernel.c) so it
// lives down here, AFTER the interact pass.  Bench-and-pick the
// winning TOpt per program shape.
#include "codegen/autotune.c"

// === aot/ ===
// AOT function-pointer table + helpers.  Declared BEFORE wnf so
// TAG_REF dispatch can call aot_lookup() / AOT_CALLS.  Forward-
// declares wnf so aot_force can call it (resolved at link time
// since we're a single TU).
fn Term wnf(Term term);
#include "aot/_.c"
// Function-pointer ABI for runtime-loadable AOT dylibs.  Loaded
// after aot/_.c so it can reference aot_register / aot_pop_app_arg
// / etc.  Must come BEFORE wnf/_.c so any future TAG_REF dispatch
// hooks see the same set of helpers.
#include "aot/runtime_ops.c"
// Auto-emitter that walks a TDef'd body and produces C source.
// Phase 0: constants and identity only -- the rest of the patterns
// (MAT chain, CTR destructure, OP2, DUP) land in follow-ups.  Sits
// next to the runtime helpers because it walks book-heap cells via
// book_read() and uses the same Term layout macros.
#include "aot/emit.c"
// Hand-coded AOT programs (proof-of-concept; will be replaced by
// auto-emitted code once src/aot/emit.c lands).  Each program file
// exposes one aot_program_<name>_register(def_id1, def_id2, ...)
// entry that the WL surface or a C test calls after TDef'ing the
// matching bodies.
#include "aot/programs/fib_nat.c"
#include "aot/programs/gab_tak.c"
#include "aot/programs/u32_fib.c"
// Build pipeline: emit + clang + dlopen + register.  References
// thvm_aot_emit_def + aot_runtime_ops + aot_register from above.
// Defines AOT_THVM_ROOT so the dylib's emitted #include can find
// abi.h.  TODO when the source moves: pass via build flag.
#ifndef AOT_THVM_ROOT
#define AOT_THVM_ROOT "/Users/swish/src/thvm"
#endif
#include "aot/build.c"

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

// Heap-rooted preserve walk; alternative to mark_preserved_chain.
#include "schedule/heap_rooted_preserve.c"

// External-caller pin table.  Tracks every Term that a foreign
// caller is holding so gc_collect_roots can keep them live.
#include "schedule/extern_pin.c"

// Collect the dyn-heap GC root set.
#include "schedule/gc_roots.c"

// Recursive mark-from-root over the collected roots.
#include "schedule/gc_mark.c"

// Mark/sweep GC for the KernelEntry arena.  Invoked at end of
// each thvm_realize so kernel/buffer counts stay bounded across
// long training loops.  Reuses freed slots via a freelist.
#include "schedule/kernel_gc.c"

// Cheney-style copying GC for the dyn heap.  Two semi-spaces; live
// cells get evacuated on gc_collect.  Triggered from thvm_realize
// once the per-step heap watermark crosses the threshold.
#include "heap/collect.c"

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
  // Also wipe the sparse pin-handle table -- entries from a prior
  // session point at the old heap, which is now freed.  WL's
  // ManagedLibraryExpression manager will eventually call
  // extern_pin_handle_drop on each old id; that becomes a harmless
  // no-op once the entry is zero, while gc_evacuate_side_tables is
  // saved from following stale Term values into bogus from-space
  // locs (the ttermRaw fast-path falls back to the cached id, which
  // never reaches the GC).
  extern_pin_handle_clear();
  jit_capture_reset_all();   // drop stale TJit capture slots from a
                             // prior session (their kid pointers
                             // would refer to the now-cleared
                             // KERNELS table).
  cpu_jit_cache_reset();     // close dlopen'd JIT variants from the
                             // previous session before new keys reuse
                             // the process-global cache table.
  cg_profile_reset();        // per-kid FLOPS / dispatch counters; reset
                             // so each session starts at zero.
  // Cheney semi-spaces: split HEAP_CAP in half.  heap_alloc bumps
  // within the active from-space; gc_collect evacuates live cells
  // into to-space and swaps when triggered from thvm_realize.
  gc_init(HEAP_CAP / 2);
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
  // Wipe every file-static cache / side table that thvm_init seeds.
  // These hold Terms / heap pointers; once the heap below is freed,
  // a stale entry would dangle until the next thvm_init reseeds it.
  // Same order as thvm_init for obvious symmetry.
  uop_const_cache_reset();
  uop_mov_cache_reset();
  kernel_program_cache_reset();
  lam_shape_reset();
  extern_pin_clear();
  extern_pin_handle_clear();
  jit_capture_reset_all();
  cpu_jit_cache_reset();
  cg_profile_reset();
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
  gc_reset();
  HEAP_NEXT       = 0;
  WNF_S_POS       = 0;
  WNF_LAST_STACK_LEN = 0;
  ITRS            = 0;
  TENS_NEXT       = 1;
  KERNELS_NEXT    = 1;
  BOOK_NEXT       = 1;
  ALO_STATES_NEXT = 1;
  alo_dup_share_reset();
  aot_reset();
  CPU_BUFS_NEXT   = 1;
  CPU_FREELIST_LEN = 0;
  memset(DEFS,             0, sizeof(((TContext *)0)->defs));
  memset(BOOK_REF_VISITED, 0, sizeof(((TContext *)0)->book_ref_visited));
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
    // No cache / side-table resets here: uop_const_cache, uop_mov_cache,
    // KP_CACHE, LAM_SHAPE_TABLE, EXTERN_PIN*, JIT_CAPTURES, K_PROFILE
    // are all file-static globals shared across every context, not
    // per-ctx state.  Wiping them here would clobber live entries from
    // other contexts.  thvm_free() is the only correct site.
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
