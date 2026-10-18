// Heap layout: [base, idx, src, NUM(dim), NUM(kind), NUM(include_self)].

// tinygrad's scatter_reduce (mixin/op.py:1126-1134): out[j] folds the src[p]
// with in-range idx[p] = j into base[j], or for a hit j into the identity when
// include_self is 0.  REPLACE: last write wins (_masked_merge, :1170-1177).

// Unchecked, for rebuilding a node whose sources a rewrite replaced.
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

// 0 when a check fails.  src.shape must equal idx.shape: the frontend shrinks
// src first, as tinygrad's _pre_scatter does (mixin/op.py:1088).
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
  // REPLACE takes no include_self; storing 0 keeps equal scatters one node.
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

// The max (want_max) or min of `dtype` as a CONST payload: tinygrad DType.max /
// DType.min (dtype.py:70-78), +-inf for a float but +-448 for the inf-free
// fp8e4m3.  0 when a 64-bit integer extreme does not fit the 32-bit payload.
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

// Writes the identity of `kind` over `dtype` as a CONST payload; 0 when none
// exists.  REPLACE never reads one; its payload is 0.
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
