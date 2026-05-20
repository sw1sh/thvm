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
EXPORT void py_thvm_init(void) { thvm_init(); }
EXPORT void py_thvm_free(void) { thvm_free(); }

// ---------------- term inspection ----------------
EXPORT uint32_t py_term_tag(uint64_t t) { return term_tag(t); }
EXPORT uint32_t py_term_ext(uint64_t t) { return term_ext(t); }
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

EXPORT uint64_t py_uop_invalid(void) { return uop_invalid(); }

EXPORT uint64_t py_uop_binary(uint32_t opcode, uint64_t a, uint64_t b) {
  return uop_binary(opcode, a, b);
}

EXPORT uint64_t py_uop_reduce(uint32_t kind, uint32_t axis, uint64_t src) {
  return uop_reduce(kind, axis, src);
}

EXPORT uint64_t py_uop_opt(uint64_t target, uint32_t kind, uint32_t factor) {
  return uop_opt(target, kind, factor);
}

EXPORT uint64_t py_uop_store(uint64_t buf, uint64_t addr, uint64_t value) {
  return uop_store(buf, addr, value);
}

EXPORT uint64_t py_uop_after(uint64_t node, uint64_t after_node) {
  return uop_after(node, after_node);
}

EXPORT uint64_t py_uop_load(uint64_t src) { return uop_load(src); }

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
EXPORT uint64_t py_nf(uint64_t t)      { return nf(t); }
EXPORT uint64_t py_realize(uint64_t t) { return thvm_realize(t); }

// --- introspection (Phase-4 cross-check surface) ---
EXPORT uint32_t py_tens_count(void) {
  return TENS_NEXT > 1 ? TENS_NEXT - 1 : 0;
}
EXPORT uint32_t py_kernel_count(void) {
  return KERNELS_NEXT > 1 ? KERNELS_NEXT - 1 : 0;
}
EXPORT uint32_t py_const_TAG_TEN(void) { return TAG_TEN; }
EXPORT uint32_t py_const_MAX_DIM(void) { return MAX_DIM; }

// --- requires_grad: canonical flag in TenDesc (Phase 3B follow-up) ---
// The Python Tensor.requires_grad_() sets this; backward() filters
// against it.  Eventually uop_grad's leaf rule can short-circuit at
// requires_grad==0 leaves -- for now the flag is bookkeeping that
// keeps the Python frontend honest about which tensors are parameters.
EXPORT int py_ten_set_requires_grad(uint64_t t, int on) {
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
EXPORT int py_ten_get_requires_grad(uint64_t t) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  return (int)TENS[id].requires_grad;
}

// Hot-path snapshots for the dedup-verification experiment.  Returns
// per-realize chain-rule memo stats and the global grad-fires counter.
EXPORT uint64_t py_grad_memo_hits  (void) { return grad_memo_hits_get();   }
EXPORT uint64_t py_grad_memo_misses(void) { return grad_memo_misses_get(); }
EXPORT uint64_t py_grad_fires      (void) { return HOT_GRAD_FIRES;         }

// ---------------- buffer accessors (handy for debug) ----------------
EXPORT uint32_t py_uop_buffer_scope(uint64_t t) { return uop_buffer_scope(t); }
EXPORT uint32_t py_uop_buffer_dtype(uint64_t t) { return uop_buffer_dtype(t); }
EXPORT uint32_t py_uop_buffer_ndim(uint64_t t)  { return uop_buffer_ndim(t); }
EXPORT uint32_t py_uop_buffer_dim(uint64_t t, uint32_t d) {
  return uop_buffer_dim(t, d);
}

// ---------------- renderer ----------------
// Returns a heap-allocated null-terminated MSL source string. Caller
// must free via py_string_free.
EXPORT char *py_render_uop_kernel(uint64_t root, const char *name) {
  char *buf = NULL;
  size_t sz = 0;
  FILE *fp = open_memstream(&buf, &sz);
  if (fp == NULL) return NULL;
  cg_render_uop_kernel_root(root, name ? name : "k", fp);
  fflush(fp);
  fclose(fp);
  return buf;
}

// CUDA structural renderer.  Returns a heap-allocated null-terminated
// .cu source string (`extern "C" __global__ void k(...)`).  Caller
// frees via py_string_free.  The CUDA-target counterpart of
// py_render_uop_kernel; the Python `Thvm.render_cuda` wraps it.
EXPORT char *py_render_uop_kernel_cuda(uint64_t root, const char *name) {
  char *buf = NULL;
  size_t sz = 0;
  FILE *fp = open_memstream(&buf, &sz);
  if (fp == NULL) return NULL;
  cg_render_uop_kernel_cuda_root(root, name ? name : "k", fp);
  fflush(fp);
  fclose(fp);
  return buf;
}

