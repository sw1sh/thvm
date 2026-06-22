// test_uop_content_hash.c -- GC-invariant structural content hash of a
// lifted UOp DAG (src/jit/capture.c: uop_dag_content_hash).
//
// This hash rekeys the Metal PSO / tile-JIT cache off kernel CONTENT
// instead of the raw heap location of cached_lift.store_root.  The
// raw-loc key was unstable: the autotune/BEAM search hash-conses
// candidate DAG variants into the shared heap, and a later GC relocates
// the captured kernels' store_root -> the heap-loc key silently changes
// -> warm-replay PSO miss/collision -> wrong or no PSO -> non-finite
// output.  These tests pin the three properties a correct rekey needs:
//
//   (1) STABLE: identical re-hashing of the same DAG is equal.
//   (2) GC-INVARIANT: a gc_collect relocates store_root, but the
//       content hash is UNCHANGED (the whole point of the fix).
//   (3) SHARING: two structurally-identical kernels hash EQUAL (they
//       SHOULD share a PSO -- that's correct, not a collision).
//   (4) DISTINCTNESS: structurally-different kernels (different op,
//       different dtype, different shape, different const) hash
//       DISTINCT (a collision here = wrong PSO = silent wrong result).

#include "../src/thvm.c"
#include "test.h"

static u32 alloc_f32(u32 *dims, u32 ndim) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) s.dims[i] = dims[i];
  return tensor_alloc(CURRENT_BACKEND, s, DT_FP32);
}

static u32 alloc_dt(u32 *dims, u32 ndim, u32 dt) {
  Shape s = {0};
  s.ndim = ndim;
  for (u32 i = 0; i < ndim; i++) s.dims[i] = dims[i];
  return tensor_alloc(CURRENT_BACKEND, s, dt);
}

// Materialize `root` and return the store_root of the freshly-emitted
// kernel (the last KERNELS[] slot with a populated lift).
static Term materialize_store_root(Term root) {
  thvm_materialize(root);
  for (u32 k = KERNELS_NEXT; k-- > 1;) {
    if (KERNELS[k].cached_lift.store_root != 0) {
      return KERNELS[k].cached_lift.store_root;
    }
  }
  return 0;
}

