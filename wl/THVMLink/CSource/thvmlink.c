// thvmlink.c - Wolfram LibraryLink bridge for thvm.
//
// Single-TU build: includes the entire runtime via ../../../src/thvm.c
// (this file lives at wl/THVMLink/CSource/).  All exported functions are
// scalar-in / scalar-out (mint <-> Integer).  Higher-level constructors
// (TLam, TApp, TSup, TDup) are synthesized on the WL side from these
// primitives - keeps the C surface tiny.

#include "WolframLibrary.h"
#include "WolframNumericArrayLibrary.h"
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

EXTERN_C DLLEXPORT mint WolframLibrary_getVersion(void) {
  return WolframLibraryVersion;
}

EXTERN_C DLLEXPORT int WolframLibrary_initialize(WolframLibraryData libData) {
  CACHED_LIB_DATA = libData;
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
  MArgument_setInteger(res, (mint)term_new(sub, tag, ext, val));
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
  MArgument_setInteger(res, (mint)wnf(t));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_itrs(WolframLibraryData libData, mint argc,
                                    MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)ITRS);
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
  // WL passes Real -> double; convert to f32 for DT_F32 buffers.
  if (d->dtype == DT_F32) {
    double *src = libData->MTensor_getRealData(data);
    f32 *tmp = (f32 *)malloc((size_t)n * sizeof(f32));
    for (mint i = 0; i < n; i++) tmp[i] = (f32)src[i];
    d->backend->buf_write(d->buf_id, tmp, (u64)n * sizeof(f32));
    free(tmp);
  } else if (d->dtype == DT_I32) {
    mint *src = libData->MTensor_getIntegerData(data);
    i32 *tmp = (i32 *)malloc((size_t)n * sizeof(i32));
    for (mint i = 0; i < n; i++) tmp[i] = (i32)src[i];
    d->backend->buf_write(d->buf_id, tmp, (u64)n * sizeof(i32));
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

  numericarray_data_t t = (d->dtype == DT_F32) ? MNumericArray_Type_Real32
                                               : MNumericArray_Type_Bit32;

  MNumericArray out;
  libData->numericarrayLibraryFunctions->MNumericArray_new(t, rank, dims, &out);
  void *dst = libData->numericarrayLibraryFunctions->MNumericArray_getData(out);
  u64   nbytes = (u64)d->view.numel * 4;   // both DT_F32 and DT_I32 are 4B
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
  if      (t == MNumericArray_Type_Real32) dtype = DT_F32;
  else if (t == MNumericArray_Type_Bit32)  dtype = DT_I32;
  else {
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
  d->backend  = &CPU_BACKEND;
  d->buf_id   = cpu_buf_alloc_external(
      naData, (u64)numel * 4, release_numeric_array, (void *)na);

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
  if (dtype == DT_F32) {
    f32 v = (f32)value;
    memcpy(&bits, &v, sizeof(bits));
  } else {
    bits = (u32)(i32)value;
  }
  MArgument_setInteger(res, (mint)uop_const((u32)dtype, bits));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_unary(WolframLibraryData libData, mint argc,
                                         MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  mint op  = MArgument_getInteger(args[0]);
  Term src = (Term)MArgument_getInteger(args[1]);
  MArgument_setInteger(res, (mint)uop_unary((u32)op, src));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_binary(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  mint op = MArgument_getInteger(args[0]);
  Term a  = (Term)MArgument_getInteger(args[1]);
  Term b  = (Term)MArgument_getInteger(args[2]);
  MArgument_setInteger(res, (mint)uop_binary((u32)op, a, b));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_reduce(WolframLibraryData libData, mint argc,
                                          MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  mint kind = MArgument_getInteger(args[0]);
  mint axis = MArgument_getInteger(args[1]);
  Term src  = (Term)MArgument_getInteger(args[2]);
  MArgument_setInteger(res, (mint)uop_reduce((u32)kind, (u32)axis, src));
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
  MArgument_setInteger(res, (mint)ctor(src, (u32)n, buf));
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
  MArgument_setInteger(res, (mint)ctor(src, (u32)(n / 2), buf));
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
  MArgument_setInteger(res, (mint)uop_flip(src, (u32)mask));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_materialize(WolframLibraryData libData, mint argc,
                                               MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term expr = (Term)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)uop_materialize(expr));
  return LIBRARY_NO_ERROR;
}

EXTERN_C DLLEXPORT int thvm_wl_uop_grad(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term y      = (Term)MArgument_getInteger(args[0]);
  Term gy     = (Term)MArgument_getInteger(args[1]);
  Term target = (Term)MArgument_getInteger(args[2]);
  MArgument_setInteger(res, (mint)uop_grad(y, gy, target));
  return LIBRARY_NO_ERROR;
}

// Direct materialize: runs the schedule + kernelize + linearize pass
// immediately and returns the scheduled DAG term.  Fires no kernels
// (that happens in TWnf via the interact_kernel rule in commit 4).
EXTERN_C DLLEXPORT int thvm_wl_materialize(WolframLibraryData libData, mint argc,
                                           MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term t = (Term)MArgument_getInteger(args[0]);
  MArgument_setInteger(res, (mint)thvm_materialize(t));
  return LIBRARY_NO_ERROR;
}

// Expose kernel-entry introspection for tests.
EXTERN_C DLLEXPORT int thvm_wl_kernel_count(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)KERNELS_NEXT);
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
  MArgument_setInteger(res, (mint)term_new_ref(name));
  return LIBRARY_NO_ERROR;
}
