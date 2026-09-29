// uop/scatter.c - UOP_SCATTER: `src` combined into a copy of `base` at the
// positions an index tensor names along axis `dim`.
//
// Heap layout: [base, idx, src, NUM(dim), NUM(kind), NUM(include_self)];
// arity 3 (base, idx, src).  The node has base's shape and dtype.  With
// t(p) = p with p_dim := idx[p], valid(p) = 0 <= idx[p] < base.shape[dim]
// and I the kind's identity (scatter_identity_bits):
//
//   SUM, PROD, MAX, MIN   out = S (op) T
//     S[j] folds src[p] over the valid p with t(p) = j, in row-major p
//          order, or is I when there is none;
//     T    = base if include_self, else (j touched ? I : base).
//   REPLACE               out[j] = src[p] for the last valid p (along dim)
//                         with t(p) = j, or base[j] when untouched.
//
// This is tinygrad's scatter_reduce algebra (mixin/op.py:1126-1134) and
// scatter's last-write-wins merge (_masked_merge, :1170-1177).
//
// uop_scatter returns 0 unless the operands have equal ndim, idx has an
// integer dtype, dim < ndim, src.shape == idx.shape (the frontend shrinks
// src first, as _pre_scatter does at :1088), idx.shape[d] <= base.shape[d]
// for every d != dim, dtype(src) == dtype(base), kind is a SCATTER_* with
// an identity the dtype can express, include_self is 0 or 1, and the shapes
// pass uop_indexed_shape_ok.  include_self does not apply to REPLACE, which
// stores 0 so equal scatters hash-cons to one node.  uop_scatter_node
// builds the node unchecked.

fn Term uop_scatter_node(Term base, Term idx, Term src, u32 dim, u32 kind,
                         u32 include_self) {
  u32 args[9] = { (u32)base, (u32)(base >> 32),
                  (u32)idx,  (u32)(idx  >> 32),
                  (u32)src,  (u32)(src  >> 32),
                  dim, kind, include_self };
  u64 key = uop_mov_hash(UOP_SCATTER, 0, args, 9);
  Term hit = uop_mov_lookup(key);
  if (hit != 0) {
    return hit;
  }
  u64 loc = heap_alloc(6);
  heap_set(loc + 0, base);
  heap_set(loc + 1, idx);
  heap_set(loc + 2, src);
  heap_set(loc + 3, term_new(0, TAG_NUM, DT_INT32, dim));
  heap_set(loc + 4, term_new(0, TAG_NUM, DT_INT32, kind));
  heap_set(loc + 5, term_new(0, TAG_NUM, DT_INT32, include_self));
  Term t = term_new(0, TAG_UOP, UOP_SCATTER, loc);
  uop_mov_insert(key, t);
  return t;
}

fn Term uop_scatter(Term base, Term idx, Term src, u32 dim, u32 kind,
                    u32 include_self) {
  Shape bs, is, ss;
  u32   idt, bdt, sdt, ident;
  if (!term_shape_in(base, 0, &bs) || !term_shape_in(idx, 0, &is)
      || !term_shape_in(src, 0, &ss)) {
    return 0;
  }
  if (bs.ndim != is.ndim || ss.ndim != is.ndim || dim >= bs.ndim) {
    return 0;
  }
  if (include_self > 1) {
    return 0;
  }
  if (!term_dtype_in(idx, 0, &idt) || !dtype_is_int(idt) || dtype_is_packed(idt)) {
    return 0;
  }
  if (!term_dtype_in(base, 0, &bdt) || !term_dtype_in(src, 0, &sdt)
      || bdt != sdt || dtype_is_packed(bdt)) {
    return 0;
  }
  if (!scatter_identity_bits(kind, bdt, &ident)) {
    return 0;
  }
  if (!uop_indexed_shape_ok(&bs) || !uop_indexed_shape_ok(&is)) {
    return 0;
  }
  for (u32 d = 0; d < bs.ndim; d++) {
    if (ss.dims[d] != is.dims[d]) {
      return 0;
    }
    if (d != dim && is.dims[d] > bs.dims[d]) {
      return 0;
    }
  }
  if (kind == SCATTER_REPLACE) {
    include_self = 0;
  }
  return uop_scatter_node(base, idx, src, dim, kind, include_self);
}

fn Term uop_scatter_base(Term s) {
  return heap_read(term_val(s) + 0);
}

fn Term uop_scatter_idx(Term s) {
  return heap_read(term_val(s) + 1);
}

fn Term uop_scatter_src(Term s) {
  return heap_read(term_val(s) + 2);
}

fn u32 uop_scatter_dim(Term s) {
  return (u32)term_val(heap_read(term_val(s) + 3));
}

fn u32 uop_scatter_kind(Term s) {
  return (u32)term_val(heap_read(term_val(s) + 4));
}

fn u32 uop_scatter_include_self(Term s) {
  return (u32)term_val(heap_read(term_val(s) + 5));
}

// The largest (want_max) or smallest value of `dtype` as a CONST payload:
// tinygrad DType.max / DType.min (dtype.py:70-78), which is +-inf for a
// float except the infinity-free fp8e4m3 (+-448, its largest normal), the
// integer range, and True / False for bool.  0 when a 64-bit integer
// extreme does not fit the 32-bit payload.
static int scatter_dtype_extreme(u32 dtype, int want_max, u32 *bits) {
  DTypeInfo const *di = dtype_info(dtype);
  if (di == NULL) {
    return 0;
  }
  switch (di->kind) {
    case DK_BOOL:
      *bits = want_max ? 1u : 0u;
      return 1;
    case DK_FLOAT: case DK_FP16: case DK_BF16: case DK_FP8: {
      f32 v = (dtype == DT_FP8E4M3) ? 448.0f : INFINITY;
      if (!want_max) {
        v = -v;
      }
      memcpy(bits, &v, sizeof *bits);
      return 1;
    }
    case DK_SINT: {
      if (di->bits > 32) {
        return 0;
      }
      i32 hi = (i32)((1ull << (di->bits - 1)) - 1);
      *bits = (u32)(want_max ? hi : -hi - 1);
      return 1;
    }
    case DK_UINT:
      if (!want_max) {
        *bits = 0;
        return 1;
      }
      if (di->bits > 32) {
        return 0;
      }
      *bits = (u32)((1ull << di->bits) - 1);
      return 1;
    default:
      return 0;
  }
}

// The identity of `kind` over `dtype` as a CONST payload (uop/const.c): 0
// for SUM, 1 for PROD, the dtype minimum for MAX and maximum for MIN.
// REPLACE never reads one; its payload is 0.
fn int scatter_identity_bits(u32 kind, u32 dtype, u32 *bits) {
  switch (kind) {
    case SCATTER_SUM:
    case SCATTER_REPLACE:
      *bits = 0;
      return 1;
    case SCATTER_PROD:
      if (dtype_is_float(dtype)) {
        f32 one = 1.0f;
        memcpy(bits, &one, sizeof *bits);
      } else {
        *bits = 1;
      }
      return 1;
    case SCATTER_MAX:
      return scatter_dtype_extreme(dtype, 0, bits);
    case SCATTER_MIN:
      return scatter_dtype_extreme(dtype, 1, bits);
    default:
      return 0;
  }
}