int main(void) {
  thvm_init();
  // Enable a copying GC so the gc-invariance test below actually
  // relocates the kernel's store_root (Cheney compaction moves live
  // cells into to-space).  Sized to a fraction of the heap so a single
  // collection is cheap.  Must run before any allocation so everything
  // lives in the semi-space gc_collect walks.
  gc_init(thvm_heap_cells() / 2);

  // === (1) STABLE: re-hash of the same DAG is identical ==============
  TEST_BEGIN("content-hash/stable-repeat");
  u32 d16[1] = {16};
  Term a = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d16, 1));
  Term b = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d16, 1));
  Term add = uop_binary(UOP_ADD, a, b);
  Term sr_add = materialize_store_root(add);
  CHECK(sr_add != 0);
  u64 h1 = uop_dag_content_hash(sr_add);
  u64 h2 = uop_dag_content_hash(sr_add);
  CHECK(h1 != 0);
  CHECK_EQ(h1, h2);

  // === (2) GC-INVARIANT: relocation does not change the hash =========
  // This is the core of the fix.  A gc_collect relocates the kernel's
  // store_root (the raw heap loc the OLD key used changes); the content
  // hash must stay identical.  Cheney compaction may or may not move
  // *this particular* root depending on heap layout, so we assert the
  // strong invariant unconditionally: whatever the post-collection loc
  // is, hashing it equals the pre-collection hash.  (The "same content
  // at a DIFFERENT loc -> same hash" property -- the literal defense
  // against the relocation bug -- is also pinned by the structural-
  // sharing test below, which hashes two distinct-loc identical DAGs.)
  TEST_BEGIN("content-hash/gc-invariant");
  {
    u32 ksl = 0;
    for (u32 k = 1; k < KERNELS_NEXT; k++) {
      if (KERNELS[k].cached_lift.store_root == sr_add) { ksl = k; break; }
    }
    CHECK(ksl != 0);
    u64 before_h = uop_dag_content_hash(KERNELS[ksl].cached_lift.store_root);

    Term roots[1] = { add };
    gc_collect(roots, 1);

    Term after_loc = KERNELS[ksl].cached_lift.store_root;
    CHECK(after_loc != 0);
    u64 after_h = uop_dag_content_hash(after_loc);
    CHECK_EQ(before_h, after_h);
  }

  // === (3) SHARING: structurally-identical kernels hash equal ========
  // Two separately-built ADD kernels over the same shape+dtype should
  // share a PSO -- they must hash EQUAL.
  TEST_BEGIN("content-hash/structural-sharing");
  {
    u32 d8[1] = {8};
    Term x1 = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d8, 1));
    Term y1 = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d8, 1));
    Term x2 = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d8, 1));
    Term y2 = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d8, 1));
    Term add1 = uop_binary(UOP_ADD, x1, y1);
    Term add2 = uop_binary(UOP_ADD, x2, y2);
    Term sr1 = materialize_store_root(add1);
    Term sr2 = materialize_store_root(add2);
    CHECK(sr1 != 0);
    CHECK(sr2 != 0);
    CHECK_EQ(uop_dag_content_hash(sr1), uop_dag_content_hash(sr2));
  }

  // === (4) DISTINCTNESS: different op / dtype / shape hash distinct ===
  TEST_BEGIN("content-hash/distinct-op");
  {
    u32 d8[1] = {8};
    Term x = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d8, 1));
    Term y = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d8, 1));
    Term add = uop_binary(UOP_ADD, x, y);
    Term mul = uop_binary(UOP_MUL, x, y);
    Term sr_add8 = materialize_store_root(add);
    Term sr_mul8 = materialize_store_root(mul);
    CHECK(sr_add8 != 0);
    CHECK(sr_mul8 != 0);
    // ADD vs MUL over identical operands: same I/O shape, different op
    // -> the OLD too-coarse key (root tag/ext + shapes only) could
    // collide on the elementwise-binary tag; the content hash must not.
    CHECK(uop_dag_content_hash(sr_add8) != uop_dag_content_hash(sr_mul8));
  }

  TEST_BEGIN("content-hash/distinct-dtype");
  {
    u32 d8[1] = {8};
    Term xf = term_new(0, TAG_TEN, DT_FP32, alloc_dt(d8, 1, DT_FP32));
    Term yf = term_new(0, TAG_TEN, DT_FP32, alloc_dt(d8, 1, DT_FP32));
    Term xh = term_new(0, TAG_TEN, DT_FP16, alloc_dt(d8, 1, DT_FP16));
    Term yh = term_new(0, TAG_TEN, DT_FP16, alloc_dt(d8, 1, DT_FP16));
    Term addf = uop_binary(UOP_ADD, xf, yf);
    Term addh = uop_binary(UOP_ADD, xh, yh);
    Term srf = materialize_store_root(addf);
    Term srh = materialize_store_root(addh);
    CHECK(srf != 0);
    CHECK(srh != 0);
    CHECK(uop_dag_content_hash(srf) != uop_dag_content_hash(srh));
  }

  TEST_BEGIN("content-hash/distinct-shape");
  {
    u32 d8[1]  = {8};
    u32 d32[1] = {32};
    Term x8 = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d8, 1));
    Term y8 = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d8, 1));
    Term x32 = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d32, 1));
    Term y32 = term_new(0, TAG_TEN, DT_FP32, alloc_f32(d32, 1));
    Term add8  = uop_binary(UOP_ADD, x8, y8);
    Term add32 = uop_binary(UOP_ADD, x32, y32);
    Term sr8  = materialize_store_root(add8);
    Term sr32 = materialize_store_root(add32);
    CHECK(sr8 != 0);
    CHECK(sr32 != 0);
    CHECK(uop_dag_content_hash(sr8) != uop_dag_content_hash(sr32));
  }

  // === (5) empty root -> 0 ==========================================
  TEST_BEGIN("content-hash/empty-root-zero");
  CHECK_EQ(uop_dag_content_hash(0), 0);

  thvm_free();
  TEST_REPORT();
}
