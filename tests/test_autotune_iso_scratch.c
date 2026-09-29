// test_autotune_iso_scratch.c - the BEAM isolated bench's scratch buffers
// hold every element the kernel can read.
//
// kautotune_iso_begin benches a kernel on fresh scratch buffers instead of
// the live graph buffers.  An input slot flagged input_chain_composed binds
// the raw buffer under its view: the kernel's loads compose the movement
// chain into addresses over that buffer.  A shrunk view is read up to the
// extent its base spans, past the view's own element count.  A kernel that
// reads two views of one buffer (x[:, :64] + x[:, 64:], a GLU split) loads
// both through the slot of the view it visited first, so that slot is read
// across both views.  tinygrad allocates each beam scratch as its PARAM's
// whole buffer (schedule/rangeify.py:284, codegen/opt/postrange.py:258).
//
// For each kernel the test bounds every address the kernel loads from its
// composed slot and checks that the slot's scratch was requested at the
// whole buffer, which covers them.  It then checks that the isolated bench
// computes the kernel from the scratch contents, that every candidate the
// search applies passes the correctness gate there, and that a BEAM=2
// search leaves the live result correct.  The freelist is emptied before
// every scratch allocation, so each scratch is a fresh block of exactly the
// requested size: a best-fit recycle could hand back a larger donor and
// hide an undersized request, and under ASan a read past a fresh block
// faults.

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

static u32 composed_slot(KernelEntry const *ke) {
  for (u32 i = 0; i < ke->n_inputs; i++) {
    if (ke->input_chain_composed[i]) {
      return i;
    }
  }
  return ke->n_inputs;
}

