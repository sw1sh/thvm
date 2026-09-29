// test_autotune_iso_scratch.c - the BEAM isolated bench's scratch buffers
// hold every element the kernel can read.
//
// kautotune_iso_begin benches a kernel on fresh scratch buffers instead of
// the live graph buffers.  A slot the dispatch binds to its raw buffer is
// not read only through its own view.  A chain-composed slot's loads
// compose the movement chain into addresses over the buffer, so a shrunk
// view is read up to the extent its base spans.  A kernel that reads two
// views of one buffer (x[:, :64] + x[:, 64:], x[0] + x[1], a GLU split)
// loads both through the slot of the view it visited first, whether that
// slot is chain-composed or a contiguous view at offset 0 such as x[0].
// tinygrad allocates each beam scratch as its PARAM's whole buffer
// (schedule/rangeify.py:284, codegen/opt/postrange.py:258).
//
// The scratch fill is pinned element by element.  For each kernel the test
// bounds every address the kernel loads from each input slot.  It checks
// that every slot's scratch was requested large enough for its loads, and
// that every slot over the leaf's buffer was requested at the whole
// buffer.  It then checks that the isolated bench computes the kernel from
// the scratch contents, that every candidate the search applies passes the
// correctness gate there, and that a BEAM=2 search leaves the live result
// correct.  The freelist is emptied before every scratch allocation, so
// each scratch is a fresh block of exactly the requested size: a best-fit
// recycle could hand back a larger donor and hide an undersized request,
// and under ASan a read past a fresh block faults.

#include "../src/thvm.c"
#include "test.h"

static Term leaf_f32(u32 ndim, u32 const *dims) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) {
    s.dims[i] = dims[i];
  }
  u32 tid = tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
  u32 n   = TENS[tid].view.numel;
  f32 *v  = (f32 *)malloc((size_t)n * sizeof(f32));
  for (u32 i = 0; i < n; i++) {
    v[i] = (f32)(i % 23) - 11.0f;
  }
  CURRENT_BACKEND->buf_write(TENS[tid].buf_id, v, (u64)n * sizeof(f32));
  free(v);
  return term_new(0, TAG_TEN, DT_FP32, tid);
}

static Term full_f32(u32 bits, u32 ndim, u32 const *dims) {
  u32 ones[MAX_DIM];
  for (u32 i = 0; i < ndim; i++) {
    ones[i] = 1;
  }
  return uop_expand(uop_reshape(uop_const(DT_FP32, bits), ndim, ones),
                    ndim, dims);
}

// The realize detaches producer_kid from a committed output, so find the
// kernel by the tensor it wrote.
static u32 kernel_writing(Term realized) {
  Term r = term_resolve(realized);
  if (term_tag(r) != TAG_TEN) {
    return 0;
  }
  u32 tid = (u32)term_val(r);
  for (u32 kid = KERNELS_NEXT; kid-- > 1;) {
    if (KERNELS[kid].output_tid == tid) {
      return kid;
    }
  }
  return 0;
}

static int slot_over_buf(KernelEntry const *ke, u32 slot, u32 buf) {
  u32 tid = ke->input_tids != NULL ? ke->input_tids[slot] : 0;
  return tid != 0 && tid < TENS_NEXT && TENS[tid].buf_id == buf;
}

// Park nothing, so the next allocation is a fresh block whose
// CPU_BUFS[].nbytes is exactly the requested size.
static void fresh_allocs(void) {
  CPU_FREELIST_LEN = 0;
}

// === Scratch fill ===

// Element i of the scratch fill, written at `out`: a sign-alternating
// (i % 17) / 17, or (i % 17) with that sign for int32; zero for a dtype
// the fill does not model.
static void fill_elem(u32 dt, u64 i, u8 *out) {
  f64 mag = (f64)(i % 17) * (1.0 / 17.0);
  f64 v   = (i & 1) ? -mag : mag;
  switch (dt) {
    case DT_FP32:    *(f32 *)out = (f32)v; break;
    case DT_FP64:    *(f64 *)out = v; break;
    case DT_BF16:    *(u16 *)out = f32_to_bf16((f32)v); break;
    case DT_FP16:    *(u16 *)out = f32_to_fp16((f32)v); break;
    case DT_FP8E4M3: *out = f32_to_fp8e4m3((f32)v); break;
    case DT_FP8E5M2: *out = f32_to_fp8e5m2((f32)v); break;
    case DT_INT32:   *(i32 *)out = (i32)((i % 17) * ((i & 1) ? -1 : 1)); break;
    default:         memset(out, 0, dtype_itemsize(dt)); break;
  }
}

