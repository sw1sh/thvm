// test_uop_gather.c -- UOP_GATHER forward through thvm_realize.
//
// Every case compares the realized tensor with a host reference byte for
// byte.  Run the binary twice: as is (kernels run through the C JIT), and
// with THVM_CPU_JIT_WARMUP=1000000000 (the precompile and both JIT caches
// stand behind it, so each kernel runs on the UOp walker).  check_dispatch asserts which backend ran each kernel,
// so the two runs agree with the reference, and with each other, on both
// backends.
//
//   1. dims 0/1/2, idx smaller than src off-axis, extent-1 axes;
//   2. embedding rows with invalid ids {-1, V, 10982377, -19238}; -inf / inf
//      / NaN rows read exactly when selected and never leak otherwise;
//   3. src forms: a TEN, W[1:], a PERMUTE view, a view-alias TEN, relu(h)
//      (one extra kernel), a prior kernel's output, a CONST;
//   4. idx forms: idx + 1 (fused on the index grid, realized before an
//      expand), a cast from float, tokens[:, 1:], edge_index[1] with V < N;
//      idx views read in place through their storage, as src is: a
//      view-alias TEN (an offset, a PERMUTE), lazy SHRINK and FLIP views
//      used directly, a column SHRINK gathering along dim 1;
//   5. fusion: gather * 2 + 1 is one kernel; a gather inside a SUM; a
//      gather feeding a matmul realizes and the matmul runs on BLAS;
//   6. the gather kernel holds no REDUCE, loops at most |idx| times per
//      store, and is flagged no_opts;
//   7. JIT capture, then replays with the index buffer rewritten in place;
//   8. a timing line at V = 1e4, N = 1024, D = 64.
#include "../src/thvm.c"
#include "test.h"
#include "tu_probes.h"
#include <time.h>

static u32 mk(u32 dtype, u32 ndim, u32 const *dims, void const *data) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 d = 0; d < ndim; d++) {
    s.dims[d] = dims[d];
  }
  u32 tid = tensor_alloc(&CPU_BACKEND, s, dtype);
  if (data != NULL) {
    CPU_BACKEND.buf_write(TENS[tid].buf_id, (void *)data,
                          (u64)TENS[tid].view.numel * dtype_itemsize(dtype));
  }
  return tid;
}

static Term ten(u32 tid) {
  return term_new(0, TAG_TEN, TENS[tid].dtype, tid);
}

static Term fconst(u32 ndim, u32 const *dims, f32 v) {
  u32 bits;
  memcpy(&bits, &v, sizeof bits);
  u32 ones[MAX_DIM] = {0};
  for (u32 i = 0; i < MAX_DIM; i++) {
    ones[i] = 1;
  }
  return uop_expand(uop_reshape(uop_const(DT_FP32, bits), ndim, ones), ndim, dims);
}

static Term iconst(u32 ndim, u32 const *dims, i32 v) {
  u32 ones[MAX_DIM] = {0};
  for (u32 i = 0; i < MAX_DIM; i++) {
    ones[i] = 1;
  }
  return uop_expand(uop_reshape(uop_const(DT_INT32, (u32)v), ndim, ones), ndim, dims);
}

// idx (N, D) that repeats ids (N) along axis 1: the embedding index.
static Term rows(Term ids, u32 n, u32 d) {
  u32 n1[2] = { n, 1 };
  u32 nd[2] = { n, d };
  return uop_expand(uop_reshape(ids, 2, n1), 2, nd);
}

static u32 numel_of(Shape const *s) {
  u32 n = 1;
  for (u32 d = 0; d < s->ndim; d++) {
    n *= s->dims[d];
  }
  return n;
}

static void ref_gather(f32 const *src, Shape const *ss, i32 const *idx,
                       Shape const *is, u32 dim, f32 *out) {
  u32 n = numel_of(is);
  for (u32 p = 0; p < n; p++) {
    u32 c[MAX_DIM];
    u32 rem = p;
    for (i32 d = (i32)is->ndim - 1; d >= 0; d--) {
      c[d] = rem % is->dims[d];
      rem /= is->dims[d];
    }
    i32 v = idx[p];
    if (v < 0 || (u32)v >= ss->dims[dim]) {
      out[p] = 0.0f;
      continue;
    }
    c[dim] = (u32)v;
    u32 q = 0;
    for (u32 d = 0; d < ss->ndim; d++) {
      q = q * ss->dims[d] + c[d];
    }
    out[p] = src[q];
  }
}

