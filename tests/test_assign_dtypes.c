// test_assign_dtypes.c - ASSIGN writes a tensor of every dtype.
//
// ASSIGN(dst, src) copies src's storage into dst's buffer byte for byte
// when the dtypes match, whatever the dtype: fp8, f16 and bf16 bit
// patterns (NaN payloads, subnormals, -0) survive unchanged, and so do
// 1-, 2- and 8-byte ints and packed nibbles.  A dst that is a row view of
// a larger buffer takes the row at its element offset, scaled by the
// dtype's width.  When the dtypes differ, dst takes src's values at its
// own dtype, as a realized CAST stores them.  tinygrad casts a weak
// (Python-scalar) value to the target dtype before assigning and raises
// on any other mismatch (tensor.py:243-244 at 365f87a8a); a fired ASSIGN
// cannot tell the two apart and converts both.  A JIT capture replays
// the write.

#include "../src/thvm.c"
#include "test.h"

#define N 8

static Term mk_assign(Term dst, Term src) {
  u64 loc = heap_alloc(2);
  heap_set(loc + 0, dst);
  heap_set(loc + 1, src);
  return term_new(0, TAG_UOP, UOP_ASSIGN, loc);
}

static u32 cpu_tensor(u32 dtype, u32 ndim, u32 const *dims, void const *data) {
  Shape s = {0};
  s.ndim  = ndim;
  u32 n   = 1;
  for (u32 i = 0; i < ndim; i++) {
    s.dims[i] = dims[i];
    n *= dims[i];
  }
  u32 tid = tensor_alloc(&CPU_BACKEND, s, dtype);
  CPU_BACKEND.buf_write(TENS[tid].buf_id, data, dtype_storage_bytes(dtype, n));
  return tid;
}

static Term ten(u32 tid) {
  return term_new(0, TAG_TEN, TENS[tid].dtype, tid);
}

static void read_bytes(u32 tid, void *out, u64 nbytes) {
  CPU_BACKEND.buf_read(TENS[tid].buf_id, out, nbytes);
}

static void put(u8 *out, void const *v, u64 vbytes, u64 nbytes) {
  memcpy(out, v, (size_t)(vbytes < nbytes ? vbytes : nbytes));
}

// Bytes that no float round trip keeps: NaN payloads (quiet and
// signaling, both signs), subnormals and -0 for each float width.
static void fill_pattern(u32 dtype, u8 *out, u64 nbytes) {
  for (u64 i = 0; i < nbytes; i++) {
    out[i] = (u8)(0xA5u ^ (u32)(i * 29u + 3u));
  }
  switch (dtype) {
    case DT_BOOL:
      for (u64 i = 0; i < nbytes; i++) {
        out[i] = (u8)(i & 1u);
      }
      break;
    case DT_FP16: {
      u16 const v[N] = { 0x7C01, 0xFE01, 0x0001, 0x8000, 0x03FF, 0x7C00, 0xABCD, 0x3C00 };
      put(out, v, sizeof v, nbytes);
      break;
    }
    case DT_BF16: {
      u16 const v[N] = { 0x7F81, 0xFFC1, 0x0001, 0x8000, 0x007F, 0x7F80, 0xABCD, 0x3F80 };
      put(out, v, sizeof v, nbytes);
      break;
    }
    case DT_FP8E4M3: {
      u8 const v[N] = { 0x7F, 0xFF, 0x01, 0x80, 0x07, 0x7E, 0xAB, 0x38 };
      put(out, v, sizeof v, nbytes);
      break;
    }
    case DT_FP8E5M2: {
      u8 const v[N] = { 0x7D, 0xFE, 0x01, 0x80, 0x03, 0x7C, 0xAB, 0x3C };
      put(out, v, sizeof v, nbytes);
      break;
    }
    case DT_FP32: {
      u32 const v[N] = { 0x7F800001u, 0xFFC00001u, 0x00000001u, 0x80000000u,
                         0x007FFFFFu, 0x7F800000u, 0xABCDEF01u, 0x3F800000u };
      put(out, v, sizeof v, nbytes);
      break;
    }
    case DT_FP64: {
      u64 const v[N] = { 0x7FF0000000000001ull, 0xFFF8000000000001ull, 1ull,
                         0x8000000000000000ull, 0x000FFFFFFFFFFFFFull,
                         0x7FF0000000000000ull, 0xABCDEF0123456789ull,
                         0x3FF0000000000000ull };
      put(out, v, sizeof v, nbytes);
      break;
    }
    default:
      break;
  }
}

static u32 const ALL_DTYPES[] = {
  DT_BOOL,   DT_INT8,    DT_UINT8,    DT_INT16, DT_UINT16, DT_INT32,
  DT_UINT32, DT_INT64,   DT_UINT64,   DT_FP8E4M3, DT_FP8E5M2, DT_FP16,
  DT_BF16,   DT_FP32,    DT_FP64,     DT_INT4,  DT_UINT4,
};