EXPORT void py_string_free(char *s) { free(s); }

// ---------------- BEAM / autotune surface ----------------
// Phase E entry: the autotune.c BEAM (slice 8 session 4) reads
// `ke->cached_lift.store_root` for matmul-shape recognition via
// uop_dag_classify_matmul_shape, then proposes KOpt[] candidates.
// We expose the smallest set of helpers needed to drive it from a
// Python autotuner: allocate a synthetic KernelEntry, populate the
// cached lift with our Python-built UOp DAG, and call propose to get
// candidate KOpts.  Applying opts and benching is left to the Python
// driver -- it composes UOp variants via the existing constructors
// and times via the Metal helpers in thvm_py_metal.m.

EXPORT uint32_t py_kernel_alloc(void) { return kernel_alloc(); }

EXPORT void py_kernel_set_cached_lift(uint32_t kid, uint64_t store_root,
                                      uint64_t out_buf,
                                      const uint64_t *in_bufs,
                                      uint32_t n_inputs) {
  if (kid == 0 || kid >= KERNELS_NEXT) return;
  KernelEntry *ke = &KERNELS[kid];
  ke->cached_lift.store_root = store_root;
  ke->cached_lift.out_buf = out_buf;
  ke->cached_lift.n_inputs = n_inputs;
  if (n_inputs > KERNEL_LIFT_MAX_INPUT) n_inputs = KERNEL_LIFT_MAX_INPUT;
  for (uint32_t i = 0; i < n_inputs; i++) {
    ke->cached_lift.in_bufs[i] = in_bufs[i];
  }
  // Also set the KernelEntry's own n_inputs -- DAG classifiers read
  // ke->n_inputs (not cached_lift.n_inputs) when validating the
  // BUFFER.instance->slot mapping (uop_dag_classify_matmul_shape:507).
  ke->n_inputs = n_inputs;
}

// Returns number of candidates filled into out_ops[]/out_axes[]/out_args[].
// Caller pre-allocates arrays of `cap` u32 each.  Each candidate is the
// triple (op, axis, arg) -- see KOpt struct in thvm.h.
EXPORT uint32_t py_kernel_opts_propose(uint32_t kid,
                                       uint8_t *out_ops,
                                       uint8_t *out_axes,
                                       uint32_t *out_args,
                                       uint32_t cap) {
  if (kid == 0 || kid >= KERNELS_NEXT) return 0;
  KernelEntry *ke = &KERNELS[kid];
  KOpt opts[KAUTOTUNE_MAX_CANDIDATES];
  uint32_t local_cap = cap;
  if (local_cap > KAUTOTUNE_MAX_CANDIDATES) local_cap = KAUTOTUNE_MAX_CANDIDATES;
  uint32_t n = kernel_opts_propose(ke, opts, local_cap);
  for (uint32_t i = 0; i < n; i++) {
    out_ops[i]  = opts[i].op;
    out_axes[i] = opts[i].axis;
    out_args[i] = opts[i].arg;
  }
  return n;
}

EXPORT uint64_t py_propose_tc_dag_count(void) {
  return kernel_opts_propose_tc_dag_count();
}
EXPORT void py_propose_tc_counters_reset(void) {
  kernel_opts_propose_tc_counters_reset();
}

// Apply a KOpt to the kernel's UOp DAG (Phase E DAG-mode path).
// Returns the new store_root term (already stored into
// ke->cached_lift.store_root); 0 on bail.  Use
// alongside the propose surface to compose the tinygrad-style BEAM
// autotune loop in Python.
EXPORT uint64_t py_kernel_apply_opt(uint32_t kid, uint8_t op,
                                    uint8_t axis, uint32_t arg) {
  if (kid == 0 || kid >= KERNELS_NEXT) return 0;
  KernelEntry *ke = &KERNELS[kid];
  KOpt opt = { op, axis, arg };
  if (!kernel_apply_opt(ke, opt)) return 0;
  return ke->cached_lift.store_root;
}

// Direct DAG-rewrite entry: apply the opt to a free-standing root term,
// without going through a KernelEntry slot.  Useful for unit tests and
// for agents that prefer to manage the DAG directly without the
// kid-keyed KERNELS[] machinery.
EXPORT uint64_t py_uop_dag_apply_kopt(uint64_t root, uint8_t op,
                                      uint8_t axis, uint32_t arg) {
  KOpt opt = { op, axis, arg };
  return uop_dag_apply_kopt(root, opt);
}