static void check_bytes(void const *got, void const *want, u32 n,
                        char const *what) {
  f32 const *g = (f32 const *)got;
  f32 const *w = (f32 const *)want;
  u32 shown = 0;
  for (u32 i = 0; i < n && shown < 8; i++) {
    if (memcmp(&g[i], &w[i], 4) != 0) {
      fprintf(stderr, "  %s: out[%u] got=%.9g want=%.9g\n", what, i, g[i], w[i]);
      shown++;
    }
  }
  CHECK(memcmp(got, want, (size_t)n * 4) == 0);
}

// Every kernel emitted since k0 that ran did so on the backend this run
// asked for (BLAS may take a matmul in either run).  DISPATCHED counts the
// kernels checked, so a run that dispatched nothing cannot pass.
static u32 DISPATCHED = 0;

static void check_dispatch(u32 k0) {
  u32 want = cpu_jit_walker_only() ? KDISPATCH_INTERPRETER : KDISPATCH_JIT;
  for (u32 k = k0; k < KERNELS_NEXT; k++) {
    u32 kind = cg_kernel_dispatch_kind(k);
    if (kind == KDISPATCH_NONE
        || kind == KDISPATCH_BLAS_GEMM) {
      continue;
    }
    CHECK_EQ(kind, want);
    DISPATCHED++;
  }
}

// Realize `t` into `out` (n f32 words).  Returns the result tid, 0 when the
// realize produced no tensor.
static u32 realize_into(Term t, void *out, u32 n) {
  Term r = term_resolve(thvm_realize(t));
  CHECK_EQ(term_tag(r), TAG_TEN);
  if (term_tag(r) != TAG_TEN) {
    memset(out, 0, (size_t)n * 4);
    return 0;
  }
  u32 tid = (u32)term_val(r);
  TENS[tid].backend->buf_read(TENS[tid].buf_id, out, (u64)n * 4);
  return tid;
}

// Distinct RANGE leaves of a store (address and value): the loop nest the
// backends run for it.
static void collect_ranges(Term t, u32 *aid, u32 *ext, u32 *n, u32 depth) {
  if (depth > 256 || term_tag(t) != TAG_UOP) {
    return;
  }
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_RANGE) {
    u32 a = uop_range_axis_id(t);
    for (u32 i = 0; i < *n; i++) {
      if (aid[i] == a) {
        return;
      }
    }
    if (*n < 64) {
      aid[*n] = a;
      ext[*n] = uop_range_extent(t);
      (*n)++;
    }
    return;
  }
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    collect_ranges(heap_read(loc + i), aid, ext, n, depth + 1);
  }
}

static u64 store_loops(Term store) {
  u32 aid[64], ext[64], n = 0;
  collect_ranges(store, aid, ext, &n, 0);
  u64 prod = 1;
  for (u32 i = 0; i < n; i++) {
    prod *= ext[i];
  }
  return prod;
}

static int max_store_loops_rec(Term root, u64 *out) {
  if (term_tag(root) == TAG_UOP && term_ext(root) == UOP_AFTER) {
    max_store_loops_rec(uop_after_node(root), out);
    max_store_loops_rec(uop_after_after_node(root), out);
    return 1;
  }
  u64 p = store_loops(root);
  if (p > *out) {
    *out = p;
  }
  return 1;
}

static int has_op(Term t, u32 want, u32 depth) {
  if (depth > 256 || term_tag(t) != TAG_UOP) {
    return 0;
  }
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == want) {
    return 1;
  }
  u8 ar = (op == UOP_AFTER) ? 2 : uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    if (has_op(heap_read(loc + i), want, depth + 1)) {
      return 1;
    }
  }
  return 0;
}

// The kernel emitted since k0 that writes tid (the realize detaches a
// committed output from its producer, so search the kernel table).
static u32 kernel_for(u32 tid, u32 k0) {
  for (u32 k = KERNELS_NEXT; k-- > k0 && k > 0;) {
    if (KERNELS[k].output_tid == tid) {
      return k;
    }
  }
  return 0;
}

// The gather kernel (the one producing tid): no REDUCE, at most |idx|
// iterations per store, no_opts set.
static void check_gather_kernel(u32 tid, u32 k0, u64 idx_numel) {
  if (tid == 0) {
    return;
  }
  u32 kid = kernel_for(tid, k0);
  CHECK(kid != 0);
  if (kid == 0) {
    return;
  }
  Term root = KERNELS[kid].cached_lift.store_root;
  CHECK(root != 0);
  CHECK(!has_op(root, UOP_REDUCE, 0));
  u64 loops = 0;
  max_store_loops_rec(root, &loops);
  CHECK(loops <= idx_numel);
  CHECK_EQ(KERNELS[kid].no_opts, 1);
}