static void check_same_dtype_copy(u32 dtype) {
  static char name[64];
  snprintf(name, sizeof name, "assign/same-dtype-copies-bytes/%s", dtype_name(dtype));
  TEST_BEGIN(name);
  u64 nbytes = dtype_storage_bytes(dtype, N);
  u8  src[64], old[64], got[64];
  fill_pattern(dtype, src, nbytes);
  memset(old, dtype == DT_BOOL ? 0x01 : 0x11, sizeof old);
  u32 const dims[1] = { N };
  u32 s_tid = cpu_tensor(dtype, 1, dims, src);
  u32 d_tid = cpu_tensor(dtype, 1, dims, old);
  Term done = term_resolve(thvm_realize(mk_assign(ten(d_tid), ten(s_tid))));
  CHECK(term_tag(done) == TAG_TEN);
  CHECK_EQ(term_val(done), d_tid);
  read_bytes(d_tid, got, nbytes);
  u32 mismatches = 0;
  for (u64 i = 0; i < nbytes; i++) {
    mismatches += (got[i] != src[i]);
  }
  CHECK_EQ(mismatches, 0);
}

// dst is row 1 of a {3,4} buffer: the write lands at element offset 4,
// rows 0 and 2 keep their bytes.
static void check_row_view(u32 dtype) {
  static char name[64];
  snprintf(name, sizeof name, "assign/row-view-at-element-offset/%s", dtype_name(dtype));
  TEST_BEGIN(name);
  u32 esz = dtype_itemsize(dtype);
  u8  cache[12 * 8], row[4 * 8], got[12 * 8];
  memset(cache, 0x11, sizeof cache);
  fill_pattern(dtype, row, 4u * esz);
  u32 const cdims[2] = { 3, 4 };
  u32 const rdims[2] = { 1, 4 };
  u32 c_tid = cpu_tensor(dtype, 2, cdims, cache);
  u32 r_tid = cpu_tensor(dtype, 2, rdims, row);
  u32 const be[4] = { 1, 2, 0, 4 };
  Term dst = uop_shrink(ten(c_tid), 2, be);
  thvm_realize(mk_assign(dst, ten(r_tid)));
  read_bytes(c_tid, got, 12u * esz);
  u32 mismatches = 0;
  for (u32 i = 0; i < 12u * esz; i++) {
    u8 want = (i >= 4u * esz && i < 8u * esz) ? row[i - 4u * esz] : 0x11;
    mismatches += (got[i] != want);
  }
  CHECK_EQ(mismatches, 0);
}

// A mismatched ASSIGN stores what a realized CAST of src to dst's dtype
// stores, and the expected bytes are spelled out too.
static void check_convert(char const *name, u32 sdt, void const *src, u32 ddt,
                          void const *want) {
  TEST_BEGIN(name);
  u32 const dims[1] = { 4 };
  u64 dbytes = dtype_storage_bytes(ddt, 4);
  u8  old[32], got[32], cast[32];
  memset(old, 0x11, sizeof old);
  u32 s_tid = cpu_tensor(sdt, 1, dims, src);
  u32 d_tid = cpu_tensor(ddt, 1, dims, old);
  Term done = term_resolve(thvm_realize(mk_assign(ten(d_tid), ten(s_tid))));
  CHECK_EQ(term_val(done), d_tid);
  read_bytes(d_tid, got, dbytes);
  CHECK(memcmp(got, want, (size_t)dbytes) == 0);
  Term c = term_resolve(thvm_realize(uop_cast(ten(s_tid), ddt)));
  CHECK(term_tag(c) == TAG_TEN);
  if (term_tag(c) == TAG_TEN) {
    read_bytes((u32)term_val(c), cast, dbytes);
    CHECK(memcmp(got, cast, (size_t)dbytes) == 0);
  }
}

