// thvm_py.c -- thin extern "C" wrappers around thvm's static-inline UOp
// constructors, so ctypes can drive thvm from Python.
//
// Build (Darwin):
//   clang -shared -fPIC -O2 -DACCELERATE_NEW_LAPACK \
//     -framework Accelerate \
//     -o py/thvm/libthvm_py.dylib py/csource/thvm_py.c
//
// The whole runtime is single-TU via #include "src/thvm.c"; the static
// inline `fn` declarations resolve inside this TU and are re-exported
// through the wrapper functions below.

#include "../../src/thvm.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPORT __attribute__((visibility("default")))

// ---------------- runtime lifecycle ----------------
// Python objects hold raw heap Terms (every lazy Tensor's graph), so the
// heap collection a realize wants is deferred: Thvm runs it through
// py_gc_collect with its live Terms as roots and rebinds them.
EXPORT void py_thvm_init(void) {
  thvm_init();
  thvm_gc_defer(1);
}
EXPORT void py_thvm_free(void) { thvm_free(); }

// ---------------- term inspection ----------------
EXPORT uint32_t py_term_tag(uint64_t t) { return term_tag(t); }
EXPORT uint64_t py_term_val(uint64_t t) { return term_val(t); }

// ---------------- atom constructors ----------------
EXPORT uint64_t py_term_iconst(int32_t v) {
  // UOP_CONST(DT_INT32, bits) -- the DAG-side classifiers
  // (uop_dag_classify_matmul_shape etc.) pattern-match on UOP_CONST
  // for stride coefficients.  A bare TAG_NUM atom won't match.
  return uop_const(DT_INT32, (uint32_t)v);
}
EXPORT uint64_t py_term_fconst(float v) {
  uint32_t bits;
  memcpy(&bits, &v, 4);
  return uop_const(DT_FP32, bits);
}

// ---------------- UOp graph constructors ----------------
EXPORT uint64_t py_uop_buffer(uint32_t scope, uint32_t dtype,
                              uint32_t ndim, const uint32_t *dims,
                              uint32_t instance) {
  if (instance == 0) return uop_buffer(scope, dtype, ndim, dims);
  return uop_buffer_inst(scope, dtype, ndim, dims, instance);
}

EXPORT uint64_t py_uop_range(uint32_t axis_id, uint32_t axis_type,
                             uint32_t extent) {
  return uop_range(axis_id, axis_type, extent);
}

EXPORT uint64_t py_uop_index_e(uint64_t buf, uint64_t addr) {
  return uop_index_e(buf, addr);
}

EXPORT uint64_t py_uop_int_binary(uint32_t opcode, uint64_t a, uint64_t b) {
  return uop_int_binary(opcode, a, b);
}

EXPORT uint64_t py_uop_iwhere(uint64_t cond, uint64_t t, uint64_t e) {
  return uop_iwhere(cond, t, e);
}

// Value-level select (tensor WHERE): no index-layer folds.
EXPORT uint64_t py_uop_where(uint64_t cond, uint64_t t, uint64_t e) {
  return uop_where(cond, t, e);
}


EXPORT uint64_t py_uop_binary(uint32_t opcode, uint64_t a, uint64_t b) {
  return uop_binary(opcode, a, b);
}

EXPORT uint64_t py_uop_reduce(uint32_t kind, uint32_t axis, uint64_t src) {
  return uop_reduce(kind, axis, src);
}

EXPORT uint64_t py_uop_store(uint64_t buf, uint64_t addr, uint64_t value) {
  return uop_store(buf, addr, value);
}

EXPORT uint64_t py_uop_detach(uint64_t src) { return uop_detach(src); }

// Indexed ops (uop/gather.c, uop/scatter.c): 0 when the operands fail the
// constructor's checks; the frontend raises.
EXPORT uint64_t py_uop_gather(uint64_t src, uint64_t idx, uint32_t dim) {
  return uop_gather(src, idx, dim);
}
EXPORT uint64_t py_uop_scatter(uint64_t base, uint64_t idx, uint64_t src,
                               uint32_t dim, uint32_t kind,
                               uint32_t include_self) {
  return uop_scatter(base, idx, src, dim, kind, include_self);
}

