// thvmlink.c - Wolfram LibraryLink bridge for thvm.
//
// Single-TU build: includes the entire runtime via ../../../src/thvm.c
// (this file lives at wl/THVMLink/CSource/).  All exported functions are
// scalar-in / scalar-out (mint <-> Integer).  Higher-level constructors
// (TLam, TApp, TSup, TDup) are synthesized on the WL side from these
// primitives - keeps the C surface tiny.

#include "WolframLibrary.h"
#include "WolframNumericArrayLibrary.h"

// Tell prims_core.c not to emit the weak no-op default for
// thvm_pri_wl_invoke_returning -- we provide the strong definition
// further down in this TU.
#define THVM_HAS_WL_BRIDGE 1
#include "../../../src/thvm.c"

// Cached libData so callbacks (e.g. NumericArray disown) can reach
// WolframLibrary functions without the original call context.  Set
// once by WolframLibrary_initialize; stable for the rest of the
// session.
static WolframLibraryData CACHED_LIB_DATA = NULL;

// Callback used when a tensor backed by a Shared NumericArray is
// released: disown the handle so WL can reclaim its memory.
static void release_numeric_array(void *handle) {
  if (!CACHED_LIB_DATA || !handle) return;
  CACHED_LIB_DATA->numericarrayLibraryFunctions->MNumericArray_disown((MNumericArray)handle);
}

// Manager for the "ExternPin" managed-library-expression family.
// WL fires mode=1 when a fresh handle is created and mode=0 when
// the handle has no remaining references and is being collected.
// The collection signal is what makes WL's standard GC release
// the corresponding C-side pin without explicit user action.
static void extern_pin_manager(WolframLibraryData libData, mbool mode, mint id) {
  (void)libData;
  if (mode == 0) extern_pin_handle_drop((u64)id);
}

EXTERN_C DLLEXPORT mint WolframLibrary_getVersion(void) {
  return WolframLibraryVersion;
}

// Manager for ConnectLibraryCallbackFunction["thvm_pri_cb", cf].  Stores
// the framework-assigned callback id so WL can pair it with a slot via
// thvm_wl_pri_last_cb_id[].
static mint LAST_PRI_CB_ID = 0;

static mbool pri_cb_manager(WolframLibraryData libData, mint id, MTensor data) {
  (void)libData; (void)data;
  LAST_PRI_CB_ID = id;
  return True;
}

EXTERN_C DLLEXPORT int WolframLibrary_initialize(WolframLibraryData libData) {
  CACHED_LIB_DATA = libData;
  libData->registerLibraryExpressionManager("ExternPin", extern_pin_manager);
  libData->registerLibraryCallbackManager("thvm_pri_cb", pri_cb_manager);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT void WolframLibrary_uninitialize(WolframLibraryData libData) {
  (void)libData;
  if (HEAP != NULL) {
    thvm_free();
  }
}

// === lifecycle ===
EXTERN_C DLLEXPORT int thvm_wl_init(WolframLibraryData libData, mint argc,
                                    MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  if (HEAP != NULL) {
    thvm_free();
  }
  thvm_init();
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_free(WolframLibraryData libData, mint argc,
                                    MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  if (HEAP != NULL) {
    thvm_free();
  }
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_reset(WolframLibraryData libData, mint argc,
                                     MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  if (HEAP == NULL) {
    thvm_init();
  } else {
    memset(HEAP, 0, HEAP_CAP * sizeof(Term));
    HEAP_NEXT = 0;
    WNF_S_POS = 0;
    ITRS      = 0;
  }
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

// === term packing ===
EXTERN_C DLLEXPORT int thvm_wl_term_new(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u8  sub = (u8) MArgument_getInteger(args[0]);
  u8  tag = (u8) MArgument_getInteger(args[1]);
  u32 ext = (u32)MArgument_getInteger(args[2]);
  u64 val = (u64)MArgument_getInteger(args[3]);
  // No automatic LAM_ERA_MASK injection here -- this entry point is
  // also used by HeapInitialize's packCell to rebuild snapshot cells,
  // where val may refer to a book loc (not yet populated in the dyn
  // heap), and lam_seal_ext would walk the wrong cells.  WL's TLam
  // helper wires the mask via the dedicated thvm_wl_lam_seal_ext FFI
  // after the body is installed.
  Term t = term_new(sub, tag, ext, val);
  MArgument_setInteger(res, (mint)t);
  return LIBRARY_NO_ERROR;
}

// Compute the LAM ext for a freshly-constructed LAM whose body has
// been installed at HEAP[lam_loc].  Called from WL's TLam helper
// after THeapSet[loc, body], before packTerm seals the LAM.
//
// Today this calls `lam_seal_ext` which only sets LAM_ERA_MASK on
// 0-use binders; non-linear bodies get NO automatic DUP chain.
//
// IC-native auto-dup is implemented in src/lam/auto_dup.c
// (`lam_seal_ext_with_auto_dup`) and tested by tests/test_auto_dup.c
// for simple atomic non-linear use (x+x with NUM args, etc.).  The
// generic DUP-NOD commute rules for APP / OP2 / MAT live in
// src/interact/dup_{app,op2,mat}.c, so the runtime can structurally
// distribute DUPs through compound nodes.
//
// Wiring auto-dup as the WL default is gated on Levy-optimal sharing
// (or a selective skip for atomic non-linear binders): naive
// structural DUP commute through deep recursive bodies (Lazy.wl's
// splits TDef) blows up exponentially without optimality.  Until
// then, Lazy.wl uses linearity-friendly specialized helpers and
// callers that want auto-dup explicitly call
// `lam_seal_ext_with_auto_dup` from C.
EXTERN_C DLLEXPORT int thvm_wl_lam_seal_ext(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u64 lam_loc  = (u64)MArgument_getInteger(args[0]);
  u32 base_ext = (u32)MArgument_getInteger(args[1]);
  u32 sealed   = lam_seal_ext_with_auto_dup(lam_loc, base_ext);
  MArgument_setInteger(res, (mint)sealed);
  return LIBRARY_NO_ERROR;
}

// Drops a Term's pin from the EXTERN_PINNED_TERMS table.  Mostly
// superseded by managed-expression auto-unpin (see
// thvm_wl_extern_pin_associate); kept for callers that want to
// release a pin explicitly without dropping the WL wrapper.
EXTERN_C DLLEXPORT int thvm_wl_term_unpin(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  extern_unpin_term(t);
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

// Binds a fresh ManagedLibraryExpression["ExternPin"] handle id to
// the Term it should keep pinned.  When WL's GC eventually
// collects the handle, extern_pin_manager fires and the pin
// drops -- standard Wolfram-host lifetime tracking.
EXTERN_C DLLEXPORT int thvm_wl_extern_pin_associate(WolframLibraryData libData,
                                                    mint argc, MArgument *args,
                                                    MArgument res) {
  (void)libData; (void)argc;
  mint id = MArgument_getInteger(args[0]);
  Term t  = (Term)MArgument_getInteger(args[1]);
  if (id >= 0) extern_pin_handle_set((u64)id, t);
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_extern_pin_count(WolframLibraryData libData,
                                                mint argc, MArgument *args,
                                                MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)EXTERN_PINNED_TERMS_LEN);
  return LIBRARY_NO_ERROR;
}

// Look up the current encoded Term for a pin handle id.  WL's
// TTerm wrapper caches the raw Term integer at construction time;
// after a copying GC moves the underlying heap loc, the cached raw
// is stale.  ttermRaw refreshes via this accessor before any C
// bridge call so downstream consumers always see the post-GC loc.
EXTERN_C DLLEXPORT int thvm_wl_extern_pin_handle_get(WolframLibraryData libData,
                                                     mint argc,
                                                     MArgument *args,
                                                     MArgument res) {
  (void)libData; (void)argc;
  mint id = MArgument_getInteger(args[0]);
  if (id < 0) {
    MArgument_setInteger(res, 0);
    return LIBRARY_NO_ERROR;
  }
  MArgument_setInteger(res, (mint)extern_pin_handle_get((u64)id));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_term_tag(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)term_tag(t));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_term_ext(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)term_ext(t));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_term_val(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)term_val(t));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_term_sub(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)term_sub_get(t));
  return LIBRARY_NO_ERROR;
}

// === heap ===
EXTERN_C DLLEXPORT int thvm_wl_heap_pos(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)HEAP_NEXT);
  return LIBRARY_NO_ERROR;
}

// Lower bound of the active heap region.  When the Cheney GC has
// swapped, live cells live in [gc_from_start(), HEAP_NEXT); WL
// iterators that walk the heap use this as the lower bound.
// Returns 0 when GC is disabled (single-region semantics).
EXTERN_C DLLEXPORT int thvm_wl_heap_base(WolframLibraryData libData,
                                         mint argc, MArgument *args,
                                         MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, gc_enabled() ? (mint)gc_from_start() : 0);
  return LIBRARY_NO_ERROR;
}

// Manually trigger a Cheney collection.  Returns the new HEAP_NEXT
// (= live cell count after evacuation).  Roots beyond the side
// tables (extern pins, DEFS, KernelEntries, WNF_LAST_STACK) are
// collected internally; this entry point is for tests + diagnostics.
EXTERN_C DLLEXPORT int thvm_wl_gc_collect(WolframLibraryData libData,
                                          mint argc, MArgument *args,
                                          MArgument res) {
  (void)libData; (void)argc; (void)args;
  gc_collect(NULL, 0);
  MArgument_setInteger(res, (mint)HEAP_NEXT);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_gc_count(WolframLibraryData libData,
                                        mint argc, MArgument *args,
                                        MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)gc_count());
  return LIBRARY_NO_ERROR;
}

// === JIT capture / replay (Phase 7 of the tinygrad-parity arc) ===

// jit_capture_begin / end / drop / replay / op_count primitives are
// in src/jit/capture.c.  Bridge surface returns 1-indexed slot ids
// to WL; slot 0 means "no slot available" and is returned on error.

