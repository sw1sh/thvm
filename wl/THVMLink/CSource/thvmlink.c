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
  Term r = nf(t);
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

// Expose kernel-entry introspection for tests.
EXTERN_C DLLEXPORT int thvm_wl_kernel_count(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)KERNELS_NEXT);
  return LIBRARY_NO_ERROR;
}

// Number of distinct KProgOp[] arrays interned in the kernel-
// program hash-cons cache.  Used by tests to assert that two
// kernels with structurally identical programs share storage.
EXTERN_C DLLEXPORT int thvm_wl_kernel_program_cache_size(WolframLibraryData libData,
                                                          mint argc,
                                                          MArgument *args,
                                                          MArgument res) {
  (void)libData; (void)argc; (void)args;
  MArgument_setInteger(res, (mint)kernel_program_cache_size());
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

EXTERN_C DLLEXPORT int thvm_wl_term_new_pri(WolframLibraryData libData, mint argc,
                                            MArgument *args, MArgument res) {
  (void)libData; (void)argc;
  u32 prim_id = (u32)MArgument_getInteger(args[0]);
  Term r = term_new_pri(prim_id);
  MArgument_setInteger(res, (mint)r);
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
  u32 elem_bytes;
  switch (sd->dtype) {
    case DT_F32: case DT_I32: elem_bytes = 4; break;
    default: return v;
  }
  u32 dst_tid = tensor_alloc(sd->backend, sd->view.shape, sd->dtype);
  if (dst_tid == 0) return v;
  u64 nbytes = (u64)sd->view.numel * (u64)elem_bytes;
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

// === ATP LibraryLink entries live in thvmlink_atp.c.  Single-TU
//     build is preserved by including the file directly. ===
#include "thvmlink_atp.c"