// ============== high-level tensor surface (TAG_TEN) ==================
// Phase 1 of the Python Tensor frontend (docs/plans/py_tensor_frontend
// .md): the high-level tensor-term layer, ported from the WL
// LibraryLink glue (wl/THVMLink/CSource/thvmlink.c, thvm_wl_*).
// thvm_py.c is a single-TU #include of src/thvm.c, so these call
// tensor_alloc / uop_grad / thvm_realize directly -- exactly as
// py_uop_binary calls uop_binary.
//
// A TAG_TEN term packs (dtype, tensor-id).  The algebra + movement
// builders compose TAG_TEN terms; py_realize drives wnf -> materialize
// -> kernelize -> schedule -> dispatch and returns the result TAG_TEN.

// --- tensor create / host I/O ---

// Allocate a fresh tensor on the DEV-selected backend; returns the
// packed TAG_TEN term.  dims[] holds `ndim` (<= MAX_DIM) extents.
EXPORT uint64_t py_ten_create(uint32_t dtype, uint32_t ndim,
                              const uint32_t *dims) {
  Shape shape;
  shape.ndim = ndim > MAX_DIM ? MAX_DIM : ndim;
  for (uint32_t i = 0; i < MAX_DIM; i++) {
    shape.dims[i] = (i < shape.ndim) ? dims[i] : 0;
  }
  u32 id = tensor_alloc(CURRENT_BACKEND, shape, dtype);
  return term_new(0, TAG_TEN, dtype, id);
}

// Copy raw host bytes into a TAG_TEN tensor's backend buffer.  The
// caller packs the dtype-native layout; the bridge memcpies nbytes.
// Returns 1 on success, 0 for a bad term / backend.
EXPORT int py_ten_write(uint64_t t, const void *data, uint64_t nbytes) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  TenDesc *d = &TENS[id];
  if (d->backend == NULL || d->backend->buf_write == NULL) return 0;
  d->backend->buf_write(d->buf_id, data, nbytes);
  // Host write is a buffer mutation the precise fire memo must see (else
  // a kernel reading this buf -- e.g. the eval forward reading a freshly
  // ten_write'n xSlot -- would wrongly skip re-firing and read stale
  // bytes).  Record it on the same ASSIGN-write ring as interact_assign.
  kernel_assign_write_record(d->backend, d->buf_id);
  return 1;
}

// Copy a (realized) TAG_TEN tensor's backend buffer out to host bytes.
EXPORT int py_ten_read(uint64_t t, void *out, uint64_t nbytes) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  TenDesc *d = &TENS[id];
  if (d->backend == NULL || d->backend->buf_read == NULL) return 0;
  d->backend->buf_read(d->buf_id, out, nbytes);
  return 1;
}

// Tensor introspection: dtype, element count, shape (writes ndim
// extents into out_dims[>=MAX_DIM], returns ndim).
EXPORT uint32_t py_ten_dtype(uint64_t t) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  return TENS[id].dtype;
}
EXPORT uint64_t py_ten_numel(uint64_t t) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  return (uint64_t)TENS[id].view.numel;
}
EXPORT uint32_t py_ten_shape(uint64_t t, uint32_t *out_dims) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  Shape *s = &TENS[id].view.shape;
  for (uint32_t i = 0; i < s->ndim && i < MAX_DIM; i++) {
    out_dims[i] = s->dims[i];
  }
  return s->ndim;
}

// --- tensor algebra (py_uop_binary / py_uop_reduce above already
//     cover BINARY / REDUCE) ---
EXPORT uint64_t py_uop_unary(uint32_t opcode, uint64_t src) {
  return uop_unary(opcode, src);
}
EXPORT uint64_t py_uop_cast(uint64_t src, uint32_t dst_dtype) {
  return uop_cast(src, dst_dtype);
}
EXPORT uint64_t py_uop_bitcast(uint64_t src, uint32_t dst_dtype) {
  return uop_bitcast(src, dst_dtype);
}

// --- movement ops (tinygrad reshape/permute/expand/pad/shrink/flip).
//     pad / shrink take begin_end[2*ndim]; flip takes an axes bitmask. ---
EXPORT uint64_t py_uop_reshape(uint64_t src, uint32_t ndim,
                               const uint32_t *dims) {
  return uop_reshape(src, ndim, dims);
}
EXPORT uint64_t py_uop_permute(uint64_t src, uint32_t ndim,
                               const uint32_t *perm) {
  return uop_permute(src, ndim, perm);
}
EXPORT uint64_t py_uop_expand(uint64_t src, uint32_t ndim,
                              const uint32_t *dims) {
  return uop_expand(src, ndim, dims);
}
EXPORT uint64_t py_uop_pad(uint64_t src, uint32_t ndim,
                           const uint32_t *begin_end) {
  return uop_pad(src, ndim, begin_end);
}
EXPORT uint64_t py_uop_shrink(uint64_t src, uint32_t ndim,
                              const uint32_t *begin_end) {
  return uop_shrink(src, ndim, begin_end);
}
EXPORT uint64_t py_uop_flip(uint64_t src, uint32_t axes_bitmask) {
  return uop_flip(src, axes_bitmask);
}

