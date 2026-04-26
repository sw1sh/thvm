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

EXTERN_C DLLEXPORT int WolframLibrary_initialize(WolframLibraryData libData) {
  CACHED_LIB_DATA = libData;
  libData->registerLibraryExpressionManager("ExternPin", extern_pin_manager);
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
  Term t = term_new(sub, tag, ext, val);
  MArgument_setInteger(res, (mint)t);
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

// Flip the f1d toggle from WL.  Returns the previous value
// (1 if was on, 0 if was off).  Used by wl tests / probes
// that want to exercise the inlined-kernel path.
EXTERN_C DLLEXPORT int thvm_wl_set_use_realize_info(WolframLibraryData libData,
                                                    mint argc, MArgument *args,
                                                    MArgument res) {
  (void)libData; (void)argc;
  u8 prev = MATERIALIZE_USE_REALIZE_INFO;
  MATERIALIZE_USE_REALIZE_INFO = (u8)(MArgument_getInteger(args[0]) ? 1 : 0);
  MArgument_setInteger(res, (mint)prev);
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
  Term r = wnf(t);
  MArgument_setInteger(res, (mint)r);
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
  d->backend  = CURRENT_BACKEND;
  if (CURRENT_BACKEND == &CPU_BACKEND) {
    // CPU fast path: zero-copy reference into the NumericArray's
    // bytes; release_numeric_array drops the WL handle when the
    // underlying buffer hits refcount 0.
    d->buf_id = cpu_buf_alloc_external(
        naData, (u64)numel * 4, release_numeric_array, (void *)na);
  } else {
    // Other backends (Metal): allocate + memcpy.  The
    // NumericArray reference is released right after the copy --
    // the data lives in backend-owned storage.
    d->buf_id = CURRENT_BACKEND->buf_alloc((u64)numel * 4);
    CURRENT_BACKEND->buf_write(d->buf_id, naData, (u64)numel * 4);
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
  if (dtype == DT_F32) {
    f32 v = (f32)value;
    memcpy(&bits, &v, sizeof(bits));
  } else {
    bits = (u32)(i32)value;
  }
  Term r = uop_const((u32)dtype, bits);
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

EXTERN_C DLLEXPORT int thvm_wl_uop_grad(WolframLibraryData libData, mint argc,
                                        MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  Term y      = (Term)MArgument_getInteger(args[0]);
  Term gy     = (Term)MArgument_getInteger(args[1]);
  Term target = (Term)MArgument_getInteger(args[2]);
  Term r = uop_grad(y, gy, target);
  MArgument_setInteger(res, (mint)r);
  return LIBRARY_NO_ERROR;
}

// Multi-target grad: args = [y, gy, MTensor{Integer}({x_1, ..., x_n})].
// Returns a single UOP_GRAD Term whose interact rule lowers to a
// TAG_CTR of n unary grads.  WL unpacks via thvm_wl_term_ctr_at.
EXTERN_C DLLEXPORT int thvm_wl_uop_grad_multi(WolframLibraryData libData, mint argc,
                                              MArgument *args, MArgument res) {
  (void)argc;
  Term y       = (Term)MArgument_getInteger(args[0]);
  Term gy      = (Term)MArgument_getInteger(args[1]);
  MTensor xs   = MArgument_getMTensor(args[2]);
  mint    n    = libData->MTensor_getFlattenedLength(xs);
  mint   *src  = libData->MTensor_getIntegerData(xs);
  Term targets[256];
  if (n > 256) n = 256;
  for (mint i = 0; i < n; i++) targets[i] = (Term)src[i];
  Term r = uop_grad_multi(y, gy, targets, (u32)n);
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

// Set the per-realize label for the THVM_MAT_STATS log.  Caller passes
// a UTF8 string (e.g., "fwd_conv1", "grad_w3"); the next thvm_realize
// dumps it on its summary line and clears the buffer.  No-op when
// THVM_MAT_STATS isn't set.  Returns 0.
EXTERN_C DLLEXPORT int thvm_wl_mat_stats_label(WolframLibraryData libData, mint argc,
                                                MArgument *args, MArgument res) {
  (void)argc;
  char *s = MArgument_getUTF8String(args[0]);
  if (s != NULL) {
    size_t n = strlen(s);
    if (n >= sizeof(MAT_STATS_LABEL)) n = sizeof(MAT_STATS_LABEL) - 1;
    memcpy(MAT_STATS_LABEL, s, n);
    MAT_STATS_LABEL[n] = '\0';
    libData->UTF8String_disown(s);
  }
  MArgument_setInteger(res, 0);
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

// Expose kernel-entry introspection for tests.
EXTERN_C DLLEXPORT int thvm_wl_kernel_count(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)KERNELS_NEXT);
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
  // Cols per kernel: [n_inputs, output_tid, fired, spliced,
  //                   consumer_count, output_numel, output_dtype].
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
    dst[k * nCols + 2] = (mint)ke->fired;
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
  Term r = term_new_ref(name);
  MArgument_setInteger(res, (mint)r);
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

// === 8.7b: ATP runner via LibraryLink ============================
//
// Inputs:
//   args[0] = MNumericArray (Int64) of packed Term values:
//             [n_axioms, lhs_0, rhs_0, lhs_1, rhs_1, ...,
//              lhs_{n-1}, rhs_{n-1}, goal_lhs, goal_rhs].
//             Length = 1 + 2*n_axioms + 2.
//   args[1] = max_steps  (mint)
//   args[2] = max_label  (mint; sizes the trivial precedence /
//             weights tables.  v0 uses a uniform config that
//             gives KBO_UN for most comparisons -- the saturator
//             falls into unfailing fallback.  Future stages can
//             pass a real precedence array.)
//
// Output: MNumericArray (Int64) `[status, n_rules, n_trace, n_cps]`.
//
// Designed for direct WL test usage with manually-built Terms
// (per the 8.7a memo's two-layer plan).  Stage 8.7c-d add the
// WL-side encoder + TATP[] surface.
#define ATP_WL_CFG_MAX_LABELS 64
EXTERN_C DLLEXPORT int thvm_wl_atp_run(WolframLibraryData libData, mint argc,
                                       MArgument *args, MArgument res) {
  (void)argc;
  MNumericArray na = MArgument_getMNumericArray(args[0]);
  mint max_steps   = MArgument_getInteger(args[1]);
  mint max_label   = MArgument_getInteger(args[2]);

  const struct st_WolframNumericArrayLibrary_Functions *naf
    = libData->numericarrayLibraryFunctions;
  if (naf->MNumericArray_getType(na) != MNumericArray_Type_Bit64) {
    return LIBRARY_FUNCTION_ERROR;
  }
  mint flat_len = naf->MNumericArray_getFlattenedLength(na);
  if (flat_len < 3) return LIBRARY_FUNCTION_ERROR;
  const int64_t *data = (const int64_t *)naf->MNumericArray_getData(na);

  int64_t n_ax_i = data[0];
  if (n_ax_i < 0 || (int64_t)flat_len != 1 + 2 * n_ax_i + 2) {
    return LIBRARY_FUNCTION_ERROR;
  }
  u32 n_ax = (u32)n_ax_i;

  // Build a trivial KboConfig: uniform weight=1, precedence=label.
  // Most comparisons return KBO_UN, falling into unfailing fallback;
  // this is functionally correct but inefficient.  Future stages can
  // accept a real precedence + weights array.
  if ((u32)max_label >= ATP_WL_CFG_MAX_LABELS) {
    return LIBRARY_FUNCTION_ERROR;
  }
  static u32 wl_weights[ATP_WL_CFG_MAX_LABELS];
  static u32 wl_precedence[ATP_WL_CFG_MAX_LABELS];
  for (u32 i = 0; i < (u32)max_label + 1; i++) {
    wl_weights[i] = 1;
    wl_precedence[i] = i + 1;
  }
  static KboConfig wl_kbo;
  wl_kbo.weights    = wl_weights;
  wl_kbo.precedence = wl_precedence;
  wl_kbo.n_labels   = (u32)max_label + 1;
  wl_kbo.var_weight = 1;

  AtpState *atp = thvm_atp_init(&wl_kbo, (u32)max_steps);
  if (atp == NULL) return LIBRARY_FUNCTION_ERROR;

  // Push axioms.
  for (u32 i = 0; i < n_ax; i++) {
    Term lhs = (Term)data[1 + 2 * i + 0];
    Term rhs = (Term)data[1 + 2 * i + 1];
    if (!thvm_atp_add_equation(atp, lhs, rhs)) {
      thvm_atp_free(atp);
      return LIBRARY_FUNCTION_ERROR;
    }
  }
  // Set goal (allow 0/0 to mean "completion mode").
  Term goal_lhs = (Term)data[1 + 2 * n_ax + 0];
  Term goal_rhs = (Term)data[1 + 2 * n_ax + 1];
  thvm_atp_set_goal(atp, goal_lhs, goal_rhs);

  AtpStatus st = thvm_atp_run(atp);

  // Pack stats into a 4-element Int64 NumericArray.
  mint dims[1] = {4};
  MNumericArray out;
  naf->MNumericArray_new(MNumericArray_Type_Bit64, 1, dims, &out);
  int64_t *odata = (int64_t *)naf->MNumericArray_getData(out);
  odata[0] = (int64_t)st;
  odata[1] = (int64_t)atp->n_rules;
  odata[2] = (int64_t)atp->n_trace;
  odata[3] = (int64_t)atp->n_cps;

  thvm_atp_free(atp);

  MArgument_setMNumericArray(res, out);
  return LIBRARY_NO_ERROR;
}
