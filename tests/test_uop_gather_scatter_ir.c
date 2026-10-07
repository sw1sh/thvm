// test_uop_gather_scatter_ir.c -- UOP_GATHER and UOP_SCATTER as IR nodes.
//
// The two indexed ops before any lowering reads them:
//   1. constructors hash-cons, and dim / kind / include_self tell nodes apart;
//   2. every check the frontends rely on returns 0;
//   3. shape and dtype inference;
//   4. scatter_identity_bits gives tinygrad's exact identities
//      (dtype.py:70-78);
//   5. a graph rewrite that unwraps a DETACH child keeps the NUM payload;
//   6. uop_leaf_tids reaches every tensor operand;
//   7. the WL lambda tables (book snapshot, ALO realize, binder use) see
//      every child, and those of the CAST, BITCAST and CMPEQ a gather's
//      adjoint builds;
//   8. a copying GC keeps the children and the NUM payload.
#include "../src/thvm.c"
#include "test.h"

static u32 mk_ten(u32 dtype, u32 ndim, u32 const *dims) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 d = 0; d < ndim; d++) {
    s.dims[d] = dims[d];
  }
  return tensor_alloc(&CPU_BACKEND, s, dtype);
}

static Term ten(u32 tid) {
  return term_new(0, TAG_TEN, TENS[tid].dtype, tid);
}

static Term ten2(u32 dtype, u32 d0, u32 d1) {
  u32 dims[2] = { d0, d1 };
  return ten(mk_ten(dtype, 2, dims));
}

static u32 f32b(f32 v) {
  u32 b;
  memcpy(&b, &v, sizeof b);
  return b;
}

static int shape_is(Term t, u32 d0, u32 d1) {
  Shape s;
  if (!term_shape_in(t, 0, &s)) {
    return 0;
  }
  return s.ndim == 2 && s.dims[0] == d0 && s.dims[1] == d1;
}

static u32 dtype_of(Term t) {
  u32 dt = 0xFFFFFFFFu;
  if (!term_dtype_in(t, 0, &dt)) {
    return 0xFFFFFFFFu;
  }
  return dt;
}

// === 1. hash-consing ===
static void test_hash_cons(void) {
  Term w = ten2(DT_FP32, 5, 3);
  Term i = ten2(DT_INT32, 4, 3);
  Term g0 = uop_gather(w, i, 0);
  CHECK(g0 != 0);
  CHECK_EQ(term_tag(g0), TAG_UOP);
  CHECK_EQ(term_ext(g0), UOP_GATHER);
  CHECK_EQ(uop_gather(w, i, 0), g0);
  Term g1 = uop_gather(w, i, 1);
  CHECK(g1 != 0);
  CHECK(g1 != g0);
  CHECK_EQ(uop_gather_src(g1), w);
  CHECK_EQ(uop_gather_idx(g1), i);
  CHECK_EQ(uop_gather_dim(g1), 1u);
  CHECK_EQ(uop_gather_dim(g0), 0u);

  Term base = ten2(DT_FP32, 5, 3);
  Term src  = ten2(DT_FP32, 4, 3);
  Term s = uop_scatter(base, i, src, 0, SCATTER_SUM, 1);
  CHECK(s != 0);
  CHECK_EQ(term_ext(s), UOP_SCATTER);
  CHECK_EQ(uop_scatter(base, i, src, 0, SCATTER_SUM, 1), s);
  CHECK_EQ(uop_scatter_base(s), base);
  CHECK_EQ(uop_scatter_idx(s), i);
  CHECK_EQ(uop_scatter_src(s), src);
  CHECK_EQ(uop_scatter_dim(s), 0u);
  CHECK_EQ(uop_scatter_kind(s), (u32)SCATTER_SUM);
  CHECK_EQ(uop_scatter_include_self(s), 1u);
  Term s_dim  = uop_scatter(base, i, src, 1, SCATTER_SUM, 1);
  Term s_kind = uop_scatter(base, i, src, 0, SCATTER_PROD, 1);
  Term s_self = uop_scatter(base, i, src, 0, SCATTER_SUM, 0);
  CHECK(s_dim != 0 && s_kind != 0 && s_self != 0);
  CHECK(s_dim != s && s_kind != s && s_self != s);
  CHECK(s_dim != s_kind && s_dim != s_self && s_kind != s_self);
  CHECK_EQ(uop_scatter_dim(s_dim), 1u);
  CHECK_EQ(uop_scatter_kind(s_kind), (u32)SCATTER_PROD);
  CHECK_EQ(uop_scatter_include_self(s_self), 0u);
  // REPLACE has no include_self: both spellings are one node.
  Term r0 = uop_scatter(base, i, src, 0, SCATTER_REPLACE, 0);
  Term r1 = uop_scatter(base, i, src, 0, SCATTER_REPLACE, 1);
  CHECK(r0 != 0);
  CHECK_EQ(r0, r1);
  CHECK_EQ(uop_scatter_include_self(r0), 0u);
}