// --- autodiff: thvm's real uop_grad chain-rule rewrite ---
// py_grad builds the BWD projection of y w.r.t. its leaves, seeded by
// gy -- Tensor.backward() routes here, not a hand-built backward DAG.
EXPORT uint64_t py_grad(uint64_t y, uint64_t gy) {
  return uop_grad(y, gy);
}
EXPORT uint64_t py_grad_with_target(uint64_t y, uint64_t gy,
                                    uint64_t target) {
  return uop_grad_with_target(y, gy, target);
}
EXPORT uint64_t py_fwd(uint64_t y, uint64_t gy) {
  return uop_fwd(y, gy);
}

// --- reduce + dispatch ---
// py_wnf: weak normal form.  py_nf: full normal form (fires every
// heap-resident UOp/GRAD/kernel -- surfaces grads before materialize).
// py_realize: the full pipeline, returns the result TAG_TEN.
EXPORT uint64_t py_wnf(uint64_t t)     { return wnf(t); }
EXPORT uint64_t py_realize(uint64_t t) { return thvm_realize(t); }

// py_realize_many: bundle n terms into one TAG_CTR and realize them
// together (thvm_realize routes a CTR to thvm_realize_many).  This is
// the tinygrad `loss.realize(*sched)` contract: every assign + consumer
// is scheduled in ONE materialize pass, so a buffer that is BOTH
// assigned and read by another assign (e.g. Adam's m read by the param
// update) fires its ASSIGN exactly once instead of being re-fired by a
// later per-tensor realize against an already-updated buffer.  The
// resolved children are written back into `terms[]` so each Python
// Tensor can refresh its handle to the materialized TAG_TEN.  Returns 0
// and leaves `terms[]` untouched when the realize failed (an assign
// cycle between the roots); py_realize_error() says why.
EXPORT uint64_t py_realize_many(uint64_t *terms, uint32_t n) {
  if (n == 0) return 0;
  if (n == 1) {
    Term r = thvm_realize(terms[0]);
    if (r != 0) terms[0] = r;
    return r;
  }
  Term ctr = term_new_ctr(0, (Term const *)terms, n);
  Term res = thvm_realize(ctr);
  if (res == 0) return 0;
  uint32_t rn = term_ctr_n(res);
  for (uint32_t i = 0; i < rn && i < n; i++) terms[i] = term_ctr_at(res, i);
  return res;
}

EXPORT const char *py_realize_error(void) { return thvm_realize_error(); }

// The deferred heap collection: `roots[]` are the Terms Python holds, and
// come back relocated.
EXPORT int32_t py_gc_pending(void) {
  return thvm_gc_pending();
}
EXPORT void py_gc_collect(uint64_t *roots, uint32_t n) {
  thvm_gc_collect_deferred((Term *)roots, n);
}

// --- live-tensor pinning + buffer reclaim ---
// A backend buffer is freed once nothing can read it, as tinygrad frees a
// Buffer when its last reference dies (device.py:194 Buffer.deallocate,
// device.py:133-134 Buffer.__del__).  thvm's references are not counted:
// lazy UOp graphs name a tensor by its TAG_TEN leaf in heap cells, and
// TenDescs, views, JIT captures and pins share buffers by id, so liveness
// is decided by tracing.  py_reclaim marks every buffer reachable from the
// frontend's live terms (`roots`: each live Tensor's lazy or realized term)
// and from the runtime roots (pins, WNF stack, DEFS), following each reached
// TenDesc.grad, then frees every other buffer that no JIT capture retains.
// Every realized Tensor also pins its term, which roots it inside a
// realize's own pool rollback.
EXPORT void py_pin_set(uint64_t handle, uint64_t term) {
  extern_pin_handle_set(handle, term);
}
EXPORT void py_pin_drop(uint64_t handle) {
  extern_pin_handle_drop(handle);
}

static uint64_t PY_RECLAIM_AFTER = 0;   // resident bytes after the last reclaim

static uint64_t py_resident_bytes(void) {
#ifdef THVM_HAS_CUDA
  return cpu_buf_live_bytes() + cuda_buf_live_bytes();
#else
  return cpu_buf_live_bytes();
#endif
}

