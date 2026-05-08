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
  return term_new(0, TAG_NUM, DT_INT32, (uint64_t)(uint32_t)v);
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

EXPORT void py_string_free(char *s) { free(s); }

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