// DAG-derived CUDA launch geometry.  The Python `Cuda.dispatch` takes
// an explicit flat grid/block and so cannot launch a LOCAL-split
// kernel: a KOP_LOCAL split moves the inner tile onto `tt`
// (threadIdx.x), which the flat one-thread-per-output shape does not
// express.  The CUDA autotune sweep calls this on the KOpt-rewritten
// root to recover the tg/tt geometry the structural renderer decodes
// (cg_render_uop_kernel_cuda_root).  Mirrors src/backend/cuda/jit.c's
// cuda_dag_dispatch_shape, but operates on a free-standing root term
// (the py CUDA bridge has no KernelEntry).
//
//   - KAX_LOOP   axes -> grid  (one block per LOOP tuple, `tg` decode)
//   - KAX_LOCAL  axes -> block (threads per block, `tt` decode)
//   - KAX_GROUP_REDUCE -> block (warp-collective reduce width)
//   - KAX_UPCAST / KAX_UNROLL / KAX_REDUCE: in-thread, not in tid.
//
// Writes (*grid_x, *block_x) and returns 1 on success; 0 if the DAG
// has no axes / overflows 32 bits / a block dim exceeds the 1024
// threads/block hardware cap (sm_70 V100).
EXPORT uint32_t py_cuda_dag_dispatch_shape(uint64_t root,
                                           uint32_t *grid_x,
                                           uint32_t *block_x) {
  if (root == 0) return 0;
  uint32_t ids[MAX_AXES], types[MAX_AXES], exts[MAX_AXES];
  uint32_t n = uop_dag_collect_axes(root, ids, types, exts, MAX_AXES);
  if (n == 0) return 0;
  uint64_t total = 1;            // product of promoted-LOOP output extents
  uint64_t local_total = 1;      // product of KAX_LOCAL extents
  uint32_t group_reduce_extent = 0;
  for (uint32_t i = 0; i < n; i++) {
    if (exts[i] == 0) return 0;
    switch (types[i]) {
      case KAX_LOOP:         total       *= (uint64_t)exts[i]; break;
      case KAX_LOCAL:        local_total *= (uint64_t)exts[i]; break;
      case KAX_GROUP_REDUCE: group_reduce_extent = exts[i]; break;
      default: break;
    }
  }
  if (total == 0 || total > 0xFFFFFFFFu) return 0;
  uint32_t grid, block;
  // SIMD_REDUCE: one warp per reduce-axis tuple -- grid = product of
  // reduce-dependent output axes (a pure-broadcast axis is spread over
  // the 32 lanes), block = 32.  Mirrors src/backend/cuda/jit.c's
  // cuda_dag_dispatch_shape.
  if (rmu_dag_has_simd_reduce(root)
      && local_total <= 1 && group_reduce_extent == 0) {
    uint64_t sg = rmu_dag_simd_warp_grid(root);
    if (sg == 0) sg = total;
    if (sg > 0xFFFFFFFFu) return 0;
    if (grid_x  != NULL) *grid_x  = (uint32_t)sg;
    if (block_x != NULL) *block_x = 32u;
    return 1;
  }
  if (group_reduce_extent != 0) {
    if (group_reduce_extent > 1024) return 0;
    grid  = (uint32_t)total;
    block = group_reduce_extent;
  } else if (local_total > 1) {
    if (local_total > 1024) return 0;
    block = (uint32_t)local_total;
    grid  = (uint32_t)total;
  } else {
    block = total < 256 ? (uint32_t)total : 256u;
    if (block < 32) block = (uint32_t)total;
    grid  = (uint32_t)((total + (uint64_t)block - 1) / (uint64_t)block);
  }
  if (grid == 0 || block == 0) return 0;
  if (grid_x  != NULL) *grid_x  = grid;
  if (block_x != NULL) *block_x = block;
  return 1;
}

EXPORT uint32_t py_const_KOP_NONE(void)     { return KOP_NONE; }
EXPORT uint32_t py_const_KOP_UPCAST(void)   { return KOP_UPCAST; }
EXPORT uint32_t py_const_KOP_UNROLL(void)   { return KOP_UNROLL; }
EXPORT uint32_t py_const_KOP_LOCAL(void)    { return KOP_LOCAL; }
EXPORT uint32_t py_const_KOP_GROUP(void)    { return KOP_GROUP; }
EXPORT uint32_t py_const_KOP_GROUPTOP(void) { return KOP_GROUPTOP; }
EXPORT uint32_t py_const_KOP_SWAP(void)     { return KOP_SWAP; }
EXPORT uint32_t py_const_KOP_PADTO(void)    { return KOP_PADTO; }
EXPORT uint32_t py_const_KOP_NOLOCALS(void) { return KOP_NOLOCALS; }
EXPORT uint32_t py_const_KOP_TC(void)       { return KOP_TC; }
EXPORT uint32_t py_const_KOP_GLOBAL(void)   { return KOP_GLOBAL; }
EXPORT uint32_t py_const_KOP_FAST_MATH(void) { return KOP_FAST_MATH; }
EXPORT uint32_t py_const_KOP_SIMD_REDUCE(void) { return KOP_SIMD_REDUCE; }
EXPORT uint32_t py_const_KOP_VEC_LOAD(void) { return KOP_VEC_LOAD; }