// Park nothing, so the next allocation is a fresh block whose
// CPU_BUFS[].nbytes is exactly the requested size.
static void fresh_allocs(void) {
  CPU_FREELIST_LEN = 0;
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
typedef u32 (*MismatchFn)(Case const *k, f32 const *in, f32 const *out);

// One kernel over one leaf buffer x of `whole` elements, whose highest
// element the kernel reads is `last`.  A sibling-view kernel computes
// out[r][j] = x[r][lhs + j] op x[r][rhs + j] for j < width over x of shape
// (rows, cols).
struct Case {
  char const *name;
  u32         whole;
  u32         last;
  u32         nviews;
  MismatchFn  mismatches;
  u32         rows;
  u32         cols;
  u32         lhs;
  u32         rhs;
  u32         width;
  char        op;
};

// out[r * 64 + c] = 2 * in[r * 128 + 64 + c]
static u32 shrink_mul_mismatches(Case const *k, f32 const *in,
                                 f32 const *out) {
  (void)k;
  u32 bad = 0;
  for (u32 r = 0; r < 2; r++) {
    for (u32 c = 0; c < 64; c++) {
      if (out[r * 64 + c] != 2.0f * in[r * 128 + 64 + c]) {
        bad++;
      }
    }
  }
  return bad;
}

// out[r] = sum of in[r * 32 + j * 8 + 4 + k] over j, k < 4
static u32 merge_sum_mismatches(Case const *k, f32 const *in,
                                f32 const *out) {
  (void)k;
  u32 bad = 0;
  for (u32 r = 0; r < 2; r++) {
    f64 want = 0.0;
    for (u32 j = 0; j < 4; j++) {
      for (u32 m = 0; m < 4; m++) {
        want += (f64)in[r * 32 + j * 8 + 4 + m];
      }
    }
    if (fabs((f64)out[r] - want) > 1e-4 * (1.0 + fabs(want))) {
      bad++;
    }
  }
  return bad;
}

static u32 sibling_mismatches(Case const *k, f32 const *in, f32 const *out) {
  u32 bad = 0;
  for (u32 r = 0; r < k->rows; r++) {
    for (u32 j = 0; j < k->width; j++) {
      f32 a    = in[r * k->cols + k->lhs + j];
      f32 b    = in[r * k->cols + k->rhs + j];
      f32 want = k->op == '+' ? a + b : k->op == '-' ? a - b : a * b;
      if (out[r * k->width + j] != want) {
        bad++;
      }
    }
  }
  return bad;
}

static Term sibling_uop(Case const *k, Term x) {
  u32  lo[4] = {0, k->rows, k->lhs, k->lhs + k->width};
  u32  hi[4] = {0, k->rows, k->rhs, k->rhs + k->width};
  Term a     = uop_shrink(x, 2, lo);
  Term b     = uop_shrink(x, 2, hi);
  switch (k->op) {
    case '+':
      return uop_binary(UOP_ADD, a, b);
    case '-':
      return uop_binary(UOP_ADD, a, uop_unary(UOP_NEG, b));
    default:
      return uop_binary(UOP_MUL, a, b);
  }
}

// === Checks ===

// Bench `kid` on the isolated scratch: the composed slot's scratch holds
// the whole buffer, which covers every address the kernel loads from it,
// the bench computes the kernel from it, and every candidate the search
// applies passes the correctness gate.
static void check_iso_scratch(Case const *k, u32 kid) {
  char         label[128];
  KernelEntry *ke   = &KERNELS[kid];
  u32          slot = composed_slot(ke);

  snprintf(label, sizeof(label), "autotune-iso/%s/slot-chain-composed",
           k->name);
  TEST_BEGIN(label);
  CHECK(slot < ke->n_inputs);
  if (slot >= ke->n_inputs) {
    return;
  }
  CHECK_EQ(TENS[ke->input_tids[slot]].nviews, k->nviews);

  snprintf(label, sizeof(label), "autotune-iso/%s/loads-reach-last", k->name);
  TEST_BEGIN(label);
  Span s = buffer_load_span(ke->cached_lift.store_root, slot + 1, 0);
  CHECK_EQ(s.st, SPAN_OK);
  CHECK(s.lo >= 0);
  CHECK_EQ(s.hi, k->last);
  u64 hi   = s.st == SPAN_OK && s.hi > (i64)k->last ? (u64)s.hi : k->last;
  u64 need = (hi + 1) * sizeof(f32);

  snprintf(label, sizeof(label), "autotune-iso/%s/bench-isolates", k->name);
  TEST_BEGIN(label);
  fresh_allocs();
  int iso = kautotune_iso_begin(kid);
  CHECK_EQ(iso, 1);
  if (!iso) {
    return;
  }

  u64 have  = CPU_BUFS[ISO.in_buf_ids[slot]].nbytes;
  u64 whole = (u64)k->whole * sizeof(f32);
  snprintf(label, sizeof(label), "autotune-iso/%s/scratch-covers-loads",
           k->name);
  TEST_BEGIN(label);
  CHECK(have >= need);
  snprintf(label, sizeof(label), "autotune-iso/%s/scratch-is-whole-buffer",
           k->name);
  TEST_BEGIN(label);
  CHECK_EQ(have, whole);

  // Fire only on a scratch that covers the loads: an undersized one would
  // make the test itself read past it.
  snprintf(label, sizeof(label), "autotune-iso/%s/bench-reads-scratch",
           k->name);
  TEST_BEGIN(label);
  int covered = have >= need && have >= whole;
  CHECK(covered);
  if (covered) {
    kernel_bench_fire(kid);
    KautotuneOutStat st = kautotune_output_stat(ke);
    CHECK(st.valid && st.all_finite);
    f32 *in  = (f32 *)malloc((size_t)whole);
    f32 *out = (f32 *)malloc((size_t)ke->output_numel * sizeof(f32));
    ISO.backend->buf_read(ISO.in_buf_ids[slot], in, whole);
    ISO.backend->buf_read(ISO.out_buf_id, out,
                          (u64)ke->output_numel * sizeof(f32));
    CHECK_EQ(k->mismatches(k, in, out), 0);
    free(in);
    free(out);
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
  u32  xt  = (u32)term_val(x);
  f32 *in  = (f32 *)malloc((size_t)k->whole * sizeof(f32));
  f32 *out = (f32 *)malloc((size_t)ke->output_numel * sizeof(f32));
  TENS[xt].backend->buf_read(TENS[xt].buf_id, in,
                             (u64)k->whole * sizeof(f32));
  TENS[ke->output_tid].backend->buf_read(TENS[ke->output_tid].buf_id, out,
                                         (u64)ke->output_numel * sizeof(f32));
  CHECK_EQ(k->mismatches(k, in, out), 0);
  free(in);
  free(out);
}

static void run_case(Case const *k, Term x, Term y) {
  char label[128];
  u32  kid = kernel_writing(thvm_realize(y));
  snprintf(label, sizeof(label), "autotune-iso/%s/kernel", k->name);
  TEST_BEGIN(label);
  CHECK(kid != 0 && kid < KERNELS_NEXT);
  if (kid == 0 || kid >= KERNELS_NEXT) {
    return;
  }
  check_iso_scratch(k, kid);
  check_beam_live(k, kid, x);
}

// Two views of one buffer: the first operand's view is the first slot, and
// both loads land on it.  With the lower view first, that slot's own view
// stops short of the buffer's end (x[:, :64] spans elements 0..191 of 256,
// x[:, :-1] 0..130 of 132, x[:, :32] 0..223 of 256); with it second, the
// first slot's view happens to reach the end.
#define SIBLING(nm, r, c, l, h, w, o)                                  \
  {.name = (nm), .whole = (r) * (c), .last = (r) * (c) - 1,             \
   .mismatches = sibling_mismatches, .rows = (r), .cols = (c),          \
   .lhs = (l), .rhs = (h), .width = (w), .op = (o)}

static Case const SIBLINGS[] = {
  SIBLING("halves-add",     2, 128,  0, 64, 64, '+'),
  SIBLING("halves-add-rev", 2, 128, 64,  0, 64, '+'),
  SIBLING("diff",           4,  33,  0,  1, 32, '-'),
  SIBLING("diff-rev",       4,  33,  1,  0, 32, '-'),
  SIBLING("glu-halves",     4,  64,  0, 32, 32, '*'),
  SIBLING("glu-halves-rev", 4,  64, 32,  0, 32, '*'),
};

int main(void) {
  unsetenv("BEAM");
  unsetenv("JITBEAM");
  unsetenv("AUTOTUNE");
  thvm_init();

  // x.shrink(((0,2),(64,128))) * 2 over x of shape (2,128): the view is
  // (2,64) at offset 64 with row stride 128, so the kernel reads x up to
  // element 1*128 + 64 + 63 = 255.
  {
    Case const k = {.name = "shrink-mul", .whole = 256, .last = 255,
                    .mismatches = shrink_mul_mismatches};
    u32  xd[2] = {2, 128};
    u32  be[4] = {0, 2, 64, 128};
    u32  yd[2] = {2, 64};
    Term x     = leaf_f32(2, xd);
    Term two   = full_f32(0x40000000u, 2, yd);
    run_case(&k, x, uop_binary(UOP_MUL, uop_shrink(x, 2, be), two));
  }

  // x.shrink(((0,2),(0,4),(4,8))).reshape(2,16).sum(1) over x of shape
  // (2,4,8): the reshape merges the shrunk axis with its outer neighbour,
  // a two-view chain whose address reaches 32 + 3*8 + 4 + 3 = 63.
  {
    Case const k = {.name = "merge-sum", .whole = 64, .last = 63, .nviews = 1,
                    .mismatches = merge_sum_mismatches};
    u32  zd[3]  = {2, 4, 8};
    u32  zbe[6] = {0, 2, 0, 4, 4, 8};
    u32  md[2]  = {2, 16};
    Term z      = leaf_f32(3, zd);
    Term merged = uop_reshape(uop_shrink(z, 3, zbe), 2, md);
    run_case(&k, z, uop_reduce(REDUCE_SUM, 1, merged));
  }

  for (u32 i = 0; i < sizeof(SIBLINGS) / sizeof(SIBLINGS[0]); i++) {
    Case const *k     = &SIBLINGS[i];
    u32         xd[2] = {k->rows, k->cols};
    Term        x     = leaf_f32(2, xd);
    run_case(k, x, sibling_uop(k, x));
  }

  thvm_free();
  TEST_REPORT();
}