EXTERN_C DLLEXPORT int thvm_wl_jit_capture_begin(WolframLibraryData libData,
                                                 mint argc, MArgument *args,
                                                 MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)jit_capture_begin());
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_jit_capture_end(WolframLibraryData libData,
                                               mint argc, MArgument *args,
                                               MArgument res) {
  (void)libData; (void)argc; (void)args;
  jit_capture_end();
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_jit_capture_end_result(WolframLibraryData libData,
                                                      mint argc, MArgument *args,
                                                      MArgument res) {
  (void)libData; (void)argc;
  Term root = (Term)MArgument_getInteger(args[0]);
  jit_capture_end_with_result(root);
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_jit_capture_drop(WolframLibraryData libData,
                                                mint argc, MArgument *args,
                                                MArgument res) {
  (void)libData; (void)argc;
  u32 slot = (u32)MArgument_getInteger(args[0]);
  jit_capture_drop(slot);
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_jit_capture_op_count(WolframLibraryData libData,
                                                    mint argc, MArgument *args,
                                                    MArgument res) {
  (void)libData; (void)argc;
  u32 slot = (u32)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)jit_capture_op_count(slot));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_jit_capture_ops(WolframLibraryData libData,
                                               mint argc, MArgument *args,
                                               MArgument res) {
  (void)argc;
  u32 slot = (u32)MArgument_getInteger(args[0]);
  u32 n    = jit_capture_op_count(slot);
  u32 row_width = JIT_CAPTURE_EXPORT_ROW_WIDTH;
  mint dims[1] = { (mint)(2 + n * row_width) };
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  u64 *tmp = (u64 *)calloc((size_t)dims[0], sizeof(u64));
  if (tmp == NULL) {
    dst[0] = 0;
    dst[1] = row_width;
    MArgument_setMTensor(res, out);
    return LIBRARY_NO_ERROR;
  }
  u32 written = jit_capture_export_ops(slot, tmp, (u32)dims[0]);
  for (u32 i = 0; i < (u32)dims[0]; i++) {
    dst[i] = i < written ? (mint)tmp[i] : 0;
  }
  free(tmp);
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_jit_replay(WolframLibraryData libData,
                                          mint argc, MArgument *args,
                                          MArgument res) {
  (void)libData; (void)argc;
  u32 slot = (u32)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)jit_replay(slot));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_heap_alloc(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u64 size = (u64)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)heap_alloc(size));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_heap_read(WolframLibraryData libData, mint argc,
                                         MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u64 loc = (u64)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)heap_read(loc));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_heap_set(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u64  loc = (u64) MArgument_getInteger(args[0]);
  Term t   = (Term)MArgument_getInteger(args[1]);
  heap_set(loc, t);
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

// === reduce / stats ===
EXTERN_C DLLEXPORT int thvm_wl_wnf(WolframLibraryData libData, mint argc,
                                   MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  Term r = wnf(t);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// Full normal-form reduction: sweeps the heap and fires every
// redex via redex_fire (see src/wnf/nf.c).  Used by callers that
// want every chain-rule-produced UOp surfaced before materialize
// (otherwise wnf's WHNF discipline leaves grads nested inside
// elementwise wrappers unfired).  Excludes TAG_REF / TAG_ALO from
// eager firing -- recursive named definitions would non-
// terminatingly unfold.
EXTERN_C DLLEXPORT int thvm_wl_nf(WolframLibraryData libData, mint argc,
                                  MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  // Drive nf first so heap-resident UOps / GRADs / kernels fire, then
  // run cnf at the surviving root so any DP wrappers left at the head
  // (Levy-opaque under wnf since the Phase 1+2 readback split) get
  // resolved before the user observes the term.
  Term r = nf(t);
  r = cnf(r);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// thvm_wl_cnf -- collapsed-normal-form readback exposed to WL as TCnf.
// Lifts SUPs to the top recursively; fires plain DUP-XXX during the
// walk.  Useful when callers want a DP-free reading of a term without
// the heap-wide nf sweep.
EXTERN_C DLLEXPORT int thvm_wl_cnf(WolframLibraryData libData, mint argc,
                                   MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  Term r = cnf(t);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// thvm_wl_collapse -- enumerate SUP-tree leaves of a term.
// Walks `t` recursively: at each step cnfs to surface a head;
// SUP -> recurse into both branches; ERA -> drop; otherwise -> emit.
// Returns the leaves as a 1-D Integer MTensor (each entry a packed
// Term).  Cap defaults to 65536; callers can pass a smaller cap if
// they only need the first N hits.  See src/collapse/_.c.
EXTERN_C DLLEXPORT int thvm_wl_collapse(WolframLibraryData libData,
                                        mint argc, MArgument *args,
                                        MArgument res) {
  (void)argc;
  Term t   = (Term)MArgument_getInteger(args[0]);
  u64  cap = (u64) MArgument_getInteger(args[1]);
  if (cap == 0 || cap > (1u << 20)) cap = (1u << 16);
  Term *buf = (Term *)malloc(cap * sizeof(Term));
  if (buf == NULL) {
    return LIBRARY_FUNCTION_ERROR;
  }
  u64 n = thvm_collapse(t, buf, cap);
  MTensor out;
  mint dims[1] = { (mint)n };
  int err = libData->MTensor_new(MType_Integer, 1, dims, &out);
  if (err != LIBRARY_NO_ERROR) {
    free(buf);
    return err;
  }
  mint *dst = libData->MTensor_getIntegerData(out);
  for (u64 i = 0; i < n; i++) dst[i] = (mint)buf[i];
  free(buf);
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

// Step-bounded reduce.  max_steps == 0 == unbounded (same as wnf).
EXTERN_C DLLEXPORT int thvm_wl_wnf_n(WolframLibraryData libData, mint argc,
                                     MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t          = (Term)MArgument_getInteger(args[0]);
  u64  max_steps  = (u64) MArgument_getInteger(args[1]);
  Term r = wnf_n(t, max_steps);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_stack_size(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)WNF_LAST_STACK_LEN);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_stack_get(WolframLibraryData libData, mint argc,
                                         MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32 i = (u32)MArgument_getInteger(args[0]);
  Term t = (i < WNF_LAST_STACK_LEN) ? WNF_LAST_STACK[i] : 0;
  MArgument_setInteger(res, (mint)t);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_itrs(WolframLibraryData libData, mint argc,
                                    MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)ITRS);
  return LIBRARY_NO_ERROR;
}

// === hot-path counters ===
// Snapshot the per-context HotCounters block into a {Integer, 1}
// MTensor.  Order matches `hot_counters_snapshot` in
// `src/instrument/hot_counters.c` and `$hotCounterNames` in
// `wl/THVMLink/Kernel/Profile.wl` -- keep them in sync.
EXTERN_C DLLEXPORT int thvm_wl_hot_counters(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)argc; (void)args;
  u64 buf[HOT_COUNTER_COUNT];
  hot_counters_snapshot(buf);
  mint dims[1] = {(mint)HOT_COUNTER_COUNT};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  for (u32 i = 0; i < HOT_COUNTER_COUNT; i++) dst[i] = (mint)buf[i];
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_hot_counters_reset(WolframLibraryData libData, mint argc,
                                                  MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  hot_counters_reset();
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

// === multicomputation reduction trace ===
// WL surface for the MultiEvent log (src/instrument/multi.c, gated by
// THVM_TRACE -- which the WL dylib *is* built with; see the Makefile).
// thvm_wl_multi_trace_supported returns 1 in a trace build, 0 in the
// stub build below so the WL side (Multicomputation.wl) can degrade
// gracefully.  Event rows carry {id, rule, family, term_a, term_b,
// delta_label, consumed[0], consumed[1]}; thvm_wl_multi_trace_snapshot
// returns the whole log as a {Integer, 2} MTensor (n x 8).  Names
// come from thvm_wl_multi_rule_name / thvm_wl_multi_family_name (the
// C side owns the RULE_* / MULTI_* -> string tables).
// MULTI_WIRE_NONE (M1 sentinel for "no producer recorded") shows up
// as 0xFFFFFFFF in the consumed columns and is translated to -1 by
// the WL wrapper.
#ifdef THVM_TRACE

EXTERN_C DLLEXPORT int thvm_wl_multi_trace_supported(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_multi_trace_init(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  multi_trace_init((u64)MArgument_getInteger(args[0]));   // 0 -> default cap
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_multi_trace_reset(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  multi_trace_reset();
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_multi_trace_free(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  multi_trace_free();
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

// Set the per-context runtime flag (0 = off, 1 = recording).
EXTERN_C DLLEXPORT int thvm_wl_multi_trace_set(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  CURRENT_CTX->trace = (u8)(MArgument_getInteger(args[0]) != 0);
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_multi_trace_count(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)multi_trace_count());
  return LIBRARY_NO_ERROR;
}

// {Integer, 2} MTensor, one row per event: {id, rule, family,
// term_a, term_b, delta_label, consumed[0], consumed[1]}.
// MULTI_WIRE_NONE is widened to a signed i64 -1 so WL sees `-1` rather
// than the raw 4294967295 (= u32 sentinel).
EXTERN_C DLLEXPORT int thvm_wl_multi_trace_snapshot(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)argc; (void)args;
  u64  n = multi_trace_count();
  MTensor out;
  mint dims[2] = { (mint)n, 8 };
  int err = libData->MTensor_new(MType_Integer, 2, dims, &out);
  if (err != LIBRARY_NO_ERROR) return err;
  mint *dst = libData->MTensor_getIntegerData(out);
  for (u64 i = 0; i < n; i++) {
    const MultiEvent *e = multi_trace_get(i);
    dst[8*i + 0] = (mint)e->id;
    dst[8*i + 1] = (mint)e->rule;
    dst[8*i + 2] = (mint)e->family;
    dst[8*i + 3] = (mint)e->term_a;
    dst[8*i + 4] = (mint)e->term_b;
    dst[8*i + 5] = (mint)e->delta_label;
    dst[8*i + 6] = (e->consumed[0] == MULTI_WIRE_NONE)
                   ? (mint)-1 : (mint)e->consumed[0];
    dst[8*i + 7] = (e->consumed[1] == MULTI_WIRE_NONE)
                   ? (mint)-1 : (mint)e->consumed[1];
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_multi_rule_name(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  MArgument_setUTF8String(res, (char *)multi_rule_name((u8)MArgument_getInteger(args[0])));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_multi_family_name(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  MArgument_setUTF8String(res, (char *)multi_family_name((u8)MArgument_getInteger(args[0])));
  return LIBRARY_NO_ERROR;
}

/* Snapshot multi_wire_prov[0 .. HEAP_NEXT) so the host can derive a
   per-event "produced" set: locs whose wire_prov[loc] == E.id are
   the cells E wrote during its rewrite.  Sentinel MULTI_WIRE_NONE
   (no producer recorded) is translated to -1 for WL ergonomics.
   Must be called BEFORE multi_trace_free wipes the table. */
EXTERN_C DLLEXPORT int thvm_wl_multi_wire_prov_snapshot(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)argc; (void)args;
  u64 n = CURRENT_CTX->heap_next;
  MTensor out;
  mint dims[1] = { (mint)n };
  int err = libData->MTensor_new(MType_Integer, 1, dims, &out);
  if (err != LIBRARY_NO_ERROR) return err;
  mint *dst = libData->MTensor_getIntegerData(out);
  for (u64 i = 0; i < n; i++) {
    u32 v = multi_wire_prov_get(i);
    dst[i] = (v == MULTI_WIRE_NONE) ? (mint)-1 : (mint)v;
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

#else  // !THVM_TRACE -- stubs so the dylib still links and the WL
       // surface reports "trace not compiled in" instead of failing
       // to load.

EXTERN_C DLLEXPORT int thvm_wl_multi_trace_supported(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT int thvm_wl_multi_trace_init(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT int thvm_wl_multi_trace_reset(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT int thvm_wl_multi_trace_free(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT int thvm_wl_multi_trace_set(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT int thvm_wl_multi_trace_count(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT int thvm_wl_multi_trace_snapshot(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)argc; (void)args;
  MTensor out;
  mint dims[2] = { 0, 8 };  // matches the trace-build snapshot shape
  int err = libData->MTensor_new(MType_Integer, 2, dims, &out);
  if (err != LIBRARY_NO_ERROR) return err;
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT int thvm_wl_multi_rule_name(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setUTF8String(res, (char *)"RULE?");
  return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT int thvm_wl_multi_family_name(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setUTF8String(res, (char *)"FAMILY?");
  return LIBRARY_NO_ERROR;
}
EXTERN_C DLLEXPORT int thvm_wl_multi_wire_prov_snapshot(WolframLibraryData libData,
        mint argc, MArgument *args, MArgument res) {
  (void)argc; (void)args;
  MTensor out;
  mint dims[1] = { 0 };
  int err = libData->MTensor_new(MType_Integer, 1, dims, &out);
  if (err != LIBRARY_NO_ERROR) return err;
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

#endif // THVM_TRACE

// === redex enumeration / single-redex firing ===
// Pattern matches TStack: snapshot into a static buffer, then expose
// length + indexed get.  thvm_wl_redex_snapshot takes a single root
// (0 = "no root, heap scan only") and returns the redex count.
//
// thvm_wl_interact takes a redex Term and returns the rewrite result
// (0 if the input wasn't actually a redex -- WL converts to Failure).

#define REDEX_BUF_CAP 4096
static Term REDEX_BUF[REDEX_BUF_CAP];
static u32  REDEX_BUF_N = 0;

EXTERN_C DLLEXPORT int thvm_wl_redex_snapshot(WolframLibraryData libData, mint argc,
                                              MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  // args[0] is a rank-1 Integer MTensor of root Terms (may be empty
  // for pure heap-scan).
  MTensor t = MArgument_getMTensor(args[0]);
  mint n    = libData->MTensor_getFlattenedLength(t);
  mint *src = libData->MTensor_getIntegerData(t);
  Term roots[64];
  u32  n_roots = (n > 64) ? 64 : (u32)n;
  for (u32 i = 0; i < n_roots; i++) roots[i] = (Term)src[i];
  REDEX_BUF_N = redex_enumerate(roots, n_roots, REDEX_BUF, REDEX_BUF_CAP);
  MArgument_setInteger(res, (mint)REDEX_BUF_N);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_redex_get(WolframLibraryData libData, mint argc,
                                         MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32 i = (u32)MArgument_getInteger(args[0]);
  Term t = (i < REDEX_BUF_N) ? REDEX_BUF[i] : 0;
  MArgument_setInteger(res, (mint)t);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_interact(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term redex = (Term)MArgument_getInteger(args[0]);
  Term r = redex_fire(redex);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// === step session ===
// Persistent across calls.  TStepBegin / TStep / TStepFresh /
// TStepEnd let the WL stepper amortise the heap scan over many
// fires.  thvm_wl_step_begin runs one full enumerate to seed the
// initial redex set; thvm_wl_step_fire does parent-slot patching
// via the inverse index in O(uses-of-redex); thvm_wl_step_fresh
// returns redex-status flips since the previous call.

EXTERN_C DLLEXPORT int thvm_wl_step_begin(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  MTensor t = MArgument_getMTensor(args[0]);
  mint n    = libData->MTensor_getFlattenedLength(t);
  mint *src = libData->MTensor_getIntegerData(t);
  Term roots[64];
  u32  n_roots = (n > 64) ? 64 : (u32)n;
  for (u32 i = 0; i < n_roots; i++) roots[i] = (Term)src[i];
  u32 seed = redex_step_attach(roots, n_roots);
  MArgument_setInteger(res, (mint)seed);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_step_fire(WolframLibraryData libData, mint argc,
                                         MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term redex = (Term)MArgument_getInteger(args[0]);
  Term r = redex_step_fire(redex);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_step_fresh(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  REDEX_BUF_N = redex_step_drain_fresh(REDEX_BUF, REDEX_BUF_CAP);
  MArgument_setInteger(res, (mint)REDEX_BUF_N);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_step_end(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  redex_step_detach();
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

// === tensors ===
// TTensor constructors, inspection, refcount hooks.  Shapes and data
// arrive as Integer / Real arrays; we pack into Shape / dtype bits
// on the C side and return a TAG_TEN-tagged term (packed Term value).

EXTERN_C DLLEXPORT int thvm_wl_tensor_alloc(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  // args[0] = dtype code; args[1] = shape MTensor (rank-1 integers).
  mint dtype   = MArgument_getInteger(args[0]);
  MTensor sh   = MArgument_getMTensor(args[1]);
  mint *dims   = libData->MTensor_getIntegerData(sh);
  mint rank    = libData->MTensor_getFlattenedLength(sh);
  Shape shape;
  shape.ndim = (u32)rank;
  for (mint i = 0; i < rank && i < MAX_DIM; i++) shape.dims[i] = (u32)dims[i];
  for (mint i = rank; i < MAX_DIM; i++)          shape.dims[i] = 0;
  u32 id = tensor_alloc(CURRENT_BACKEND, shape, (u32)dtype);
  // Return the full TAG_TEN term (packed), so WL-side TTerm wrappers
  // can inspect tag/ext/val uniformly with other terms.
  Term t = term_new(0, TAG_TEN, (u32)dtype, id);
  MArgument_setInteger(res, (mint)t);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_tensor_write(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  mint id      = MArgument_getInteger(args[0]);
  MTensor data = MArgument_getMTensor(args[1]);
  mint n       = libData->MTensor_getFlattenedLength(data);
  TenDesc *d   = &TENS[id];
  // WL passes Real -> double / Integer -> mint; both go through a
  // per-dtype convert+pack into the tensor's native storage.
  if (dtype_is_float(d->dtype)) {
    double *src = libData->MTensor_getRealData(data);
    if (d->dtype == DT_FP32) {
      f32 *tmp = (f32 *)malloc((size_t)n * sizeof(f32));
      for (mint i = 0; i < n; i++) tmp[i] = (f32)src[i];
      d->backend->buf_write(d->buf_id, tmp, (u64)n * sizeof(f32));
      free(tmp);
    } else if (d->dtype == DT_FP64) {
      // Already double-precision -- pass through.
      d->backend->buf_write(d->buf_id, src, (u64)n * sizeof(f64));
    } else if (d->dtype == DT_FP16 || d->dtype == DT_BF16) {
      // Promote double -> f32 -> narrow float; lane.c handles the
      // round-to-nearest-even bit pack.
      f32 *tmp32 = (f32 *)malloc((size_t)n * sizeof(f32));
      for (mint i = 0; i < n; i++) tmp32[i] = (f32)src[i];
      u16 *tmp16 = (u16 *)malloc((size_t)n * sizeof(u16));
      from_fp32_lane(tmp16, d->dtype, tmp32, (u32)n);
      d->backend->buf_write(d->buf_id, tmp16, (u64)n * sizeof(u16));
      free(tmp32);
      free(tmp16);
    } else if (d->dtype == DT_FP8E4M3 || d->dtype == DT_FP8E5M2) {
      f32 *tmp32 = (f32 *)malloc((size_t)n * sizeof(f32));
      for (mint i = 0; i < n; i++) tmp32[i] = (f32)src[i];
      u8 *tmp8 = (u8 *)malloc((size_t)n * sizeof(u8));
      from_fp32_lane(tmp8, d->dtype, tmp32, (u32)n);
      d->backend->buf_write(d->buf_id, tmp8, (u64)n * sizeof(u8));
      free(tmp32);
      free(tmp8);
    } else {
      fprintf(stderr, "tensor_write: float dtype %u not yet wired\n", d->dtype);
      return LIBRARY_FUNCTION_ERROR;
    }
  } else if (dtype_is_int(d->dtype) || dtype_is_bool(d->dtype)) {
    mint *src = libData->MTensor_getIntegerData(data);
    u64 nbytes = dtype_storage_bytes(d->dtype, (u64)n);
    void *tmp = malloc((size_t)nbytes);
    switch (d->dtype) {
      case DT_BOOL:   { u8  *t8  = (u8  *)tmp; for (mint i = 0; i < n; i++) t8 [i] = (u8 )(src[i] & 1); break; }
      case DT_INT8:   { i8  *t8  = (i8  *)tmp; for (mint i = 0; i < n; i++) t8 [i] = (i8 )src[i]; break; }
      case DT_UINT8:  { u8  *t8  = (u8  *)tmp; for (mint i = 0; i < n; i++) t8 [i] = (u8 )src[i]; break; }
      case DT_INT16:  { i16 *t16 = (i16 *)tmp; for (mint i = 0; i < n; i++) t16[i] = (i16)src[i]; break; }
      case DT_UINT16: { u16 *t16 = (u16 *)tmp; for (mint i = 0; i < n; i++) t16[i] = (u16)src[i]; break; }
      case DT_INT32:  { i32 *t32 = (i32 *)tmp; for (mint i = 0; i < n; i++) t32[i] = (i32)src[i]; break; }
      case DT_UINT32: { u32 *t32 = (u32 *)tmp; for (mint i = 0; i < n; i++) t32[i] = (u32)src[i]; break; }
      case DT_INT64:  { i64 *t64 = (i64 *)tmp; for (mint i = 0; i < n; i++) t64[i] = (i64)src[i]; break; }
      case DT_UINT64: { u64 *t64 = (u64 *)tmp; for (mint i = 0; i < n; i++) t64[i] = (u64)src[i]; break; }
      default:
        free(tmp);
        fprintf(stderr, "tensor_write: int dtype %u unsupported\n", d->dtype);
        return LIBRARY_FUNCTION_ERROR;
    }
    d->backend->buf_write(d->buf_id, tmp, nbytes);
    free(tmp);
  }
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

// tensor_read returns a NumericArray of the tensor's dtype + full
// multi-dimensional shape.  NumericArray maps directly onto the
// C-side buffer layout (no f32 -> f64 conversion), so a Real32
// tensor round-trips back to a Real32 NumericArray with no loss
// or copy beyond the single memcpy into NumericArray-owned storage.
EXTERN_C DLLEXPORT int thvm_wl_tensor_read(WolframLibraryData libData, mint argc,
                                           MArgument *args, MArgument res) {
  (void)argc;
  mint     id = MArgument_getInteger(args[0]);
  TenDesc *d  = &TENS[id];

  mint dims[MAX_DIM];
  mint rank = (mint)d->view.shape.ndim;
  for (mint i = 0; i < rank; i++) dims[i] = (mint)d->view.shape.dims[i];
  // Packed nibble dtypes: collapse to a flat byte count for the
  // NumericArray.  WL reconstructs the logical shape via
  // TTensorShape[]; the byte count = ceil(numel/2).
  if (d->dtype == DT_INT4 || d->dtype == DT_UINT4) {
    rank = 1;
    dims[0] = (mint)dtype_storage_bytes(d->dtype, d->view.numel);
  }

  numericarray_data_t t;
  switch (d->dtype) {
    case DT_BOOL:   t = MNumericArray_Type_UBit8;   break;
    case DT_INT8:   t = MNumericArray_Type_Bit8;    break;
    case DT_UINT8:  t = MNumericArray_Type_UBit8;   break;
    case DT_INT16:  t = MNumericArray_Type_Bit16;   break;
    case DT_UINT16: t = MNumericArray_Type_UBit16;  break;
    case DT_INT32:  t = MNumericArray_Type_Bit32;   break;
    case DT_UINT32: t = MNumericArray_Type_UBit32;  break;
    case DT_INT64:  t = MNumericArray_Type_Bit64;   break;
    case DT_UINT64: t = MNumericArray_Type_UBit64;  break;
    case DT_FP32:   t = MNumericArray_Type_Real32;  break;
    case DT_FP64:   t = MNumericArray_Type_Real64;  break;
    // f16 / bf16 ride on raw u16 bytes; the WL surface decodes via
    // TFP16ToReal / TBf16ToReal helpers in Tensor.wl.
    case DT_FP16:
    case DT_BF16:   t = MNumericArray_Type_UBit16;  break;
    // fp8 rides on raw u8 bytes; decoded via TFP8E4M3ToReal etc.
    case DT_FP8E4M3:
    case DT_FP8E5M2: t = MNumericArray_Type_UBit8;  break;
    // int4 / uint4: 2 nibbles per byte, packed.  Read returns the
    // raw byte buffer; WL helpers (TUnpackInt4 etc.) walk the
    // logical numel.  Shape carries the LOGICAL nibble count, so
    // the returned NumericArray has length ceil(numel/2) -- callers
    // must reconstruct the source numel from TTensorShape[].
    case DT_INT4:
    case DT_UINT4:   t = MNumericArray_Type_UBit8;  break;
    default:
      fprintf(stderr, "tensor_read: dtype %u (%s) not yet supported\n",
              d->dtype, dtype_name(d->dtype));
      return LIBRARY_FUNCTION_ERROR;
  }

  MNumericArray out;
  libData->numericarrayLibraryFunctions->MNumericArray_new(t, rank, dims, &out);
  void *dst = libData->numericarrayLibraryFunctions->MNumericArray_getData(out);
  u64   nbytes = dtype_storage_bytes(d->dtype, d->view.numel);
  d->backend->buf_read(d->buf_id, dst, nbytes);

  MArgument_setMNumericArray(res, out);
  return LIBRARY_NO_ERROR;
}

// Build a tensor by *sharing* the bytes of a NumericArray passed in
// with "Shared" passing mode.  The tensor holds the NumericArray alive
// (via MNumericArray_disown on release) and reads its buffer pointer
// directly -- zero copy on the CPU backend.
EXTERN_C DLLEXPORT int thvm_wl_tensor_from_na(WolframLibraryData libData, mint argc,
                                              MArgument *args, MArgument res) {
  (void)argc;
  MNumericArray na = MArgument_getMNumericArray(args[0]);
  const struct st_WolframNumericArrayLibrary_Functions *naf
      = libData->numericarrayLibraryFunctions;

  numericarray_data_t t       = naf->MNumericArray_getType(na);
  mint                 rank    = naf->MNumericArray_getRank(na);
  mint const          *naDims  = naf->MNumericArray_getDimensions(na);
  mint                 numel   = naf->MNumericArray_getFlattenedLength(na);
  void                *naData  = naf->MNumericArray_getData(na);

  u32 dtype;
  switch (t) {
    case MNumericArray_Type_Real32:  dtype = DT_FP32;   break;
    case MNumericArray_Type_Real64:  dtype = DT_FP64;   break;
    case MNumericArray_Type_Bit8:    dtype = DT_INT8;   break;
    case MNumericArray_Type_UBit8:   dtype = DT_UINT8;  break;
    case MNumericArray_Type_Bit16:   dtype = DT_INT16;  break;
    case MNumericArray_Type_UBit16:  dtype = DT_UINT16; break;
    case MNumericArray_Type_Bit32:   dtype = DT_INT32;  break;
    case MNumericArray_Type_UBit32:  dtype = DT_UINT32; break;
    case MNumericArray_Type_Bit64:   dtype = DT_INT64;  break;
    case MNumericArray_Type_UBit64:  dtype = DT_UINT64; break;
    default:
      fprintf(stderr, "tensor_from_na: unsupported NumericArray type %d\n", (int)t);
      return LIBRARY_FUNCTION_ERROR;
  }

  // Build the TenDesc manually so we can point its buf_id at an
  // external CPU buffer instead of a freshly malloc'd one.
  if (TENS_NEXT >= TENS_CAP) {
    fprintf(stderr, "tensor_from_na: out of descriptor slots\n");
    return LIBRARY_FUNCTION_ERROR;
  }
  u32 id = TENS_NEXT++;
  TenDesc *d = &TENS[id];
  Shape shape;
  shape.ndim = (u32)rank;
  for (mint i = 0; i < rank && i < MAX_DIM; i++) shape.dims[i] = (u32)naDims[i];
  for (mint i = rank; i < MAX_DIM; i++)          shape.dims[i] = 0;
  d->dtype    = dtype;
  d->refcount = 1;
  d->view     = view_create(shape);
  d->backend  = CURRENT_BACKEND;
  u64 nbytes = dtype_storage_bytes(dtype, (u64)numel);
  if (CURRENT_BACKEND == &CPU_BACKEND) {
    // CPU fast path: zero-copy reference into the NumericArray's
    // bytes; release_numeric_array drops the WL handle when the
    // underlying buffer hits refcount 0.
    d->buf_id = cpu_buf_alloc_external(
        naData, nbytes, release_numeric_array, (void *)na);
  } else {
    // Other backends (Metal): allocate + memcpy.  The
    // NumericArray reference is released right after the copy --
    // the data lives in backend-owned storage.
    d->buf_id = CURRENT_BACKEND->buf_alloc(nbytes);
    CURRENT_BACKEND->buf_write(d->buf_id, naData, nbytes);
    libData->numericarrayLibraryFunctions->MNumericArray_disown(na);
  }

  Term term = term_new(0, TAG_TEN, dtype, id);
  MArgument_setInteger(res, (mint)term);
  return LIBRARY_NO_ERROR;
}

// Variant of tensor_from_na that overrides the inferred dtype from
// the NumericArray type and accepts an explicit logical shape.
// Used by f16 / bf16 / fp8 / int4 / uint4 where the WL surface
// carries the bytes in a generic Unsigned* NumericArray and the
// dtype tag is supplied separately.  For packed nibble dtypes the
// NumericArray has shape = ceil(numel/2) bytes (1D) but the logical
// shape is multi-dimensional and must be passed via shape_arg.
EXTERN_C DLLEXPORT int thvm_wl_tensor_from_na_typed(WolframLibraryData libData, mint argc,
                                                    MArgument *args, MArgument res) {
  (void)argc;
  MNumericArray na = MArgument_getMNumericArray(args[0]);
  mint dtype_arg   = MArgument_getInteger(args[1]);
  MTensor sh       = MArgument_getMTensor(args[2]);
  const struct st_WolframNumericArrayLibrary_Functions *naf
      = libData->numericarrayLibraryFunctions;

  mint *shape_dims  = libData->MTensor_getIntegerData(sh);
  mint  shape_rank  = libData->MTensor_getFlattenedLength(sh);
  void *naData      = naf->MNumericArray_getData(na);
  u32   dtype       = (u32)dtype_arg;

  if (TENS_NEXT >= TENS_CAP) {
    fprintf(stderr, "tensor_from_na_typed: out of descriptor slots\n");
    return LIBRARY_FUNCTION_ERROR;
  }
  u32 id = TENS_NEXT++;
  TenDesc *d = &TENS[id];
  Shape shape;
  shape.ndim = (u32)shape_rank;
  for (mint i = 0; i < shape_rank && i < MAX_DIM; i++) shape.dims[i] = (u32)shape_dims[i];
  for (mint i = shape_rank; i < MAX_DIM; i++)          shape.dims[i] = 0;
  d->dtype    = dtype;
  d->refcount = 1;
  d->view     = view_create(shape);
  d->backend  = CURRENT_BACKEND;
  u64 nbytes  = dtype_storage_bytes(dtype, d->view.numel);
  if (CURRENT_BACKEND == &CPU_BACKEND) {
    d->buf_id = cpu_buf_alloc_external(
        naData, nbytes, release_numeric_array, (void *)na);
  } else {
    d->buf_id = CURRENT_BACKEND->buf_alloc(nbytes);
    CURRENT_BACKEND->buf_write(d->buf_id, naData, nbytes);
    libData->numericarrayLibraryFunctions->MNumericArray_disown(na);
  }
  Term term = term_new(0, TAG_TEN, dtype, id);
  MArgument_setInteger(res, (mint)term);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_tensor_shape(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  mint     id = MArgument_getInteger(args[0]);
  TenDesc *d  = &TENS[id];
  mint n      = (mint)d->view.shape.ndim;
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, &n, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  for (mint i = 0; i < n; i++) dst[i] = (mint)d->view.shape.dims[i];
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_tensor_refcount(WolframLibraryData libData, mint argc,
                                               MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  mint id = MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)TENS[id].refcount);
  return LIBRARY_NO_ERROR;
}

// === UOp graph constructors ===
// Each returns a packed TAG_UOP term.  Shape / axis args come in as
// integer MTensors where relevant.

EXTERN_C DLLEXPORT int thvm_wl_uop_const(WolframLibraryData libData, mint argc,
                                         MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  mint    dtype = MArgument_getInteger(args[0]);
  mreal   value = MArgument_getReal   (args[1]);
  u32 bits;
  if (dtype_is_float((u32)dtype)) {
    // The KProgOp arg is u32, so all float constants pass through
    // an f32 intermediate at materialize time.  For DT_FP64 inputs
    // outside f32 range, the WL caller can fall back to a typed
    // tensor (TTensorCreate from a Real64 NumericArray).
    f32 v = (f32)value;
    memcpy(&bits, &v, sizeof(bits));
  } else {
    bits = (u32)(i32)value;
  }
  Term r = uop_const((u32)dtype, bits);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// Convert a packed UnsignedInteger16 NumericArray (raw f16 bytes) to
// a Real64 list.  Used by Tensor.wl's TFP16ToReal helper when reading
// f16 tensors back through the bridge.
EXTERN_C DLLEXPORT int thvm_wl_fp16_unpack(WolframLibraryData libData, mint argc,
                                           MArgument *args, MArgument res) {
  (void)argc;
  MNumericArray na = MArgument_getMNumericArray(args[0]);
  mint dtype_code  = MArgument_getInteger(args[1]);   // DT_FP16 or DT_BF16
  const struct st_WolframNumericArrayLibrary_Functions *naf
      = libData->numericarrayLibraryFunctions;
  mint n = naf->MNumericArray_getFlattenedLength(na);
  u16 *src = (u16 *)naf->MNumericArray_getData(na);
  MTensor out;
  libData->MTensor_new(MType_Real, 1, &n, &out);
  double *dst = libData->MTensor_getRealData(out);
  for (mint i = 0; i < n; i++) {
    f32 v = (dtype_code == DT_BF16) ? bf16_to_f32(src[i]) : fp16_to_f32(src[i]);
    dst[i] = (double)v;
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

// Pack a list of Integers (-8..7 for int4, 0..15 for uint4) into a
// UnsignedInteger8 NumericArray of packed nibbles.  Inverse unpacks
// back to a list of Integers (sign-extended for int4).  Logical
// numel is implicit in the source list length; storage byte count
// = ceil(numel/2).
EXTERN_C DLLEXPORT int thvm_wl_int4_pack(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)argc;
  MTensor data    = MArgument_getMTensor(args[0]);
  mint dtype_code = MArgument_getInteger(args[1]);
  mint n          = libData->MTensor_getFlattenedLength(data);
  mint *src       = libData->MTensor_getIntegerData(data);
  const struct st_WolframNumericArrayLibrary_Functions *naf
      = libData->numericarrayLibraryFunctions;
  mint nbytes     = (n + 1) / 2;
  MNumericArray out;
  naf->MNumericArray_new(MNumericArray_Type_UBit8, 1, &nbytes, &out);
  u8 *dst = (u8 *)naf->MNumericArray_getData(out);
  if (dtype_code == DT_INT4) {
    i8 *tmp = (i8 *)malloc((size_t)n);
    for (mint i = 0; i < n; i++) tmp[i] = (i8)src[i];
    pack_int4(dst, tmp, (u32)n);
    free(tmp);
  } else {
    u8 *tmp = (u8 *)malloc((size_t)n);
    for (mint i = 0; i < n; i++) tmp[i] = (u8)src[i];
    pack_uint4(dst, tmp, (u32)n);
    free(tmp);
  }
  MArgument_setMNumericArray(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_int4_unpack(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)argc;
  MNumericArray na = MArgument_getMNumericArray(args[0]);
  mint dtype_code  = MArgument_getInteger(args[1]);
  mint logical_n   = MArgument_getInteger(args[2]);   // logical nibble count
  const struct st_WolframNumericArrayLibrary_Functions *naf
      = libData->numericarrayLibraryFunctions;
  u8 *src = (u8 *)naf->MNumericArray_getData(na);
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, &logical_n, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  if (dtype_code == DT_INT4) {
    i8 *tmp = (i8 *)malloc((size_t)logical_n);
    unpack_int4(tmp, src, (u32)logical_n);
    for (mint i = 0; i < logical_n; i++) dst[i] = (mint)tmp[i];
    free(tmp);
  } else {
    u8 *tmp = (u8 *)malloc((size_t)logical_n);
    unpack_uint4(tmp, src, (u32)logical_n);
    for (mint i = 0; i < logical_n; i++) dst[i] = (mint)tmp[i];
    free(tmp);
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

// Pack a Real list into a UnsignedInteger8 NumericArray of raw fp8
// bytes; inverse below.  dtype_code argument selects e4m3 / e5m2.
EXTERN_C DLLEXPORT int thvm_wl_fp8_pack(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)argc;
  MTensor data    = MArgument_getMTensor(args[0]);
  mint dtype_code = MArgument_getInteger(args[1]);
  mint n          = libData->MTensor_getFlattenedLength(data);
  double *src     = libData->MTensor_getRealData(data);
  const struct st_WolframNumericArrayLibrary_Functions *naf
      = libData->numericarrayLibraryFunctions;
  MNumericArray out;
  naf->MNumericArray_new(MNumericArray_Type_UBit8, 1, &n, &out);
  u8 *dst = (u8 *)naf->MNumericArray_getData(out);
  for (mint i = 0; i < n; i++) {
    f32 v = (f32)src[i];
    dst[i] = (dtype_code == DT_FP8E5M2) ? f32_to_fp8e5m2(v) : f32_to_fp8e4m3(v);
  }
  MArgument_setMNumericArray(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_fp8_unpack(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)argc;
  MNumericArray na = MArgument_getMNumericArray(args[0]);
  mint dtype_code  = MArgument_getInteger(args[1]);
  const struct st_WolframNumericArrayLibrary_Functions *naf
      = libData->numericarrayLibraryFunctions;
  mint n  = naf->MNumericArray_getFlattenedLength(na);
  u8 *src = (u8 *)naf->MNumericArray_getData(na);
  MTensor out;
  libData->MTensor_new(MType_Real, 1, &n, &out);
  double *dst = libData->MTensor_getRealData(out);
  for (mint i = 0; i < n; i++) {
    f32 v = (dtype_code == DT_FP8E5M2) ? fp8e5m2_to_f32(src[i]) : fp8e4m3_to_f32(src[i]);
    dst[i] = (double)v;
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

// Pack a Real list into a UnsignedInteger16 NumericArray of raw f16
// or bf16 bytes.  Inverse of thvm_wl_fp16_unpack.
EXTERN_C DLLEXPORT int thvm_wl_fp16_pack(WolframLibraryData libData, mint argc,
                                         MArgument *args, MArgument res) {
  (void)argc;
  MTensor data    = MArgument_getMTensor(args[0]);
  mint dtype_code = MArgument_getInteger(args[1]);
  mint n          = libData->MTensor_getFlattenedLength(data);
  double *src     = libData->MTensor_getRealData(data);
  const struct st_WolframNumericArrayLibrary_Functions *naf
      = libData->numericarrayLibraryFunctions;
  MNumericArray out;
  naf->MNumericArray_new(MNumericArray_Type_UBit16, 1, &n, &out);
  u16 *dst = (u16 *)naf->MNumericArray_getData(out);
  for (mint i = 0; i < n; i++) {
    f32 v = (f32)src[i];
    dst[i] = (dtype_code == DT_BF16) ? f32_to_bf16(v) : f32_to_fp16(v);
  }
  MArgument_setMNumericArray(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_cast(WolframLibraryData libData, mint argc,
                                         MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term src        = (Term)MArgument_getInteger(args[0]);
  mint dst_dtype  = MArgument_getInteger(args[1]);
  Term r = uop_cast(src, (u32)dst_dtype);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_bitcast(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term src        = (Term)MArgument_getInteger(args[0]);
  mint dst_dtype  = MArgument_getInteger(args[1]);
  Term r = uop_bitcast(src, (u32)dst_dtype);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_unary(WolframLibraryData libData, mint argc,
                                         MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  mint op  = MArgument_getInteger(args[0]);
  Term src = (Term)MArgument_getInteger(args[1]);
  Term r = uop_unary((u32)op, src);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_load(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term src = (Term)MArgument_getInteger(args[0]);
  Term r = uop_load(src);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_binary(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  mint op = MArgument_getInteger(args[0]);
  Term a  = (Term)MArgument_getInteger(args[1]);
  Term b  = (Term)MArgument_getInteger(args[2]);
  Term r = uop_binary((u32)op, a, b);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_reduce(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  mint kind = MArgument_getInteger(args[0]);
  mint axis = MArgument_getInteger(args[1]);
  Term src  = (Term)MArgument_getInteger(args[2]);
  Term r = uop_reduce((u32)kind, (u32)axis, src);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// Shared helper for the movement ops: packs an MTensor of dim
// integers into a C u32 array, calls the supplied constructor,
// returns the packed term.
typedef Term (*uop_move_ctor)(Term src, u32 ndim, const u32 *dims);

static int movement_op_shared(WolframLibraryData libData, MArgument *args,
                              MArgument res, uop_move_ctor ctor) {
  Term     src  = (Term)MArgument_getInteger(args[0]);
  MTensor  dims = MArgument_getMTensor(args[1]);
  mint     n    = libData->MTensor_getFlattenedLength(dims);
  mint    *raw  = libData->MTensor_getIntegerData(dims);
  u32      buf[2 * MAX_DIM];
  for (mint i = 0; i < n && i < (mint)(2 * MAX_DIM); i++) buf[i] = (u32)raw[i];
  Term r = ctor(src, (u32)n, buf);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_reshape(WolframLibraryData libData, mint argc,
                                           MArgument *args, MArgument res) {
  (void)argc;
  return movement_op_shared(libData, args, res, uop_reshape);
}

EXTERN_C DLLEXPORT int thvm_wl_uop_permute(WolframLibraryData libData, mint argc,
                                           MArgument *args, MArgument res) {
  (void)argc;
  return movement_op_shared(libData, args, res, uop_permute);
}

EXTERN_C DLLEXPORT int thvm_wl_uop_expand(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)argc;
  return movement_op_shared(libData, args, res, uop_expand);
}

// PAD/SHRINK use 2*ndim entries (begin, end pairs per axis).
static int pad_shrink_shared(WolframLibraryData libData, MArgument *args,
                             MArgument res, uop_move_ctor ctor) {
  Term     src = (Term)MArgument_getInteger(args[0]);
  MTensor  be  = MArgument_getMTensor(args[1]);
  mint     n   = libData->MTensor_getFlattenedLength(be);
  mint    *raw = libData->MTensor_getIntegerData(be);
  u32      buf[2 * MAX_DIM];
  for (mint i = 0; i < n && i < (mint)(2 * MAX_DIM); i++) buf[i] = (u32)raw[i];
  Term r = ctor(src, (u32)(n / 2), buf);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_pad(WolframLibraryData libData, mint argc,
                                       MArgument *args, MArgument res) {
  (void)argc;
  return pad_shrink_shared(libData, args, res, uop_pad);
}

EXTERN_C DLLEXPORT int thvm_wl_uop_shrink(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)argc;
  return pad_shrink_shared(libData, args, res, uop_shrink);
}

EXTERN_C DLLEXPORT int thvm_wl_uop_flip(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term src  = (Term)MArgument_getInteger(args[0]);
  mint mask = MArgument_getInteger(args[1]);
  Term r = uop_flip(src, (u32)mask);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// Builds the dup-like GRAD pair: heap cell holds [y, gy]; the returned
// Term is the BWD projection (TAG_DP1 + DUP_GRAD_FLAG).  WL pairs it
// with a FWD projection (TAG_DP0) at the same cell loc via packTerm.
EXTERN_C DLLEXPORT int thvm_wl_uop_grad(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term y  = (Term)MArgument_getInteger(args[0]);
  Term gy = (Term)MArgument_getInteger(args[1]);
  Term r  = uop_grad(y, gy);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_fwd(WolframLibraryData libData, mint argc,
                                       MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term y  = (Term)MArgument_getInteger(args[0]);
  Term gy = (Term)MArgument_getInteger(args[1]);
  Term r  = uop_fwd(y, gy);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// Same as thvm_wl_uop_grad but with an explicit `target` Term.
// When non-zero, the chain rule's leaf-handler does direct
// tid-equality match against target (returning gy on match, scalar
// zero on mismatch) without needing the WL DUP nest.  Used by
// TGrad when the target is a TVAR (lambda-bound variable) so leaf
// tids aren't statically known at WL construction time.
EXTERN_C DLLEXPORT int thvm_wl_uop_grad_with_target(WolframLibraryData libData,
                                                    mint argc,
                                                    MArgument *args,
                                                    MArgument res) {
  (void)libData; (void)argc;
  Term y      = (Term)MArgument_getInteger(args[0]);
  Term gy     = (Term)MArgument_getInteger(args[1]);
  Term target = (Term)MArgument_getInteger(args[2]);
  Term r      = uop_grad_with_target(y, gy, target);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// TAG_CTR accessors: thvm_wl_term_ctr_n(t) -> arity, and
// thvm_wl_term_ctr_at(t, i) -> i-th child Term (0 if out-of-range).
EXTERN_C DLLEXPORT int thvm_wl_term_ctr_n(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)term_ctr_n(t));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_term_ctr_at(WolframLibraryData libData, mint argc,
                                           MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  mint i = MArgument_getInteger(args[1]);
  MArgument_setInteger(res, (mint)term_ctr_at(t, (u32)i));
  return LIBRARY_NO_ERROR;
}

// Direct materialize: runs the schedule + kernelize + linearize pass
// immediately and returns the scheduled DAG term.  Fires no kernels
// (that happens in TWnf via the interact_kernel rule in commit 4).
EXTERN_C DLLEXPORT int thvm_wl_materialize(WolframLibraryData libData, mint argc,
                                           MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  Term r = thvm_materialize(t);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_realize(WolframLibraryData libData, mint argc,
                                       MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  Term r = thvm_realize(t);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// Bundle a list of independent root Terms (passed as an Integer
// MTensor of packed Term values) into a TAG_CTR and dispatch through
// thvm_realize_many so they share one materialize pass / pool
// boundary.  Used by TAdam to fire all per-param ASSIGNs in one go;
// without this every param's TRealize re-walks the whole gradient
// chain and emits ~5K kernel slots per step on LeNet.
EXTERN_C DLLEXPORT int thvm_wl_realize_many(WolframLibraryData libData,
                                            mint argc, MArgument *args,
                                            MArgument res) {
  (void)argc;
  MTensor t = MArgument_getMTensor(args[0]);
  mint n    = libData->MTensor_getFlattenedLength(t);
  const mint *data = libData->MTensor_getIntegerData(t);
  if (n <= 0) { MArgument_setInteger(res, 0); return LIBRARY_NO_ERROR; }
  if (n > 256) return LIBRARY_FUNCTION_ERROR;
  Term children[256];
  for (mint i = 0; i < n; i++) children[i] = (Term)data[i];
  Term ctr = term_new_ctr(0, children, (u32)n);
  Term r   = thvm_realize_many(ctr);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// Expose kernel-entry introspection for tests.
EXTERN_C DLLEXPORT int thvm_wl_kernel_count(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)KERNELS_NEXT);
  return LIBRARY_NO_ERROR;
}

// Render a kernel's program as C99 by lifting to the UOp DAG and
// emitting via cg_render_uop_kernel_c (matches the path cpu_jit_build
// itself takes after F6 step 15). Used by tests + diagnostics to
// inspect what the JIT will compile.  Pass kid (KERNELS table index,
// 1..KERNELS_NEXT-1).
EXTERN_C DLLEXPORT int thvm_wl_kernel_source_c(WolframLibraryData libData,
                                               mint argc, MArgument *args,
                                               MArgument res) {
  (void)libData; (void)argc;
  u32 kid = (u32)MArgument_getInteger(args[0]);
  if (kid == 0 || kid >= KERNELS_NEXT) {
    MArgument_setUTF8String(res, (char *)"");
    return LIBRARY_NO_ERROR;
  }
  KernelUopLift lift = {0};
  if (!kernel_lift_to_uop(&KERNELS[kid], &lift)) {
    MArgument_setUTF8String(res, (char *)"");
    return LIBRARY_NO_ERROR;
  }
  char buf[16384];
  FILE *fp = fmemopen(buf, sizeof(buf), "w");
  if (fp == NULL) {
    MArgument_setUTF8String(res, (char *)"");
    return LIBRARY_NO_ERROR;
  }
  cg_render_uop_kernel_c(lift.store_root, "k", lift.out_buf,
                         lift.in_bufs, lift.n_inputs, fp);
  long n = ftell(fp);
  fclose(fp);
  if (n <= 0) {
    MArgument_setUTF8String(res, (char *)"");
    return LIBRARY_NO_ERROR;
  }
  char *src = (char *)malloc((size_t)n + 1);
  if (src == NULL) {
    MArgument_setUTF8String(res, (char *)"");
    return LIBRARY_NO_ERROR;
  }
  memcpy(src, buf, (size_t)n);
  src[n] = '\0';
  // libData->UTF8String_disown is the matching free; WL retains the
  // pointer until that's called.  Caller (WL side) holds it long
  // enough to pull the string contents and then it gets reaped.
  MArgument_setUTF8String(res, src);
  return LIBRARY_NO_ERROR;
}

// Same but for the Metal renderer.  Routes through cg_emit_metal ->
// cg_emit_tile_metal -> cg_emit_via_uop (kernel_lift_to_uop +
// cg_render_uop_kernel) so the WL inspector sees exactly the MSL
// the dispatch ladder will hand to MTLLibrary.
EXTERN_C DLLEXPORT int thvm_wl_kernel_source_metal(WolframLibraryData libData,
                                                   mint argc, MArgument *args,
                                                   MArgument res) {
  (void)libData; (void)argc;
  u32 kid = (u32)MArgument_getInteger(args[0]);
  if (kid == 0 || kid >= KERNELS_NEXT) {
    MArgument_setUTF8String(res, (char *)"");
    return LIBRARY_NO_ERROR;
  }
  char *src = cg_emit_metal(&KERNELS[kid]);
  if (src == NULL) {
    MArgument_setUTF8String(res, (char *)"");
    return LIBRARY_NO_ERROR;
  }
  MArgument_setUTF8String(res, src);
  return LIBRARY_NO_ERROR;
}

// Per-kernel profile inspection.  All four return Integer; cap kid at
// KERNELS_NEXT to keep WL probes safe.
EXTERN_C DLLEXPORT int thvm_wl_kernel_flops(WolframLibraryData l, mint a,
                                            MArgument *args, MArgument res) {
  (void)l;(void)a;
  u32 kid = (u32)MArgument_getInteger(args[0]);
  u64 f = (kid > 0 && kid < KERNELS_NEXT) ? cg_kernel_flops(&KERNELS[kid]) : 0;
  MArgument_setInteger(res, (mint)f);
  return LIBRARY_NO_ERROR;
}

// 0=none, 1=blas-dot, 2=blas-gemv, 3=blas-gemm, 4=jit, 5=interpreter,
// 6=metal-jit, 7=metal-op, 8=tile, 9=metal-tile, 10=metal-gemm,
// 11=metal-conv, 12=metal-gemv, 13=metal-alias.
EXTERN_C DLLEXPORT int thvm_wl_kernel_dispatch_kind(WolframLibraryData l, mint a,
                                                    MArgument *args, MArgument res) {
  (void)l;(void)a;
  u32 kid = (u32)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)cg_kernel_dispatch_kind(kid));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_kernel_dispatch_count(WolframLibraryData l, mint a,
                                                     MArgument *args, MArgument res) {
  (void)l;(void)a;
  u32 kid = (u32)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)cg_kernel_dispatch_count(kid));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_kernel_total_us(WolframLibraryData l, mint a,
                                               MArgument *args, MArgument res) {
  (void)l;(void)a;
  u32 kid = (u32)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)cg_kernel_total_us(kid));
  return LIBRARY_NO_ERROR;
}

// Path of the JIT'd .dylib for kid's program (deterministic from
// the program hash; the file may not exist if the JIT bailed at
// codegen time).
EXTERN_C DLLEXPORT int thvm_wl_kernel_jit_dylib_path(WolframLibraryData l, mint a,
                                                     MArgument *args, MArgument res) {
  (void)l;(void)a;
  u32 kid = (u32)MArgument_getInteger(args[0]);
  if (kid == 0 || kid >= KERNELS_NEXT) {
    MArgument_setUTF8String(res, (char *)"");
    return LIBRARY_NO_ERROR;
  }
  static char path[256];
  u64 key = cpu_jit_hash(&KERNELS[kid]);
  snprintf(path, sizeof path, "/tmp/thvm_jit_%016llx.dylib",
           (unsigned long long)key);
  MArgument_setUTF8String(res, path);
  return LIBRARY_NO_ERROR;
}

// Number of distinct KProgOp[] arrays interned in the kernel-
// program hash-cons cache.  Used by tests to assert that two
// kernels with structurally identical programs share storage.
// === KpSchedule / TOpt surface (Phase 16 codegen variant scaffold) ===
//
// Snapshot of a kernel's axis-typed scheduling plan + applied opts.
// Packed as a flat {Integer, 1} for the WL side to decode:
//
//   [0]  n_axes
//   [1]  n_applied
//   [2 .. 2 + n_axes - 1]                  axis_types[i]
//   [2 + n_axes .. 2 + 2*n_axes - 1]       full_shape[i]
//   [2 + 2*n_axes ..]                      applied_opts as (op, axis, arg) triples
//
// The WL TKernelOpts wrapper turns this into the
// <|"AxisTypes", "FullShape", "Applied"|> association.
EXTERN_C DLLEXPORT int thvm_wl_kernel_axes_get(WolframLibraryData libData,
                                               mint argc, MArgument *args,
                                               MArgument res) {
  (void)argc;
  u32 kid = (u32)MArgument_getInteger(args[0]);
  if (kid == 0 || kid >= KERNELS_NEXT) {
    mint dims[1] = {0};
    MTensor empty;
    libData->MTensor_new(MType_Integer, 1, dims, &empty);
    MArgument_setMTensor(res, empty);
    return LIBRARY_NO_ERROR;
  }
  KernelEntry const *ke = &KERNELS[kid];
  KpSchedule const *ax = ke->schedule;
  if (ax == NULL) {
    mint dims[1] = {0};
    MTensor empty;
    libData->MTensor_new(MType_Integer, 1, dims, &empty);
    MArgument_setMTensor(res, empty);
    return LIBRARY_NO_ERROR;
  }
  // E9 session 3: read n_axes + per-axis (kax_type, extent) through the
  // resolvers (signal-derived from output_shape + tail-reduce +
  // scalar-reduce + applied_opts).  Wire format stays byte-equal: by
  // writer-trio determinism, axes_resolve_n_axes(ke) == ax->n_axes and
  // axes_resolve_full_shape(ke, i) == ax->full_shape[i] under
  // THVM_E9_VALIDATE=1.  applied_opts is still sourced from KpSchedule
  // (Piece B / future session moves the ownership).
  u32 n_axes = axes_resolve_n_axes(ke);
  u32 n_applied = (u32)ax->n_applied;
  mint total = (mint)(2 + 2 * n_axes + 3 * n_applied);
  mint dims[1] = {total};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  dst[0] = (mint)n_axes;
  dst[1] = (mint)n_applied;
  for (u32 i = 0; i < n_axes; i++) {
    u32 extent = 0;
    (void)axes_resolve_full_shape(ke, i, &extent);
    dst[2 + i]           = (mint)axes_resolve_kax_type(ke, i);
    dst[2 + n_axes + i]  = (mint)extent;
  }
  mint base = 2 + 2 * n_axes;
  for (u32 i = 0; i < n_applied; i++) {
    dst[base + 3 * i + 0] = (mint)ax->applied_opts[i].op;
    dst[base + 3 * i + 1] = (mint)ax->applied_opts[i].axis;
    dst[base + 3 * i + 2] = (mint)ax->applied_opts[i].arg;
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

// Apply one TOpt to a kernel.  Returns 1 on success, 0 on validation
// failure (axis out of range, arg doesn't divide axis size, opts
// table full).  Op encoding matches KOP_* in src/thvm.h; the WL
// surface translates the TOpt op_String into the integer.
EXTERN_C DLLEXPORT int thvm_wl_kernel_apply_opt(WolframLibraryData l, mint a,
                                                MArgument *args, MArgument res) {
  (void)l; (void)a;
  u32 kid  = (u32)MArgument_getInteger(args[0]);
  u32 op   = (u32)MArgument_getInteger(args[1]);
  u32 axis = (u32)MArgument_getInteger(args[2]);
  u32 arg  = (u32)MArgument_getInteger(args[3]);
  if (kid == 0 || kid >= KERNELS_NEXT) {
    MArgument_setInteger(res, 0);
    return LIBRARY_NO_ERROR;
  }
  if (KERNELS[kid].schedule == NULL) {
    MArgument_setInteger(res, 0);
    return LIBRARY_NO_ERROR;
  }
  KOpt opt = { (u8)op, (u8)axis, arg };
  int ok = tile_anno_apply_opt(&KERNELS[kid], opt);
  if (ok) {
  }
  MArgument_setInteger(res, (mint)ok);
  return LIBRARY_NO_ERROR;
}

// Autotune: bench-and-pick the winning TOpt for kid.  Returns 1 on
// "winning opt applied", 0 on "no opt beat baseline / no candidates
// / invalid kid".
EXTERN_C DLLEXPORT int thvm_wl_kernel_autotune(WolframLibraryData l, mint a,
                                               MArgument *args, MArgument res) {
  (void)l; (void)a;
  u32 kid = (u32)MArgument_getInteger(args[0]);
  int won = kernel_autotune(kid);
  MArgument_setInteger(res, (mint)won);
  return LIBRARY_NO_ERROR;
}

// Bench: time n_runs back-to-back kid fires, return min wallclock us.
EXTERN_C DLLEXPORT int thvm_wl_kernel_bench_us(WolframLibraryData l, mint a,
                                               MArgument *args, MArgument res) {
  (void)l; (void)a;
  u32 kid    = (u32)MArgument_getInteger(args[0]);
  u32 n_runs = (u32)MArgument_getInteger(args[1]);
  u64 us = (kid > 0 && kid < KERNELS_NEXT) ? kernel_bench_us(kid, n_runs) : 0;
  MArgument_setInteger(res, (mint)us);
  return LIBRARY_NO_ERROR;
}

// Bench every proposer candidate + the no-opt baseline.  Returns a
// flat {Integer, 1} of (op, axis, arg, us) quads -- slot 0 is
// baseline.  Restores axes to baseline at exit so WL TKernelVariants
// can surface raw measurements without committing any opt.
EXTERN_C DLLEXPORT int thvm_wl_kernel_bench_variants(WolframLibraryData libData,
                                                     mint argc, MArgument *args,
                                                     MArgument res) {
  (void)argc;
  u32 kid = (u32)MArgument_getInteger(args[0]);
  KOpt opts[16];
  u64  uss [16];
  u32 n = 0;
  if (kid > 0 && kid < KERNELS_NEXT) {
    n = kernel_bench_variants(kid, opts, uss, (u32)(sizeof(opts)/sizeof(*opts)));
  }
  mint dims[1] = {(mint)(4 * n)};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  for (u32 i = 0; i < n; i++) {
    dst[4*i + 0] = (mint)opts[i].op;
    dst[4*i + 1] = (mint)opts[i].axis;
    dst[4*i + 2] = (mint)opts[i].arg;
    dst[4*i + 3] = (mint)uss [i];
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

// Shape-heuristic proposer.  Returns a flat {Integer, 1} of (op,
// axis, arg) triples; the WL surface decodes into a list of TOpt.
// Empty result for kid 0 / out-of-range / kernels with no
// proposable opts.
EXTERN_C DLLEXPORT int thvm_wl_kernel_propose(WolframLibraryData libData,
                                              mint argc, MArgument *args,
                                              MArgument res) {
  (void)argc;
  u32 kid = (u32)MArgument_getInteger(args[0]);
  KOpt buf[16];
  u32  n = 0;
  if (kid > 0 && kid < KERNELS_NEXT) {
    n = kernel_opts_propose(&KERNELS[kid], buf, (u32)(sizeof(buf)/sizeof(*buf)));
  }
  mint dims[1] = {(mint)(3 * n)};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  for (u32 i = 0; i < n; i++) {
    dst[3*i + 0] = (mint)buf[i].op;
    dst[3*i + 1] = (mint)buf[i].axis;
    dst[3*i + 2] = (mint)buf[i].arg;
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_kernel_program_cache_size(WolframLibraryData libData,
                                                          mint argc,
                                                          MArgument *args,
                                                          MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_kernel_program_key(WolframLibraryData libData,
                                                  mint argc,
                                                  MArgument *args,
                                                  MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

// Register a shape annotation for a TLam-bound variable.  args[0]
// is the LAM's heap loc; args[1] is a rank-1 Int array of
// dimension extents (length = ndim).  TVAR(loc) lookups via
// term_shape_in then return this shape -- letting materialize
// compile bodies whose bound vars are still pre-substitution.
EXTERN_C DLLEXPORT int thvm_wl_lam_shape_set(WolframLibraryData libData,
                                              mint argc,
                                              MArgument *args,
                                              MArgument res) {
  (void)argc;
  u64 lam_loc = (u64)MArgument_getInteger(args[0]);
  MTensor dims = MArgument_getMTensor(args[1]);
  mint *src   = libData->MTensor_getIntegerData(dims);
  mint nrank  = libData->MTensor_getFlattenedLength(dims);
  Shape s = {0};
  s.ndim = (u32)(nrank > MAX_DIM ? MAX_DIM : nrank);
  for (u32 i = 0; i < s.ndim; i++) s.dims[i] = (u32)src[i];
  lam_shape_set(lam_loc, &s);
  MArgument_setInteger(res, 0);
  return LIBRARY_NO_ERROR;
}

// Number of currently-registered LAM shape annotations.  Tests
// use this to assert "the annotation survived a round-trip
// through TDef + TRef + alo_realize".
EXTERN_C DLLEXPORT int thvm_wl_lam_shape_count(WolframLibraryData libData,
                                                mint argc,
                                                MArgument *args,
                                                MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)lam_shape_count());
  return LIBRARY_NO_ERROR;
}

// Run term_shape_in on an arbitrary Term and return the shape as
// a rank-1 Int array.  Empty array on failure (shape unknown).
EXTERN_C DLLEXPORT int thvm_wl_term_shape_in(WolframLibraryData libData,
                                              mint argc,
                                              MArgument *args,
                                              MArgument res) {
  (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  Shape s = {0};
  int ok = term_shape_in(t, 0, &s);
  mint dims[1] = {ok ? (mint)s.ndim : 0};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  if (ok) {
    mint *dst = libData->MTensor_getIntegerData(out);
    for (u32 i = 0; i < s.ndim; i++) dst[i] = (mint)s.dims[i];
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

// === memory introspection (used by lenet-mnist/memory-probe.wls) ===
EXTERN_C DLLEXPORT int thvm_wl_tens_count(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  // Count tensors actually allocated (slot 0 is reserved sentinel).
  MArgument_setInteger(res, (mint)(TENS_NEXT > 0 ? TENS_NEXT - 1 : 0));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_total_buf_bytes(WolframLibraryData libData, mint argc,
                                               MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  // Sum live CPU buffer bytes (refcount > 0).  Walks the CPU_BUFS
  // table directly; not backend-agnostic, but the CPU backend is
  // the only one we currently train on.
  u64 total = 0;
  for (u64 i = 1; i < CPU_BUFS_NEXT; i++) {
    if (CPU_BUFS[i].refcount > 0) total += CPU_BUFS[i].nbytes;
  }
  MArgument_setInteger(res, (mint)total);
  return LIBRARY_NO_ERROR;
}

// === TMemoryPlan snapshot tables (mp1 of the visualization arc) ===
// Each function returns a flat MTensor of mints sized to the
// current table.  The WL side (MemoryPlan.wl) reshapes them into
// Association lists.

EXTERN_C DLLEXPORT int thvm_wl_kernel_table(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)argc; (void)args;
  // Cols per kernel: [n_inputs, output_tid, _reserved0, spliced,
  //                   consumer_count, output_numel, output_dtype].
  // Slot 2 was `fired`; removed (kernels re-fire on every redex,
  // OP2-style).  Kept as a 0 placeholder so the existing 7-col
  // schema and TMemoryPlan column indexing stay stable.
  mint nRows = (mint)(KERNELS_NEXT > 0 ? KERNELS_NEXT - 1 : 0);
  mint nCols = 7;
  mint dims[1] = {nRows * nCols};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  for (mint k = 0; k < nRows; k++) {
    KernelEntry *ke = &KERNELS[k + 1];
    dst[k * nCols + 0] = (mint)ke->n_inputs;
    dst[k * nCols + 1] = (mint)ke->output_tid;
    dst[k * nCols + 2] = 0;        /* reserved (was `fired`) */
    dst[k * nCols + 3] = (mint)ke->spliced;
    dst[k * nCols + 4] = (mint)ke->consumer_count;
    dst[k * nCols + 5] = (mint)ke->output_numel;
    dst[k * nCols + 6] = (mint)ke->output_dtype;
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_kernel_inputs(WolframLibraryData libData, mint argc,
                                             MArgument *args, MArgument res) {
  (void)argc;
  mint kid = MArgument_getInteger(args[0]);
  if (kid <= 0 || (u32)kid >= KERNELS_NEXT) {
    MArgument_setMTensor(res, NULL);
    return LIBRARY_FUNCTION_ERROR;
  }
  KernelEntry *ke = &KERNELS[kid];
  mint dims[1] = {(mint)ke->n_inputs};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  for (u32 i = 0; i < ke->n_inputs; i++) dst[i] = (mint)ke->input_tids[i];
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

// Walk the UOP DAG rooted at `root` (a Term integer) and return the
// distinct TAG_TEN-leaf tids encountered.  Generic UOP-DAG -> leaves
// utility; TGrad / TGradMany are the first callers.  Cap silently
// truncates past 256 distinct leaves (well above any current
// workload's per-realize live-tensor count).
#define UOP_LEAF_TIDS_CAP 256
EXTERN_C DLLEXPORT int thvm_wl_uop_leaf_tids(WolframLibraryData libData, mint argc,
                                             MArgument *args, MArgument res) {
  (void)argc;
  Term root = (Term)MArgument_getInteger(args[0]);
  u32 buf[UOP_LEAF_TIDS_CAP];
  u32 n = 0;
  uop_leaf_tids(root, buf, UOP_LEAF_TIDS_CAP, &n);
  mint dims[1] = {(mint)n};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  for (u32 i = 0; i < n; i++) dst[i] = (mint)buf[i];
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_tens_table(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)argc; (void)args;
  // Cols per tid: [producer_kid, buf_id, dtype, view_numel,
  //                view_contiguous, refcount, backend_id].
  mint nRows = (mint)(TENS_NEXT > 0 ? TENS_NEXT - 1 : 0);
  mint nCols = 7;
  mint dims[1] = {nRows * nCols};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  for (mint t = 0; t < nRows; t++) {
    TenDesc *d = &TENS[t + 1];
    dst[t * nCols + 0] = (mint)d->producer_kid;
    dst[t * nCols + 1] = (mint)d->buf_id;
    dst[t * nCols + 2] = (mint)d->dtype;
    dst[t * nCols + 3] = (mint)d->view.numel;
    dst[t * nCols + 4] = (mint)d->view.contiguous;
    dst[t * nCols + 5] = (mint)d->refcount;
    dst[t * nCols + 6] = (mint)(d->backend ? d->backend->id : 0);
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_cpu_buf_table(WolframLibraryData libData, mint argc,
                                             MArgument *args, MArgument res) {
  (void)argc; (void)args;
  // Cols per buf: [nbytes, refcount, preserved, freeable, owns_data].
  mint nRows = (mint)(CPU_BUFS_NEXT > 0 ? CPU_BUFS_NEXT - 1 : 0);
  mint nCols = 5;
  mint dims[1] = {nRows * nCols};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  for (mint b = 0; b < nRows; b++) {
    CpuBuf *cb = &CPU_BUFS[b + 1];
    dst[b * nCols + 0] = (mint)cb->nbytes;
    dst[b * nCols + 1] = (mint)cb->refcount;
    dst[b * nCols + 2] = (mint)cb->preserved;
    dst[b * nCols + 3] = (mint)cb->freeable;
    dst[b * nCols + 4] = (mint)cb->owns_data;
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

#ifdef THVM_HAS_METAL
extern u32  thvm_metal_buf_count(void);
extern void thvm_metal_buf_get(u32 i, u64 *nbytes_out, u32 *refcount_out);
extern u64  thvm_metal_live_bytes(void);
extern u64  thvm_metal_retained_bytes(void);
extern u64  thvm_metal_deferred_bytes(void);
extern u32  thvm_metal_deferred_len(void);
extern u32  thvm_metal_freelist_len(void);
extern u64  thvm_metal_peak_live_bytes(void);
extern u64  thvm_metal_peak_retained_bytes(void);
extern u64  thvm_metal_peak_deferred_bytes(void);
extern void thvm_metal_gpu_time(u64 *out_total_us, u64 *out_flush_count);
#endif

EXTERN_C DLLEXPORT int thvm_wl_metal_buf_table(WolframLibraryData libData, mint argc,
                                               MArgument *args, MArgument res) {
  (void)argc; (void)args;
  // Cols per buf: [nbytes, refcount].  Metal has no preserved /
  // freeable bookkeeping, so the schema is narrower than CPU.
  // When the dylib was built without Metal, return an empty 0x2
  // tensor so the WL side can treat the result uniformly.
  mint nRows = 0;
#ifdef THVM_HAS_METAL
  u32 c = thvm_metal_buf_count();
  if (c > 1) nRows = (mint)(c - 1);
#endif
  mint nCols = 2;
  mint dims[1] = {nRows * nCols};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
#ifdef THVM_HAS_METAL
  for (mint b = 0; b < nRows; b++) {
    u64 nbytes = 0; u32 refcount = 0;
    thvm_metal_buf_get((u32)(b + 1), &nbytes, &refcount);
    dst[b * nCols + 0] = (mint)nbytes;
    dst[b * nCols + 1] = (mint)refcount;
  }
#endif
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_metal_buf_summary(WolframLibraryData libData, mint argc,
                                                 MArgument *args, MArgument res) {
  (void)argc; (void)args;
  // [live_bytes, retained_bytes, deferred_bytes, deferred_len,
  //  freelist_len, peak_live_bytes, peak_retained_bytes,
  //  peak_deferred_bytes].  live_bytes counts refcounted
  // user-visible buffers; retained_bytes also includes recycle-list
  // buffers still holding MTLBuffer storage.
  mint dims[1] = {8};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  for (u32 i = 0; i < 8; i++) dst[i] = 0;
#ifdef THVM_HAS_METAL
  dst[0] = (mint)thvm_metal_live_bytes();
  dst[1] = (mint)thvm_metal_retained_bytes();
  dst[2] = (mint)thvm_metal_deferred_bytes();
  dst[3] = (mint)thvm_metal_deferred_len();
  dst[4] = (mint)thvm_metal_freelist_len();
  dst[5] = (mint)thvm_metal_peak_live_bytes();
  dst[6] = (mint)thvm_metal_peak_retained_bytes();
  dst[7] = (mint)thvm_metal_peak_deferred_bytes();
#endif
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

// thvm_wl_metal_gpu_time() -- {total_gpu_us, flush_count}.  total_gpu_us
// is the summed [cmd GPUEndTime]-[cmd GPUStartTime] microseconds across
// every command-buffer flush/submit since metal_init; flush_count is
// the number of flushes/submits (incl. empty).  Zero/zero on a non-
// Metal build.  WL side reads via TMetalGpuTime[].
EXTERN_C DLLEXPORT int thvm_wl_metal_gpu_time(WolframLibraryData libData, mint argc,
                                              MArgument *args, MArgument res) {
  (void)argc; (void)args;
  mint dims[1] = {2};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  dst[0] = 0; dst[1] = 0;
#ifdef THVM_HAS_METAL
  u64 total_us = 0, flush_count = 0;
  thvm_metal_gpu_time(&total_us, &flush_count);
  dst[0] = (mint)total_us;
  dst[1] = (mint)flush_count;
#endif
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

// thvm_wl_metal_perop_profile() -- per-kernel true GPU-time breakdown.
// Returns a flat Integer MTensor of 6-tuples
//   [kid, gpu_us, gpu_samples, dispatch_count, flops, dispatch_kind]
// one row per live kernel that has at least one GPU-time sample (i.e.
// fired under THVM_METAL_PROFILE_PEROP=1 on the Metal backend).  Empty
// (0-row) tensor when no kernel has samples -- including non-Metal
// builds and runs without THVM_METAL_PROFILE_PEROP=1.  WL side reads
// via TMetalPerOpProfile[].
EXTERN_C DLLEXPORT int thvm_wl_metal_perop_profile(WolframLibraryData libData, mint argc,
                                                   MArgument *args, MArgument res) {
  (void)argc; (void)args;
  enum { NCOLS = 6 };
  // First pass: count rows with samples.
  mint nRows = 0;
  for (u32 kid = 1; kid < KERNELS_NEXT; kid++) {
    if (cg_kernel_gpu_samples(kid) > 0) nRows++;
  }
  mint dims[1] = {nRows * NCOLS};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  mint r = 0;
  for (u32 kid = 1; kid < KERNELS_NEXT; kid++) {
    u64 samples = cg_kernel_gpu_samples(kid);
    if (samples == 0) continue;
    dst[r * NCOLS + 0] = (mint)kid;
    dst[r * NCOLS + 1] = (mint)cg_kernel_gpu_us(kid);
    dst[r * NCOLS + 2] = (mint)samples;
    dst[r * NCOLS + 3] = (mint)cg_kernel_dispatch_count(kid);
    dst[r * NCOLS + 4] = (mint)cg_kernel_flops(&KERNELS[kid]);
    dst[r * NCOLS + 5] = (mint)cg_kernel_dispatch_kind(kid);
    r++;
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_kernel_info(WolframLibraryData libData, mint argc,
                                           MArgument *args, MArgument res) {
  (void)argc;
  mint kid = MArgument_getInteger(args[0]);
  if (kid < 0 || (u32)kid >= KERNELS_NEXT) {
    MArgument_setMTensor(res, NULL);
    return LIBRARY_FUNCTION_ERROR;
  }
  KernelEntry *ke = &KERNELS[kid];
  // Return a flat MTensor: [n_inputs, n_ops, output_numel, output_dtype,
  //                         op0_opcode, op0_n_src, op0_src0, op0_src1, op0_arg, op0_numel,
  //                         ... repeat for each op ...]
  mint nFields = 4 + (mint)ke->n_ops * 6;
  mint dims[1] = {nFields};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  mint idx = 0;
  dst[idx++] = (mint)ke->n_inputs;
  dst[idx++] = (mint)ke->n_ops;
  dst[idx++] = (mint)ke->output_numel;
  dst[idx++] = (mint)ke->output_dtype;
  for (u32 i = 0; i < ke->n_ops; i++) {
    KProgOp *p = &ke->program[i];
    dst[idx++] = (mint)p->opcode;
    dst[idx++] = (mint)p->n_src;
    dst[idx++] = (mint)p->src[0];
    dst[idx++] = (mint)(p->n_src >= 2 ? p->src[1] : 0);
    dst[idx++] = (mint)p->arg;
    dst[idx++] = (mint)p->numel;
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

// thvm_wl_kernel_scalar_uops(kid)  -- Phase A introspection of the
// rangeify lowering snapshot retained on KERNELS[kid].  Returns:
//   - empty MTensor (length 0) when ke->scalar_uops is NULL (legacy
//     visit() path emitted this kernel; WL-side reads as Missing[]).
//   - flat Integer MTensor encoding [n_scalar_uops, src_width,
//     op0_op, op0_dtype, op0_src_count, op0_src..., op0_extra_lo,
//     op0_extra_hi, pad, ... per op ...].  The per-op row is
//     6 + src_width integers; extra is u64 split into two i32 halves
//     to fit MType_Integer.
//   Slot 0 (S_NONE sentinel) IS included so caller-side indices
//   match the C-side ScalarUop[] indexing.
EXTERN_C DLLEXPORT int thvm_wl_kernel_scalar_uops(WolframLibraryData libData,
                                                  mint argc, MArgument *args,
                                                  MArgument res) {
  (void)argc;
  mint kid = MArgument_getInteger(args[0]);
  if (kid < 0 || (u32)kid >= KERNELS_NEXT) {
    MArgument_setMTensor(res, NULL);
    return LIBRARY_FUNCTION_ERROR;
  }
  KernelEntry *ke = &KERNELS[kid];
  mint n = (ke->scalar_uops == NULL) ? 0 : (mint)ke->n_scalar_uops;
  // Header is 2 ints (n_scalar_uops, src_width); body is
  // (6 + SCALAR_MAX_SRC) ints per op.
  mint srcWidth = SCALAR_MAX_SRC;
  mint rowWidth = 6 + srcWidth;
  mint nFields = 2 + n * rowWidth;
  mint dims[1] = {nFields};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  mint idx  = 0;
  dst[idx++] = n;
  dst[idx++] = srcWidth;
  for (mint i = 0; i < n; i++) {
    ScalarUop *u = &ke->scalar_uops[i];
    dst[idx++] = (mint)u->op;
    dst[idx++] = (mint)u->dtype;
    dst[idx++] = (mint)u->src_count;
    for (mint s = 0; s < srcWidth; s++) {
      dst[idx++] = (mint)u->src[s];
    }
    dst[idx++] = (mint)(u->extra & 0xFFFFFFFFu);
    dst[idx++] = (mint)((u->extra >> 32) & 0xFFFFFFFFu);
    dst[idx++] = 0;   // padding to keep alignment + future use
  }
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

// === REF / ALO surface ===

EXTERN_C DLLEXPORT int thvm_wl_def_register(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32  name = (u32) MArgument_getInteger(args[0]);
  Term body = (Term)MArgument_getInteger(args[1]);
  thvm_def_register(name, body);
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_term_new_ref(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32 name = (u32)MArgument_getInteger(args[0]);
  Term r = term_new_ref(name);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// === AOT (Bend2-style fork-emitting AOT, Phase 4 surface) ============
//
// Phase 1+2 built the runtime + emitter in src/aot/.  This is the
// minimum WL bridge so users can drive the emitter from
// THVMLink`AOT and inspect what gets generated.  TAOTCompile (the
// dlopen path) lands as the OPS-indirection ABI matures; for now
// TAOTEmit returns the source string and tests / docs can run it
// through the standalone-binary path proven by tests/test_aot_e2e.c.

EXTERN_C DLLEXPORT int thvm_wl_aot_emit_program(WolframLibraryData libData,
                                                mint argc, MArgument *args,
                                                MArgument res) {
  (void)argc;
  u32 def_id      = (u32) MArgument_getInteger(args[0]);
  const char *nm  =       MArgument_getUTF8String(args[1]);

  char *src = thvm_aot_emit_program(def_id, nm);
  if (libData != NULL && libData->UTF8String_disown != NULL) {
    libData->UTF8String_disown((char *)nm);
  }
  if (src == NULL) {
    MArgument_setUTF8String(res, (char *)"");
    return LIBRARY_NO_ERROR;
  }
  MArgument_setUTF8String(res, src);
  return LIBRARY_NO_ERROR;
}

// Phase 4 iter E3: compile + load+run bridges.

EXTERN_C DLLEXPORT int thvm_wl_aot_compile(WolframLibraryData libData,
                                           mint argc, MArgument *args,
                                           MArgument res) {
  (void)argc;
  u32 def_id      = (u32) MArgument_getInteger(args[0]);
  const char *nm  =       MArgument_getUTF8String(args[1]);

  char *path = thvm_aot_compile_to_dylib(def_id, nm);
  if (libData != NULL && libData->UTF8String_disown != NULL) {
    libData->UTF8String_disown((char *)nm);
  }
  if (path == NULL) {
    MArgument_setUTF8String(res, (char *)"");
    return LIBRARY_NO_ERROR;
  }
  // path is malloc'd by build.c -- WL takes ownership via UTF8String
  // and frees via UTF8String_disown when done.  Hand it over.
  MArgument_setUTF8String(res, path);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_aot_run(WolframLibraryData libData,
                                       mint argc, MArgument *args,
                                       MArgument res) {
  (void)argc;
  const char *path = MArgument_getUTF8String(args[0]);
  const char *nm   = MArgument_getUTF8String(args[1]);
  u64 input        = (u64)MArgument_getInteger(args[2]);

  u64 result = thvm_aot_load_and_run(path, nm, input);

  if (libData != NULL && libData->UTF8String_disown != NULL) {
    libData->UTF8String_disown((char *)path);
    libData->UTF8String_disown((char *)nm);
  }
  MArgument_setInteger(res, (mint)result);
  return LIBRARY_NO_ERROR;
}

// 4-input variant for multi-arg defs (build, ack, gab_tak, ...).
// Extra slots default to 0; the dispatch ignores trailing args.
EXTERN_C DLLEXPORT int thvm_wl_aot_run4(WolframLibraryData libData,
                                        mint argc, MArgument *args,
                                        MArgument res) {
  (void)argc;
  const char *path = MArgument_getUTF8String(args[0]);
  const char *nm   = MArgument_getUTF8String(args[1]);
  u64 in0 = (u64)MArgument_getInteger(args[2]);
  u64 in1 = (u64)MArgument_getInteger(args[3]);
  u64 in2 = (u64)MArgument_getInteger(args[4]);
  u64 in3 = (u64)MArgument_getInteger(args[5]);

  u64 result = thvm_aot_load_and_run4(path, nm, in0, in1, in2, in3);

  if (libData != NULL && libData->UTF8String_disown != NULL) {
    libData->UTF8String_disown((char *)path);
    libData->UTF8String_disown((char *)nm);
  }
  MArgument_setInteger(res, (mint)result);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_term_new_pri(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32 prim_id = (u32)MArgument_getInteger(args[0]);
  Term r = term_new_pri(prim_id);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// Phase 7 iter T: build a CTR cell in BOOK_HEAP (rather than the
// dynamic HEAP that term_new_ctr / TCtr use).  The Metal kernel's
// `device Term *heap` MTLBuffer is zero-copy bound to BOOK_HEAP, so
// CTRs destined as inputs to the Metal AOT path need to live there
// for the kernel's destructure path (heap[scrutinee_val + 1+i]) to
// dereference correctly.
//
// Args: [label (Integer), children (Integer rank-1 MTensor)].
// Returns: TAG_CTR Term whose val points into BOOK_HEAP.
EXTERN_C DLLEXPORT int thvm_wl_term_new_book_ctr(WolframLibraryData libData,
                                                  mint argc, MArgument *args,
                                                  MArgument res) {
  (void)argc;
  mint label = MArgument_getInteger(args[0]);
  MTensor t  = MArgument_getMTensor(args[1]);
  mint n     = libData->MTensor_getFlattenedLength(t);
  const mint *data = libData->MTensor_getIntegerData(t);
  if (n < 0 || n > 64) return LIBRARY_FUNCTION_ERROR;

  u64 loc = book_alloc((u64)(1 + n));
  // n_cell: NUM(n) with DT_INT32 ext, mirroring term_new_ctr.
  book_set(loc, term_new(0, TAG_NUM, DT_INT32, (u64)n));
  for (mint i = 0; i < n; i++) {
    book_set(loc + 1 + (u64)i, (Term)data[i]);
  }
  Term out = term_new(0, TAG_CTR, (u32)label, loc);
  MArgument_setInteger(res, (mint)out);
  return LIBRARY_NO_ERROR;
}

// Phase 7 iter J + Q: parallel-pool variant.  Args:
//   [path, name, n_threads, arg0, arg1, arg2, arg3]
// Dispatches the AOT'd def through aot_run_parallel with n_threads
// workers via the dylib's pooled run entry.  iter Q's per-worker
// CURRENT_WNF_STATE init in aot_worker_main makes spawned pthreads
// safe for AOT bodies that re-enter wnf.
EXTERN_C DLLEXPORT int thvm_wl_aot_run4_pooled(WolframLibraryData libData,
                                                mint argc, MArgument *args,
                                                MArgument res) {
  (void)argc;
  const char *path = MArgument_getUTF8String(args[0]);
  const char *nm   = MArgument_getUTF8String(args[1]);
  u32 n_threads = (u32)MArgument_getInteger(args[2]);
  u64 in0 = (u64)MArgument_getInteger(args[3]);
  u64 in1 = (u64)MArgument_getInteger(args[4]);
  u64 in2 = (u64)MArgument_getInteger(args[5]);
  u64 in3 = (u64)MArgument_getInteger(args[6]);

  u64 result = thvm_aot_load_and_run4_pooled(path, nm, n_threads,
                                              in0, in1, in2, in3);

  if (libData != NULL && libData->UTF8String_disown != NULL) {
    libData->UTF8String_disown((char *)path);
    libData->UTF8String_disown((char *)nm);
  }
  MArgument_setInteger(res, (mint)result);
  return LIBRARY_NO_ERROR;
}

// Phase 7 iter E: AOT-on-Metal end-to-end via WL.
// Args: [def_id, name, arg0, arg1, arg2, arg3].  Unused arg slots
// (beyond the def's TLam-peel arity) are ignored by the emitted
// kernel.  Returns the resulting Term as an Integer.
//
// Forward decl mirrors the prototype in src/backend/metal/_.m.
extern Term thvm_aot_metal_compile_and_run(
    const char *name, u32 def_id,
    Term *args, u32 n_args,
    Term *book_heap, u64 book_cells,
    u64 *book_next_inout);

EXTERN_C DLLEXPORT int thvm_wl_aot_metal_run4(
    WolframLibraryData libData, mint argc, MArgument *args, MArgument res) {
  (void)argc;
  u32 def_id = (u32)MArgument_getInteger(args[0]);
  const char *name = MArgument_getUTF8String(args[1]);
  Term targs[4];
  for (int i = 0; i < 4; i++) {
    targs[i] = (Term)MArgument_getInteger(args[2 + i]);
  }
  u64 book_next_state = BOOK_NEXT;
  Term result = thvm_aot_metal_compile_and_run(
      name, def_id, targs, 4,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  BOOK_NEXT = book_next_state;
  if (libData != NULL && libData->UTF8String_disown != NULL) {
    libData->UTF8String_disown((char *)name);
  }
  MArgument_setInteger(res, (mint)result);
  return LIBRARY_NO_ERROR;
}

// Phase 7 iter QQ: WL surface for the OP2 batch dispatcher.  Args:
//   [root_locs_mtensor (Integer rank-1 of book_heap locs of OP2 cells)]
// Returns: Integer rank-1 MTensor of N result Terms.
extern int thvm_aot_metal_op2_fold_batch(Term *book_heap, u64 book_cells,
                                          u64 *root_locs, u32 n_roots,
                                          Term *result_out);

EXTERN_C DLLEXPORT int thvm_wl_aot_metal_op2_fold_batch(
    WolframLibraryData libData, mint argc, MArgument *args, MArgument res) {
  (void)argc;
  MTensor in_t = MArgument_getMTensor(args[0]);
  mint n = libData->MTensor_getFlattenedLength(in_t);
  if (n < 0 || n > 65536) return LIBRARY_FUNCTION_ERROR;
  const mint *in_data = libData->MTensor_getIntegerData(in_t);

  // Marshal root_locs (mint -> u64) and allocate result buffer.
  u64 *root_locs = (u64 *)malloc((size_t)n * sizeof(u64));
  Term *results  = (Term *)malloc((size_t)n * sizeof(Term));
  if (root_locs == NULL || results == NULL) {
    free(root_locs); free(results);
    return LIBRARY_FUNCTION_ERROR;
  }
  for (mint i = 0; i < n; i++) root_locs[i] = (u64)in_data[i];

  int rc = thvm_aot_metal_op2_fold_batch(
      BOOK_HEAP, BOOK_CAP, root_locs, (u32)n, results);
  if (rc != 0) {
    free(root_locs); free(results);
    return LIBRARY_FUNCTION_ERROR;
  }

  // Pack results into an output MTensor.
  MTensor out_t;
  mint dims[1] = { n };
  if (libData->MTensor_new(MType_Integer, 1, dims, &out_t) != 0) {
    free(root_locs); free(results);
    return LIBRARY_FUNCTION_ERROR;
  }
  mint *out_data = libData->MTensor_getIntegerData(out_t);
  for (mint i = 0; i < n; i++) out_data[i] = (mint)results[i];

  free(root_locs); free(results);
  MArgument_setMTensor(res, out_t);
  return LIBRARY_NO_ERROR;
}

// Phase 7 iter Y: variable-arity Metal AOT run.  Args:
//   [def_id, name, args_mtensor (Integer rank-1)]
// args_mtensor carries the Term values packed as int64s.  No upper
// bound on length beyond what AotMaxBufferLength allows the kernel
// to bind.  Replaces thvm_wl_aot_metal_run4 once the WL side
// switches over.
EXTERN_C DLLEXPORT int thvm_wl_aot_metal_run_n(
    WolframLibraryData libData, mint argc, MArgument *args, MArgument res) {
  (void)argc;
  u32 def_id = (u32)MArgument_getInteger(args[0]);
  const char *name = MArgument_getUTF8String(args[1]);
  MTensor t = MArgument_getMTensor(args[2]);
  mint n    = libData->MTensor_getFlattenedLength(t);
  if (n < 0 || n > 64) {
    if (libData->UTF8String_disown != NULL) {
      libData->UTF8String_disown((char *)name);
    }
    return LIBRARY_FUNCTION_ERROR;
  }
  const mint *data = libData->MTensor_getIntegerData(t);
  Term targs[64];
  for (mint i = 0; i < n; i++) targs[i] = (Term)data[i];

  u64 book_next_state = BOOK_NEXT;
  Term result = thvm_aot_metal_compile_and_run(
      name, def_id, targs, (u32)n,
      BOOK_HEAP, BOOK_CAP, &book_next_state);
  BOOK_NEXT = book_next_state;
  if (libData != NULL && libData->UTF8String_disown != NULL) {
    libData->UTF8String_disown((char *)name);
  }
  MArgument_setInteger(res, (mint)result);
  return LIBRARY_NO_ERROR;
}

// === Iter Z+2 step 4: generic per-def runner =============================
//
// Dispatches the static aot_ic_def_run PSO with the def's book root
// Term as a constant.  No per-def MSL emit / xcrun roundtrip.
//
// Args (mint Integer):
//   args[0] : def_id (looks up DEFS[def_id] for the book root)
//   args[1] : MTensor of args (Integer, 1d) -- caller args as Term raws
// Returns: Integer Term (the WHNF result, dyn-migrated unless KEEP_BOOK).

extern Term thvm_aot_metal_ic_def_run(Term root, Term *args, u32 n_args,
    Term *book_heap, u64 book_cells, u64 *book_next_inout);

EXTERN_C DLLEXPORT int thvm_wl_aot_metal_ic_def_run(
    WolframLibraryData libData, mint argc, MArgument *args, MArgument res) {
  (void)argc;
  u32 def_id = (u32)MArgument_getInteger(args[0]);
  if (def_id >= DEFS_CAP || DEFS[def_id] == 0) return LIBRARY_FUNCTION_ERROR;
  Term root = DEFS[def_id];
  MTensor t = MArgument_getMTensor(args[1]);
  mint n = libData->MTensor_getFlattenedLength(t);
  if (n < 0 || n > 64) return LIBRARY_FUNCTION_ERROR;
  const mint *data = libData->MTensor_getIntegerData(t);
  Term targs[64];
  for (mint i = 0; i < n; i++) targs[i] = (Term)data[i];

  u64 book_next_state = BOOK_NEXT;
  Term result = thvm_aot_metal_ic_def_run(
      root, targs, (u32)n, BOOK_HEAP, BOOK_CAP, &book_next_state);
  BOOK_NEXT = book_next_state;
  MArgument_setInteger(res, (mint)result);
  return LIBRARY_NO_ERROR;
}

// === Iter Z+1: parallel cnf+collapse via aot_ic_collapse.metal ==========
//
// Walks the SUP-tree-rooted Term left in BOOK_HEAP by an iter-Z
// kernel, dispatches the static aot_ic_collapse PSO with grid =
// 2^depth, returns the per-leaf-path Term vector to WL.
//
// Args (mint Integer):
//   args[0] : root Term (the iter-Z output, BOOK_HEAP-rooted)
//   args[1] : depth (uint, must be <= 30)
// Returns: MTensor of mint Term values, length 2^depth.

extern u64 thvm_aot_metal_ic_collapse(
    Term root, u32 depth,
    Term *book_heap, u64 book_cells,
    u64 *book_next_inout,
    Term *out, u64 out_cap);

EXTERN_C DLLEXPORT int thvm_wl_aot_ic_collapse(
    WolframLibraryData libData, mint argc, MArgument *args, MArgument res) {
  (void)argc;
  Term root = (Term)MArgument_getInteger(args[0]);
  u32  depth = (u32)MArgument_getInteger(args[1]);
  if (depth > 30) return LIBRARY_FUNCTION_ERROR;
  u64 n = 1ULL << depth;
  Term *out = (Term *)malloc(n * sizeof(Term));
  if (out == NULL) return LIBRARY_FUNCTION_ERROR;
  u64 book_next_state = BOOK_NEXT;
  u64 nout = thvm_aot_metal_ic_collapse(
      root, depth, BOOK_HEAP, BOOK_CAP, &book_next_state,
      out, n);
  BOOK_NEXT = book_next_state;
  if (nout == 0) { free(out); return LIBRARY_FUNCTION_ERROR; }

  MTensor t;
  mint dims[1] = { (mint)nout };
  int err = libData->MTensor_new(MType_Integer, 1, dims, &t);
  if (err != LIBRARY_NO_ERROR) { free(out); return err; }
  mint *dst = libData->MTensor_getIntegerData(t);
  for (u64 i = 0; i < nout; i++) dst[i] = (mint)out[i];
  free(out);
  MArgument_setMTensor(res, t);
  return LIBRARY_NO_ERROR;
}

// === Path B: Survey Propagation =========================================
//
// SP iteration loop on a CNF formula.  Caller passes the formula as
// CSR-flat literal arrays (signed 1-based ints; positive = +var,
// negative = -var) plus a clause-boundary array.  Returns the per-edge
// final eta vector after convergence or max_iters.
//
// Args:
//   args[0] : MTensor [Integer, 1] cnf_lits flat -- all signed lits
//             concatenated across clauses
//   args[1] : MTensor [Integer, 1] cnf_bounds -- bounds[c+1] is the
//             exclusive end index in cnf_lits of clause c (n_clauses+1
//             elements; bounds[0] always 0)
//   args[2] : Integer n_vars
//   args[3] : Integer max_iters
//   args[4] : Real damping (alpha in [0,1])
//   args[5] : Real threshold (convergence)
// Returns: MTensor [Real, 1] of length n_edges (final eta values).

extern int thvm_aot_metal_sp_run(
    const uint32_t *edges_clause, const uint32_t *edges_var,
    const uint8_t  *edges_sign,
    const uint32_t *clause_edges_off, const uint32_t *clause_edges_flat,
    const uint32_t *var_edges_off,    const uint32_t *var_edges_flat,
    uint32_t n_edges, uint32_t n_clauses, uint32_t n_vars,
    uint32_t max_iters, float damping, float threshold,
    float *out_eta);

EXTERN_C DLLEXPORT int thvm_wl_aot_sp_run(
    WolframLibraryData libData, mint argc, MArgument *args, MArgument res) {
  (void)argc;
  MTensor lits_t   = MArgument_getMTensor(args[0]);
  MTensor bounds_t = MArgument_getMTensor(args[1]);
  u32 n_vars       = (u32)MArgument_getInteger(args[2]);
  u32 max_iters    = (u32)MArgument_getInteger(args[3]);
  double damping_d = MArgument_getReal(args[4]);
  double thresh_d  = MArgument_getReal(args[5]);

  mint n_lits   = libData->MTensor_getFlattenedLength(lits_t);
  mint n_bounds = libData->MTensor_getFlattenedLength(bounds_t);
  if (n_bounds < 2 || n_lits < 0 || n_vars == 0) return LIBRARY_FUNCTION_ERROR;
  u32 n_edges   = (u32)n_lits;
  u32 n_clauses = (u32)(n_bounds - 1);

  const mint *src_lits   = libData->MTensor_getIntegerData(lits_t);
  const mint *src_bounds = libData->MTensor_getIntegerData(bounds_t);

  // Build edge arrays + CSR adjacencies.
  uint32_t *edges_clause = (uint32_t *)malloc(n_edges * sizeof(uint32_t));
  uint32_t *edges_var    = (uint32_t *)malloc(n_edges * sizeof(uint32_t));
  uint8_t  *edges_sign   = (uint8_t  *)malloc(n_edges * sizeof(uint8_t));
  uint32_t *clause_off   = (uint32_t *)malloc((n_clauses + 1) * sizeof(uint32_t));
  uint32_t *clause_flat  = (uint32_t *)malloc(n_edges * sizeof(uint32_t));
  uint32_t *var_off      = (uint32_t *)malloc((n_vars + 1) * sizeof(uint32_t));
  uint32_t *var_flat     = (uint32_t *)malloc(n_edges * sizeof(uint32_t));
  float    *out_eta      = (float    *)malloc(n_edges * sizeof(float));
  if (!edges_clause || !edges_var || !edges_sign ||
      !clause_off || !clause_flat || !var_off || !var_flat || !out_eta) {
    free(edges_clause); free(edges_var); free(edges_sign);
    free(clause_off); free(clause_flat); free(var_off); free(var_flat);
    free(out_eta);
    return LIBRARY_FUNCTION_ERROR;
  }

  // Fill edges from lits/bounds.  Clause c's edges are at indices
  // [bounds[c], bounds[c+1]).  Each lit is a signed 1-based var idx.
  for (u32 c = 0; c < n_clauses; c++) {
    clause_off[c] = (uint32_t)src_bounds[c];
  }
  clause_off[n_clauses] = (uint32_t)src_bounds[n_clauses];
  // clause_flat is the identity mapping at this point: edge e is at
  // position e in clause_flat for the clause it belongs to.
  for (u32 e = 0; e < n_edges; e++) clause_flat[e] = e;
  // Determine which clause each edge belongs to.
  u32 e = 0;
  for (u32 c = 0; c < n_clauses; c++) {
    u32 c_end = (uint32_t)src_bounds[c + 1];
    while (e < c_end) {
      mint lit = src_lits[e];
      u32 var_idx = (uint32_t)((lit > 0) ? (lit - 1) : (-lit - 1));
      edges_clause[e] = c;
      edges_var[e]    = var_idx;
      edges_sign[e]   = (lit > 0) ? 0u : 1u;  // 0=positive, 1=negative
      e++;
    }
  }
  // Build var->edges CSR.  Two-pass count + scatter.
  for (u32 v = 0; v <= n_vars; v++) var_off[v] = 0;
  for (u32 ee = 0; ee < n_edges; ee++) var_off[edges_var[ee] + 1]++;
  for (u32 v = 1; v <= n_vars; v++) var_off[v] += var_off[v - 1];
  uint32_t *var_cursor = (uint32_t *)malloc(n_vars * sizeof(uint32_t));
  for (u32 v = 0; v < n_vars; v++) var_cursor[v] = var_off[v];
  for (u32 ee = 0; ee < n_edges; ee++) {
    u32 v = edges_var[ee];
    var_flat[var_cursor[v]++] = ee;
  }
  free(var_cursor);

  int rc = thvm_aot_metal_sp_run(
      edges_clause, edges_var, edges_sign,
      clause_off, clause_flat, var_off, var_flat,
      n_edges, n_clauses, n_vars,
      max_iters, (float)damping_d, (float)thresh_d,
      out_eta);

  free(edges_clause); free(edges_var); free(edges_sign);
  free(clause_off); free(clause_flat); free(var_off); free(var_flat);
  if (rc < 0) { free(out_eta); return LIBRARY_FUNCTION_ERROR; }

  MTensor rt;
  mint dims[1] = { (mint)n_edges };
  int err = libData->MTensor_new(MType_Real, 1, dims, &rt);
  if (err != LIBRARY_NO_ERROR) { free(out_eta); return err; }
  double *dst = libData->MTensor_getRealData(rt);
  for (u32 ee = 0; ee < n_edges; ee++) dst[ee] = (double)out_eta[ee];
  free(out_eta);
  MArgument_setMTensor(res, rt);
  return LIBRARY_NO_ERROR;
}

// === Path B step 2: SP-DEC SAT solver ===================================
//
// Top-level Survey Propagation + decimation solver.  Returns a
// satisfying assignment if found, indicates UNSAT, or reports
// "gave up" if neither resolved within max-decimations.
//
// Args:
//   args[0] : MTensor [Integer, 1] cnf_lits flat (signed 1-based)
//   args[1] : MTensor [Integer, 1] cnf_bounds (n_clauses+1)
//   args[2] : Integer n_vars
//   args[3] : Integer sp_max_iters (per-decimation SP cap)
//   args[4] : Real damping
//   args[5] : Real threshold
// Returns: MTensor [Integer, 1] of length n_vars+1; first entry is
//   status code (0=SAT, -1=UNSAT, 1=GAVE_UP, -2=ERROR), then n_vars
//   entries of {-1,+1} (assignment); meaningful only if status=0.

extern int thvm_aot_metal_sp_solve(
    const int32_t *cnf_lits, const uint32_t *cnf_bounds,
    uint32_t n_clauses, uint32_t n_vars,
    uint32_t sp_max_iters, float damping, float threshold,
    int8_t *out_assignment);

EXTERN_C DLLEXPORT int thvm_wl_aot_sp_solve(
    WolframLibraryData libData, mint argc, MArgument *args, MArgument res) {
  (void)argc;
  MTensor lits_t   = MArgument_getMTensor(args[0]);
  MTensor bounds_t = MArgument_getMTensor(args[1]);
  u32 n_vars       = (u32)MArgument_getInteger(args[2]);
  u32 sp_max_iters = (u32)MArgument_getInteger(args[3]);
  double damping_d = MArgument_getReal(args[4]);
  double thresh_d  = MArgument_getReal(args[5]);

  mint n_lits   = libData->MTensor_getFlattenedLength(lits_t);
  mint n_bounds = libData->MTensor_getFlattenedLength(bounds_t);
  if (n_bounds < 2 || n_lits < 0 || n_vars == 0) return LIBRARY_FUNCTION_ERROR;
  u32 n_clauses = (u32)(n_bounds - 1);

  const mint *src_lits   = libData->MTensor_getIntegerData(lits_t);
  const mint *src_bounds = libData->MTensor_getIntegerData(bounds_t);

  int32_t  *cnf_lits    = (int32_t *)malloc(n_lits * sizeof(int32_t));
  uint32_t *cnf_bounds  = (uint32_t *)malloc(n_bounds * sizeof(uint32_t));
  int8_t   *assign      = (int8_t   *)malloc(n_vars * sizeof(int8_t));
  if (!cnf_lits || !cnf_bounds || !assign) {
    free(cnf_lits); free(cnf_bounds); free(assign);
    return LIBRARY_FUNCTION_ERROR;
  }
  for (mint i = 0; i < n_lits; i++)   cnf_lits[i]   = (int32_t)src_lits[i];
  for (mint i = 0; i < n_bounds; i++) cnf_bounds[i] = (uint32_t)src_bounds[i];

  int rc = thvm_aot_metal_sp_solve(
      cnf_lits, cnf_bounds, n_clauses, n_vars,
      sp_max_iters, (float)damping_d, (float)thresh_d, assign);

  free(cnf_lits); free(cnf_bounds);

  MTensor rt;
  mint dims[1] = { (mint)(n_vars + 1) };
  int err = libData->MTensor_new(MType_Integer, 1, dims, &rt);
  if (err != LIBRARY_NO_ERROR) { free(assign); return err; }
  mint *dst = libData->MTensor_getIntegerData(rt);
  dst[0] = (mint)rc;
  for (u32 v = 0; v < n_vars; v++) dst[v + 1] = (mint)assign[v];
  free(assign);
  MArgument_setMTensor(res, rt);
  return LIBRARY_NO_ERROR;
}

// === Lever 3: bitmask CNF eval ===========================================
//
// Direct CNF evaluation kernel; bypasses IC reduction.
//
// Args:
//   args[0] : MTensor of pos_masks (Integer, 1D, length n_clauses)
//   args[1] : MTensor of neg_masks (Integer, 1D, length n_clauses)
//   args[2] : n_vars (must equal log2 of returned tensor length, <= 30)
// Returns: MTensor of length 2^n_vars (Integer 0/1 per assignment).

extern u64 thvm_aot_metal_cnf_bitmask(
    const uint32_t *clauses_pos, const uint32_t *clauses_neg,
    uint32_t n_clauses, uint32_t n_vars,
    uint32_t *out, u64 out_cap);

EXTERN_C DLLEXPORT int thvm_wl_aot_cnf_bitmask(
    WolframLibraryData libData, mint argc, MArgument *args, MArgument res) {
  (void)argc;
  MTensor pos_t = MArgument_getMTensor(args[0]);
  MTensor neg_t = MArgument_getMTensor(args[1]);
  u32 n_vars = (u32)MArgument_getInteger(args[2]);

  mint n_clauses_p = libData->MTensor_getFlattenedLength(pos_t);
  mint n_clauses_n = libData->MTensor_getFlattenedLength(neg_t);
  if (n_clauses_p != n_clauses_n || n_clauses_p < 0 || n_vars > 30) {
    return LIBRARY_FUNCTION_ERROR;
  }
  u32 n_clauses = (u32)n_clauses_p;
  u64 n_leaves = 1ULL << n_vars;

  const mint *src_pos = libData->MTensor_getIntegerData(pos_t);
  const mint *src_neg = libData->MTensor_getIntegerData(neg_t);

  uint32_t *pos = (uint32_t *)malloc(n_clauses * sizeof(uint32_t));
  uint32_t *neg = (uint32_t *)malloc(n_clauses * sizeof(uint32_t));
  uint32_t *out = (uint32_t *)malloc(n_leaves * sizeof(uint32_t));
  if (!pos || !neg || !out) { free(pos); free(neg); free(out); return LIBRARY_FUNCTION_ERROR; }
  for (u32 i = 0; i < n_clauses; i++) {
    pos[i] = (uint32_t)src_pos[i];
    neg[i] = (uint32_t)src_neg[i];
  }

  u64 nout = thvm_aot_metal_cnf_bitmask(pos, neg, n_clauses, n_vars,
                                         out, n_leaves);
  free(pos); free(neg);
  if (nout == 0) { free(out); return LIBRARY_FUNCTION_ERROR; }

  MTensor rt;
  mint dims[1] = { (mint)nout };
  int err = libData->MTensor_new(MType_Integer, 1, dims, &rt);
  if (err != LIBRARY_NO_ERROR) { free(out); return err; }
  mint *dst = libData->MTensor_getIntegerData(rt);
  for (u64 i = 0; i < nout; i++) dst[i] = (mint)out[i];
  free(out);
  MArgument_setMTensor(res, rt);
  return LIBRARY_NO_ERROR;
}

// === PRI-WL callback dispatch =============================================
// THVM_PRIM_PRI fires inside wnf, deep in C-side recursion.  Two paths:
//
//   (A) SYNCHRONOUS via callLibraryCallbackFunction.  Available for
//       slots whose callback is a CompiledFunction (the ONLY thing
//       ConnectLibraryCallbackFunction accepts).  Re-entry is safe
//       because compiled code runs in OUR thread, not the kernel
//       evaluator -- no deadlock.  Restriction: CompiledFunction's
//       body must be numerical; Print/$var/patterns hit cfex and
//       silently fail INSIDE the re-entry.  WL surface checks the
//       Head and routes to this path only for CompiledFunctions.
//
//   (B) QUEUED FALLBACK.  For Function / Symbol / anything non-
//       compiled, prim_pri appends (slot, snapshotted-value) to a
//       fixed-cap queue; WL's TPriDrain[] dequeues + dispatches
//       between TWnf invocations.  Snapshotting (pri_snapshot_value)
//       ensures recursive-loop iterations each see THEIR value
//       rather than the buffer's last-written state.
//
// Why not WSTP for arbitrary-WL sync?  EvaluatePacket from inside a
// LibraryFunction call deadlocks: the kernel is blocked waiting on
// our return, can't process incoming packets.  Verified empirically.
#define THVM_PRI_QUEUE_CAP 65536
#define THVM_PRI_SLOT_CAP    256
typedef struct { u32 slot; Term value; } PriQueueEntry;
static PriQueueEntry PRI_QUEUE[THVM_PRI_QUEUE_CAP];
static u32           PRI_QUEUE_LEN = 0;
static mint          PRI_CB_ID[THVM_PRI_SLOT_CAP] = {0};   // 0 = unbound

// (C) FOREIGN CALLBACK pointers: CreateForeignCallback in WL produces
// a libffi closure -- a regular C function pointer that, when called,
// transitions back into the WL kernel evaluator and runs the registered
// WL function.  Re-entry is safe (libffi handles the kernel state) and
// works for ARBITRARY WL functions (no Compile restriction).
//
// Signature: int64_t (*)(int64_t).  WL callbacks receive the wnf'd Term
// as a raw int64; their return value OVERRIDES the redex result if
// nonzero, else the redex falls through to `cont`.  Trace-only callbacks
// just return 0; rewriting callbacks return the new Term.
typedef int64_t (*pri_foreign_fn)(int64_t);
static pri_foreign_fn PRI_FOREIGN_CB[THVM_PRI_SLOT_CAP] = {NULL};

EXTERN_C DLLEXPORT void thvm_pri_bind_foreign(int slot, void *fnptr) {
  if (slot < 0 || slot >= THVM_PRI_SLOT_CAP) return;
  PRI_FOREIGN_CB[slot] = (pri_foreign_fn)fnptr;
}

EXTERN_C DLLEXPORT void thvm_pri_unbind_foreign(int slot) {
  if (slot < 0 || slot >= THVM_PRI_SLOT_CAP) return;
  PRI_FOREIGN_CB[slot] = NULL;
}

EXTERN_C DLLEXPORT int thvm_wl_pri_last_cb_id(WolframLibraryData libData, mint argc,
                                              MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, LAST_PRI_CB_ID);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_bind_pri_slot(WolframLibraryData libData, mint argc,
                                             MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  mint slot  = MArgument_getInteger(args[0]);
  mint cb_id = MArgument_getInteger(args[1]);
  if (slot < 0 || slot >= THVM_PRI_SLOT_CAP) {
    MArgument_setInteger(res, 0);
    return LIBRARY_FUNCTION_ERROR;
  }
  PRI_CB_ID[slot] = cb_id;
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_unbind_pri_slot(WolframLibraryData libData, mint argc,
                                               MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  mint slot = MArgument_getInteger(args[0]);
  if (slot < 0 || slot >= THVM_PRI_SLOT_CAP) {
    MArgument_setInteger(res, 0);
    return LIBRARY_FUNCTION_ERROR;
  }
  if (PRI_CB_ID[slot] != 0 && CACHED_LIB_DATA) {
    CACHED_LIB_DATA->releaseLibraryCallbackFunction(PRI_CB_ID[slot]);
    PRI_CB_ID[slot] = 0;
  }
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

static Term pri_snapshot_value(Term v) {
  if (term_tag(v) != TAG_TEN) return v;
  u32 src_tid = (u32)term_val(v);
  if (src_tid == 0 || src_tid >= TENS_NEXT) return v;
  TenDesc *sd = &TENS[src_tid];
  if (sd->backend == NULL) return v;
  // Phase A: gate on the dtypes whose buf_read/buf_write paths exist.
  if (sd->dtype != DT_FP32 && sd->dtype != DT_INT32) return v;
  u32 dst_tid = tensor_alloc(sd->backend, sd->view.shape, sd->dtype);
  if (dst_tid == 0) return v;
  u64 nbytes = dtype_storage_bytes(sd->dtype, sd->view.numel);
  void *tmp = malloc((size_t)nbytes);
  if (!tmp) { tensor_release(dst_tid); return v; }
  sd->backend->buf_read (sd->buf_id, tmp, nbytes);
  TENS[dst_tid].backend->buf_write(TENS[dst_tid].buf_id, tmp, nbytes);
  free(tmp);
  return term_new(0, TAG_TEN, sd->dtype, dst_tid);
}

// Returning variant: invoke the slot's callback synchronously and
// return its result Term (0 = no override, anything else = override
// the redex result).  prim_pri uses this; a non-zero return becomes
// the new redex value, otherwise the redex falls through to `cont`.
//
// Only the foreign-callback path can RETURN a value (libffi marshalls
// the WL fn's Integer return back as int64).  CompiledFunction +
// queued paths are observe-only -- they always yield 0 here.
Term thvm_pri_wl_invoke_returning(u32 slot, Term value) {
  if (slot < THVM_PRI_SLOT_CAP && PRI_FOREIGN_CB[slot] != NULL) {
    return (Term)PRI_FOREIGN_CB[slot]((int64_t)value);
  }
  if (slot < THVM_PRI_SLOT_CAP && PRI_CB_ID[slot] != 0 && CACHED_LIB_DATA) {
    MArgument cb_args[1];
    MArgument cb_res;
    mint v_int = (mint)value;
    cb_args[0].integer = &v_int;
    mint res_int = 0;
    cb_res.integer = &res_int;
    CACHED_LIB_DATA->callLibraryCallbackFunction(
        PRI_CB_ID[slot], 1, cb_args, cb_res);
    return 0;   // Compiled callbacks don't return Terms
  }
  if (PRI_QUEUE_LEN < THVM_PRI_QUEUE_CAP) {
    PRI_QUEUE[PRI_QUEUE_LEN].slot  = slot;
    PRI_QUEUE[PRI_QUEUE_LEN].value = pri_snapshot_value(value);
    PRI_QUEUE_LEN++;
  }
  return 0;
}

// Legacy void variant -- kept for compatibility; just discards the
// returned override.
void thvm_pri_wl_invoke(u32 slot, Term value) {
  (void)thvm_pri_wl_invoke_returning(slot, value);
}

EXTERN_C DLLEXPORT int thvm_wl_pri_drain(WolframLibraryData libData, mint argc,
                                         MArgument *args, MArgument res) {
  (void)argc; (void)args;
  mint n     = (mint)PRI_QUEUE_LEN;
  mint dims[1] = {n * 2};
  MTensor out;
  libData->MTensor_new(MType_Integer, 1, dims, &out);
  mint *dst = libData->MTensor_getIntegerData(out);
  for (mint i = 0; i < n; i++) {
    dst[i * 2 + 0] = (mint)PRI_QUEUE[i].slot;
    dst[i * 2 + 1] = (mint)PRI_QUEUE[i].value;
  }
  PRI_QUEUE_LEN = 0;
  MArgument_setMTensor(res, out);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_term_new_op2(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32  op = (u32) MArgument_getInteger(args[0]);
  Term x  = (Term)MArgument_getInteger(args[1]);
  Term y  = (Term)MArgument_getInteger(args[2]);
  Term r = term_new_op2(op, x, y);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_term_new_mat(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32  m = (u32) MArgument_getInteger(args[0]);
  Term h = (Term)MArgument_getInteger(args[1]);
  Term f = (Term)MArgument_getInteger(args[2]);
  Term r = term_new_mat(m, h, f);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// TAG_EQL constructor: structural equality, strict on both args.
// Reducer (src/wnf/_.c) handles NUM-NUM, ERA/ANY short-circuits, and
// EQL-SUP commutes (both sides).  EQL-CTR / EQL-LAM rules come with
// the upcoming HVM4-port.  See `term_new_eql` in src/term/new_eql.c.
EXTERN_C DLLEXPORT int thvm_wl_term_new_eql(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term a = (Term)MArgument_getInteger(args[0]);
  Term b = (Term)MArgument_getInteger(args[1]);
  Term r = term_new_eql(a, b);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// Dynamic-label SUP / DUP constructors.  The label is a *Term* (the
// caller already packed it via TVarFor / TNum / etc) so DSU/DDU can
// be built around any computed label term.
EXTERN_C DLLEXPORT int thvm_wl_term_new_dsu(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term lab = (Term)MArgument_getInteger(args[0]);
  Term a   = (Term)MArgument_getInteger(args[1]);
  Term b   = (Term)MArgument_getInteger(args[2]);
  Term r   = term_new_dsu(lab, a, b);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_term_new_ddu(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term lab = (Term)MArgument_getInteger(args[0]);
  Term v   = (Term)MArgument_getInteger(args[1]);
  Term bod = (Term)MArgument_getInteger(args[2]);
  Term r   = term_new_ddu(lab, v, bod);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// Structural equality (no reduction): 1 if a and b decode to the
// same tree shape modulo VAR alpha-aliasing, 0 otherwise.  Use
// thvm_wl_term_eq for the reducing variant.
EXTERN_C DLLEXPORT int thvm_wl_term_eq_struct(WolframLibraryData libData,
                                              mint argc, MArgument *args,
                                              MArgument res) {
  (void)libData; (void)argc;
  Term a = (Term)MArgument_getInteger(args[0]);
  Term b = (Term)MArgument_getInteger(args[1]);
  MArgument_setInteger(res, (mint)term_eq_struct(a, b));
  return LIBRARY_NO_ERROR;
}

// Reducing equality: cnf both sides, then structural.  Returns 1 /
// 0.  The result is well-defined only when both terms cnf to a
// SUP-free / CNF shape; if cnf returns a SUP at the root, the
// caller should collapse explicitly first.
EXTERN_C DLLEXPORT int thvm_wl_term_eq(WolframLibraryData libData,
                                       mint argc, MArgument *args,
                                       MArgument res) {
  (void)libData; (void)argc;
  Term a = (Term)MArgument_getInteger(args[0]);
  Term b = (Term)MArgument_getInteger(args[1]);
  MArgument_setInteger(res, (mint)term_eq_cnf(a, b));
  return LIBRARY_NO_ERROR;
}

// === book heap / defs / ALO state I/O ===
//
// Used by Heap.wl HeapSnapshot / HeapInitialize to bundle DEFS,
// BOOK_HEAP, and ALO_STATES into a portable snapshot so a heap can
// survive a fresh kernel (TFree + TInit).  thvm_wl_reset clears only
// the dynamic heap; cross-restart roundtrip needs explicit access to
// the book/defs/alo state tables.

EXTERN_C DLLEXPORT int thvm_wl_book_pos(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)BOOK_NEXT);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_book_read(WolframLibraryData libData, mint argc,
                                         MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u64 loc = (u64)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)book_read(loc));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_book_alloc(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u64 size = (u64)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)book_alloc(size));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_book_set(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u64  loc = (u64) MArgument_getInteger(args[0]);
  Term t   = (Term)MArgument_getInteger(args[1]);
  book_set(loc, t);
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_def_get(WolframLibraryData libData, mint argc,
                                       MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32 slot = (u32)MArgument_getInteger(args[0]);
  Term t = (slot < DEFS_CAP) ? DEFS[slot] : 0;
  MArgument_setInteger(res, (mint)t);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_def_set(WolframLibraryData libData, mint argc,
                                       MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32  slot = (u32) MArgument_getInteger(args[0]);
  Term t    = (Term)MArgument_getInteger(args[1]);
  if (slot < DEFS_CAP) DEFS[slot] = t;
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_alo_states_next(WolframLibraryData libData, mint argc,
                                               MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)ALO_STATES_NEXT);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_alo_state_parent(WolframLibraryData libData, mint argc,
                                                MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32 id = (u32)MArgument_getInteger(args[0]);
  u32 v  = (id < ALO_STATE_CAP && ALO_STATES) ? ALO_STATES[id].parent : 0;
  MArgument_setInteger(res, (mint)v);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_alo_state_old_loc(WolframLibraryData libData, mint argc,
                                                 MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32 id = (u32)MArgument_getInteger(args[0]);
  u64 v  = (id < ALO_STATE_CAP && ALO_STATES) ? ALO_STATES[id].old_loc : 0;
  MArgument_setInteger(res, (mint)v);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_alo_state_new_loc(WolframLibraryData libData, mint argc,
                                                 MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32 id = (u32)MArgument_getInteger(args[0]);
  u64 v  = (id < ALO_STATE_CAP && ALO_STATES) ? ALO_STATES[id].new_loc : 0;
  MArgument_setInteger(res, (mint)v);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_alo_state_set(WolframLibraryData libData, mint argc,
                                             MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32 id      = (u32)MArgument_getInteger(args[0]);
  u32 parent  = (u32)MArgument_getInteger(args[1]);
  u64 old_loc = (u64)MArgument_getInteger(args[2]);
  u64 new_loc = (u64)MArgument_getInteger(args[3]);
  if (id < ALO_STATE_CAP && ALO_STATES) {
    ALO_STATES[id].parent  = parent;
    ALO_STATES[id].old_loc = old_loc;
    ALO_STATES[id].new_loc = new_loc;
  }
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_alo_states_set_next(WolframLibraryData libData, mint argc,
                                                   MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32 n = (u32)MArgument_getInteger(args[0]);
  ALO_STATES_NEXT = n;
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_book_set_next(WolframLibraryData libData, mint argc,
                                             MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u64 n = (u64)MArgument_getInteger(args[0]);
  BOOK_NEXT = n;
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_book_reset(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  if (BOOK_HEAP)  memset(BOOK_HEAP,  0, BOOK_CAP * sizeof(Term));
  if (ALO_STATES) memset(ALO_STATES, 0, ALO_STATE_CAP * sizeof(AloState));
  for (u32 i = 0; i < DEFS_CAP; i++) DEFS[i] = 0;
  BOOK_NEXT       = 1;
  ALO_STATES_NEXT = 1;
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

// === multi-context API ===
//
// Used by WL Context.wl to allocate / select / inspect / destroy
// contexts.  All scalar-in / scalar-out (slot ids are u32).

EXTERN_C DLLEXPORT int thvm_wl_context_create(WolframLibraryData libData, mint argc,
                                              MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  // arg 0 = device name as Integer code (0=cpu, 1=metal); WL bridge
  // passes through dtypeCode-style enums so the C side avoids any
  // string handling.  -1 = "default" (NULL).
  mint dev = MArgument_getInteger(args[0]);
  const char *name = NULL;
  if      (dev == THVM_DEV_CPU)   name = "cpu";
  else if (dev == THVM_DEV_METAL) name = "metal";
  u32 slot = thvm_context_create(name);
  MArgument_setInteger(res, (mint)slot);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_context_select(WolframLibraryData libData, mint argc,
                                              MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32 slot = (u32)MArgument_getInteger(args[0]);
  u32 prev = thvm_context_select(slot);
  MArgument_setInteger(res, (mint)prev);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_context_current(WolframLibraryData libData, mint argc,
                                               MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)thvm_context_current());
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_context_destroy(WolframLibraryData libData, mint argc,
                                               MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32 slot = (u32)MArgument_getInteger(args[0]);
  thvm_context_destroy(slot);
  MArgument_setInteger(res, 1);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_context_count(WolframLibraryData libData, mint argc,
                                             MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  u32 n = 0;
  for (u32 i = 0; i < CONTEXTS_CAP; i++) if (CONTEXTS[i]) n++;
  MArgument_setInteger(res, (mint)n);
  return LIBRARY_NO_ERROR;
}

// === Worker pool: thread count + per-run stats introspection ======
//
// `TThreads[n]` -> thvm_wl_pool_set_threads sets the worker count nf
// uses on the next call; 0 reverts to env-var precedence.
// `TThreads[]`  -> thvm_wl_pool_get_threads returns the resolved count.
// `TPoolStats[]` then reads scalar fields out of the most recent
// captured snapshot.
//
// Stats fields are exposed by integer code so the WL side can iterate
// without depending on struct layout.  The codes are mirrored in
// Pool.wl ($TPoolStatField).

EXTERN_C DLLEXPORT int thvm_wl_pool_set_threads(WolframLibraryData libData,
                                                mint argc, MArgument *args,
                                                MArgument res) {
  (void)libData; (void)argc;
  mint n = MArgument_getInteger(args[0]);
  if (n < 0) n = 0;
  nf_set_threads((u32)n);
  MArgument_setInteger(res, (mint)nf_get_threads());
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_pool_get_threads(WolframLibraryData libData,
                                                mint argc, MArgument *args,
                                                MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)nf_get_threads());
  return LIBRARY_NO_ERROR;
}

// Pool-level scalars (n_workers, drain_rounds, drain_wall_ns, total_fires)
// addressed by integer code.  Returns 0 for unknown codes.
EXTERN_C DLLEXPORT int thvm_wl_pool_stats_pool_field(WolframLibraryData libData,
                                                     mint argc, MArgument *args,
                                                     MArgument res) {
  (void)libData; (void)argc;
  mint code = MArgument_getInteger(args[0]);
  const WnfPoolStats *s = wnf_pool_last_stats();
  u64 v = 0;
  switch (code) {
    case 0: v = s->n_workers;     break;   // n_workers
    case 1: v = s->drain_rounds;  break;   // drain_rounds
    case 2: v = s->drain_wall_ns; break;   // drain_wall_ns
    case 3: v = s->total_fires;   break;   // total_fires
    default: v = 0;
  }
  MArgument_setInteger(res, (mint)v);
  return LIBRARY_NO_ERROR;
}

// Per-worker scalar by (worker_id, field_code).  Field codes:
//   0 fires, 1 steals, 2 steal_attempts, 3 pushes,
//   4 active_ns, 5 idle_ns, 6 wakeups, 7 itrs_delta.
// Returns 0 if the worker_id is out of range or the code is unknown.
EXTERN_C DLLEXPORT int thvm_wl_pool_stats_worker_field(WolframLibraryData libData,
                                                       mint argc, MArgument *args,
                                                       MArgument res) {
  (void)libData; (void)argc;
  mint wid  = MArgument_getInteger(args[0]);
  mint code = MArgument_getInteger(args[1]);
  const WnfPoolStats *s = wnf_pool_last_stats();
  u64 v = 0;
  if (wid >= 0 && wid < (mint)s->n_workers) {
    const WnfWorkerStats *w = &s->workers[wid];
    switch (code) {
      case 0: v = w->fires;          break;
      case 1: v = w->steals;         break;
      case 2: v = w->steal_attempts; break;
      case 3: v = w->pushes;         break;
      case 4: v = w->active_ns;      break;
      case 5: v = w->idle_ns;        break;
      case 6: v = w->wakeups;        break;
      case 7: v = w->itrs_delta;     break;
      default: v = 0;
    }
  }
  MArgument_setInteger(res, (mint)v);
  return LIBRARY_NO_ERROR;
}

// === Phase E DAG-mode KOpt apply ===
// Mirror of py_uop_dag_apply_kopt in py/csource/thvm_py.c.  Lets WL
// drive the C-side DAG rewriter so wl/THVMLink/Kernel/Rewrite.wl can
// cross-validate its symbolic rule output against C output (both
// snapshotted via TTermExpr).
//
// args = (root_term, op, axis, arg)
// returns: new root term (0 on bail / unsupported KOpt)
EXTERN_C DLLEXPORT int thvm_wl_uop_dag_apply_kopt(WolframLibraryData libData,
                                                  mint argc,
                                                  MArgument *args,
                                                  MArgument res) {
  (void)libData; (void)argc;
  Term root = (Term)MArgument_getInteger(args[0]);
  KOpt opt;
  opt.op   = (u8)MArgument_getInteger(args[1]);
  opt.axis = (u8)MArgument_getInteger(args[2]);
  opt.arg  = (u32)MArgument_getInteger(args[3]);
  Term out = uop_dag_apply_kopt(root, opt);
  MArgument_setInteger(res, (mint)out);
  return LIBRARY_NO_ERROR;
}

// === Phase E UOp constructors ===
// The Phase-E INDEX-layer + BUFFER/STORE/AFTER/OPT opcodes need
// extern wrappers exposed to WL so the rewrite.wlt cross-validation
// suite can build canonical DAGs (RANGE-based matmul, softmax-shape
// reduces, etc.) without going through Python.  Mirror the matching
// py_uop_* exports in py/csource/thvm_py.c.

EXTERN_C DLLEXPORT int thvm_wl_uop_range(WolframLibraryData libData,
                                          mint argc, MArgument *args,
                                          MArgument res) {
  (void)libData; (void)argc;
  u32 axis_id   = (u32)MArgument_getInteger(args[0]);
  u32 axis_type = (u32)MArgument_getInteger(args[1]);
  u32 extent    = (u32)MArgument_getInteger(args[2]);
  MArgument_setInteger(res, (mint)uop_range(axis_id, axis_type, extent));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_buffer(WolframLibraryData libData,
                                           mint argc, MArgument *args,
                                           MArgument res) {
  (void)argc;
  u32 scope    = (u32)MArgument_getInteger(args[0]);
  u32 dtype    = (u32)MArgument_getInteger(args[1]);
  MTensor dims = MArgument_getMTensor(args[2]);
  u32 instance = (u32)MArgument_getInteger(args[3]);
  mint  n      = libData->MTensor_getFlattenedLength(dims);
  mint *raw    = libData->MTensor_getIntegerData(dims);
  u32   buf[MAX_DIM];
  for (mint i = 0; i < n && i < (mint)MAX_DIM; i++) buf[i] = (u32)raw[i];
  Term r = (instance == 0)
    ? uop_buffer     (scope, dtype, (u32)n, buf)
    : uop_buffer_inst(scope, dtype, (u32)n, buf, instance);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_index_e(WolframLibraryData libData,
                                            mint argc, MArgument *args,
                                            MArgument res) {
  (void)libData; (void)argc;
  Term buf  = (Term)MArgument_getInteger(args[0]);
  Term addr = (Term)MArgument_getInteger(args[1]);
  MArgument_setInteger(res, (mint)uop_index_e(buf, addr));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_int_binary(WolframLibraryData libData,
                                               mint argc, MArgument *args,
                                               MArgument res) {
  (void)libData; (void)argc;
  u32  opcode = (u32)MArgument_getInteger(args[0]);
  Term a = (Term)MArgument_getInteger(args[1]);
  Term b = (Term)MArgument_getInteger(args[2]);
  MArgument_setInteger(res, (mint)uop_int_binary(opcode, a, b));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_iwhere(WolframLibraryData libData,
                                           mint argc, MArgument *args,
                                           MArgument res) {
  (void)libData; (void)argc;
  Term cond  = (Term)MArgument_getInteger(args[0]);
  Term then_ = (Term)MArgument_getInteger(args[1]);
  Term else_ = (Term)MArgument_getInteger(args[2]);
  MArgument_setInteger(res, (mint)uop_iwhere(cond, then_, else_));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_invalid(WolframLibraryData libData,
                                            mint argc, MArgument *args,
                                            MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)uop_invalid());
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_opt(WolframLibraryData libData,
                                        mint argc, MArgument *args,
                                        MArgument res) {
  (void)libData; (void)argc;
  Term target = (Term)MArgument_getInteger(args[0]);
  u32  kind   = (u32)MArgument_getInteger(args[1]);
  u32  factor = (u32)MArgument_getInteger(args[2]);
  MArgument_setInteger(res, (mint)uop_opt(target, kind, factor));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_store(WolframLibraryData libData,
                                          mint argc, MArgument *args,
                                          MArgument res) {
  (void)libData; (void)argc;
  Term buf   = (Term)MArgument_getInteger(args[0]);
  Term addr  = (Term)MArgument_getInteger(args[1]);
  Term value = (Term)MArgument_getInteger(args[2]);
  MArgument_setInteger(res, (mint)uop_store(buf, addr, value));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_after(WolframLibraryData libData,
                                          mint argc, MArgument *args,
                                          MArgument res) {
  (void)libData; (void)argc;
  Term node       = (Term)MArgument_getInteger(args[0]);
  Term after_node = (Term)MArgument_getInteger(args[1]);
  MArgument_setInteger(res, (mint)uop_after(node, after_node));
  return LIBRARY_NO_ERROR;
}

// Integer-NUM atom (for stride coefficients and similar where the
// classifiers expect UOP_CONST(DT_INT32, bits).  Mirror of
// py_term_iconst.
EXTERN_C DLLEXPORT int thvm_wl_term_iconst(WolframLibraryData libData,
                                            mint argc, MArgument *args,
                                            MArgument res) {
  (void)libData; (void)argc;
  i32 v = (i32)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)uop_const(DT_INT32, (u32)v));
  return LIBRARY_NO_ERROR;
}

// === ATP LibraryLink entries live in thvmlink_atp.c.  Single-TU
//     build is preserved by including the file directly. ===
#include "thvmlink_atp.c"