// ---------------- exposed enums (to avoid magic numbers in Python) ----------------
EXPORT uint32_t py_const_DT_INT32(void)         { return DT_INT32; }
EXPORT uint32_t py_const_DT_FP32(void)          { return DT_FP32; }
EXPORT uint32_t py_const_UOP_SCOPE_GLOBAL(void) { return UOP_SCOPE_GLOBAL; }
EXPORT uint32_t py_const_UOP_SCOPE_LOCAL(void)  { return UOP_SCOPE_LOCAL; }
EXPORT uint32_t py_const_UOP_SCOPE_REG(void)    { return UOP_SCOPE_REG; }

EXPORT uint32_t py_const_UOP_ADD(void)   { return UOP_ADD; }
EXPORT uint32_t py_const_UOP_MUL(void)   { return UOP_MUL; }
EXPORT uint32_t py_const_UOP_NEG(void)   { return UOP_NEG; }
EXPORT uint32_t py_const_UOP_CMPLT(void) { return UOP_CMPLT; }
EXPORT uint32_t py_const_UOP_CMPEQ(void) { return UOP_CMPEQ; }
EXPORT uint32_t py_const_UOP_RECIP(void) { return UOP_RECIP; }
EXPORT uint32_t py_const_UOP_EXP2(void)  { return UOP_EXP2; }
EXPORT uint32_t py_const_UOP_LOG2(void)  { return UOP_LOG2; }
EXPORT uint32_t py_const_UOP_SQRT(void)  { return UOP_SQRT; }

EXPORT uint32_t py_const_UOP_IADD(void)  { return UOP_IADD; }
EXPORT uint32_t py_const_UOP_ISUB(void)  { return UOP_ISUB; }
EXPORT uint32_t py_const_UOP_IMUL(void)  { return UOP_IMUL; }
EXPORT uint32_t py_const_UOP_IDIV(void)  { return UOP_IDIV; }
EXPORT uint32_t py_const_UOP_IMOD(void)  { return UOP_IMOD; }
EXPORT uint32_t py_const_UOP_ILT(void)   { return UOP_ILT; }
EXPORT uint32_t py_const_UOP_IAND(void)  { return UOP_IAND; }

EXPORT uint32_t py_const_REDUCE_SUM(void) { return REDUCE_SUM; }
EXPORT uint32_t py_const_REDUCE_MAX(void) { return REDUCE_MAX; }

EXPORT uint32_t py_const_KAX_LOOP(void)         { return KAX_LOOP; }
EXPORT uint32_t py_const_KAX_REDUCE(void)       { return KAX_REDUCE; }
EXPORT uint32_t py_const_KAX_UPCAST(void)       { return KAX_UPCAST; }
EXPORT uint32_t py_const_KAX_UNROLL(void)       { return KAX_UNROLL; }
EXPORT uint32_t py_const_KAX_LOCAL(void)        { return KAX_LOCAL; }
EXPORT uint32_t py_const_KAX_GLOBAL(void)       { return KAX_GLOBAL; }
EXPORT uint32_t py_const_KAX_GROUP_REDUCE(void) { return KAX_GROUP_REDUCE; }

EXPORT uint32_t py_const_UOP_OPT_UNROLL(void)       { return UOP_OPT_UNROLL; }
EXPORT uint32_t py_const_UOP_OPT_UPCAST(void)       { return UOP_OPT_UPCAST; }
EXPORT uint32_t py_const_UOP_OPT_TC(void)           { return UOP_OPT_TC; }
EXPORT uint32_t py_const_UOP_OPT_LOCAL(void)        { return UOP_OPT_LOCAL; }
EXPORT uint32_t py_const_UOP_OPT_GROUP_REDUCE(void) { return UOP_OPT_GROUP_REDUCE; }
EXPORT uint32_t py_const_UOP_OPT_CONV(void)         { return UOP_OPT_CONV; }
EXPORT uint32_t py_const_UOP_OPT_FAST_MATH(void)    { return UOP_OPT_FAST_MATH; }
EXPORT uint32_t py_const_UOP_OPT_SIMD_REDUCE(void)  { return UOP_OPT_SIMD_REDUCE; }
EXPORT uint32_t py_const_UOP_OPT_VEC_LOAD(void)     { return UOP_OPT_VEC_LOAD; }
