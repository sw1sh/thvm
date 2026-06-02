// thvm.c - single-translation-unit hub.
//
// Each .c contributes one function (or a tiny family of helpers).
// Include order is the dependency order: term -> dtype -> heap ->
// view -> backend -> tensor -> uop -> schedule -> interact -> wnf.
// Interactions depend on the schedule pipeline (uop_kernel reaches
// into KERNELS), and wnf drives the interactions.

#include "thvm.h"

// === util/ ===
// Shared concurrency primitives.  Foundation for the parallel WNF /
// CNF redex bag (Phase 1 of docs/aot.md / docs/plans/levy_optimal.md).
// Pulled in early because the data structures are pure (no thvm
// runtime dependency) and downstream code may build per-context
// scheduler state on top.
#include "util/atomics.h"
#include "util/wsq.c"
#include "util/wspq.c"

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
    ctx->wnf_state.s_pos  = 0;
    ctx->wnf_last_stack_len = 0;
    ctx->itrs             = 0;
    ctx->tens_next        = 1;   // 0 reserved
    ctx->kernels_next     = 1;   // 0 reserved
    ctx->book_next        = 1;   // 0 reserved
    ctx->alo_states_next  = 1;   // 0 reserved
    ctx->cpu_bufs_next    = 1;   // 0 reserved
    ctx->cpu_freelist_len = 0;
}

// Thread-local pointer that the WNF_STACK / WNF_S_POS macros
// dereference.  Default per thread is NULL; thvm_init binds it to the
// main thread's CURRENT_CTX->wnf_state.  Worker threads spawned by the
// parallel pool re-bind it to their own WnfThreadState before draining.
_Thread_local WnfThreadState *CURRENT_WNF_STATE = NULL;