// === 2. the checks return 0 ===
static void test_gather_checks(void) {
  Term w = ten2(DT_FP32, 5, 3);
  Term i = ten2(DT_INT32, 4, 3);
  u32 d4[1] = { 4 };
  CHECK_EQ(uop_gather(w, ten(mk_ten(DT_INT32, 1, d4)), 0), 0u);   // ndim
  CHECK_EQ(uop_gather(w, ten2(DT_FP32, 4, 3), 0), 0u);             // float idx
  CHECK_EQ(uop_gather(w, ten2(DT_BOOL, 4, 3), 0), 0u);             // bool idx
  CHECK_EQ(uop_gather(w, i, 2), 0u);                               // dim >= ndim
  CHECK_EQ(uop_gather(w, ten2(DT_INT32, 4, 4), 0), 0u);            // idx wider off-axis
  CHECK(uop_gather(w, ten2(DT_INT64, 4, 3), 0) != 0);             // any int dtype
  CHECK(uop_gather(w, ten2(DT_UINT8, 4, 3), 0) != 0);
  CHECK(uop_gather(w, ten2(DT_INT32, 7, 3), 0) != 0);             // longer along dim
  // A symbolic extent on either operand.
  u32 v = kvar_alloc("n", 1, 8);
  u32 sym[2] = { kvar_pack_extent(v), 3 };
  u32 one[2] = { 1, 1 };
  Term ws = uop_expand(uop_reshape(uop_const(DT_FP32, 0), 2, one), 2, sym);
  Term is = uop_expand(uop_reshape(uop_const(DT_INT32, 0), 2, one), 2, sym);
  CHECK_EQ(uop_gather(ws, i, 0), 0u);
  CHECK_EQ(uop_gather(w, is, 0), 0u);
  // An empty operand (the frontends return early on empty input).
  u32 none[4] = { 0, 0, 0, 3 };
  CHECK_EQ(uop_gather(uop_shrink(w, 2, none), uop_shrink(i, 2, none), 1), 0u);
  CHECK_EQ(uop_gather(w, uop_shrink(i, 2, none), 1), 0u);
}

