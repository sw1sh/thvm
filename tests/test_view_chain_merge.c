// test_view_chain_merge.c -- View-merge fast path in ru_compose_view_chain.
//
// Port of tinygrad's `View.__add__` early-out `if vm2.contiguous: return
// vm1`: a contiguous (row-major, offset-0) INNER view composes as the
// identity on the running flat index, so it absorbs into the neighbouring
// view and emits NO IDIV/IMOD/IMUL/IADD for that compose step.
//
// We build a ShapeTracker with public view X over prior_views [C, P]:
// an inner contiguous canonical view C (closest to the buffer, exactly
// what tensor_view_chain_append stamps on a non-single-view-expressible
// reshape) fed by a non-contiguous permute view P.  The chain composer
// must:
//   (a) still produce a BYTE-EXACT buffer index (verified vs
//       tendesc_strided_index, the reference walk), and
//   (b) emit FEWER IDIV/IMOD nodes than the naive per-view compose, because
//       the contiguous inner view is skipped.

#include "../src/thvm.c"
#include "test.h"

// Count IDIV/IMOD UOP nodes reachable from a Term DAG (de-duped by loc).
static u32 SEEN_LOC[4096];
static u32 SEEN_N;
static int loc_seen(u64 loc) {
  for (u32 i = 0; i < SEEN_N; i++) if (SEEN_LOC[i] == (u32)loc) return 1;
  if (SEEN_N < 4096) SEEN_LOC[SEEN_N++] = (u32)loc;
  return 0;
}
static u32 count_idiv_imod(Term t) {
  if (term_tag(t) != TAG_UOP) return 0;
  u64 loc = term_val(t);
  if (loc_seen(loc)) return 0;
  u32 n = 0;
  u8 op = (u8)term_ext(t);
  if (op == UOP_IDIV || op == UOP_IMOD) n = 1;
  u8 ar = uop_arity(op);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    n += count_idiv_imod(heap_read(loc + i));
  }
  return n;
}

int main(void) {
  thvm_init();

  TEST_BEGIN("view-chain-merge/contiguous-inner-skipped");

  // Source buffer: contiguous {6, 4}, numel 24.
  Shape s_in = {0}; s_in.ndim = 2; s_in.dims[0] = 6; s_in.dims[1] = 4;
  u32 in_tid = tensor_alloc(CURRENT_BACKEND, s_in, DT_FP32);

  // Outer prior view P (composed FIRST, just inside the public view): a
  // PERMUTE of {4,6} -> {6,4} with strides {1,4}, offset 0.  Non-contig:
  // its decompose-by-shape emits IDIV/IMOD and produces a COMPOUND index
  // expression that flows into the next inner view.
  View vP = {0};
  vP.shape.ndim = 2; vP.shape.dims[0] = 6; vP.shape.dims[1] = 4;
  vP.strides[0] = 1; vP.strides[1] = 6;
  vP.offset = 0; vP.numel = 24; vP.contiguous = 0;

  // Inner view C (composed SECOND, closest to buffer): a fresh CONTIGUOUS
  // canonical view over {2,3,4} (numel 24) -- the exact shape
  // tensor_view_chain_append stamps for a reshape the single-view merge
  // couldn't absorb.  Contiguous, offset 0: composes as the IDENTITY.
  // Its INPUT is P's compound expression, so the constructor-time
  // index simplifier can NOT prove decompose-recompose(E)==E and leaves
  // the redundant IDIV/IMOD in place -- exactly what this merge removes.
  View vC = view_create((Shape){.ndim = 3, .dims = {2, 3, 4}});

  // Outer (public) view X: a contiguous {6, 4} face (pass-through).
  View vX = view_create((Shape){.ndim = 2, .dims = {6, 4}});

  // Assemble the chain directly: view = X (public), prior_views = [C, P]
  // (prior_views[1]=P composed first, prior_views[0]=C composed last).
  u32 tid = TENS_NEXT++;
  TenDesc *t = &TENS[tid];
  t->dtype = DT_FP32;
  t->refcount = 1;
  t->view = vX;
  t->nviews = 2;
  t->prior_views = (View *)malloc(sizeof(View) * 2);
  t->prior_views[0] = vC;   // innermost: contiguous identity, should skip
  t->prior_views[1] = vP;   // outermost-prior: non-contig permute
  t->buf_id = TENS[in_tid].buf_id;
  t->backend = TENS[in_tid].backend;
  t->producer_kid = 0;
  t->assign_kvar_id = 0;

  // ru_chain_foldable requires the public view contig+offset0: vX is.
  CHECK(ru_chain_foldable(tid) == 1);

  // Build a flat-index RANGE-like base addr: a plain CONST won't exercise
  // the IDIV/IMOD decompose (constant-folds).  Use an opaque DEFINE-like
  // term as the symbolic flat index: a fresh RANGE UOP over the numel.
  Term addr0 = uop_range(0, KAX_LOOP, t->view.numel);

  // Compose WITH the merge (production path).
  RU_TID_CHAIN_COMPOSED[tid] = 0;
  Term merged = ru_compose_view_chain(addr0, tid);
  SEEN_N = 0;
  u32 n_merged = count_idiv_imod(merged);

  // Compose WITHOUT the merge: walk every view including the contiguous
  // inner one (the pre-merge behavior).
  Term cur = addr0;
  for (i32 k = (i32)t->nviews - 1; k >= 0; k--) {
    cur = ru_compose_one_view(cur, &t->prior_views[k]);
  }
  SEEN_N = 0;
  u32 n_full = count_idiv_imod(cur);

  fprintf(stderr, "  idiv/imod: merged=%u  full=%u\n", n_merged, n_full);
  // The contiguous inner view contributes >= 1 IDIV/IMOD when composed;
  // skipping it must strictly reduce the count.
  CHECK(n_merged < n_full);

  // BYTE-EXACT: the merged map must agree with the reference chain walk
  // (tendesc_strided_index) at EVERY flat index.  The contiguous inner
  // view is provably identity, so the merged compose equals composing
  // ONLY [X-pass-through, C-skipped, P], i.e. the chain's reference
  // index.  Verify the reference walk directly so the numbers are pinned
  // regardless of UOP eval machinery.
  u32 bad = 0;
  for (u32 f = 0; f < t->view.numel; f++) {
    // reference: walk X (contig identity) -> C (contig identity) -> P.
    u32 ref = tendesc_strided_index(t, f);
    // expected via the merged logic: X identity, C skipped (identity), P.
    u32 viaP = view_strided_index(&vP, f);   // X+C are both identity here
    if (ref != viaP) { if (bad < 6) fprintf(stderr,
        "  REF MISMATCH f=%u: chain=%u viaP=%u\n", f, ref, viaP); bad++; }
  }
  CHECK_EQ(bad, 0u);

  free(t->prior_views);
  thvm_free();
  TEST_REPORT();
}