// Single point of mutation for CURRENT_CTX so the WNF spine routing
// stays in sync.  Every site that reassigns CURRENT_CTX must go
// through this helper -- otherwise wnf() called after the switch
// would dereference the old context's stack.
static void thvm_set_current_ctx(TContext *ctx) {
    CURRENT_CTX       = ctx;
    if (ctx != NULL) CURRENT_WNF_STATE = &ctx->wnf_state;
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
#include "term/new_ann.c"
#include "term/new_pri.c"
#include "term/new_dsu.c"
#include "term/new_ddu.c"

// === dtype/ ===
// dtype info table + element-size primitives + integer-kernel macros.
// Must come before any TU that calls dtype_itemsize / dtype_storage_bytes
// (tensor/alloc.c, schedule/materialize.c, backend/cpu/uop_walk.c,
// jit/capture.c).  fp_convert + lane primitives ride alongside;
// they're consumed by the scalar UOp interpreter and the UOp DAG
// walker for the f16 / bf16 / fp8 promote-to-f32 ALU path.
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
// Atomic-flavoured siblings.  Same body as the plain helpers but
// __atomic_*-based so workers in the parallel WNF / NF pool see
// release-ordered writes.  Single-thread cost is identical on x86 /
// arm64 TSO (relaxed loads compile to plain loads); the existing call
// sites use the plain variants and opt in explicitly where SUB-bit
// synchronisation matters.
#include "heap/read_acq.c"
#include "heap/set_rel.c"
#include "heap/take_atomic.c"
#include "heap/cas.c"
#include "heap/subst_var_rel.c"

// === instrument/ ===
// Process-global hot-path counters for WL-side debugging.  Single
// file-scope statics; downstream consumers `HOT_*++` directly.
#include "instrument/hot_counters.c"
// Multicomputation reduction trace.  Self-gated by `#ifdef THVM_TRACE`:
// in default builds the entire translation unit is empty and the
// `multi_emit(...)` macro at every call site is `((void)0)`.  See
// docs/plans/multicomputation_trace.md and src/instrument/multi.c.
#include "instrument/multi.c"

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
// helpers reference them, then the scalar / tile interpreters, and
// finally _.c assembles the Backend vtable.  The UOp DAG walker
// (uop_walk.c) is the sole dispatch path; per-op interpreters are
// gone.
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
// codegen/ is backend-agnostic.  axis.c + apply_opt.c land first
// so renderer + cg passes see the KpSchedule scheduling structure.
// so they can call cg_profile_record / cg_now_us / cg_kernel_flops
// + reference KDispatchKind enum constants.
// kvar.c lives in src/schedule/ but is included here so the codegen
// + Metal backend (both above the schedule/ include block) can call
// kvar_extent_is_var / kvar_collect_from_dag without forward decls.
#include "schedule/kvar.c"
#include "codegen/axis.c"
#include "codegen/apply_opt.c"
#include "codegen/propose.c"
#include "codegen/tile_anno.c"
#include "codegen/cg.c"
#include "codegen/profile.c"
#include "codegen/render_metal.c"
#include "codegen/render_uop.c"
// codegen/render_linearized.c -- new emit walk that consumes a
// LinKernel (uop_linearize output) instead of walking the legacy
// DAG.  Test-only stub today (single-store elementwise over one
// LOOP range); the production renderer entry points still call the
// legacy emit.  See file header for the architectural rationale.
#include "codegen/render_linearized.c"
// codegen/render_ptx.c -- PTX assembly emitter consuming the SAME
// LinKernel as render_linearized.c, emitting PTX text directly so the
// CUDA jit can cuModuleLoadData it without nvrtc's C++ frontend.  Port
// of tinygrad/renderer/ptx.py.  Milestone 1: elementwise + loop core.
#include "codegen/render_ptx.c"
// CPU dispatch: interpreter + BLAS pattern dispatch + clang-JIT.
// cpu_dispatch_kernel composes the three (BLAS first, then JIT, then
// interpreter); each records its route via cg_profile_record.
#include "backend/cpu/interpret.c"
#include "backend/cpu/blas.c"
#include "backend/cpu/jit.c"
#include "backend/cpu/uop_walk.c"
#include "backend/cpu/_.c"

// === backend/metal/ ===
// On non-Apple builds (and Apple builds that DON'T compile the .m
// glue separately), include the C stub so METAL_BACKEND still
// resolves -- DEV=metal selects it but every compute call
// returns an error.  Apple builds that link build/backend_metal.o
// (the dual-TU Metal path) define THVM_HAS_METAL to skip this
// include and use the .m-defined symbols instead.
#ifndef THVM_HAS_METAL
#include "backend/metal/_.c"
#endif

// === backend/cuda/ ===
// Unlike the Objective-C Metal backend (a separate .m TU), the CUDA
// backend is plain C99 -- the CUDA driver API and nvrtc are C headers
// -- so it is #included straight into this single-TU build, exactly
// like backend/cpu/.  Guarded by THVM_HAS_CUDA: only the Linux+CUDA
// build (see the Makefile CUDA block) defines it and links -lcuda
// -lnvrtc.  Mac / plain-Linux builds skip the whole subtree.  Order
// mirrors backend/cpu/: init defines CUDA_BUFS first, the buf_*
// helpers reference it, jit.c depends on cg_render_uop_kernel_cuda_root
// (already included via codegen/render_uop.c above), and _.c assembles
// the vtable + thvm_cuda_* entry points last.
#ifdef THVM_HAS_CUDA
#include "backend/cuda/init.c"           // cuda.h/nvrtc.h, CudaBuf, state
#include "backend/cuda/buf_freelist.c"
#include "backend/cuda/buf_alloc.c"
#include "backend/cuda/buf_free.c"
#include "backend/cuda/buf_incref.c"
#include "backend/cuda/buf_decref.c"
#include "backend/cuda/buf_read.c"
#include "backend/cuda/buf_write.c"
#include "backend/cuda/buf_copy.c"
#include "backend/cuda/buf_pool.c"
#include "backend/cuda/jit.c"
#include "backend/cuda/_.c"
#endif

#include "backend/dispatch/begin_all.c"
#include "backend/dispatch/flush_all.c"
#include "backend/dispatch/end_all.c"

// === tensor/ ===
#include "tensor/alloc.c"
#include "tensor/incref.c"
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
#include "uop/detach.c"
#include "uop/cast.c"
#include "uop/bitcast.c"
#include "uop/index_simplify.c"
#include "uop/index.c"
#include "uop/movement_index.c"
#include "uop/buffer.c"
#include "uop/store.c"
#include "uop/opt.c"
#include "uop/view.c"
#include "uop/graph_rewrite.c"
#include "uop/upat.c"
#include "uop/graph_simplify.c"
#include "uop/recognise_tc.c"
#include "uop/recognise_conv.c"
#include "uop/apply_opt.c"
#include "uop/apply_opt_dag.c"
#include "uop/dag_scan.c"
// uop/expander.c -- port of tinygrad codegen/late/expander.py to thvm's
// UOp graph rewrite framework.  Introduces UOP_UNROLL/CONTRACT/VCONST/GEP
// + uop_expand_graph(root).  Not wired into render_uop.c yet -- runs only
// when explicitly invoked (test_uop_expand exercises it directly).  See
// docs/tinygrad_late_passes.md for the architectural alternative.
#include "uop/expander.c"
// uop/devectorize.c -- port of tinygrad codegen/late/devectorizer.py
// (reduce_to_acc + devectorize + pm_render + load_store_folding) to
// thvm's UOp graph rewrite framework.  Introduces UOP_STACK /
// UOP_PLACEHOLDER / UOP_END + uop_devectorize_graph(root) +
// uop_load_store_fold_graph(root).  Same disposition as expander: NOT
// WIRED into render_uop.c -- runs only via test_uop_devectorize for now.
#include "uop/devectorize.c"
// uop/symbolic_rewrite.c -- port of (a subset of) tinygrad/uop/symbolic.py
// "sym" PatternMatcher passes.  Provides uop_symbolic_rewrite(root) which
// re-runs the simplifying constructors over a bottom-up rebuild and adds
// the scalar-lane-only MUL-by-0 + GEP-on-STACK + STACK-singleton rules.
// Wired into render_uop.c between expander/devectorize/load_store_fold so
// the post-devectorize graph shrinks before reaching the linearizer.
#include "uop/symbolic_rewrite.c"
// uop/linearize.c -- port of tinygrad codegen/late/linearizer.py.
// Walks a post-devectorize DAG and produces a LinKernel: an ordered
// list of UOp Terms ready for the new render_linearized.c emit walk.
// Stage (a) of the architectural piece #3 wiring; runs only via the
// dedicated tests today (gates 3-5 keep the legacy renderer routed
// for production paths until the new emit can match it on every
// test_render_uop case).
#include "uop/linearize.c"
// codegen/hand_opts.c -- tinygrad's hand_coded_optimizations port.
// Needs kernel_apply_opt (codegen/apply_opt.c) + the DAG axis
// scanners (uop/dag_scan.c) above, so it lands here.
#include "codegen/hand_opts.c"

// === schedule/ ===
// Materialize pipeline: schedule + kernelize + lift.  Produces the
// scheduled DAG of UOP_KERNEL terms that interact_kernel fires
// bottom-up.
#include "schedule/indexing.c"
#include "schedule/kernel_lift.c"
#include "schedule/tile.c"
#include "schedule/kernel_alloc.c"
#include "schedule/uop_meta.c"
#include "schedule/consumer_count.c"
#include "schedule/bufferize.c"
#include "schedule/bufferize_classify.c"
// Phase 2 (ideal_pipeline_v2): unified rangeify pass. Must follow
// bufferize_classify.c (uses BUFFERIZE_NODES + CMAP_LL +
// bufferize_consumers_for_loc) and indexing.c (apply_movement_op_*).
#include "schedule/rangeify_unified.c"
// Per-realize arena memory planner uses a TLSF suballocator.  tlsf.c
// is standalone (no thvm globals); materialize.c uses tlsf_init /
// tlsf_alloc / tlsf_free at the start of every emit pass.  Port of
// tinygrad/runtime/support/memory.py TLSFAllocator.
#include "schedule/tlsf.c"
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
#include "interact/dup_nod.c"
#include "interact/op2_sup.c"
#include "interact/op2_num_sup.c"
#include "interact/app_mat_sup.c"
#include "interact/app_bri.c"
#include "interact/app_pri.c"
#include "interact/app_sup.c"
#include "interact/ann_lam.c"
#include "interact/ann_bri.c"
#include "interact/dup_bri.c"
#include "interact/uop_grad.c"
#include "interact/uop_kernel.c"
#include "interact/uop_assign.c"
#include "interact/dsu_num.c"
#include "interact/dsu_era.c"
#include "interact/dsu_sup.c"
#include "interact/ddu_num.c"
#include "interact/ddu_era.c"
#include "interact/ddu_sup.c"

// codegen/autotune.c needs kernel_fire_by_id (uop_kernel.c) so it
// lives down here, AFTER the interact pass.  Bench-and-pick the
// winning TOpt per program shape.
#include "codegen/autotune.c"

// === wnf/ pool ===
// Worker pool data structures + CURRENT_WORKER thread-local.
// Pool only references types from term/ and thvm.h, so it has no
// dependency on wnf/ reducer code; the new (Bend2-style) AOT
// runtime will reuse its pthread management when it lands.
#include "wnf/pool.c"

// === aot/ ===
// Bend2-style fork-emitting AOT runtime.  Phase 1: Task/Result +
// continuation cells + worker pool; CPS emit lands in Phase 2.
// See src/aot/_.c for the bundle layout.
#include "aot/_.c"

// === wnf/ ===
// The reducer dispatches to the interactions and to materialize,
// so every file it calls must be defined above.
#include "wnf/_.c"
// wnf/pool.c is included higher up (before the empty aot/ slot) so
// the future Bend-style AOT runtime can reuse it.  The pool API
// stays available to wnf_/redex_/nf_ callers below transparently.
#include "wnf/redex.c"
#include "wnf/nf.c"

// === cnf/ ===
// CNF (collapsed normal form) readback.  Depends on wnf() and on
// the dup-* interaction rules; loaded right after wnf/ so the
// dispatch helpers (interact_dup_sup / dup_lam / dup_ctr / ...)
// are visible.  Plain DPs are Levy-opaque under wnf; cnf is the
// readback layer where their duplication actually fires.
#include "cnf/_.c"

// === eval/ ===
// Single-threaded collapse: enumerate every pure leaf reachable
// through SUP branches in a term.  Calls cnf() at each step.
#include "eval/collapse.c"

// === term/eq.c ===
// Structural term equality for pattern-matching consumers.
// Loaded after cnf/ since the reducing wrapper drives both sides
// through cnf first.
#include "term/eq.c"

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
#include "rpo/_.c"
#include "wpo/_.c"

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
// Algebraic-structure detection + precedence generation, ported
// from Waldmeister's PhilMarlow + Praezedenzgenerator.  Included
// BEFORE the saturator so atp/ac.c (transitively included by
// atp/_.c) can call atp_analyze_axioms from thvm_atp_auto_ac.
#include "atp/precedence.c"

// Saturation-loop state: AtpState struct + init / free /
// add_equation / set_goal.  Step + run drivers land in 5.2.
#include "atp/_.c"

// === fol/ ===
// First-order clausal reasoning (Vampire/E-class).  Generalises the
// unit-equational saturator with multi-literal clauses + binary
// resolution + factoring.  Builds on thvm_unify / thvm_rename_vars
// from src/unify and on kbo_eq for structural literal comparison;
// must come after both.
#include "fol/_.c"

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
    ctx->heap             = (Term *)calloc(thvm_heap_cells(), sizeof(Term));
    ctx->wnf_state.stack  = (Term *)calloc(WNF_CAP,      sizeof(Term));
    ctx->wnf_last_stack   = (Term *)calloc(WNF_CAP,      sizeof(Term));
    ctx->tens           = (TenDesc *)calloc(TENS_CAP,  sizeof(TenDesc));
    ctx->kernels        = (KernelEntry *)calloc(KERNELS_CAP, sizeof(KernelEntry));
    // book_heap: 16KB page-aligned so it can be wrapped as a
    // shared MTLBuffer via newBufferWithBytesNoCopy without an
    // intermediate copy on the AOT-on-Metal path.  BOOK_CAP *
    // sizeof(Term) = 2 MiB = 128 pages on Apple Silicon, satisfying
    // aligned_alloc's "size is a multiple of alignment" requirement.
    ctx->book_heap      = (Term *)aligned_alloc(16384, BOOK_CAP * sizeof(Term));
    memset(ctx->book_heap, 0, BOOK_CAP * sizeof(Term));
    ctx->alo_states     = (AloState *)calloc(ALO_STATE_CAP, sizeof(AloState));
    ctx->cpu_bufs       = (CpuBuf *)calloc(CPU_BUFS_CAP, sizeof(CpuBuf));
    init_default_ctx_scalars(ctx);
    memset(ctx->defs,             0, sizeof(ctx->defs));
    memset(ctx->book_ref_visited, 0, sizeof(ctx->book_ref_visited));
}