static void test_scatter_checks(void) {
  Term base = ten2(DT_FP32, 5, 3);
  Term i    = ten2(DT_INT32, 4, 3);
  Term src  = ten2(DT_FP32, 4, 3);
  u32 d4[1] = { 4 };
  CHECK_EQ(uop_scatter(base, i, ten2(DT_FP32, 5, 3), 0, SCATTER_SUM, 1), 0u);  // src != idx shape
  CHECK_EQ(uop_scatter(base, ten2(DT_INT32, 4, 4), ten2(DT_FP32, 4, 4), 0,
                       SCATTER_SUM, 1), 0u);                                   // idx wider off-axis
  CHECK(uop_scatter(base, ten2(DT_INT32, 7, 3), ten2(DT_FP32, 7, 3), 0,
                    SCATTER_SUM, 1) != 0);                                     // longer along dim
  CHECK_EQ(uop_scatter(base, i, ten2(DT_INT32, 4, 3), 0, SCATTER_SUM, 1), 0u); // src dtype
  CHECK_EQ(uop_scatter(base, ten(mk_ten(DT_INT32, 1, d4)), src, 0,
                       SCATTER_SUM, 1), 0u);                                   // ndim
  CHECK_EQ(uop_scatter(base, ten2(DT_FP32, 4, 3), src, 0, SCATTER_SUM, 1), 0u);// float idx
  CHECK_EQ(uop_scatter(base, i, src, 2, SCATTER_SUM, 1), 0u);                  // dim >= ndim
  CHECK_EQ(uop_scatter(base, i, src, 0, SCATTER_REPLACE + 1, 1), 0u);          // kind
  CHECK_EQ(uop_scatter(base, i, src, 0, SCATTER_SUM, 2), 0u);                  // include_self
  // A 64-bit integer min / max has no CONST payload.
  Term b64 = ten2(DT_INT64, 5, 3);
  Term s64 = ten2(DT_INT64, 4, 3);
  CHECK_EQ(uop_scatter(b64, i, s64, 0, SCATTER_MAX, 1), 0u);
  CHECK(uop_scatter(b64, i, s64, 0, SCATTER_SUM, 1) != 0);
  CHECK(uop_scatter(ten2(DT_INT32, 5, 3), i, ten2(DT_INT32, 4, 3), 0,
                    SCATTER_MAX, 1) != 0);
}

// === 3. shape and dtype ===
static void test_shape_dtype(void) {
  Term w  = ten2(DT_FP32, 5, 3);
  Term wi = ten2(DT_INT32, 5, 3);
  Term i  = ten2(DT_INT32, 4, 3);
  Term g = uop_gather(w, i, 1);
  CHECK(shape_is(g, 4, 3));
  CHECK_EQ(dtype_of(g), (u32)DT_FP32);
  CHECK_EQ(dtype_of(uop_gather(wi, i, 1)), (u32)DT_INT32);
  Term s = uop_scatter(w, i, ten2(DT_FP32, 4, 3), 0, SCATTER_MAX, 0);
  CHECK(shape_is(s, 5, 3));
  CHECK_EQ(dtype_of(s), (u32)DT_FP32);
  CHECK_EQ(uop_arity(UOP_GATHER), 2u);
  CHECK_EQ(uop_arity(UOP_SCATTER), 3u);
}

// === 4. identities ===
static u32 ident(u32 kind, u32 dtype, int *ok) {
  u32 bits = 0xDEADBEEFu;
  *ok = scatter_identity_bits(kind, dtype, &bits);
  return bits;
}

static void test_identities(void) {
  int ok;
  CHECK_EQ(ident(SCATTER_SUM, DT_FP32, &ok), 0u);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_PROD, DT_FP32, &ok), f32b(1.0f));
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MAX, DT_FP32, &ok), 0xFF800000u);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MIN, DT_FP32, &ok), 0x7F800000u);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MAX, DT_BF16, &ok), 0xFF800000u);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MIN, DT_FP64, &ok), 0x7F800000u);
  CHECK(ok);
  // fp8e4m3 has no infinity: its extremes are the largest normal, 448.
  CHECK_EQ(ident(SCATTER_MAX, DT_FP8E4M3, &ok), f32b(-448.0f));
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MIN, DT_FP8E4M3, &ok), f32b(448.0f));
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MAX, DT_FP8E5M2, &ok), 0xFF800000u);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_PROD, DT_INT32, &ok), 1u);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MAX, DT_INT32, &ok), 0x80000000u);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MIN, DT_INT32, &ok), 0x7FFFFFFFu);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MAX, DT_INT8, &ok), 0xFFFFFF80u);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MIN, DT_INT16, &ok), 0x7FFFu);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MAX, DT_UINT8, &ok), 0u);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MIN, DT_UINT8, &ok), 0xFFu);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MIN, DT_UINT32, &ok), 0xFFFFFFFFu);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MAX, DT_BOOL, &ok), 0u);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MIN, DT_BOOL, &ok), 1u);
  CHECK(ok);
  CHECK_EQ(ident(SCATTER_MAX, DT_UINT64, &ok), 0u);
  CHECK(ok);
  ident(SCATTER_MIN, DT_UINT64, &ok);
  CHECK(!ok);
  ident(SCATTER_MAX, DT_INT64, &ok);
  CHECK(!ok);
  ident(SCATTER_REPLACE + 1, DT_FP32, &ok);
  CHECK(!ok);
}