static void fill_src(f32 *v, u32 n, f32 scale) {
  for (u32 q = 0; q < n; q++) {
    v[q] = scale * (f32)((i32)(q * 7u % 23u) - 11);
  }
}

// === 1. dims 0/1/2 and extent-1 axes ===
static void gather_case(Shape const *ss, Shape const *is, u32 dim,
                        i32 const *idx, char const *what) {
  u32 sn = numel_of(ss), in = numel_of(is);
  f32 *src  = (f32 *)malloc((size_t)sn * 4);
  f32 *want = (f32 *)malloc((size_t)in * 4);
  f32 *got  = (f32 *)malloc((size_t)in * 4);
  fill_src(src, sn, 0.25f);
  ref_gather(src, ss, idx, is, dim, want);
  u32 k0 = KERNELS_NEXT;
  Term g = uop_gather(ten(mk(DT_FP32, ss->ndim, ss->dims, src)),
                      ten(mk(DT_INT32, is->ndim, is->dims, idx)), dim);
  CHECK(g != 0);
  u32 tid = realize_into(g, got, in);
  check_bytes(got, want, in, what);
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
  check_gather_kernel(tid, k0, in);
  free(src);
  free(want);
  free(got);
}

static void test_dims(void) {
  Shape ss = {0}, is = {0};
  ss.ndim = 3; ss.dims[0] = 4; ss.dims[1] = 5; ss.dims[2] = 6;
  is.ndim = 3; is.dims[0] = 3; is.dims[1] = 4; is.dims[2] = 5;
  i32 idx[60];
  for (u32 dim = 0; dim < 3; dim++) {
    for (u32 p = 0; p < 60; p++) {
      idx[p] = (i32)((p * 5u + dim) % (ss.dims[dim] + 3)) - 1;
    }
    char what[32];
    snprintf(what, sizeof what, "gather dim %u", dim);
    gather_case(&ss, &is, dim, idx, what);
  }
  Shape s1 = {0}, i1 = {0};
  s1.ndim = 2; s1.dims[0] = 1; s1.dims[1] = 7;
  i1.ndim = 2; i1.dims[0] = 1; i1.dims[1] = 5;
  i32 idx1[5] = { 6, 0, 7, -1, 3 };
  gather_case(&s1, &i1, 1, idx1, "gather extent-1 row");
  Shape s2 = {0}, i2 = {0};
  s2.ndim = 2; s2.dims[0] = 6; s2.dims[1] = 1;
  i2.ndim = 2; i2.dims[0] = 4; i2.dims[1] = 1;
  i32 idx2[4] = { 5, 2, 6, 0 };
  gather_case(&s2, &i2, 0, idx2, "gather extent-1 column");
}

// === 2. embedding rows, invalid ids, non-finite rows ===
#define EV 6
#define ED 4
#define EN 8

static void ref_rows(f32 const *w, i32 const *ids, u32 n, u32 v, u32 d,
                     f32 *out) {
  for (u32 r = 0; r < n; r++) {
    for (u32 c = 0; c < d; c++) {
      int ok = ids[r] >= 0 && (u32)ids[r] < v;
      out[r * d + c] = ok ? w[(u32)ids[r] * d + c] : 0.0f;
    }
  }
}

static void test_embedding_rows(void) {
  f32 w[EV * ED];
  fill_src(w, EV * ED, 0.5f);
  w[5 * ED + 0] = -INFINITY;
  w[5 * ED + 1] = INFINITY;
  w[5 * ED + 2] = NAN;
  u32 wd[2] = { EV, ED };
  u32 wt = mk(DT_FP32, 2, wd, w);
  static i32 const bad[EN] = { 2, -1, 0, EV, 3, 10982377, 1, -19238 };
  static i32 const hot[EN] = { 5, 4, 5, 0, -1, 5, 2, 3 };
  i32 const *cases[2] = { bad, hot };
  for (u32 k = 0; k < 2; k++) {
    u32 nd[1] = { EN };
    Term g = uop_gather(ten(wt), rows(ten(mk(DT_INT32, 1, nd, cases[k])), EN, ED), 0);
    f32 want[EN * ED], got[EN * ED];
    ref_rows(w, cases[k], EN, EV, ED, want);
    u32 k0 = KERNELS_NEXT;
    u32 tid = realize_into(g, got, EN * ED);
    check_bytes(got, want, EN * ED, k == 0 ? "invalid ids" : "non-finite rows");
    CHECK_EQ(KERNELS_NEXT - k0, 1u);
    check_dispatch(k0);
    check_gather_kernel(tid, k0, EN * ED);
  }
}

