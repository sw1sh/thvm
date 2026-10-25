// The ctypes surface of py/thvm: exported wrappers over the static-inline `fn`s of
// the single-TU runtime, built by `make py`.

#include "../../src/thvm.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXPORT __attribute__((visibility("default")))

// Python holds raw heap Terms (every lazy Tensor's graph), so a realize defers
// its heap collection: Thvm runs it through py_gc_collect with its live Terms.
EXPORT void py_thvm_init(void) {
  thvm_init();
  thvm_gc_defer(1);
}
EXPORT void py_thvm_free(void) { thvm_free(); }

EXPORT uint32_t py_term_tag(uint64_t t) { return term_tag(t); }
EXPORT uint64_t py_term_val(uint64_t t) { return term_val(t); }
EXPORT uint32_t py_term_ext(uint64_t t) { return term_ext(t); }
// 1 iff t is a UOp graph that reads no buffer, which Tensor.realize leaves lazy.
EXPORT uint32_t py_term_reads_no_buffer(uint64_t t) {
  return term_tag(t) == TAG_UOP && bufferize_reads_no_buffer(term_val(t), (u8)term_ext(t));
}

EXPORT uint64_t py_term_iconst(int32_t v) {
  // A UOP_CONST, not a bare TAG_NUM: the DAG classifiers match UOP_CONST
  // stride coefficients (uop_dag_classify_matmul_shape).
  return uop_const(DT_INT32, (uint32_t)v);
}
EXPORT uint64_t py_term_fconst(float v) {
  uint32_t bits;
  memcpy(&bits, &v, 4);
  return uop_const(DT_FP32, bits);
}

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

// The value-level select (tensor WHERE), without uop_iwhere's index-layer folds.
EXPORT uint64_t py_uop_where(uint64_t cond, uint64_t t, uint64_t e) {
  return uop_where(cond, t, e);
}


EXPORT uint64_t py_uop_binary(uint32_t opcode, uint64_t a, uint64_t b) {
  return uop_binary(opcode, a, b);
}

EXPORT uint64_t py_uop_reduce(uint32_t kind, uint32_t n, uint32_t const *axes, uint64_t src) {
  return uop_reduce_axes(kind, n, axes, src);
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

// A fresh TAG_TEN tensor on the DEV-selected backend.
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

// py_ten_write / py_ten_read copy nbytes of dtype-native host data: 1, or 0
// when `t` is not a known TAG_TEN or its backend cannot copy.
EXPORT int py_ten_write(uint64_t t, const void *data, uint64_t nbytes) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  TenDesc *d = &TENS[id];
  if (d->backend == NULL || d->backend->buf_write == NULL) return 0;
  d->backend->buf_write(d->buf_id, data, nbytes);
  // The precise fire memo skips a kernel whose buffers saw no write since its
  // last dispatch, so a host write is logged as an ASSIGN's is.
  kernel_assign_write_record(d->backend, d->buf_id);
  return 1;
}

EXPORT int py_ten_read(uint64_t t, void *out, uint64_t nbytes) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  TenDesc *d = &TENS[id];
  if (d->backend == NULL || d->backend->buf_read == NULL) return 0;
  d->backend->buf_read(d->buf_id, out, nbytes);
  return 1;
}

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
// The rank, with the extents written to out_dims[MAX_DIM].
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

EXPORT uint64_t py_uop_unary(uint32_t opcode, uint64_t src) {
  return uop_unary(opcode, src);
}
EXPORT uint64_t py_uop_cast(uint64_t src, uint32_t dst_dtype) {
  return uop_cast(src, dst_dtype);
}
EXPORT uint64_t py_uop_bitcast(uint64_t src, uint32_t dst_dtype) {
  return uop_bitcast(src, dst_dtype);
}