// === 5. a rewrite that unwraps DETACH keeps the payload ===
static Term rule_strip_detach(Term t, void *user) {
  (void)user;
  if (term_tag(t) != TAG_UOP || term_ext(t) != UOP_DETACH) {
    return 0;
  }
  return heap_read(term_val(t));
}

static Term strip_detach(Term root) {
  static UOpGraphRewriteRule const rules[] = {{"strip-detach", rule_strip_detach}};
  return uop_graph_rewrite(root, rules, 1, NULL);
}

static void test_detach_rewrite(void) {
  Term w = ten2(DT_FP32, 5, 3);
  Term i = ten2(DT_INT32, 4, 3);
  Term g = strip_detach(uop_gather(uop_detach(w), i, 1));
  CHECK_EQ(term_ext(g), UOP_GATHER);
  CHECK_EQ(uop_gather_src(g), w);
  CHECK_EQ(uop_gather_idx(g), i);
  CHECK_EQ(uop_gather_dim(g), 1u);
  Term base = ten2(DT_FP32, 5, 3);
  Term src  = ten2(DT_FP32, 4, 3);
  Term s = strip_detach(uop_scatter(base, i, uop_detach(src), 1, SCATTER_MIN, 0));
  CHECK_EQ(term_ext(s), UOP_SCATTER);
  CHECK_EQ(uop_scatter_base(s), base);
  CHECK_EQ(uop_scatter_idx(s), i);
  CHECK_EQ(uop_scatter_src(s), src);
  CHECK_EQ(uop_scatter_dim(s), 1u);
  CHECK_EQ(uop_scatter_kind(s), (u32)SCATTER_MIN);
  CHECK_EQ(uop_scatter_include_self(s), 0u);
}

// === 6. leaf tids ===
static int has_tid(u32 const *tids, u32 n, Term t) {
  for (u32 k = 0; k < n; k++) {
    if (tids[k] == (u32)term_val(t)) {
      return 1;
    }
  }
  return 0;
}

static void test_leaf_tids(void) {
  Term w = ten2(DT_FP32, 5, 3);
  Term i = ten2(DT_INT32, 4, 3);
  u32 tids[8];
  u32 n = 0;
  uop_leaf_tids(uop_gather(uop_unary(UOP_NEG, w), i, 0), tids, 8, &n);
  CHECK_EQ(n, 2u);
  CHECK(has_tid(tids, n, w) && has_tid(tids, n, i));
  Term base = ten2(DT_FP32, 5, 3);
  Term src  = ten2(DT_FP32, 4, 3);
  uop_leaf_tids(uop_scatter(base, i, src, 0, SCATTER_SUM, 1), tids, 8, &n);
  CHECK_EQ(n, 3u);
  CHECK(has_tid(tids, n, base) && has_tid(tids, n, i) && has_tid(tids, n, src));
}