// kautotune_iso_fill writes element i as fill_elem does, for sizes on
// both sides of the pattern's 34-element period and past the ~1 MB block
// a backend with buf_write_at receives in pieces.
static void check_fill(void) {
  static u32 const dts[]   = {DT_FP32, DT_FP64, DT_BF16, DT_FP16,
                              DT_FP8E4M3, DT_FP8E5M2, DT_INT32, DT_INT8};
  static u64 const sizes[] = {1, 17, 33, 34, 35, 68, 69, 1000, 4099,
                              2500007};
  TEST_BEGIN("autotune-iso/fill-pattern");
  for (u32 d = 0; d < sizeof(dts) / sizeof(dts[0]); d++) {
    for (u32 s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
      u32 dt  = dts[d];
      u64 n   = sizes[s];
      u64 esz = dtype_itemsize(dt);
      u32 buf = CURRENT_BACKEND->buf_alloc(n * esz);
      u8 *got = (u8 *)malloc((size_t)(n * esz));
      u8 *want = (u8 *)malloc((size_t)(n * esz));
      kautotune_iso_fill(CURRENT_BACKEND, buf, dt, n);
      CURRENT_BACKEND->buf_read(buf, got, n * esz);
      for (u64 i = 0; i < n; i++) {
        fill_elem(dt, i, want + i * esz);
      }
      int same = memcmp(got, want, (size_t)(n * esz)) == 0;
      if (!same) {
        fprintf(stderr, "  fill dt=%u n=%llu differs\n", dt,
                (unsigned long long)n);
      }
      CHECK(same);
      free(got);
      free(want);
      CURRENT_BACKEND->buf_free(buf);
    }
  }
}

// === Load-address bounds ===

enum { SPAN_OK, SPAN_EMPTY, SPAN_UNKNOWN };

typedef struct {
  i64 lo;
  i64 hi;
  int st;
} Span;

static Span span_of(i64 lo, i64 hi) {
  Span s = {lo, hi, SPAN_OK};
  return s;
}

static Span span_st(int st) {
  Span s = {0, 0, st};
  return s;
}

static Span span_join(Span a, Span b) {
  if (a.st == SPAN_UNKNOWN || b.st == SPAN_UNKNOWN) {
    return span_st(SPAN_UNKNOWN);
  }
  if (a.st == SPAN_EMPTY) {
    return b;
  }
  if (b.st == SPAN_EMPTY) {
    return a;
  }
  return span_of(a.lo < b.lo ? a.lo : b.lo, a.hi > b.hi ? a.hi : b.hi);
}