// pad and shrink take begin_end[2 * ndim], a (begin, end) pair per axis; flip
// takes an axes bitmask.
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
// ab[0] (shape sa) and ab[1] (shape sb) broadcast in place to one shape
// (uop_broadcast2): its rank, written to out, or -1.
EXPORT int py_uop_broadcast2(uint64_t *ab, uint32_t na, const uint32_t *sa,
                             uint32_t nb, const uint32_t *sb, uint32_t *out) {
  return uop_broadcast2((Term *)&ab[0], na, sa, (Term *)&ab[1], nb, sb, out);
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

// The src/nn ops (nn_call, src/nn/call.c).  A tensor crosses as its term plus
// desc[2 + MAX_DIM] = [dtype, ndim, dims...] (term 0: an absent bias or weight);
// the result term comes back (0 when the op fails) with its desc written over
// desc[0..2 + MAX_DIM).
#define PY_NN_DESC (2 + MAX_DIM)

EXPORT uint64_t py_nn(uint32_t op, uint64_t const *terms, uint32_t *desc, uint32_t nt,
                      int32_t const *ia, uint32_t ni, double const *ra, uint32_t nr) {
  Tensor t[8];
  if (nt > 8) {
    return 0;
  }
  for (uint32_t i = 0; i < nt; i++) {
    uint32_t const *d = desc + i * PY_NN_DESC;
    if (d[1] > MAX_DIM) {
      return 0;
    }
    t[i] = (Tensor){terms[i], d[0], {d[1], {0}}};
    memcpy(t[i].shape.dims, d + 2, d[1] * sizeof d[0]);
  }
  Tensor r = nn_call(op, t, nt, ia, ni, ra, nr);
  desc[0] = r.dtype;
  desc[1] = r.shape.ndim;
  memcpy(desc + 2, r.shape.dims, r.shape.ndim * sizeof desc[0]);
  return r.t;
}

// A DT_* id by its short name ("f32", "bf16", ...), UINT32_MAX for none; the
// least upper dtype of a mixed pair (nn_promote); n values converted between f32
// and a dtype numpy has no type for (bf16, fp8).
EXPORT uint32_t py_dtype_id(char const *name) {
  for (u32 i = 0; i < DT_COUNT; i++) {
    DTypeInfo const *d = dtype_info(i);
    if (d != NULL && d->name != NULL && strcmp(d->name, name) == 0) {
      return i;
    }
  }
  return UINT32_MAX;
}
EXPORT uint32_t py_dtype_promote(uint32_t a, uint32_t b) {
  return nn_promote(a, b);
}
EXPORT void py_dtype_from_f32(void *dst, uint32_t dtype, float const *src, uint32_t n) {
  from_fp32_lane(dst, dtype, src, n);
}
EXPORT void py_dtype_to_f32(float *dst, void const *src, uint32_t dtype, uint32_t n) {
  to_fp32_lane(dst, src, dtype, n);
}

// An nn op's id by its name ("conv2d", "relu", ...); UINT32_MAX for none.
EXPORT uint32_t py_nn_op(char const *name) {
  return nn_op_id(name);
}

// The chain-rule (DP1) projection of a grad cell [y, gy, target]
// (uop/grad.c); a nonzero target runs the target walk.
EXPORT uint64_t py_grad_with_target(uint64_t y, uint64_t gy,
                                    uint64_t target) {
  return uop_grad_with_target(y, gy, target);
}

// py_realize: the result TAG_TEN, or 0 with py_realize_error() saying why.
EXPORT uint64_t py_realize(uint64_t t) { return thvm_realize(t); }

// The n roots realized as one TAG_CTR bundle (tinygrad Tensor.realize(*lst)), so
// an assign another root reads fires once; the resolved roots go back into terms[],
// which a failed realize (0, py_realize_error() says why) leaves untouched.
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

// The deferred heap collection: roots[] are the Terms Python holds, and come
// back relocated.
EXPORT int32_t py_gc_pending(void) {
  return thvm_gc_pending();
}
EXPORT void py_gc_collect(uint64_t *roots, uint32_t n) {
  thvm_gc_collect_deferred((Term *)roots, n);
}

// A realized Tensor pins its term, a root of each realize's own pool rollback.
EXPORT void py_pin_set(uint64_t handle, uint64_t term) {
  extern_pin_handle_set(handle, term);
}
EXPORT void py_pin_drop(uint64_t handle) {
  extern_pin_handle_drop(handle);
}
// A handle's current term, which a collection relocates; 0 when unset.  A host
// that holds only handles reads the term here right before an op.
EXPORT uint64_t py_pin_get(uint64_t handle) {
  return extern_pin_handle_get(handle);
}

// A CPU TAG_TEN over the caller's bytes, without a copy: the buffer's last
// release calls release(handle).  0 when nbytes does not cover the shape.
EXPORT uint64_t py_ten_external(uint32_t dtype, uint32_t ndim, uint32_t const *dims, void *data,
                                uint64_t nbytes, void (*release)(void *), void *handle) {
  if (ndim > MAX_DIM) {
    return 0;
  }
  Shape shape = {ndim, {0}};
  memcpy(shape.dims, dims, ndim * sizeof dims[0]);
  if (dtype_storage_bytes(dtype, shape_numel(shape)) > nbytes) {
    return 0;
  }
  u32 buf = cpu_buf_alloc_external(data, nbytes, release, handle);
  u32 id  = tensor_alloc_adopt(&CPU_BACKEND, shape, dtype, buf);
  CPU_BACKEND.buf_decref(buf);   // the descriptor holds the one reference
  return term_new(0, TAG_TEN, dtype, id);
}

// A CPU TAG_TEN's buffer, with its view's element offset and strides (in
// elements, 0 broadcasts, MAX_DIM of them); NULL off the CPU.  The pointer
// stays valid while the tensor is pinned.
EXPORT void *py_ten_data(uint64_t t, int32_t *offset, int32_t *strides) {
  if (term_tag(t) != TAG_TEN) {
    return NULL;
  }
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT || TENS[id].backend != &CPU_BACKEND) {
    return NULL;
  }
  TenDesc *d = &TENS[id];
  *offset = d->view.offset;
  memcpy(strides, d->view.strides, sizeof d->view.strides);
  return CPU_BUFS[d->buf_id].data;
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

// Heap cells hold uncounted tensor references, so this frees each CPU or CUDA
// buffer that tracing from roots[] (each live Tensor's term), pins, the WNF stack
// and DEFS misses: tinygrad frees a Buffer on its last reference (device.py:133-134).
EXPORT void py_reclaim(uint64_t const *roots, uint32_t n) {
  // A realize clears preserve bits only from its own watermark up, so an older
  // buffer can carry a stale one; clear the table so the mark alone decides.
  py_clear_preserved();
  if (!mark_gc_preserve_host_roots((Term const *)roots, n)) return;
  // A live capture reads buffers no Tensor reaches, and jit_pinned is a flag:
  // dropping one capture unpins a buffer another still retains.
  jit_capture_mark_preserved();
  cpu_buf_pool_free_unpreserved(1);
#ifdef THVM_HAS_CUDA
  cuda_buf_pool_free_unpreserved(1);
#endif
  py_clear_preserved();
  PY_RECLAIM_AFTER = py_resident_bytes();
}

// The slack, max(THVM_RECLAIM_MB (default 64), half the survivors), amortizes a
// reclaim's live-set cost; `force` or THVM_RECLAIM_MB=0 makes any growth due.
// Never due while a JIT capture records: its finalize retains the buffers.
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

// CPU and CUDA buffers holding storage, freelisted ones excluded.
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

// A capture records the kernel dispatches and in-place ASSIGNs of the realizes
// from py_jit_begin (its slot, 0 when none is free) to py_jit_end; a replay
// re-dispatches them on the same buffers.
EXPORT uint32_t py_jit_begin(void)          { return jit_capture_begin(); }
// roots[] are the result's realized terms, whose dispatches every replay keeps.
EXPORT void py_jit_end(uint64_t const *roots, uint32_t n) {
  jit_capture_end_with_results((Term const *)roots, n);
}
EXPORT uint32_t py_jit_replay(uint32_t s)   { return jit_replay(s); }
// The capture's input buf_ids in argument order; py_jit_replay_with_inputs
// rebinds their sites to each call's fresh inputs.
EXPORT void py_jit_set_inputs(uint32_t s, uint32_t const *ids, uint32_t n) {
  jit_capture_set_inputs(s, ids, n);
}
EXPORT uint32_t py_jit_replay_with_inputs(uint32_t s, uint32_t const *new_ids,
                                          uint32_t n) {
  return jit_replay_with_inputs(s, new_ids, n);
}
EXPORT void     py_jit_drop(uint32_t s)     { jit_capture_drop(s); }
EXPORT uint32_t py_jit_prune(uint32_t s)    { return jit_capture_prune(s); }
// Kernels realized between pause and resume dispatch but are not recorded.
EXPORT void     py_jit_pause(void)          { jit_capture_pause(); }
EXPORT void     py_jit_resume(void)         { jit_capture_resume(); }
EXPORT uint32_t py_jit_op_count(uint32_t s) { return jit_capture_op_count(s); }
// {n_ops, row_width, rows...} as jit_capture_export_ops writes them.
EXPORT uint32_t py_jit_export_ops(uint32_t s, uint64_t *out, uint32_t cap) {
  return jit_capture_export_ops(s, out, cap);
}

// The views a TEN reads its buffer through, outermost first, each as
// {ndim, offset, dims[ndim], strides[ndim]}: equal words, equal reads of the
// buffer.  Returns the word count; 0 for a term that is not a TEN.
EXPORT uint32_t py_ten_view_words(uint64_t t, int64_t *out, uint32_t cap) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  TenDesc const *td = &TENS[id];
  u32 n = 0;
  for (u32 v = 0; v <= td->nviews && n + 2 + 2 * MAX_DIM <= cap; v++) {
    View const *w = v == 0 ? &td->view : &td->prior_views[td->nviews - v];
    out[n++] = w->shape.ndim;
    out[n++] = w->offset;
    for (u32 d = 0; d < w->shape.ndim; d++) out[n++] = w->shape.dims[d];
    for (u32 d = 0; d < w->shape.ndim; d++) out[n++] = w->strides[d];
  }
  return n;
}

