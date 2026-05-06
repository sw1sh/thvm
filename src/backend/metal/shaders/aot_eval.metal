// backend/metal/shaders/aot_eval.metal -- AOT-on-Metal Phase 7 iter A.
//
// Smallest end-to-end slice: one kernel folds OP2(NUM, NUM) at
// heap[root_loc] and writes the resulting NUM into result[0].
// No allocator, no MAT/CTR/REF dispatch, no general wnf().
// Validates: Term encoding round-trips through MTLBuffer; MSL can
// do the bit-ops we need; xcrun metal compile succeeds; Metal
// dispatch + readback works for our buffer convention.
//
// Buffer-binding convention (set by aot_metal_op2_fold in _.m):
//   buffer(0)  : device Term *heap   -- BOOK_HEAP, copied in
//   buffer(1)  : constant ulong *root_loc
//   buffer(2)  : device Term *result -- single cell
//
// One thread.  Iter B+ adds wide dispatch + a real allocator.

#include <metal_stdlib>
using namespace metal;

// Mirror of src/thvm.h Term encoding:
//   bits 0-37  : val (heap loc / literal payload)
//   bits 38-55 : ext (label / dtype / opcode)
//   bits 56-62 : tag
//   bit  63    : sub
#define TAG_SHIFT  56
#define EXT_SHIFT  38
#define TAG_MASK   0x7FUL
#define EXT_MASK   0x3FFFFUL
#define VAL_MASK   0x3FFFFFFFFFUL

#define TAG_APP    0u
#define TAG_NUM    10u
#define TAG_OP2    13u
#define TAG_MAT    14u

#define OP_ADD     0u
#define OP_SUB     1u
#define OP_MUL     2u
#define OP_EQ      3u
#define OP_LT      4u

typedef ulong Term;

static inline uint  msl_term_tag(Term t) {
  return uint((t >> TAG_SHIFT) & TAG_MASK);
}
static inline uint  msl_term_ext(Term t) {
  return uint((t >> EXT_SHIFT) & EXT_MASK);
}
static inline ulong msl_term_val(Term t) {
  return t & VAL_MASK;
}
static inline Term  msl_term_new(uint tag, uint ext, ulong val) {
  return ((ulong(tag) & TAG_MASK) << TAG_SHIFT)
       | ((ulong(ext) & EXT_MASK) << EXT_SHIFT)
       | ( val        & VAL_MASK);
}

// === Bump allocator =================================================
//
// Iter C-2: a single shared atomic counter (`book_next`) tracks the
// next free cell in the heap.  Each thread that needs space does a
// relaxed atomic_fetch_add and gets a contiguous run of `n` cells.
// No wrap, no per-warp regions yet -- those are later iters tuning
// L2 locality.  Caller (host) seeds book_next to the current host
// BOOK_NEXT before dispatch, reads it back after.
//
// 32-bit atomic: Apple GPU families don't expose 64-bit fetch_add on
// `device atomic_ulong`.  BOOK_CAP = 1<<18 fits in u32 with 14 bits
// to spare, so a u32 cell-index counter is plenty.
static inline ulong aot_book_alloc(device atomic_uint *book_next, uint n) {
  return ulong(atomic_fetch_add_explicit(book_next, n,
                                          memory_order_relaxed));
}

// Shared OP2 fold helper: read root_loc -> heap[root_loc], expect
// OP2(NUM,NUM), return the folded NUM Term.  Same semantics as iter
// A; factored so both the single-fold kernel and the batch kernel
// call identical code.
static inline Term aot_op2_fold_at(device Term *heap, ulong root_loc) {
  Term root = heap[root_loc];
  if (msl_term_tag(root) != TAG_OP2) {
    return msl_term_new(TAG_NUM, 0, 0);  // sentinel: not OP2
  }
  ulong arg_loc = msl_term_val(root);
  Term  a       = heap[arg_loc + 0];
  Term  b       = heap[arg_loc + 1];
  uint  av      = uint(msl_term_val(a));
  uint  bv      = uint(msl_term_val(b));
  uint  op      = msl_term_ext(root);
  uint  rv;
  switch (op) {
    case OP_ADD: rv = av + bv;                break;
    case OP_SUB: rv = av - bv;                break;
    case OP_MUL: rv = av * bv;                break;
    case OP_EQ:  rv = (av == bv) ? 1u : 0u;   break;
    case OP_LT:  rv = (av <  bv) ? 1u : 0u;   break;
    default:     rv = 0u;                     break;
  }
  uint dtype = msl_term_ext(a);   // preserve the lhs dtype
  return msl_term_new(TAG_NUM, dtype, ulong(rv));
}