// Case-insensitive match of a DEV string against a backend name
// ("cpu"/"metal"/"cuda").  A trailing ":renderer" suffix (tinygrad's
// DEV=CUDA:rdna form) is ignored.  Returns 0 for a NULL `want`.
int thvm_dev_name_is(char const *want, char const *name) {
  if (want == NULL) return 0;
  for (; *want != '\0' && *want != ':' && *name != '\0'; want++, name++) {
    char c = (*want >= 'A' && *want <= 'Z') ? (char)(*want + 32) : *want;
    if (c != *name) return 0;
  }
  return (*want == '\0' || *want == ':') && *name == '\0';
}

static void install_ctx_backends(TContext *ctx, const char *want) {
    for (u32 i = 0; i < THVM_MAX_BACKENDS; i++) ctx->backends[i] = NULL;
    ctx->backends[THVM_DEV_CPU] = &CPU_BACKEND;
    ctx->n_backends             = 1;
    ctx->default_device         = THVM_DEV_CPU;
    if (thvm_dev_name_is(want, "metal")) {
        ctx->backends[THVM_DEV_METAL] = &METAL_BACKEND;
        ctx->n_backends               = 2;
        ctx->default_device           = THVM_DEV_METAL;
    }
#ifdef THVM_HAS_CUDA
    // DEV=cuda registers the CUDA backend in the vtable
    // slot and makes it the default device.  cuda_dispatch_kernel
    // (Stage 3) routes a scheduled KernelEntry through the
    // structural-lift CUDA path, so a kernel built + scheduled with
    // DEV=cuda runs end to end on the GPU.
    if (thvm_dev_name_is(want, "cuda")) {
        ctx->backends[THVM_DEV_CUDA] = &CUDA_BACKEND;
        ctx->n_backends              = THVM_DEV_CUDA + 1;
        ctx->default_device          = THVM_DEV_CUDA;
    }
#endif
}