EXPORT uint32_t py_tens_count(void) {
  return TENS_NEXT > 1 ? TENS_NEXT - 1 : 0;
}
EXPORT uint32_t py_kernel_count(void) {
  return KERNELS_NEXT > 1 ? KERNELS_NEXT - 1 : 0;
}
// Resident CPU plus (with CUDA) CUDA buffer bytes, peak and live, the live one as
// tinygrad's GlobalCounters.mem_used; Metal buffers are not counted.
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
// tinygrad's GlobalCounters (helpers.py:296-304): the kernels run from start,
// their ops and bytes (cg_count_dispatch), and mem_used, the bytes of the CPU
// and CUDA buffers a reference holds: a freelisted buffer is uncounted, as
// tinygrad's Buffer.deallocate uncounts what its LRU allocator keeps.
EXPORT void py_global_counters(uint64_t *out) {
  out[0] = GC_COUNTS[0];
  out[1] = GC_COUNTS[1];
  out[2] = GC_COUNTS[2];
  out[3] = thvm_metal_live_bytes();
  for (u64 i = 1; CPU_BUFS != NULL && i < CPU_BUFS_NEXT; i++) {
    if (CPU_BUFS[i].refcount > 0 && CPU_BUFS[i].owns_data) out[3] += CPU_BUFS[i].nbytes;
  }
#ifdef THVM_HAS_CUDA
  for (u64 i = 1; CUDA_BUFS != NULL && i < CUDA_BUFS_NEXT; i++) {
    if (CUDA_BUFS[i].refcount > 0 && CUDA_BUFS[i].owns_data) out[3] += CUDA_BUFS[i].nbytes;
  }
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
EXPORT uint32_t py_const_TAG_UOP(void) { return TAG_UOP; }
EXPORT uint32_t py_const_MAX_DIM(void) { return MAX_DIM; }

// Marks or unmarks a leaf for the single walk, in which grad_leaf_sup adds the
// leaf's cotangent to TenDesc.grad; py_backward marks its leaves itself.
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
// The distinct TAG_TEN tids under `root`, written to the caller's out[cap], and
// their count: backward()'s in-scope leaves (tinygrad tensor.py:487-488).
EXPORT uint32_t py_uop_leaf_tids(uint64_t root, uint32_t *out, uint32_t cap) {
  u32 n = 0;
  uop_leaf_tids((Term)root, out, cap, &n);
  return n;
}

// `root` with each from[i] replaced by to[i] (uop_substitute): gradient() swaps a
// target's buffer in for its node.
EXPORT uint64_t py_uop_substitute(uint64_t root, uint64_t const *from, uint64_t const *to,
                                  uint32_t n) {
  return uop_substitute((Term)root, (Term const *)from, (Term const *)to, n);
}

// The chain rule's pending error message (a node it has no rule for),
// cleared by the call; NULL when there is none.
EXPORT char const *py_grad_err_take(void) { return grad_err_take(); }

// Tensor.backward's chain rule (uop_backward): each leaf's fresh gradient, and
// NULL or the rule's error message.
EXPORT char const *py_backward(uint64_t y, uint64_t gy, uint32_t const *tids, uint32_t n,
                               uint64_t *grads) {
  return uop_backward(y, gy, tids, n, (Term *)grads);
}

EXPORT uint32_t py_ten_get_buf_id(uint64_t t) {
  if (term_tag(t) != TAG_TEN) return 0;
  u32 id = (u32)term_val(t);
  if (id == 0 || id >= TENS_NEXT) return 0;
  return TENS[id].buf_id;
}

// Runs a free-standing kernel root once on host buffers (cpu_jit_run_root): 0, or
// -1 when the renderer declines, -2 when the compile or load fails.
EXPORT int py_run_uop_kernel_c(uint64_t root, void *out, const void *const *ins,
                               uint32_t n, const uint32_t *in_numels) {
  return cpu_jit_run_root(root, out, ins, n, in_numels);
}

// `root` with the KOpt applied, outside any KernelEntry; 0 when it bails.
EXPORT uint64_t py_uop_dag_apply_kopt(uint64_t root, uint8_t op,
                                      uint32_t axis, uint32_t arg) {
  KOpt opt = { op, axis, arg };
  return uop_dag_apply_kopt(root, opt);
}

EXPORT uint32_t py_const_KOP_UPCAST(void)   { return KOP_UPCAST; }

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
EXPORT uint32_t py_const_UOP_SIN(void)   { return UOP_SIN; }
EXPORT uint32_t py_const_UOP_LOG2(void)  { return UOP_LOG2; }
EXPORT uint32_t py_const_UOP_SQRT(void)  { return UOP_SQRT; }

EXPORT uint32_t py_const_UOP_IADD(void)  { return UOP_IADD; }
EXPORT uint32_t py_const_UOP_IMUL(void)  { return UOP_IMUL; }
EXPORT uint32_t py_const_UOP_IDIV(void)  { return UOP_IDIV; }
EXPORT uint32_t py_const_UOP_IMOD(void)  { return UOP_IMOD; }
EXPORT uint32_t py_const_UOP_IAND(void)  { return UOP_IAND; }
EXPORT uint32_t py_const_UOP_IOR(void)   { return UOP_IOR; }
EXPORT uint32_t py_const_UOP_IXOR(void)  { return UOP_IXOR; }
EXPORT uint32_t py_const_UOP_ISHL(void)  { return UOP_ISHL; }
EXPORT uint32_t py_const_UOP_THREEFRY(void) { return UOP_THREEFRY; }
EXPORT uint32_t py_const_UOP_ISHR(void)  { return UOP_ISHR; }

EXPORT uint32_t py_const_REDUCE_SUM(void) { return REDUCE_SUM; }
EXPORT uint32_t py_const_REDUCE_MAX(void) { return REDUCE_MAX; }
EXPORT uint32_t py_const_REDUCE_PROD(void) { return REDUCE_PROD; }

EXPORT uint32_t py_const_SCATTER_SUM(void)     { return SCATTER_SUM; }
EXPORT uint32_t py_const_SCATTER_PROD(void)    { return SCATTER_PROD; }
EXPORT uint32_t py_const_SCATTER_MAX(void)     { return SCATTER_MAX; }
EXPORT uint32_t py_const_SCATTER_MIN(void)     { return SCATTER_MIN; }
EXPORT uint32_t py_const_SCATTER_REPLACE(void) { return SCATTER_REPLACE; }

EXPORT uint32_t py_const_KAX_LOOP(void)         { return KAX_LOOP; }
EXPORT uint32_t py_const_KAX_REDUCE(void)       { return KAX_REDUCE; }
EXPORT uint32_t py_const_KAX_LOCAL(void)        { return KAX_LOCAL; }
EXPORT uint32_t py_const_KAX_GLOBAL(void)       { return KAX_GLOBAL; }