// === 3. src forms ===
static void rows_case(Term src, f32 const *w_eff, u32 v, i32 const *ids,
                      u32 want_kernels, char const *what) {
  u32 nd[1] = { EN };
  Term g = uop_gather(src, rows(ten(mk(DT_INT32, 1, nd, ids)), EN, ED), 0);
  CHECK(g != 0);
  f32 want[EN * ED], got[EN * ED];
  ref_rows(w_eff, ids, EN, v, ED, want);
  u32 k0 = KERNELS_NEXT;
  u32 tid = realize_into(g, got, EN * ED);
  check_bytes(got, want, EN * ED, what);
  CHECK_EQ(KERNELS_NEXT - k0, want_kernels);
  check_dispatch(k0);
  check_gather_kernel(tid, k0, EN * ED);
}

static void test_src_forms(void) {
  static i32 const ids[EN] = { 2, -1, 0, 4, 3, 5, 1, 6 };
  f32 w[(EV + 1) * ED];
  fill_src(w, (EV + 1) * ED, 0.5f);
  u32 wd[2] = { EV + 1, ED };
  u32 wt = mk(DT_FP32, 2, wd, w);
  // W[1:] (SHRINK with an offset): row r of the view is row r + 1.
  u32 be[4] = { 1, EV + 1, 0, ED };
  rows_case(uop_shrink(ten(wt), 2, be), w + ED, EV, ids, 1, "src W[1:]");
  // A PERMUTE view: Wt is (D, V), gathered as (V, D).
  f32 wt_data[ED * EV];
  for (u32 r = 0; r < EV; r++) {
    for (u32 c = 0; c < ED; c++) {
      wt_data[c * EV + r] = w[r * ED + c];
    }
  }
  u32 td[2] = { ED, EV };
  u32 perm[2] = { 1, 0 };
  rows_case(uop_permute(ten(mk(DT_FP32, 2, td, wt_data)), 2, perm), w, EV, ids, 1,
            "src PERMUTE view");
  // A TEN whose descriptor is itself a view (an alias with an offset).
  Term alias = materialize_uop_in_env(uop_shrink(ten(wt), 2, be), 0);
  CHECK_EQ(term_tag(alias), TAG_TEN);
  CHECK(TENS[(u32)term_val(alias)].view.offset != 0);
  rows_case(alias, w + ED, EV, ids, 1, "src view-alias TEN");
  // relu(h): a computed source realizes into its own kernel first.
  u32 hd[2] = { EV, ED };
  Term h = ten(mk(DT_FP32, 2, hd, w));
  Term zero = fconst(2, hd, 0.0f);
  Term relu = uop_where(uop_binary(UOP_CMPLT, zero, h), h, zero);
  f32 w_relu[EV * ED];
  for (u32 q = 0; q < EV * ED; q++) {
    w_relu[q] = (0.0f < w[q]) ? w[q] : 0.0f;
  }
  rows_case(relu, w_relu, EV, ids, 2, "src relu(h)");
  // A PAD in the operand's views realizes the operand itself: one zero row
  // above and below the table.
  u32 pw[4] = { 1, 1, 0, 0 };
  f32 w_pad[(EV + 2) * ED];
  memset(w_pad, 0, sizeof w_pad);
  memcpy(w_pad + ED, w, (size_t)EV * ED * 4);
  static i32 const pad_ids[EN] = { 0, 7, 1, 6, 8, -1, 3, 4 };
  rows_case(uop_pad(h, 2, pw), w_pad, EV + 2, pad_ids, 2, "src PAD view");
  // A prior kernel's output.
  Term neg = uop_unary(UOP_NEG, h);
  f32 w_neg[EV * ED];
  u32 kn = KERNELS_NEXT;
  u32 nt = realize_into(neg, w_neg, EV * ED);
  CHECK(nt != 0 && kernel_for(nt, kn) != 0);
  rows_case(ten(nt), w_neg, EV, ids, 1, "src prior kernel output");
  // A CONST.
  f32 w_c[EV * ED];
  for (u32 q = 0; q < EV * ED; q++) {
    w_c[q] = 3.5f;
  }
  u32 k0 = KERNELS_NEXT;
  u32 nd[1] = { EN };
  Term gc = uop_gather(fconst(2, hd, 3.5f), rows(ten(mk(DT_INT32, 1, nd, ids)), EN, ED), 0);
  f32 want[EN * ED], got[EN * ED];
  ref_rows(w_c, ids, EN, EV, ED, want);
  realize_into(gc, got, EN * ED);
  check_bytes(got, want, EN * ED, "src CONST");
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
}