static void py_clear_preserved(void) {
  cpu_buf_clear_preserved(1);
  thvm_metal_buf_clear_preserved(1);
#ifdef THVM_HAS_CUDA
  cuda_buf_clear_preserved(1);
#endif
}

// A realize clears the preserve bits only above its own watermark, so a
// buffer allocated before it (an input it read) keeps a stale bit; clear the
// table first so the mark alone decides.
EXPORT void py_reclaim(uint64_t const *roots, uint32_t n) {
  py_clear_preserved();
  if (!mark_gc_preserve_host_roots((Term const *)roots, n)) return;
  // A live TinyJit capture reads buffers no Python Tensor reaches; jit_pinned
  // alone cannot protect them (one dropped capture unpins a buffer another
  // still retains, and CUDA has no such skip).
  jit_capture_mark_preserved();
  cpu_buf_pool_free_unpreserved(1);
#ifdef THVM_HAS_CUDA
  cuda_buf_pool_free_unpreserved(1);
#endif
  py_clear_preserved();
  PY_RECLAIM_AFTER = py_resident_bytes();
}

// Whether resident bytes grew past the last reclaim's survivors by
// max(THVM_RECLAIM_MB (default 64), half the survivors): the trigger the
// frontend checks before it allocates a tensor and after a realize, so
// garbage stays bounded and each reclaim's cost (proportional to the live
// set) is amortized over at least that much allocation.  `force` (and
// THVM_RECLAIM_MB=0) drops the slack: any growth is due.  Never due while a
// JIT capture records, whose buffers the capture finalize retains.
EXPORT int32_t py_reclaim_due(int32_t force) {
  static int64_t floor_bytes = -1;
  if (floor_bytes < 0) {
    char const *e = getenv("THVM_RECLAIM_MB");
    floor_bytes = (e != NULL && e[0] != 0) ? (int64_t)atoll(e) << 20 : (int64_t)64 << 20;
  }
  if (jit_is_capturing()) return 0;
  uint64_t live = py_resident_bytes();
  if (force || floor_bytes == 0) return live > PY_RECLAIM_AFTER;
  uint64_t slack = PY_RECLAIM_AFTER / 2;
  if (slack < (uint64_t)floor_bytes) slack = (uint64_t)floor_bytes;
  return live > PY_RECLAIM_AFTER + slack;
}

// Backend buffers that hold storage for a tensor (freelisted ones excluded).
EXPORT uint32_t py_buf_live_count(void) {
  uint32_t n = 0;
  for (u64 i = 1; CPU_BUFS != NULL && i < CPU_BUFS_NEXT; i++) {
    if (CPU_BUFS[i].refcount > 0 && (CPU_BUFS[i].data != NULL || CPU_BUFS[i].handle != NULL)) n++;
  }
#ifdef THVM_HAS_CUDA
  for (u64 i = 1; CUDA_BUFS != NULL && i < CUDA_BUFS_NEXT; i++) {
    if (CUDA_BUFS[i].refcount > 0 && CUDA_BUFS[i].dptr != 0) n++;
  }
#endif
  return n;
}

// --- JIT capture / replay (TinyJit) ---
// jit_capture_begin arms recording; kernel dispatches + in-place ASSIGNs
// during the realize land in the active slot.  jit_replay re-dispatches
// the SAME kernel sequence against the SAME buffers -- no re-render, no
// nvrtc, no fresh allocs (so per-step time + arena growth stay flat).
// Records GPU work only; the wrapped step must read its changing inputs
// from stable slots written (py_ten_write) before each replay.
EXPORT uint32_t py_jit_begin(void)          { return jit_capture_begin(); }
// roots[] are the result's realized terms: the replay regenerates each one
// (none for a capture whose result is kept some other way).
EXPORT void py_jit_end(uint64_t const *roots, uint32_t n) {
  jit_capture_end_with_results((Term const *)roots, n);
}
EXPORT uint32_t py_jit_replay(uint32_t s)   { return jit_replay(s); }
// Declare the JIT-input tensors' buf_ids (argument order) so replay can
// rebind them to fresh inputs -- tinygrad input_replace baseline.
EXPORT void py_jit_set_inputs(uint32_t s, uint32_t const *ids, uint32_t n) {
  jit_capture_set_inputs(s, ids, n);
}
// Replay rebinding the captured input sites to this call's fresh inputs.
EXPORT uint32_t py_jit_replay_with_inputs(uint32_t s, uint32_t const *new_ids,
                                          uint32_t n) {
  return jit_replay_with_inputs(s, new_ids, n);
}
EXPORT void     py_jit_drop(uint32_t s)     { jit_capture_drop(s); }

