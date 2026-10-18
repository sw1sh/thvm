// Heap layout: [NUM(bits)], ext the dtype.  A float payload is the value's
// f32 bits truncated to the dtype (tinygrad DType.const, dtype.py:79-84), so
// f64 is f32-precise; an integer is its low 32 bits, sign-extended if signed.

// The cache returns the CONST already built: CONSTs are immutable and
// materialize dedups by heap loc, so a shared CONST is one kernel input.  It is
// reset wherever the heap is freed or compacted (thvm_init / free / reset, GC).
#define UOP_CONST_CACHE_CAP  (1u << 14)
typedef struct {
  u64 key;                                       // 0 = empty
  Term term;
} UopConstSlot;
static UopConstSlot UOP_CONST_CACHE[UOP_CONST_CACHE_CAP];
static u32          UOP_CONST_CACHE_SIZE = 0;

fn void uop_const_cache_reset(void) {
  memset(UOP_CONST_CACHE, 0, sizeof(UOP_CONST_CACHE));
  UOP_CONST_CACHE_SIZE = 0;
}

static inline u64 uop_const_key(u32 dtype, u32 bits) {
  // Bit 63 keeps a key off the empty slot's 0.  The context slot (bits 48..51)
  // keeps a Term, whose heap loc is valid only in its context, from another.
  u64 ctx = (u64)thvm_context_current() & 0xF;
  return ((u64)dtype << 32) | (u64)bits | (ctx << 48) | (1ULL << 63);
}

// f16 / bf16 / fp8 round-trip the f32 payload through their storage
// encoding, so equal values at the dtype share one cache entry.
static u32 uop_const_truncate(u32 dtype, u32 bits) {
  switch (dtype_kind(dtype)) {
    case DK_FP16:
    case DK_BF16:
    case DK_FP8: {
      f32 v;
      u16 storage = 0;
      memcpy(&v, &bits, sizeof v);
      from_fp32_lane(&storage, dtype, &v, 1);
      to_fp32_lane(&v, &storage, dtype, 1);
      memcpy(&bits, &v, sizeof bits);
      return bits;
    }
    default:
      return bits;
  }
}

fn Term uop_const(u32 dtype, u32 bits) {
  bits = uop_const_truncate(dtype, bits);
  u64 key = uop_const_key(dtype, bits);
  u32 mask = UOP_CONST_CACHE_CAP - 1;
  u32 h = (u32)(key ^ (key >> 32));
  h ^= h >> 13;  h *= 0x5bd1e995u;  h ^= h >> 15;
  for (u32 probe = 0; probe < UOP_CONST_CACHE_CAP; probe++) {
    u32 i = (h + probe) & mask;
    UopConstSlot *s = &UOP_CONST_CACHE[i];
    if (s->key == key) return s->term;
    if (s->key == 0) {
      u64 loc = heap_alloc(1);
      heap_set(loc, term_new(0, TAG_NUM, dtype, bits));
      Term t = term_new(0, TAG_UOP, UOP_CONST, loc);
      s->key  = key;
      s->term = t;
      UOP_CONST_CACHE_SIZE++;
      return t;
    }
  }
  // A full cache returns an unshared CONST rather than fail.
  u64 loc = heap_alloc(1);
  heap_set(loc, term_new(0, TAG_NUM, dtype, bits));
  return term_new(0, TAG_UOP, UOP_CONST, loc);
}