void thvm_init(void) {
  init_ctx_arrays(CURRENT_CTX);
  // Bind the main thread's WNF spine routing to the just-allocated
  // ctx state.  Worker threads spawned by the parallel pool re-bind
  // CURRENT_WNF_STATE to their own per-worker WnfThreadState.
  CURRENT_WNF_STATE = &CURRENT_CTX->wnf_state;
  uop_const_cache_reset();   // CONST cache keyed by raw bits + dtype;
                             // stale entries point into a freed heap.
  uop_mov_cache_reset();     // movement-op cache, same lifecycle.
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
  gc_init(thvm_heap_cells() / 2);
  // Backend selection: DEV=metal picks Metal as the default
  // device for newly allocated tensors.  Per-tensor backends are still
  // stored on TenDesc.backend, so tensors created in a future session
  // could in principle live on a different backend than the default.
  install_ctx_backends(CURRENT_CTX, getenv("DEV"));
  DEFAULT_BACKEND->init();
  // Register core PRI primitives (SEQ + LOG) -- idempotent overwrite,
  // safe across re-init.  ATP registers its own block separately on
  // first thvm_atp_init.
  thvm_register_core_prims();
}

void thvm_free(void) {
  // Tear down the parallel-NF process-global pool first so its
  // worker pthreads exit before we free the heap they've been
  // sharing -- otherwise a worker that wakes mid-shutdown would
  // dereference freed memory.  no-op if no nf call has happened yet.
  nf_pool_global_release();
  // Lifter coverage dump.  Env-gated so the noise doesn't pollute
  // normal test output; turn on with THVM_DUMP_LIFT_COVERAGE=1 to
  // see how many cg_emit_tile_metal calls the lifter handled.
  if (getenv("THVM_DUMP_LIFT_COVERAGE")) {
    fprintf(stderr,
            "thvm: kernel_lift coverage -- attempts=%llu successes=%llu\n",
            (unsigned long long)kernel_lift_attempts(),
            (unsigned long long)kernel_lift_successes());
    fprintf(stderr,
            "thvm: tile_conv2d_flat -- dag=%llu\n",
            (unsigned long long)tile_conv2d_flat_dag_count());
    fprintf(stderr,
            "thvm: bypass coverage -- total=%llu used_unified=%llu "
            "(resid=%llu stranded=%llu bcast=%llu)\n",
            (unsigned long long)bypass_kernel_total_count(),
            (unsigned long long)bypass_kernel_used_unified_count(),
            (unsigned long long)bypass_gate_resid_count(),
            (unsigned long long)bypass_gate_stranded_count(),
            (unsigned long long)bypass_gate_bcast_count());
  }
  // Per-kernel wall-time profile. THVM_KERNEL_PROFILE=N -> top N rows
  // (N=0 / unset disables; setting "" or "1" defaults to 20).
  {
    char const *e = getenv("THVM_KERNEL_PROFILE");
    if (e != NULL && e[0] != '\0' && !(e[0] == '0' && e[1] == '\0')) {
      u32 n = (u32)atoi(e);
      if (n == 0) n = 20;
      cg_profile_dump(stderr, n);
    }
  }
  if (DEFAULT_BACKEND) DEFAULT_BACKEND->shutdown();
  // Wipe every file-static cache / side table that thvm_init seeds.
  // These hold Terms / heap pointers; once the heap below is freed,
  // a stale entry would dangle until the next thvm_init reseeds it.
  // Same order as thvm_init for obvious symmetry.
  uop_const_cache_reset();
  uop_mov_cache_reset();
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
  // itself (each KernelEntry owns input_*[] on the heap; calloc-
  // zeroed entries have NULL pointers, free is NULL-safe).
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
        thvm_set_current_ctx(ctx);
        init_ctx_arrays(ctx);
        install_ctx_backends(ctx, default_device);
        DEFAULT_BACKEND->init();
        thvm_set_current_ctx(prev);
        return slot;
    }
    return 0;
}

u32 thvm_context_select(u32 slot) {
    u32 prev = thvm_context_current();
    if (slot < CONTEXTS_CAP && CONTEXTS[slot] != NULL) {
        thvm_set_current_ctx(CONTEXTS[slot]);
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
    thvm_set_current_ctx(ctx);
    if (DEFAULT_BACKEND) DEFAULT_BACKEND->shutdown();
    // No cache / side-table resets here: uop_const_cache, uop_mov_cache,
    // LAM_SHAPE_TABLE, EXTERN_PIN*, JIT_CAPTURES, K_PROFILE
    // are all file-static globals shared across every context, not
    // per-ctx state.  Wiping them here would clobber live entries from
    // other contexts.  thvm_free() is the only correct site.
    free(ctx->heap);
    free(ctx->wnf_state.stack);
    free(ctx->wnf_last_stack);
    free(ctx->tens);
    free(ctx->kernels);
    free(ctx->book_heap);
    free(ctx->alo_states);
    free(ctx->cpu_bufs);
    thvm_set_current_ctx(prev);
    if (CURRENT_CTX == ctx) thvm_set_current_ctx(CONTEXTS[0]);
    free(ctx);
    CONTEXTS[slot] = NULL;
}