// Every value the integer address expression `t` takes over its ranges.
// A UOP_INVALID lane (masked off by a PAD) loads nothing.
static Span addr_span(Term t, u32 depth) {
  Term r = term_resolve(t);
  if (depth > 256 || term_tag(r) != TAG_UOP) {
    return span_st(SPAN_UNKNOWN);
  }
  u32 op  = term_ext(r);
  u64 loc = term_val(r);
  switch (op) {
    case UOP_CONST: {
      i64 v = (i64)(i32)(u32)term_val(heap_read(loc));
      return span_of(v, v);
    }
    case UOP_RANGE: {
      i64 ext = (i64)uop_range_static_extent(r);
      return span_of(0, ext > 0 ? ext - 1 : 0);
    }
    case UOP_INVALID:
      return span_st(SPAN_EMPTY);
    case UOP_OPT:
      return addr_span(heap_read(loc), depth + 1);
    case UOP_IWHERE:
      return span_join(addr_span(heap_read(loc + 1), depth + 1),
                       addr_span(heap_read(loc + 2), depth + 1));
    case UOP_ILT:
      return span_of(0, 1);
    default:
      break;
  }
  Span a = addr_span(heap_read(loc + 0), depth + 1);
  Span b = addr_span(heap_read(loc + 1), depth + 1);
  if (a.st != SPAN_OK || b.st != SPAN_OK) {
    return span_st(SPAN_UNKNOWN);
  }
  int b_const = (b.lo == b.hi);
  switch (op) {
    case UOP_IADD:
      return span_of(a.lo + b.lo, a.hi + b.hi);
    case UOP_ISUB:
      return span_of(a.lo - b.hi, a.hi - b.lo);
    case UOP_IMUL: {
      i64 p[4] = {a.lo * b.lo, a.lo * b.hi, a.hi * b.lo, a.hi * b.hi};
      i64 lo   = p[0];
      i64 hi   = p[0];
      for (u32 i = 1; i < 4; i++) {
        lo = p[i] < lo ? p[i] : lo;
        hi = p[i] > hi ? p[i] : hi;
      }
      return span_of(lo, hi);
    }
    case UOP_IDIV:
      if (b_const && b.lo > 0) {
        return span_of(a.lo / b.lo, a.hi / b.lo);
      }
      break;
    case UOP_IMOD:
      if (b_const && b.lo > 0 && a.lo >= 0) {
        return span_of(0, a.hi < b.lo - 1 ? a.hi : b.lo - 1);
      }
      break;
    case UOP_ISHR:
      if (b_const && b.lo >= 0 && b.lo < 63 && a.lo >= 0) {
        return span_of(a.lo >> b.lo, a.hi >> b.lo);
      }
      break;
    case UOP_IAND:
      if (a.lo >= 0 && b.lo >= 0) {
        return span_of(0, a.hi < b.hi ? a.hi : b.hi);
      }
      break;
    default:
      break;
  }
  return span_st(SPAN_UNKNOWN);
}

// Every address the INDEX_E loads under `t` read from UOP_BUFFER `inst`.
static Span buffer_load_span(Term t, u32 inst, u32 depth) {
  if (depth > 256) {
    return span_st(SPAN_UNKNOWN);
  }
  Term r = term_resolve(t);
  if (term_tag(r) != TAG_UOP) {
    return span_st(SPAN_EMPTY);
  }
  u32  op  = term_ext(r);
  u64  loc = term_val(r);
  Span s   = span_st(SPAN_EMPTY);
  if (op == UOP_BUFFER || op == UOP_KERNEL) {
    return s;
  }
  if (op == UOP_INDEX_E) {
    Term buf = term_resolve(heap_read(loc + 0));
    if (term_tag(buf) == TAG_UOP && term_ext(buf) == UOP_BUFFER
        && uop_buffer_inst_get(buf) == inst) {
      s = addr_span(heap_read(loc + 1), 0);
    }
  }
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    s = span_join(s, buffer_load_span(heap_read(loc + i), inst, depth + 1));
  }
  return s;
}

// === Kernels ===

typedef struct Case Case;
typedef Term (*BuildFn)(Case const *k, Term x);
typedef void (*RefFn)(Case const *k, f32 const *x, f32 *want);

// One kernel over one leaf buffer x of shape `dims`, computing `out`
// elements and reading x up to element `last`.  The slot x's loads land on
// is chain-composed iff `composed`, over a TenDesc with `nviews` prior
// views.  A pair kernel computes out[r * width + j] = x[r * stride + lhs +
// j] op x[r * stride + rhs + j] for r < rows and j < width; op 'd' sums
// those products over j into out[r].  `sums`: an opt may reorder a sum, so
// results match within a tolerance.
struct Case {
  char const *name;
  BuildFn     build;
  RefFn       ref;
  u32         ndim;
  u32         dims[3];
  u32         out;
  u32         last;
  int         composed;
  u32         nviews;
  int         sums;
  u32         rows;
  u32         stride;
  u32         lhs;
  u32         rhs;
  u32         width;
  char        op;
};

static u32 leaf_numel(Case const *k) {
  u32 n = 1;
  for (u32 a = 0; a < k->ndim; a++) {
    n *= k->dims[a];
  }
  return n;
}

// x.shrink(((0,2),(64,128))) * 2 over x of shape (2,128).
static Term shrink_mul(Case const *k, Term x) {
  (void)k;
  u32 be[4] = {0, 2, 64, 128};
  u32 yd[2] = {2, 64};
  return uop_binary(UOP_MUL, uop_shrink(x, 2, be),
                    full_f32(0x40000000u, 2, yd));
}