// Iter A entry point: 1-thread single-root fold.
kernel void aot_eval_op2_fold(
    device   Term  *heap     [[buffer(0)]],
    constant ulong &root_loc [[buffer(1)]],
    device   Term  *result   [[buffer(2)]],
    uint            tid      [[thread_position_in_grid]])
{
  if (tid != 0) return;
  result[0] = aot_op2_fold_at(heap, root_loc);
}

// Iter C-1+C-2: MAT-on-NUM dispatch.  Given root_loc holding
// App(MAT[v, [handler, fallback]], NUM(x)):
//   matched (x == v)     -> return `handler`
//   unmatched (x != v)   -> allocate App(fallback, NUM(x)) on the
//                            heap (via aot_book_alloc) and return
//                            it as a TAG_APP Term.  Proper IC
//                            semantics for the fallback branch.
//
// Bit-pattern shape (x = NUM scrutinee, v = MAT match-value):
//   heap[root_loc]      = APP {.val = a_loc}
//   heap[a_loc + 0]     = MAT {.ext = v, .val = m_loc}
//   heap[a_loc + 1]     = NUM {.val = x}
//   heap[m_loc + 0]     = handler  (returned if x == v)
//   heap[m_loc + 1]     = fallback
static inline Term aot_mat_app_at(device Term *heap,
                                   device atomic_uint *book_next,
                                   ulong root_loc) {
  Term app = heap[root_loc];
  if (msl_term_tag(app) != TAG_APP) {
    return msl_term_new(TAG_NUM, 0, 0xDEAD0u);  // sentinel: not APP
  }
  ulong a_loc = msl_term_val(app);
  Term  mat   = heap[a_loc + 0];
  Term  arg   = heap[a_loc + 1];
  if (msl_term_tag(mat) != TAG_MAT) {
    return msl_term_new(TAG_NUM, 0, 0xDEAD1u);  // sentinel: not MAT
  }
  if (msl_term_tag(arg) != TAG_NUM) {
    return msl_term_new(TAG_NUM, 0, 0xDEAD2u);  // sentinel: scrutinee not NUM
  }
  uint  v     = msl_term_ext(mat);
  uint  x     = uint(msl_term_val(arg));
  ulong m_loc = msl_term_val(mat);
  if (x == v) {
    return heap[m_loc + 0];                     // matched: raw handler
  }
  // Unmatched: construct App(fallback, NUM(x)) on the heap.
  Term fallback = heap[m_loc + 1];
  ulong new_loc = aot_book_alloc(book_next, 2);
  heap[new_loc + 0] = fallback;
  heap[new_loc + 1] = arg;                      // reuse the NUM(x) cell
  return msl_term_new(TAG_APP, 0, new_loc);
}

kernel void aot_eval_mat_app(
    device   Term        *heap      [[buffer(0)]],
    constant ulong       &root_loc  [[buffer(1)]],
    device   Term        *result    [[buffer(2)]],
    device   atomic_uint *book_next [[buffer(3)]],
    uint                  tid       [[thread_position_in_grid]])
{
  if (tid != 0) return;
  result[0] = aot_mat_app_at(heap, book_next, root_loc);
}

// Iter B-2 entry point: N independent OP2 folds in one dispatch.
// Each thread handles roots[tid] -> result[tid].  Demonstrates the
// parallelism story -- one kernel launch amortized over N folds.
kernel void aot_eval_op2_fold_batch(
    device   Term  *heap    [[buffer(0)]],
    device   ulong *roots   [[buffer(1)]],
    device   Term  *result  [[buffer(2)]],
    constant uint  &n_roots [[buffer(3)]],
    uint            tid     [[thread_position_in_grid]])
{
  if (tid >= n_roots) return;
  result[tid] = aot_op2_fold_at(heap, roots[tid]);
}