// === 4. idx forms ===
static void test_idx_forms(void) {
  f32 w[EV * ED];
  fill_src(w, EV * ED, 0.5f);
  u32 wd[2] = { EV, ED };
  u32 wt = mk(DT_FP32, 2, wd, w);
  u32 nd[1] = { EN };
  // idx + 1 on the index grid fuses into the gather kernel.
  static i32 const ids[EN] = { 2, -1, 0, 4, -2, 5, 1, 3 };
  i32 ids1[EN];
  for (u32 r = 0; r < EN; r++) {
    ids1[r] = ids[r] + 1;
  }
  u32 od[2] = { EN, ED };
  Term it = ten(mk(DT_INT32, 1, nd, ids));
  Term plus_grid = uop_binary(UOP_ADD, rows(it, EN, ED), iconst(2, od, 1));
  f32 want[EN * ED], got[EN * ED];
  ref_rows(w, ids1, EN, EV, ED, want);
  u32 k0 = KERNELS_NEXT;
  u32 tid = realize_into(uop_gather(ten(wt), plus_grid, 0), got, EN * ED);
  check_bytes(got, want, EN * ED, "idx + 1 on the grid");
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
  check_gather_kernel(tid, k0, EN * ED);
  // idx + 1 before the expand: an elementwise op whose ranges the expand
  // ended realizes on its own (tinygrad indexing.py:223-232), so two kernels.
  Term plus = uop_binary(UOP_ADD, it, iconst(1, nd, 1));
  k0 = KERNELS_NEXT;
  tid = realize_into(uop_gather(ten(wt), rows(plus, EN, ED), 0), got, EN * ED);
  check_bytes(got, want, EN * ED, "idx + 1 then expand");
  CHECK_EQ(KERNELS_NEXT - k0, 2u);
  check_dispatch(k0);
  check_gather_kernel(tid, k0, EN * ED);
  // A cast from float (truncates toward zero).
  static f32 const fids[EN] = { 2.0f, -1.0f, 0.5f, 4.9f, 3.0f, 6.0f, 1.0f, -0.5f };
  i32 tids[EN];
  for (u32 r = 0; r < EN; r++) {
    tids[r] = (i32)fids[r];
  }
  Term cast = uop_cast(ten(mk(DT_FP32, 1, nd, fids)), DT_INT32);
  ref_rows(w, tids, EN, EV, ED, want);
  k0 = KERNELS_NEXT;
  tid = realize_into(uop_gather(ten(wt), rows(cast, EN, ED), 0), got, EN * ED);
  check_bytes(got, want, EN * ED, "idx cast from float");
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
  check_gather_kernel(tid, k0, EN * ED);
  // tokens[:, 1:]: an offset view of a (B, T + 1) token grid.
  enum { B = 2, T = 4 };
  static i32 const tok[B * (T + 1)] = { 9, 1, 5, -1, 0,  9, 3, 2, 6, 4 };
  i32 shifted[B * T];
  for (u32 b = 0; b < B; b++) {
    for (u32 t = 0; t < T; t++) {
      shifted[b * T + t] = tok[b * (T + 1) + t + 1];
    }
  }
  u32 tokd[2] = { B, T + 1 };
  u32 tbe[4] = { 0, B, 1, T + 1 };
  u32 flat[1] = { B * T };
  Term tv = uop_reshape(uop_shrink(ten(mk(DT_INT32, 2, tokd, tok)), 2, tbe), 1, flat);
  ref_rows(w, shifted, B * T, EV, ED, want);
  k0 = KERNELS_NEXT;
  tid = realize_into(uop_gather(ten(wt), rows(tv, B * T, ED), 0), got, B * T * ED);
  check_bytes(got, want, B * T * ED, "idx tokens[:, 1:]");
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
  check_gather_kernel(tid, k0, B * T * ED);
  // edge_index[1] with V < N: E = 11 edges over a 6-row table.
  enum { E = 11 };
  static i32 const edges[2 * E] = { 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4,
                                    5, 4, 3, 2, 1, 0, 5, -1, 6, 2, 2 };
  u32 ed[2] = { 2, E };
  u32 ebe[4] = { 1, 2, 0, E };
  u32 e1[1] = { E };
  Term row1 = uop_reshape(uop_shrink(ten(mk(DT_INT32, 2, ed, edges)), 2, ebe), 1, e1);
  f32 want_e[E * ED], got_e[E * ED];
  ref_rows(w, edges + E, E, EV, ED, want_e);
  k0 = KERNELS_NEXT;
  tid = realize_into(uop_gather(ten(wt), rows(row1, E, ED), 0), got_e, E * ED);
  check_bytes(got_e, want_e, E * ED, "idx edge_index[1]");
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
  check_gather_kernel(tid, k0, E * ED);
}