// === 7. WL lambda tables ===
static void test_lambda_tables(void) {
  Term w = ten2(DT_FP32, 5, 3);
  Term i = ten2(DT_INT32, 4, 3);
  Term g = uop_gather(uop_unary(UOP_NEG, w), i, 1);
  // The book snapshot owns every cell, the NUM payload included.
  Term bg = thvm_book_from_dynamic(g);
  CHECK_EQ(term_ext(bg), UOP_GATHER);
  CHECK_EQ(term_ext(book_read(term_val(bg) + 0)), UOP_NEG);
  CHECK_EQ(book_read(term_val(bg) + 1), i);
  Term bdim = book_read(term_val(bg) + 2);
  CHECK_EQ(term_tag(bdim), TAG_NUM);
  CHECK_EQ(term_val(bdim), 1u);
  // ALO realize copies the cells into the dynamic heap.
  Term dg = alo_realize(bg, 0);
  CHECK_EQ(term_ext(dg), UOP_GATHER);
  CHECK_EQ(term_tag(heap_read(term_val(dg) + 0)), TAG_ALO);
  CHECK_EQ(heap_read(term_val(dg) + 1), i);
  CHECK_EQ(term_val(heap_read(term_val(dg) + 2)), 1u);

  Term base = ten2(DT_FP32, 5, 3);
  Term src  = ten2(DT_FP32, 4, 3);
  Term s  = uop_scatter(base, i, uop_unary(UOP_NEG, src), 1, SCATTER_MAX, 0);
  Term bs = thvm_book_from_dynamic(s);
  CHECK_EQ(term_ext(bs), UOP_SCATTER);
  CHECK_EQ(term_ext(book_read(term_val(bs) + 2)), UOP_NEG);
  CHECK_EQ(term_val(book_read(term_val(bs) + 3)), 1u);
  CHECK_EQ(term_val(book_read(term_val(bs) + 4)), (u32)SCATTER_MAX);
  CHECK_EQ(term_tag(book_read(term_val(bs) + 5)), TAG_NUM);
  CHECK_EQ(term_val(book_read(term_val(bs) + 5)), 0u);
  Term ds = alo_realize(bs, 0);
  CHECK_EQ(term_ext(ds), UOP_SCATTER);
  CHECK_EQ(term_tag(heap_read(term_val(ds) + 2)), TAG_ALO);
  CHECK_EQ(term_val(heap_read(term_val(ds) + 4)), (u32)SCATTER_MAX);

  // A binder used only under an indexed op is still used.
  Shape vs = {0};
  vs.ndim = 2;
  vs.dims[0] = 5;
  vs.dims[1] = 3;
  u64 lam = heap_alloc(1);
  heap_set(lam, 0);
  lam_shape_set(lam, &vs);
  Term gb = uop_gather(term_new(0, TAG_VAR, 0, lam), i, 0);
  CHECK(gb != 0);
  heap_set(lam, gb);
  CHECK(lam_body_uses_var(lam));
  vs.dims[0] = 4;
  u64 lam2 = heap_alloc(1);
  heap_set(lam2, 0);
  lam_shape_set(lam2, &vs);
  Term sb = uop_scatter(base, i, term_new(0, TAG_VAR, 0, lam2), 0, SCATTER_SUM, 1);
  CHECK(sb != 0);
  heap_set(lam2, sb);
  CHECK(lam_body_uses_var(lam2));

  // A half gather's adjoint casts between f16 and f32: a CAST, a BITCAST and
  // a CMPEQ keep their cells in the book and get ALO children when realized,
  // instead of passing through with a stale heap location.
  Term nw     = uop_unary(UOP_NEG, w);
  Term ops[3] = { uop_cast(nw, DT_FP16), uop_bitcast(nw, DT_INT32),
                  uop_binary(UOP_CMPEQ, nw, w) };
  u32  opc[3] = { UOP_CAST, UOP_BITCAST, UOP_CMPEQ };
  for (u32 q = 0; q < 3; q++) {
    CHECK_EQ(term_tag(ops[q]), TAG_UOP);
    CHECK_EQ(term_ext(ops[q]), opc[q]);
    Term bo = thvm_book_from_dynamic(ops[q]);
    CHECK_EQ(term_ext(bo), term_ext(ops[q]));
    CHECK_EQ(term_ext(book_read(term_val(bo) + 0)), UOP_NEG);
    CHECK_EQ(book_read(term_val(bo) + 1), heap_read(term_val(ops[q]) + 1));
    Term dq = alo_realize(bo, 0);
    CHECK_EQ(term_ext(dq), term_ext(ops[q]));
    CHECK_EQ(term_tag(heap_read(term_val(dq) + 0)), TAG_ALO);
    CHECK_EQ(heap_read(term_val(dq) + 1), heap_read(term_val(ops[q]) + 1));
  }
}