// --- introspection (Phase-4 cross-check surface) ---
EXPORT uint32_t py_tens_count(void) {
  return TENS_NEXT > 1 ? TENS_NEXT - 1 : 0;
}
EXPORT uint32_t py_kernel_count(void) {
  return KERNELS_NEXT > 1 ? KERNELS_NEXT - 1 : 0;
}
// Device-agnostic resident buffer-byte counters, the analog of
// tinygrad's GlobalCounters live mem.  Sums the CPU and (when built with
// CUDA) CUDA counters; only the active backend's is nonzero.
EXPORT uint64_t py_cpu_peak_bytes(void) {
#ifdef THVM_HAS_CUDA
  return cpu_buf_peak_bytes() + cuda_buf_peak_bytes();
#else
  return cpu_buf_peak_bytes();
#endif
}
EXPORT uint64_t py_cpu_live_bytes(void) {
#ifdef THVM_HAS_CUDA
  return cpu_buf_live_bytes() + cuda_buf_live_bytes();
#else
  return cpu_buf_live_bytes();
#endif
}
EXPORT void py_cpu_peak_reset(void) {
  cpu_buf_peak_reset();
#ifdef THVM_HAS_CUDA
  cuda_buf_peak_reset();
#endif
}
EXPORT uint64_t py_cuda_jit_compiles(void) {
#ifdef THVM_HAS_CUDA
  return cuda_jit_compiles();
#else
  return 0;
#endif
}
EXPORT uint64_t py_cuda_jit_evictions(void) {
#ifdef THVM_HAS_CUDA
  return cuda_jit_evictions();
#else
  return 0;
#endif
}
EXPORT void py_cg_profile_dump(uint32_t top_n) {
  cg_profile_dump(stderr, top_n);
  fflush(stderr);
}
EXPORT uint32_t py_const_TAG_TEN(void) { return TAG_TEN; }
EXPORT uint32_t py_const_MAX_DIM(void) { return MAX_DIM; }

// --- grad-leaf mark: INTERNAL mechanism, not a user-facing flag ---
// backward() auto-marks every in-scope non-CONST float leaf right
// before the walk (tinygrad spec: no requires_grad anywhere; .grad is
// filled for every reachable float leaf, and the OPTIMIZER's param
// list -- not a flag -- decides what updates).  grad_leaf_sup's
// target==0 path consults this mark to drive the "accumulate each
// reached leaf's cotangent into TenDesc.grad" walk; it is set/cleared
// transiently by backward() and never exposed on the Tensor surface.
EXPORT int py_ten_set_grad_leaf(uint64_t t, int on) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  int was = (int)TENS[id].requires_grad;
  int now = on ? 1 : 0;
  TENS[id].requires_grad = (u8)now;
  if (now && !was) grad_req_ncount_inc();
  else if (was && !now) grad_req_ncount_dec();
  return 1;
}
// Walk the UOP DAG rooted at `root` and write the distinct TAG_TEN
// leaf tids into `out` (caller-allocated, `cap` slots).  Returns the
// count written.  backward() uses this to enumerate the in-scope
// leaves it should auto-mark + read back -- the tinygrad
// "all_tensors whose uop is in scope" filter, done over thvm's term
// graph instead of a Python all_tensors set.
EXPORT uint32_t py_uop_leaf_tids(uint64_t root, uint32_t *out, uint32_t cap) {
  u32 n = 0;
  uop_leaf_tids((Term)root, out, cap, &n);
  return n;
}

// The chain rule's pending error message (a node it has no rule for),
// cleared by the call; NULL when there is none.
EXPORT char const *py_grad_err_take(void) { return grad_err_take(); }

// Walk-once backward readback: the canonical "this tensor's grad"
// after a uop_grad-driven realize.  grad_leaf_sup's target==0 path
// accumulates contributions into TENS[tid].grad; this returns the
// lazy grad term (callers realize it).  set / clear let the frontend
// reset between backward calls (PyTorch zero_grad analogue).
EXPORT uint64_t py_ten_get_grad(uint64_t t) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  return (uint64_t)TENS[id].grad;
}
EXPORT int py_ten_clear_grad(uint64_t t) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  TENS[id].grad = 0;
  return 1;
}
EXPORT int py_ten_set_grad(uint64_t t, uint64_t g) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  TENS[id].grad = (Term)g;
  return 1;
}