// A gather of a fresh (EV, ED) table by the view `idx`, whose logical
// values are idx_eff: one kernel, the view read in place.
static void idx_view_case(Term idx, i32 const *idx_eff, Shape const *is,
                          u32 dim, char const *what) {
  Shape ss = {0};
  ss.ndim = 2; ss.dims[0] = EV; ss.dims[1] = ED;
  f32 w[EV * ED];
  fill_src(w, EV * ED, 0.5f);
  u32 in = numel_of(is);
  f32 want[EN * ED], got[EN * ED];
  CHECK(in <= EN * ED);
  ref_gather(w, &ss, idx_eff, is, dim, want);
  Term g = uop_gather(ten(mk(DT_FP32, 2, ss.dims, w)), idx, dim);
  CHECK(g != 0);
  u32 k0 = KERNELS_NEXT;
  u32 tid = realize_into(g, got, in);
  check_bytes(got, want, in, what);
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
  check_gather_kernel(tid, k0, in);
}

static void test_idx_views(void) {
  Shape is = {0};
  is.ndim = 2; is.dims[0] = EN; is.dims[1] = ED;
  u32 id[2] = { EN, ED };
  // Row ids behind three junk entries: W[ids] through an offset view.
  static i32 const ids[EN] = { 2, -1, 0, EV, 3, 5, 1, 10982377 };
  i32 grid[EN * ED];
  for (u32 p = 0; p < EN * ED; p++) {
    grid[p] = ids[p / ED];
  }
  i32 big[EN + 3] = { 4, -7, 1 };
  memcpy(big + 3, ids, sizeof ids);
  u32 bd[1] = { EN + 3 }, be[2] = { 3, EN + 3 };
  Term alias = materialize_uop_in_env(uop_shrink(ten(mk(DT_INT32, 1, bd, big)), 1, be), 0);
  CHECK_EQ(term_tag(alias), TAG_TEN);
  CHECK(TENS[(u32)term_val(alias)].view.offset != 0);
  idx_view_case(rows(alias, EN, ED), grid, &is, 0, "idx view-alias TEN");
  idx_view_case(rows(uop_shrink(ten(mk(DT_INT32, 1, bd, big)), 1, be), EN, ED), grid,
                &is, 0, "idx lazy SHRINK then expand");
  // An index that varies along both axes, with invalid entries.
  i32 gv[EN * ED];
  for (u32 r = 0; r < EN; r++) {
    for (u32 c = 0; c < ED; c++) {
      gv[r * ED + c] = (i32)((r * 3u + c * 5u) % (EV + 3)) - 1;
    }
  }
  // A PERMUTE view-alias TEN (offset 0, not contiguous): gv's transpose.
  i32 gt[ED * EN];
  for (u32 r = 0; r < EN; r++) {
    for (u32 c = 0; c < ED; c++) {
      gt[c * EN + r] = gv[r * ED + c];
    }
  }
  u32 td[2] = { ED, EN }, perm[2] = { 1, 0 };
  Term palias = materialize_uop_in_env(uop_permute(ten(mk(DT_INT32, 2, td, gt)), 2, perm), 0);
  CHECK_EQ(term_tag(palias), TAG_TEN);
  CHECK(!TENS[(u32)term_val(palias)].view.contiguous);
  idx_view_case(palias, gv, &is, 0, "idx PERMUTE view-alias TEN");
  // tokens[:, 1:] used directly as a 2-D index.
  i32 wide[EN * (ED + 1)];
  for (u32 r = 0; r < EN; r++) {
    wide[r * (ED + 1)] = 9;
    for (u32 c = 0; c < ED; c++) {
      wide[r * (ED + 1) + c + 1] = gv[r * ED + c];
    }
  }
  u32 wd[2] = { EN, ED + 1 }, wbe[4] = { 0, EN, 1, ED + 1 };
  idx_view_case(uop_shrink(ten(mk(DT_INT32, 2, wd, wide)), 2, wbe), gv, &is, 0,
                "idx 2-D SHRINK view");
  // A FLIP view (a negative stride and an offset).
  i32 fl[EN * ED];
  for (u32 r = 0; r < EN; r++) {
    for (u32 c = 0; c < ED; c++) {
      fl[r * ED + c] = gv[(EN - 1 - r) * ED + c];
    }
  }
  idx_view_case(uop_flip(ten(mk(DT_INT32, 2, id, fl)), 1u), gv, &is, 0, "idx FLIP view");
  // Along dim 1: out[i][j] = W[i][idx[i][j]], idx a column SHRINK of an
  // (EV, 5) grid.
  Shape is1 = {0};
  is1.ndim = 2; is1.dims[0] = EV; is1.dims[1] = 3;
  i32 cols[EV * 5], ceff[EV * 3];
  for (u32 r = 0; r < EV; r++) {
    for (u32 c = 0; c < 5; c++) {
      cols[r * 5 + c] = (i32)((r * 2u + c * 3u) % (ED + 2)) - 1;
    }
    for (u32 c = 0; c < 3; c++) {
      ceff[r * 3 + c] = cols[r * 5 + c + 2];
    }
  }
  u32 cd[2] = { EV, 5 }, cbe[4] = { 0, EV, 2, 5 };
  idx_view_case(uop_shrink(ten(mk(DT_INT32, 2, cd, cols)), 2, cbe), ceff, &is1, 1,
                "idx column SHRINK, dim 1");
}