static void check_conversions(void) {
  f32 const f[4] = { 1.5f, -2.0f, INFINITY, 65520.0f };
  u16 const f_as_f16[4] = { 0x3E00, 0xC000, 0x7C00, 0x7C00 };
  check_convert("assign/converts/f32-to-f16", DT_FP32, f, DT_FP16, f_as_f16);

  u16 const f_as_bf16[4] = { 0x3FC0, 0xC000, 0x7F80, 0x4780 };
  check_convert("assign/converts/f32-to-bf16", DT_FP32, f, DT_BF16, f_as_bf16);

  // The kernel store saturates fp8: inf stores as maxnorm.
  u8 const f_as_e5m2[4] = { 0x3E, 0xC0, 0x7B, 0x7B };
  check_convert("assign/converts/f32-to-fp8e5m2", DT_FP32, f, DT_FP8E5M2, f_as_e5m2);

  f32 const g[4] = { 2.75f, -2.75f, 100.0f, -128.0f };
  i8  const g_as_i8[4] = { 2, -2, 100, -128 };
  check_convert("assign/converts/f32-to-i8", DT_FP32, g, DT_INT8, g_as_i8);

  i32 const k[4] = { 3, -7, 16777217, -1 };
  f32 const k_as_f32[4] = { 3.0f, -7.0f, 16777216.0f, -1.0f };
  check_convert("assign/converts/i32-to-f32", DT_INT32, k, DT_FP32, k_as_f32);

  u16 const h[4] = { 0x3C00, 0x0400, 0xFC00, 0xC500 };
  f64 const h_as_f64[4] = { 1.0, 0x1p-14, -INFINITY, -5.0 };
  check_convert("assign/converts/f16-to-f64", DT_FP16, h, DT_FP64, h_as_f64);

  // A bool is x != 0, tinygrad's bool(x): 0.5 is true.
  f32 const t[4] = { 0.5f, 2.0f, -0.0f, -3.0f };
  u8  const t_as_bool[4] = { 1, 1, 0, 1 };
  check_convert("assign/converts/f32-to-bool", DT_FP32, t, DT_BOOL, t_as_bool);

  // An int reaches f32 rounded once: 2^60 + 2^36 + 1 is past the halfway
  // point 2^60 + 2^36, where a double already lands.
  i64 const big = ((i64)1 << 60) + ((i64)1 << 36) + 1;
  i64 const l[4] = { big, 3, -5, -big };
  f32 const l_as_f32[4] = { 0x1.000002p+60f, 3.0f, -5.0f, -0x1.000002p+60f };
  check_convert("assign/converts/i64-to-f32", DT_INT64, l, DT_FP32, l_as_f32);

  u64 const u[4] = { ((u64)1 << 63) + ((u64)1 << 39) + 1, 1, UINT64_MAX,
                     ((u64)1 << 53) + 1 };
  f32 const u_as_f32[4] = { 0x1.000002p+63f, 1.0f, 0x1p+64f, 0x1p+53f };
  check_convert("assign/converts/u64-to-f32", DT_UINT64, u, DT_FP32, u_as_f32);
}

// A captured ASSIGN of a narrow dtype replays: the second write reads
// the source's new bytes.
static void check_jit_replay(u32 sdt, u32 ddt, void const *src0, void const *src1,
                             void const *want0, void const *want1, char const *name) {
  TEST_BEGIN(name);
  u32 const dims[1] = { 4 };
  u64 sbytes = dtype_storage_bytes(sdt, 4);
  u64 dbytes = dtype_storage_bytes(ddt, 4);
  u8  old[32], got[32];
  memset(old, 0x11, sizeof old);
  u32 s_tid = cpu_tensor(sdt, 1, dims, src0);
  u32 d_tid = cpu_tensor(ddt, 1, dims, old);
  u32 slot  = jit_capture_begin();
  thvm_realize(mk_assign(ten(d_tid), ten(s_tid)));
  jit_capture_end();
  read_bytes(d_tid, got, dbytes);
  CHECK(memcmp(got, want0, (size_t)dbytes) == 0);
  CPU_BACKEND.buf_write(TENS[s_tid].buf_id, src1, sbytes);
  jit_replay(slot);
  read_bytes(d_tid, got, dbytes);
  CHECK(memcmp(got, want1, (size_t)dbytes) == 0);
}

int main(void) {
  thvm_init();

  for (u32 i = 0; i < sizeof ALL_DTYPES / sizeof ALL_DTYPES[0]; i++) {
    check_same_dtype_copy(ALL_DTYPES[i]);
  }

  u32 const row_dtypes[5] = { DT_FP8E5M2, DT_FP16, DT_BF16, DT_INT8, DT_FP64 };
  for (u32 i = 0; i < 5; i++) {
    check_row_view(row_dtypes[i]);
  }

  check_conversions();

  u16 const h0[4] = { 0x3C00, 0x7C01, 0x0001, 0x8000 };
  u16 const h1[4] = { 0xC000, 0x3555, 0xFE01, 0x03FF };
  check_jit_replay(DT_FP16, DT_FP16, h0, h1, h0, h1, "assign/jit-replay/f16");

  f32 const f0[4] = { 1.0f, -3.0f, 0.5f, 2.0f };
  f32 const f1[4] = { 7.0f, 0.25f, -1.0f, 1.5f };
  u16 const b0[4] = { 0x3F80, 0xC040, 0x3F00, 0x4000 };
  u16 const b1[4] = { 0x40E0, 0x3E80, 0xBF80, 0x3FC0 };
  check_jit_replay(DT_FP32, DT_BF16, f0, f1, b0, b1, "assign/jit-replay/f32-to-bf16");

  thvm_free();
  TEST_REPORT();
}