static void shrink_mul_ref(Case const *k, f32 const *x, f32 *want) {
  (void)k;
  for (u32 r = 0; r < 2; r++) {
    for (u32 c = 0; c < 64; c++) {
      want[r * 64 + c] = 2.0f * x[r * 128 + 64 + c];
    }
  }
}

// x.shrink(((0,2),(0,4),(4,8))).reshape(2,16).sum(1) over x of shape (2,4,8).
static Term merge_sum(Case const *k, Term x) {
  (void)k;
  u32 be[6] = {0, 2, 0, 4, 4, 8};
  u32 md[2] = {2, 16};
  return uop_reduce(REDUCE_SUM, 1, uop_reshape(uop_shrink(x, 3, be), 2, md));
}

static void merge_sum_ref(Case const *k, f32 const *x, f32 *want) {
  (void)k;
  for (u32 r = 0; r < 2; r++) {
    f64 s = 0.0;
    for (u32 j = 0; j < 4; j++) {
      for (u32 m = 0; m < 4; m++) {
        s += (f64)x[r * 32 + j * 8 + 4 + m];
      }
    }
    want[r] = (f32)s;
  }
}

static void pair_ref(Case const *k, f32 const *x, f32 *want) {
  for (u32 r = 0; r < k->rows; r++) {
    f64 dot = 0.0;
    for (u32 j = 0; j < k->width; j++) {
      f32 a = x[r * k->stride + k->lhs + j];
      f32 b = x[r * k->stride + k->rhs + j];
      switch (k->op) {
        case '+': want[r * k->width + j] = a + b; break;
        case '-': want[r * k->width + j] = a - b; break;
        case '*': want[r * k->width + j] = a * b; break;
        default:  dot += (f64)a * (f64)b;         break;
      }
    }
    if (k->op == 'd') {
      want[r] = (f32)dot;
    }
  }
}

static Term pair_op(Case const *k, Term a, Term b) {
  switch (k->op) {
    case '+':
      return uop_binary(UOP_ADD, a, b);
    case '-':
      return uop_binary(UOP_ADD, a, uop_unary(UOP_NEG, b));
    case '*':
      return uop_binary(UOP_MUL, a, b);
    default:
      return uop_reduce(REDUCE_SUM, 0, uop_binary(UOP_MUL, a, b));
  }
}

// x[:, lhs:lhs+width] op x[:, rhs:rhs+width] over x of shape (rows, stride).
static Term cols_pair(Case const *k, Term x) {
  u32 lo[4] = {0, k->rows, k->lhs, k->lhs + k->width};
  u32 hi[4] = {0, k->rows, k->rhs, k->rhs + k->width};
  return pair_op(k, uop_shrink(x, 2, lo), uop_shrink(x, 2, hi));
}

// x[i] op x[j] for i = lhs / width and j = rhs / width: one index along
// axis 0, reshaped to the remaining axes, as Python's x[0] builds it.
static Term rows_pair(Case const *k, Term x) {
  u32 lo[2 * MAX_DIM];
  u32 hi[2 * MAX_DIM];
  u32 rest[MAX_DIM];
  for (u32 a = 0; a < k->ndim; a++) {
    lo[2 * a]     = 0;
    lo[2 * a + 1] = k->dims[a];
    hi[2 * a]     = 0;
    hi[2 * a + 1] = k->dims[a];
    if (a > 0) {
      rest[a - 1] = k->dims[a];
    }
  }
  lo[0] = k->lhs / k->width;
  lo[1] = lo[0] + 1;
  hi[0] = k->rhs / k->width;
  hi[1] = hi[0] + 1;
  Term a = uop_reshape(uop_shrink(x, k->ndim, lo), k->ndim - 1, rest);
  Term b = uop_reshape(uop_shrink(x, k->ndim, hi), k->ndim - 1, rest);
  return pair_op(k, a, b);
}