// === 5. fusion ===
static void test_fusion(void) {
  f32 w[EV * ED];
  fill_src(w, EV * ED, 0.5f);
  u32 wd[2] = { EV, ED };
  u32 wt = mk(DT_FP32, 2, wd, w);
  static i32 const ids[EN] = { 2, -1, 0, 4, 3, 5, 1, 7 };
  u32 nd[1] = { EN };
  u32 od[2] = { EN, ED };
  Term idx = rows(ten(mk(DT_INT32, 1, nd, ids)), EN, ED);
  f32 base[EN * ED];
  ref_rows(w, ids, EN, EV, ED, base);
  // gather * 2 + 1: one kernel (an invalid row reads 0 * 2 + 1).
  Term g = uop_gather(ten(wt), idx, 0);
  Term y = uop_binary(UOP_ADD, uop_binary(UOP_MUL, g, fconst(2, od, 2.0f)),
                      fconst(2, od, 1.0f));
  f32 want[EN * ED], got[EN * ED];
  for (u32 q = 0; q < EN * ED; q++) {
    want[q] = base[q] * 2.0f + 1.0f;
  }
  u32 k0 = KERNELS_NEXT;
  u32 tid = realize_into(y, got, EN * ED);
  check_bytes(got, want, EN * ED, "gather * 2 + 1");
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
  u32 kid = kernel_for(tid, k0);
  CHECK(kid != 0);
  CHECK_EQ(KERNELS[kid].no_opts, 1);
  // A gather inside a SUM over the row.
  Term s = uop_reduce(REDUCE_SUM, 1, uop_gather(ten(wt), idx, 0));
  f32 want_s[EN], got_s[EN];
  for (u32 r = 0; r < EN; r++) {
    f32 acc = 0.0f;
    for (u32 c = 0; c < ED; c++) {
      acc += base[r * ED + c];
    }
    want_s[r] = acc;
  }
  k0 = KERNELS_NEXT;
  realize_into(s, got_s, EN);
  check_bytes(got_s, want_s, EN, "sum of gather");
  CHECK_EQ(KERNELS_NEXT - k0, 1u);
  check_dispatch(k0);
  // A gather feeding a matmul: the gather realizes, the matmul runs on BLAS.
  enum { K = 3 };
  f32 m[ED * K];
  for (u32 q = 0; q < ED * K; q++) {
    m[q] = (f32)((i32)(q % 5u) - 2);
  }
  u32 mdims[2] = { ED, K };
  Term mt = ten(mk(DT_FP32, 2, mdims, m));
  u32 nd1[3] = { EN, ED, 1 }, ndk[3] = { EN, ED, K }, _dk[3] = { 1, ED, K };
  Term ae = uop_expand(uop_reshape(uop_gather(ten(wt), idx, 0), 3, nd1), 3, ndk);
  Term be = uop_expand(uop_reshape(mt, 3, _dk), 3, ndk);
  Term mm = uop_reduce(REDUCE_SUM, 1, uop_binary(UOP_MUL, ae, be));
  f32 want_m[EN * K], got_m[EN * K];
  for (u32 r = 0; r < EN; r++) {
    for (u32 j = 0; j < K; j++) {
      f32 acc = 0.0f;
      for (u32 c = 0; c < ED; c++) {
        acc += base[r * ED + c] * m[c * K + j];
      }
      want_m[r * K + j] = acc;
    }
  }
  u64 g0 = cpu_blas_gemm_dispatch_dag_count();
  k0 = KERNELS_NEXT;
  realize_into(mm, got_m, EN * K);
  check_bytes(got_m, want_m, EN * K, "gather @ M");
  CHECK_EQ(KERNELS_NEXT - k0, 2u);
  CHECK(cpu_blas_gemm_dispatch_dag_count() > g0);
  check_dispatch(k0);
}

