// Heap layout: [src, NUM(dst_dtype)], equal itemsizes.  A CONST folds as
// tinygrad's fold_bitcast (uop/symbolic.py:23-26), except packed int4 / uint4
// (a node) and 8-byte dtypes (payload kept: a u32 cannot hold 64 bits).

static u32 uop_bitcast_int_mask(u32 dt) {
  u32 size = dtype_itemsize(dt);
  return size >= 4 ? 0xFFFFFFFFu : (1u << (8 * size)) - 1u;
}

// Storage bits of CONST payload `bits` at dtype `dt`: to_storage_scalar
// + struct.pack (tinygrad dtype.py:286-290, 303-307).
static u32 uop_bitcast_const_store(u32 dt, u32 bits) {
  f32 v;
  memcpy(&v, &bits, sizeof v);
  switch (dtype_kind(dt)) {
    case DK_FP16:
    case DK_BF16: {
      u16 h;
      from_fp32_lane(&h, dt, &v, 1);
      return h;
    }
    case DK_FP8: {
      u8 q;
      from_fp32_lane(&q, dt, &v, 1);
      return q;
    }
    case DK_BOOL:
      return bits != 0;
    case DK_SINT:
    case DK_UINT:
      return bits & uop_bitcast_int_mask(dt);
    default:
      return bits;
  }
}

// CONST payload of storage bits `raw` read at dtype `dt`: struct.unpack
// + from_storage_scalar (tinygrad dtype.py:292-295, 303-307).
static u32 uop_bitcast_const_load(u32 dt, u32 raw) {
  f32 v;
  switch (dtype_kind(dt)) {
    case DK_FP16:
    case DK_BF16: {
      u16 h = (u16)raw;
      to_fp32_lane(&v, &h, dt, 1);
      break;
    }
    case DK_FP8: {
      u8 q = (u8)raw;
      to_fp32_lane(&v, &q, dt, 1);
      break;
    }
    case DK_BOOL:
      return raw != 0;
    case DK_SINT:
      switch (dtype_itemsize(dt)) {
        case 1:  return (u32)(i32)(i8)raw;
        case 2:  return (u32)(i32)(i16)raw;
        default: return raw;
      }
    default:
      return raw;
  }
  u32 bits;
  memcpy(&bits, &v, sizeof bits);
  return bits;
}

// 0 declines the fold.
static Term uop_bitcast_const(u32 src_dt, u32 bits, u32 dst_dt) {
  if (dtype_is_packed(src_dt) || dtype_is_packed(dst_dt)) {
    return 0;
  }
  if (dtype_itemsize(dst_dt) == 8) {
    return uop_const(dst_dt, bits);
  }
  u32 raw = uop_bitcast_const_store(src_dt, bits);
  return uop_const(dst_dt, uop_bitcast_const_load(dst_dt, raw));
}

fn Term uop_bitcast(Term src, u32 dst_dtype) {
  u32 src_dtype = DT_FP32;
  if (term_dtype_in(src, 0, &src_dtype) && src_dtype == dst_dtype) {
    return src;
  }

  // Differing itemsizes return src unchanged, not a bad BITCAST (callers CAST
  // first); packed dtypes skip the check, as dtype_itemsize aborts on them.
  if (!dtype_is_packed(src_dtype) && !dtype_is_packed(dst_dtype)
      && dtype_itemsize(src_dtype) != dtype_itemsize(dst_dtype)) {
    fprintf(stderr, "uop_bitcast: itemsize mismatch: src=%s (%u B) dst=%s (%u B)\n",
            dtype_name(src_dtype), dtype_itemsize(src_dtype),
            dtype_name(dst_dtype), dtype_itemsize(dst_dtype));
    return src;
  }

  // bitcast(bitcast(x, mid), dst) = bitcast(x, dst): both keep the bits.
  if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_BITCAST) {
    Term inner = heap_read(term_val(src));
    return uop_bitcast(inner, dst_dtype);
  }

  if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_CONST) {
    Term num = heap_read(term_val(src));
    if (term_tag(num) == TAG_NUM) {
      Term folded = uop_bitcast_const(term_ext(num), (u32)term_val(num), dst_dtype);
      if (folded != 0) {
        return folded;
      }
    }
  }

  u64 loc = heap_alloc(2);
  heap_set(loc + 0, src);
  heap_set(loc + 1, term_new(0, TAG_NUM, DT_INT32, dst_dtype));
  return term_new(0, TAG_UOP, UOP_BITCAST, loc);
}