// x[lhs:lhs+width].reshape(4, width/4) op x[rhs:rhs+width].reshape(4,
// width/4) over a 1-D x.
static Term flat_pair(Case const *k, Term x) {
  u32 lo[2] = {k->lhs, k->lhs + k->width};
  u32 hi[2] = {k->rhs, k->rhs + k->width};
  u32 md[2] = {4, k->width / 4};
  Term a = uop_reshape(uop_shrink(x, 1, lo), 2, md);
  Term b = uop_reshape(uop_shrink(x, 1, hi), 2, md);
  return pair_op(k, a, b);
}

#define COLS(nm, r, c, l, h, w, o)                                         \
  {.name = (nm), .build = cols_pair, .ref = pair_ref, .ndim = 2,           \
   .dims = {(r), (c)}, .out = (r) * (w), .last = (r) * (c) - 1,            \
   .composed = 1, .rows = (r), .stride = (c), .lhs = (l), .rhs = (h),      \
   .width = (w), .op = (o)}

static Case const CASES[] = {
  // The view is (2,64) at offset 64 with row stride 128, so the kernel
  // reads x up to element 1*128 + 64 + 63 = 255.
  {.name = "shrink-mul", .build = shrink_mul, .ref = shrink_mul_ref,
   .ndim = 2, .dims = {2, 128}, .out = 128, .last = 255, .composed = 1},
  // The reshape merges the shrunk axis with its outer neighbour, a
  // two-view chain whose address reaches 32 + 3*8 + 4 + 3 = 63.
  {.name = "merge-sum", .build = merge_sum, .ref = merge_sum_ref,
   .ndim = 3, .dims = {2, 4, 8}, .out = 2, .last = 63, .composed = 1,
   .nviews = 1, .sums = 1},
  // Two column views of one buffer: both loads land on the first operand's
  // slot.  With the lower view first, that slot's own view stops short of
  // the buffer's end (x[:, :64] spans elements 0..191 of 256, x[:, :-1]
  // 0..130 of 132, x[:, :32] 0..223 of 256).
  COLS("halves-add",     2, 128,  0, 64, 64, '+'),
  COLS("halves-add-rev", 2, 128, 64,  0, 64, '+'),
  COLS("diff",           4,  33,  0,  1, 32, '-'),
  COLS("diff-rev",       4,  33,  1,  0, 32, '-'),
  COLS("glu-halves",     4,  64,  0, 32, 32, '*'),
  COLS("glu-halves-rev", 4,  64, 32,  0, 32, '*'),
  // Two row views: x[0] is contiguous at offset 0, so its slot binds the
  // raw buffer without being chain-composed, and x[1]'s loads land on it.
  {.name = "rows-add", .build = rows_pair, .ref = pair_ref, .ndim = 2,
   .dims = {2, 128}, .out = 128, .last = 255, .rows = 1, .lhs = 0,
   .rhs = 128, .width = 128, .op = '+'},
  {.name = "rows-add-rev", .build = rows_pair, .ref = pair_ref, .ndim = 2,
   .dims = {2, 128}, .out = 128, .last = 255, .composed = 1, .rows = 1,
   .lhs = 128, .rhs = 0, .width = 128, .op = '+'},
  // x[0] * x[1] over x (4,2,32): reads the first two of four rows.
  {.name = "rows-mul", .build = rows_pair, .ref = pair_ref, .ndim = 3,
   .dims = {4, 2, 32}, .out = 64, .last = 127, .rows = 1, .lhs = 0,
   .rhs = 64, .width = 64, .op = '*'},
  // (x[0] * x[1]).sum() over x (2,128).
  {.name = "rows-dot", .build = rows_pair, .ref = pair_ref, .ndim = 2,
   .dims = {2, 128}, .out = 1, .last = 255, .sums = 1, .rows = 1,
   .lhs = 0, .rhs = 128, .width = 128, .op = 'd'},
  // x[:32].reshape(4,8) - x[1:].reshape(4,8) over x (33): the reshape
  // makes x[:32] contiguous at offset 0, one element short of the buffer.
  {.name = "diff-flat", .build = flat_pair, .ref = pair_ref, .ndim = 1,
   .dims = {33}, .out = 32, .last = 32, .rows = 1, .lhs = 0, .rhs = 1,
   .width = 32, .op = '-'},
  {.name = "diff-flat-rev", .build = flat_pair, .ref = pair_ref, .ndim = 1,
   .dims = {33}, .out = 32, .last = 32, .composed = 1, .rows = 1, .lhs = 1,
   .rhs = 0, .width = 32, .op = '-'},
};