// === 8. a copying GC keeps children and payload ===
static int in_from_space(Term t) {
  u64 loc = term_val(t);
  return loc >= gc_from_start() && loc < gc_from_end();
}

static void test_gc_round_trip(void) {
  Term w    = ten2(DT_FP32, 5, 3);
  Term i    = ten2(DT_INT32, 4, 3);
  Term base = ten2(DT_FP32, 5, 3);
  Term src  = ten2(DT_FP32, 4, 3);
  Term roots[2];
  roots[0] = uop_gather(uop_unary(UOP_NEG, w), i, 1);
  roots[1] = uop_scatter(base, i, uop_unary(UOP_NEG, src), 1, SCATTER_PROD, 0);
  CHECK(roots[0] != 0 && roots[1] != 0);
  CHECK(gc_enabled());
  gc_collect(roots, 2);
  Term g = roots[0];
  CHECK_EQ(term_ext(g), UOP_GATHER);
  CHECK(in_from_space(g));
  CHECK_EQ(uop_gather_dim(g), 1u);
  CHECK_EQ(uop_gather_idx(g), i);
  Term gs = uop_gather_src(g);
  CHECK_EQ(term_ext(gs), UOP_NEG);
  CHECK(in_from_space(gs));
  CHECK_EQ(heap_read(term_val(gs)), w);
  Term s = roots[1];
  CHECK_EQ(term_ext(s), UOP_SCATTER);
  CHECK(in_from_space(s));
  CHECK_EQ(uop_scatter_base(s), base);
  CHECK_EQ(uop_scatter_idx(s), i);
  CHECK_EQ(uop_scatter_dim(s), 1u);
  CHECK_EQ(uop_scatter_kind(s), (u32)SCATTER_PROD);
  CHECK_EQ(uop_scatter_include_self(s), 0u);
  Term ss = uop_scatter_src(s);
  CHECK_EQ(term_ext(ss), UOP_NEG);
  CHECK(in_from_space(ss));
  CHECK_EQ(heap_read(term_val(ss)), src);
}

int main(void) {
  thvm_init();
  TEST_BEGIN("gather-scatter-ir/hash-cons");
  test_hash_cons();
  TEST_BEGIN("gather-scatter-ir/gather-checks");
  test_gather_checks();
  TEST_BEGIN("gather-scatter-ir/scatter-checks");
  test_scatter_checks();
  TEST_BEGIN("gather-scatter-ir/shape-dtype");
  test_shape_dtype();
  TEST_BEGIN("gather-scatter-ir/identities");
  test_identities();
  TEST_BEGIN("gather-scatter-ir/detach-rewrite");
  test_detach_rewrite();
  TEST_BEGIN("gather-scatter-ir/leaf-tids");
  test_leaf_tids();
  TEST_BEGIN("gather-scatter-ir/lambda-tables");
  test_lambda_tables();
  TEST_BEGIN("gather-scatter-ir/gc-round-trip");
  test_gc_round_trip();
  thvm_free();
  TEST_REPORT();
}