// === 7. JIT capture, replays with the index rewritten in place ===
static void test_jit_replay(void) {
  f32 w[EV * ED];
  fill_src(w, EV * ED, 0.5f);
  u32 wd[2] = { EV, ED };
  u32 wt = mk(DT_FP32, 2, wd, w);
  static i32 const ids[4][EN] = {
    { 2, -1, 0, 4, 3, 5, 1, 6 },
    { 5, 5, 5, 5, 0, 0, 0, 0 },
    { -19238, 3, 10982377, 2, 1, EV, 4, 0 },
    { 1, 2, 3, 4, 5, 0, -1, 2 },
  };
  u32 nd[1] = { EN };
  u32 it = mk(DT_INT32, 1, nd, ids[0]);
  Term g = uop_gather(ten(wt), rows(ten(it), EN, ED), 0);
  u32 slot = jit_capture_begin();
  CHECK(slot != 0);
  Term r = term_resolve(thvm_realize(g));
  jit_capture_end_with_result(r);
  CHECK_EQ(term_tag(r), TAG_TEN);
  if (term_tag(r) != TAG_TEN) {
    return;
  }
  u32 tid = (u32)term_val(r);
  f32 want[EN * ED], got[EN * ED];
  TENS[tid].backend->buf_read(TENS[tid].buf_id, got, EN * ED * 4);
  ref_rows(w, ids[0], EN, EV, ED, want);
  check_bytes(got, want, EN * ED, "capture");
  for (u32 rep = 1; rep < 4; rep++) {
    CPU_BACKEND.buf_write(TENS[it].buf_id, (void *)ids[rep], EN * 4);
    f32 poison[EN * ED];
    for (u32 q = 0; q < EN * ED; q++) {
      poison[q] = -999.0f;
    }
    TENS[tid].backend->buf_write(TENS[tid].buf_id, poison, EN * ED * 4);
    jit_replay(slot);
    TENS[tid].backend->buf_read(TENS[tid].buf_id, got, EN * ED * 4);
    ref_rows(w, ids[rep], EN, EV, ED, want);
    char what[32];
    snprintf(what, sizeof what, "replay %u", rep);
    check_bytes(got, want, EN * ED, what);
  }
  jit_capture_drop(slot);
}

// === 8. timing ===
static double now_s(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static void test_timing(void) {
  enum { V = 10000, N = 1024, D = 64 };
  f32 *w = (f32 *)malloc((size_t)V * D * 4);
  i32 *ids = (i32 *)malloc((size_t)N * 4);
  f32 *want = (f32 *)malloc((size_t)N * D * 4);
  f32 *got = (f32 *)malloc((size_t)N * D * 4);
  for (u32 q = 0; q < (u32)V * D; q++) {
    w[q] = (f32)(q % 1000u) * 0.001f;
  }
  for (u32 r = 0; r < N; r++) {
    ids[r] = (i32)((r * 7919u) % V);
  }
  u32 wd[2] = { V, D };
  u32 nd[1] = { N };
  Term wt = ten(mk(DT_FP32, 2, wd, w));
  Term it = ten(mk(DT_INT32, 1, nd, ids));
  ref_rows(w, ids, N, V, D, want);
  realize_into(uop_gather(wt, rows(it, N, D), 0), got, N * D);
  check_bytes(got, want, N * D, "timing warm-up");
  double t0 = now_s();
  realize_into(uop_gather(wt, rows(it, N, D), 0), got, N * D);
  double dt = now_s() - t0;
  check_bytes(got, want, N * D, "timing");
  printf("  gather V=%u N=%u D=%u: %.4f s (%s; one-hot baseline 0.89 s)\n",
         V, N, D, dt, cpu_jit_walker_only() ? "walker" : "C JIT");
  free(w);
  free(ids);
  free(want);
  free(got);
}

int main(void) {
  thvm_init();
  TEST_BEGIN("gather/dims");
  test_dims();
  TEST_BEGIN("gather/embedding-rows");
  test_embedding_rows();
  TEST_BEGIN("gather/src-forms");
  test_src_forms();
  TEST_BEGIN("gather/idx-forms");
  test_idx_forms();
  TEST_BEGIN("gather/idx-views");
  test_idx_views();
  TEST_BEGIN("gather/fusion");
  test_fusion();
  TEST_BEGIN("gather/jit-replay");
  test_jit_replay();
  TEST_BEGIN("gather/timing");
  test_timing();
  TEST_BEGIN("gather/dispatched");
  CHECK(DISPATCHED >= 20);
  thvm_free();
  TEST_REPORT();
}