// Elements of the output `out_buf` that differ from the kernel's value over
// the input contents `x`.
static u32 mismatches(Case const *k, f32 const *x, Backend *b, u32 out_buf) {
  f32 *want = (f32 *)calloc(k->out, sizeof(f32));
  f32 *got  = (f32 *)calloc(k->out, sizeof(f32));
  k->ref(k, x, want);
  b->buf_read(out_buf, got, (u64)k->out * sizeof(f32));
  u32 bad = 0;
  for (u32 i = 0; i < k->out; i++) {
    f64 w   = (f64)want[i];
    f64 tol = k->sums ? 1e-4 * (1.0 + fabs(w)) : 0.0;
    if (!(fabs((f64)got[i] - w) <= tol)) {
      bad++;
    }
  }
  free(want);
  free(got);
  return bad;
}

// === Checks ===

// Bench `kid` on the isolated scratch: every slot's scratch covers the
// addresses the kernel loads from it and every slot over x's buffer holds
// the whole buffer, the bench computes the kernel from the scratch, and
// every candidate the search applies passes the correctness gate.
static void check_iso_scratch(Case const *k, u32 kid, Term x) {
  char         label[128];
  KernelEntry *ke    = &KERNELS[kid];
  u32          n     = ke->n_inputs;
  u32          xbuf  = TENS[(u32)term_val(x)].buf_id;
  u64          whole = (u64)leaf_numel(k) * sizeof(f32);
  Span         span[n > 0 ? n : 1];
  u32          leaf  = n;
  i64          last  = -1;
  int          known = 1;
  int          lo_ok = 1;
  for (u32 i = 0; i < n; i++) {
    span[i] = buffer_load_span(ke->cached_lift.store_root, i + 1, 0);
    known   = known && span[i].st != SPAN_UNKNOWN;
    lo_ok   = lo_ok && (span[i].st != SPAN_OK || span[i].lo >= 0);
    if (span[i].st == SPAN_OK && slot_over_buf(ke, i, xbuf)) {
      leaf = leaf < n ? leaf : i;
      last = span[i].hi > last ? span[i].hi : last;
    }
  }

  snprintf(label, sizeof(label), "autotune-iso/%s/leaf-slot", k->name);
  TEST_BEGIN(label);
  CHECK(leaf < n);
  if (leaf >= n) {
    return;
  }
  CHECK_EQ(ke->input_chain_composed[leaf] != 0, k->composed);
  CHECK_EQ(TENS[ke->input_tids[leaf]].nviews, k->nviews);

  snprintf(label, sizeof(label), "autotune-iso/%s/loads-reach-last", k->name);
  TEST_BEGIN(label);
  CHECK(known);
  CHECK(lo_ok);
  CHECK_EQ(last, k->last);

  snprintf(label, sizeof(label), "autotune-iso/%s/bench-isolates", k->name);
  TEST_BEGIN(label);
  fresh_allocs();
  int iso = kautotune_iso_begin(kid);
  CHECK_EQ(iso, 1);
  if (!iso) {
    return;
  }

  snprintf(label, sizeof(label), "autotune-iso/%s/scratch-covers-loads",
           k->name);
  TEST_BEGIN(label);
  int covered = known;
  for (u32 i = 0; i < n; i++) {
    if (span[i].st != SPAN_OK) {
      continue;
    }
    u64 need = dtype_storage_bytes(ke->input_dtypes[i], (u64)span[i].hi + 1);
    u64 have = CPU_BUFS[ISO.in_buf_ids[i]].nbytes;
    if (have < need) {
      fprintf(stderr, "  %s: slot %u scratch %llu bytes, loads need %llu\n",
              k->name, i, (unsigned long long)have, (unsigned long long)need);
      covered = 0;
    }
  }
  CHECK(covered);

  snprintf(label, sizeof(label), "autotune-iso/%s/scratch-is-whole-buffer",
           k->name);
  TEST_BEGIN(label);
  for (u32 i = 0; i < n; i++) {
    if (slot_over_buf(ke, i, xbuf)) {
      CHECK_EQ(CPU_BUFS[ISO.in_buf_ids[i]].nbytes, whole);
    }
  }

  // Fire only on scratch that covers the loads: an undersized one would
  // make the test itself read past it.
  snprintf(label, sizeof(label), "autotune-iso/%s/bench-reads-scratch",
           k->name);
  TEST_BEGIN(label);
  covered = covered && CPU_BUFS[ISO.in_buf_ids[leaf]].nbytes >= whole;
  CHECK(covered);
  if (covered) {
    kernel_bench_fire(kid);
    KautotuneOutStat st = kautotune_output_stat(ke);
    CHECK(st.valid && st.all_finite);
    f32 *in = (f32 *)malloc((size_t)whole);
    ISO.backend->buf_read(ISO.in_buf_ids[leaf], in, whole);
    CHECK_EQ(mismatches(k, in, ISO.backend, ISO.out_buf_id), 0);
    free(in);
  }

  snprintf(label, sizeof(label), "autotune-iso/%s/gate-accepts-candidates",
           k->name);
  TEST_BEGIN(label);
  CHECK(covered);
  if (covered) {
    KOpt cands[KAUTOTUNE_MAX_CANDIDATES];
    u32  n_cand = kernel_opts_propose(ke, cands, KAUTOTUNE_MAX_CANDIDATES);
    axes_reset_to_default(ke);
    kernel_bench_fire(kid);
    KautotuneOutStat base     = kautotune_output_stat(ke);
    u32              applied  = 0;
    u32              accepted = 0;
    for (u32 i = 0; i < n_cand; i++) {
      KOptSeq seq = {0};
      seq.n       = 1;
      seq.opts[0] = cands[i];
      axes_reset_to_default(ke);
      if (!kautotune_apply_seq(ke, &seq)) {
        continue;
      }
      applied++;
      u64 us = 0;
      accepted += (u32)kautotune_bench_seq(kid, ke, &seq, 1, &base, 0, &us);
    }
    axes_reset_to_default(ke);
    CHECK(base.valid && base.all_finite);
    CHECK(applied > 0);
    CHECK_EQ(accepted, applied);
  }
  kautotune_iso_end();
}

