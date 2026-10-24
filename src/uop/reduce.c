// Heap layout: [src, NUM(kind), NUM(n_axes), NUM(axis_0), ...].  One REDUCE
// takes every axis, as tinygrad's Ops.REDUCE takes all its ranges as srcs:
// chained single-axis REDUCEs would each become a kernel of their own.

static Term uop_reduce_cell(u32 kind, u32 n_axes, u32 const *axes, Term src) {
  if (n_axes > MAX_DIM) n_axes = MAX_DIM;
  u32 args[2 + MAX_DIM];
  args[0] = kind;
  args[1] = n_axes;
  for (u32 i = 0; i < n_axes; i++) args[2 + i] = axes[i];
  u64 key = uop_mov_hash(UOP_REDUCE, src, args, 2 + n_axes);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) return hit;
  u64 loc = heap_alloc(3 + n_axes);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, kind));
  heap_set(loc + 2, term_new(0, TAG_NUM, DT_INT32, n_axes));
  for (u32 i = 0; i < n_axes; i++) {
    heap_set(loc + 3 + i, term_new(0, TAG_NUM, DT_INT32, axes[i]));
  }
  Term t = term_new(0, TAG_UOP, UOP_REDUCE, loc);
  uop_mov_insert(key, t);
  return t;
}

fn Term uop_reduce_multi(u32 kind, u32 n_axes, u32 const *axes, Term src) {
  if (n_axes > MAX_DIM) n_axes = MAX_DIM;
  // Same-kind REDUCE(REDUCE(body, inner), outer) is one REDUCE of body.  The
  // outer axes number the inner output, so each moves past the inner axes at or
  // below it ((x@w).sum(): K=1 of [M,K,N], then (0,1) of [M,N] = body (0,2)).
  if (n_axes >= 1 && term_tag(src) == TAG_UOP && term_ext(src) == UOP_REDUCE) {
    u32 inner_kind = uop_reduce_kind(src);
    if (inner_kind == kind) {
      u32 inner_n = uop_reduce_n_axes(src);
      if (inner_n + n_axes <= MAX_DIM) {
        u32 inner_sorted[MAX_DIM];
        for (u32 i = 0; i < inner_n; i++) inner_sorted[i] = uop_reduce_axis(src, i);
        for (u32 i = 1; i < inner_n; i++) {
          u32 v = inner_sorted[i];
          u32 j = i;
          while (j > 0 && inner_sorted[j-1] > v) {
            inner_sorted[j] = inner_sorted[j-1]; j--;
          }
          inner_sorted[j] = v;
        }
        u32 merged[MAX_DIM];
        u32 m = 0;
        for (u32 i = 0; i < inner_n; i++) merged[m++] = inner_sorted[i];
        for (u32 i = 0; i < n_axes; i++) {
          u32 remap = axes[i];
          for (u32 j = 0; j < inner_n; j++) {
            if (inner_sorted[j] <= remap) remap++;
            else break;
          }
          merged[m++] = remap;
        }
        return uop_reduce_cell(kind, m, merged, uop_reduce_src(src));
      }
    }
  }
  return uop_reduce_cell(kind, n_axes, axes, src);
}

fn Term uop_reduce(u32 kind, u32 axis, Term src) {
  return uop_reduce_multi(kind, 1, &axis, src);
}

// x ** n (n >= 1) by squaring, as MULs: a PROD over n copies of x, which
// tinygrad's reduce_unparented writes ret ** extent (codegen/simplify.py:89-90).
fn Term uop_reduce_pow(Term x, u64 n) {
  Term acc = 0;
  for (Term p = x;; p = uop_binary(UOP_MUL, p, p)) {
    if (n & 1) {
      acc = acc ? uop_binary(UOP_MUL, acc, p) : p;
    }
    n >>= 1;
    if (n == 0) {
      return acc;
    }
  }
}

fn u32 uop_reduce_kind(Term red) {
  return (u32)term_val(heap_read(term_val(red) + 1));
}
fn u32 uop_reduce_n_axes(Term red) {
  return (u32)term_val(heap_read(term_val(red) + 2));
}
fn u32 uop_reduce_axis(Term red, u32 i) {
  return (u32)term_val(heap_read(term_val(red) + 3 + i));
}
fn Term uop_reduce_src(Term red) {
  return heap_read(term_val(red) + 0);
}