// ---------------- buffer accessors (handy for debug) ----------------
EXPORT uint32_t py_ten_get_buf_id(uint64_t t) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  return TENS[id].buf_id;
}

// C99 structural renderer: the CPU-JIT kernel contract
// (`void k(void *out_v, const void *const *ins_v, unsigned n,
//          const unsigned *in_numels, const unsigned *kvar_vals)`).
// An empty string means the renderer declined the signature.  Caller
// frees via py_string_free.
EXPORT char *py_render_uop_kernel_c(uint64_t root, const char *name) {
  char *buf = NULL;
  size_t sz = 0;
  FILE *fp = open_memstream(&buf, &sz);
  if (fp == NULL) return NULL;
  cg_render_uop_kernel_c_root(root, name ? name : "k", fp);
  fflush(fp);
  fclose(fp);
  return buf;
}

EXPORT void py_string_free(char *s) { free(s); }

// Compile a C99 file into a shared object with the CPU JIT's compiler and
// flags, so the lowered cross-validator builds exactly as the runtime does.
EXPORT int py_cpu_jit_compile_shared(const char *src_path, const char *so_path) {
  return cpu_jit_compile_shared(src_path, so_path, 0);
}

// Apply a KOpt to a free-standing root term (the lowered cross-validator's
// `opts`), without a KernelEntry slot.
EXPORT uint64_t py_uop_dag_apply_kopt(uint64_t root, uint8_t op,
                                      uint32_t axis, uint32_t arg) {
  KOpt opt = { op, axis, arg };
  return uop_dag_apply_kopt(root, opt);
}

EXPORT uint32_t py_const_KOP_UPCAST(void)   { return KOP_UPCAST; }

// ---------------- exposed enums (to avoid magic numbers in Python) ----------------
EXPORT uint32_t py_const_DT_INT32(void)         { return DT_INT32; }
EXPORT uint32_t py_const_DT_FP32(void)          { return DT_FP32; }
EXPORT uint32_t py_const_UOP_SCOPE_GLOBAL(void) { return UOP_SCOPE_GLOBAL; }

EXPORT uint32_t py_const_UOP_ADD(void)   { return UOP_ADD; }
EXPORT uint32_t py_const_UOP_MUL(void)   { return UOP_MUL; }
EXPORT uint32_t py_const_UOP_NEG(void)   { return UOP_NEG; }
EXPORT uint32_t py_const_UOP_ASSIGN(void){ return UOP_ASSIGN; }
EXPORT uint32_t py_const_UOP_CMPLT(void) { return UOP_CMPLT; }
EXPORT uint32_t py_const_UOP_CMPEQ(void) { return UOP_CMPEQ; }
EXPORT uint32_t py_const_UOP_RECIP(void) { return UOP_RECIP; }
EXPORT uint32_t py_const_UOP_EXP2(void)  { return UOP_EXP2; }
EXPORT uint32_t py_const_UOP_LOG2(void)  { return UOP_LOG2; }
EXPORT uint32_t py_const_UOP_SQRT(void)  { return UOP_SQRT; }

EXPORT uint32_t py_const_UOP_IADD(void)  { return UOP_IADD; }
EXPORT uint32_t py_const_UOP_IMUL(void)  { return UOP_IMUL; }

EXPORT uint32_t py_const_REDUCE_SUM(void) { return REDUCE_SUM; }
EXPORT uint32_t py_const_REDUCE_MAX(void) { return REDUCE_MAX; }

EXPORT uint32_t py_const_SCATTER_SUM(void)     { return SCATTER_SUM; }
EXPORT uint32_t py_const_SCATTER_PROD(void)    { return SCATTER_PROD; }
EXPORT uint32_t py_const_SCATTER_MAX(void)     { return SCATTER_MAX; }
EXPORT uint32_t py_const_SCATTER_MIN(void)     { return SCATTER_MIN; }
EXPORT uint32_t py_const_SCATTER_REPLACE(void) { return SCATTER_REPLACE; }

EXPORT uint32_t py_const_KAX_LOOP(void)         { return KAX_LOOP; }
EXPORT uint32_t py_const_KAX_REDUCE(void)       { return KAX_REDUCE; }
EXPORT uint32_t py_const_KAX_LOCAL(void)        { return KAX_LOCAL; }
EXPORT uint32_t py_const_KAX_GLOBAL(void)       { return KAX_GLOBAL; }