// A BEAM=2 search over `kid`, then a live fire: the result must still be the
// kernel's value over the live leaf buffer `x`.
static void check_beam_live(Case const *k, u32 kid, Term x) {
  char         label[128];
  KernelEntry *ke = &KERNELS[kid];
  snprintf(label, sizeof(label), "autotune-iso/%s/beam2-live-result",
           k->name);
  TEST_BEGIN(label);
  setenv("BEAM", "2", 1);
  setenv("AUTOTUNE_CACHE", "0", 1);
  ke->schedule->autotuned = 0;
  fresh_allocs();
  kernel_autotune(kid);
  unsetenv("BEAM");
  kernel_bench_fire(kid);
  u32  xt    = (u32)term_val(x);
  u64  whole = (u64)leaf_numel(k) * sizeof(f32);
  f32 *in    = (f32 *)malloc((size_t)whole);
  TENS[xt].backend->buf_read(TENS[xt].buf_id, in, whole);
  TenDesc const *out = &TENS[ke->output_tid];
  CHECK_EQ(mismatches(k, in, out->backend, out->buf_id), 0);
  free(in);
}

static void run_case(Case const *k) {
  char label[128];
  Term x   = leaf_f32(k->ndim, k->dims);
  u32  kid = kernel_writing(thvm_realize(k->build(k, x)));
  snprintf(label, sizeof(label), "autotune-iso/%s/kernel", k->name);
  TEST_BEGIN(label);
  CHECK(kid != 0 && kid < KERNELS_NEXT);
  if (kid == 0 || kid >= KERNELS_NEXT) {
    return;
  }
  CHECK_EQ(KERNELS[kid].output_numel, k->out);
  check_iso_scratch(k, kid, x);
  check_beam_live(k, kid, x);
}

int main(void) {
  unsetenv("BEAM");
  unsetenv("JITBEAM");
  unsetenv("AUTOTUNE");
  thvm_init();
  check_fill();
  for (u32 i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++) {
    run_case(&CASES[i]);
  }
  thvm_free();
  TEST_REPORT();
}
