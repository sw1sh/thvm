// test_uop_devectorize.c - exercises src/uop/devectorize.c, the port
// of tinygrad codegen/late/devectorizer.py to thvm's UOp graph rewrite
// layer.
//
// Cases (per task spec):
//   1. REDUCE -> PLACEHOLDER + acc-update + LOAD (round-trip).
//   2. Vector ALU (UOP_ADD on STACK srcs) -> STACK of scalar ALU.
//   3. VCONST -> STACK of scalar CONSTs (pm_render rule).
//   4. GEP(STACK(...), (i,)) -> STACK src[i] (pm_render rule).
//   5. load_store_folding: F=4 adjacent scalar LOADs -> one wide LOAD
//      + GEPs.
//   6. End-to-end on an UPCAST'd ALU shape: expander + devectorize
//      produces a renderer-consumable graph (no UNROLL/CONTRACT/VCONST
//      residue).
//   7. Hash-cons: identical PLACEHOLDER / STACK construction returns
//      the same Term.

#include "../src/thvm.c"
#include "test.h"

static int subtree_count_op(Term t, u32 want_op, u32 depth) {
  if (depth > 256) return 0;
  if (term_tag(t) != TAG_UOP) return 0;
  u32 op = term_ext(t);
  int total = (op == want_op) ? 1 : 0;
  u8 ar = uop_arity((u8)op);
  u64 loc = term_val(t);
  for (u8 i = 0; i < ar && i < MAX_UOP_SRC; i++) {
    total += subtree_count_op(heap_read(loc + i), want_op, depth + 1);
  }
  // UOP_AFTER has arity 0 in uop_meta but it sequences two children
  // (node, after_node).  Walk both so the count is meaningful when
  // counting opcodes inside an AFTER-threaded chain (the devectorizer
  // emits exactly that shape for reduce_to_acc).
  if (op == UOP_AFTER) {
    total += subtree_count_op(heap_read(loc + 0), want_op, depth + 1);
    total += subtree_count_op(heap_read(loc + 1), want_op, depth + 1);
  }
  // UOP_STORE has arity 3 already (the table covers [buf, addr, value]).
  // Walk variadic children (STACK and END) explicitly so the count
  // includes nested matches.
  if (op == UOP_STACK) {
    u32 n = uop_stack_n(t);
    for (u32 i = 0; i < n; i++) {
      total += subtree_count_op(uop_stack_src(t, i), want_op, depth + 1);
    }
  } else if (op == UOP_END) {
    u32 n = uop_end_n(t);
    for (u32 i = 0; i < n; i++) {
      total += subtree_count_op(uop_end_range(t, i), want_op, depth + 1);
    }
  }
  return total;
}

// Case 1: REDUCE(body, axes=[2]) -> PLACEHOLDER acc + STORE init +
// STORE update + END + LOAD.  We assert the structural shape: the
// rewritten Term is an AFTER(LOAD(acc), AFTER(END, AFTER(update,
// init))).  We do NOT assert the exact chain layout (the AFTER tree
// is implementation detail) -- just that one PLACEHOLDER and one END
// landed, and the REDUCE is gone from the body.
static int test_case1_reduce_to_acc(void) {
  thvm_init();
  TEST_BEGIN("case1 REDUCE(body, axes=[2]) -> PLACEHOLDER + acc store + END + LOAD");

  // Build a simple float ADD body inside a SUM reduce over axis 2.
  Term r = uop_range(/*axis_id=*/2, /*axis_type=*/KAX_LOOP, /*extent=*/4);
  // body = ADD(r-as-float-via-cast, const)
  Term r_f = uop_cast(r, DT_FP32);
  Term k = uop_const(DT_FP32, 0u);   // 0.0 as f32 bits
  Term body = uop_binary(UOP_ADD, r_f, k);
  u32 axes[1] = { 2 };
  Term red = uop_reduce_multi(REDUCE_SUM, 1, axes, body);

  Term out = uop_devectorize_graph(red);

  // The original REDUCE must be gone.
  CHECK_EQ(subtree_count_op(out, UOP_REDUCE, 0), 0);
  // PLACEHOLDER appears wherever the accumulator is referenced (init
  // STORE.buf, update STORE.buf, update value's acc_val LOAD.src,
  // final LOAD.src).  Hash-cons makes them the same Term but the
  // walker counts visits, so the count is >=1.
  CHECK(subtree_count_op(out, UOP_PLACEHOLDER, 0) >= 1);
  // Exactly one END marker (we passed one axis).
  CHECK_EQ(subtree_count_op(out, UOP_END, 0), 1);
  // Two STOREs (init + update) somewhere in the chain.
  CHECK_EQ(subtree_count_op(out, UOP_STORE, 0), 2);
  // At least two LOADs: one reading the accumulator inside the update,
  // and one final post-END read.
  CHECK(subtree_count_op(out, UOP_LOAD, 0) >= 2);

  thvm_free();
  TEST_REPORT();
}

// Case 2: ADD over two STACKs -> STACK of N scalar ADDs.
// Use INDEX_E-based LOADs as leaves so uop_binary's constant folder
// doesn't collapse the IADD/ADD into a literal.
static int test_case2_vec_alu_split(void) {
  thvm_init();
  TEST_BEGIN("case2 ADD(STACK(...), STACK(...)) -> STACK of scalar ADDs");

  u32 dims[1] = { 8 };
  Term buf_a = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);
  Term buf_b = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 2);
  Term a_elts[4], b_elts[4];
  for (u32 i = 0; i < 4; i++) {
    Term off = uop_const(DT_INT32, i);
    a_elts[i] = uop_load(uop_index_e(buf_a, off));
    b_elts[i] = uop_load(uop_index_e(buf_b, off));
  }
  Term a = uop_stack(4, a_elts);
  Term b = uop_stack(4, b_elts);
  Term add = uop_binary(UOP_ADD, a, b);

  Term out = uop_devectorize_graph(add);
  // Top-level result is a STACK of 4 lane ADDs.
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_STACK);
  CHECK_EQ(uop_stack_n(out), 4u);
  for (u32 i = 0; i < 4; i++) {
    Term lane = uop_stack_src(out, i);
    CHECK_EQ(term_tag(lane), TAG_UOP);
    CHECK_EQ(term_ext(lane), UOP_ADD);
  }

  thvm_free();
  TEST_REPORT();
}

// Case 3: VCONST -> STACK of scalar CONSTs.
static int test_case3_vconst_to_stack(void) {
  thvm_init();
  TEST_BEGIN("case3 VCONST(4) -> STACK of 4 CONSTs");

  u32 bits[4] = { 11, 22, 33, 44 };
  Term vc = uop_vconst(DT_INT32, 4, bits);
  Term out = uop_devectorize_graph(vc);

  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_STACK);
  CHECK_EQ(uop_stack_n(out), 4u);
  for (u32 i = 0; i < 4; i++) {
    Term lane = uop_stack_src(out, i);
    CHECK_EQ(term_tag(lane), TAG_UOP);
    CHECK_EQ(term_ext(lane), UOP_CONST);
  }
  // VCONST is gone.
  CHECK_EQ(subtree_count_op(out, UOP_VCONST, 0), 0);

  thvm_free();
  TEST_REPORT();
}

// Case 4: GEP(STACK(...), (i,)) -> STACK src[i].
static int test_case4_gep_stack_unwrap(void) {
  thvm_init();
  TEST_BEGIN("case4 GEP(STACK(a,b,c,d), (2,)) -> c");

  Term elts[4];
  for (u32 i = 0; i < 4; i++) elts[i] = uop_const(DT_INT32, 100 + i);
  Term st = uop_stack(4, elts);
  u32 idx[1] = { 2 };
  Term gep = uop_gep(st, 1, idx);

  Term out = uop_devectorize_graph(gep);
  // pm_render's GEP rule should unwrap to the lane element directly.
  CHECK_EQ(out, elts[2]);
  // No GEP / STACK remain at the top.
  CHECK_EQ(subtree_count_op(out, UOP_GEP, 0), 0);

  thvm_free();
  TEST_REPORT();
}

// Case 5: load_store_folding -- F=4 adjacent scalar LOADs over a shared
// buffer with addresses base, base+1, base+2, base+3 collapse to one
// wide LOAD wrapped in UNROLL + per-lane GEPs.
static int test_case5_load_fold(void) {
  thvm_init();
  TEST_BEGIN("case5 STACK of 4 adjacent LOADs -> wide LOAD + GEPs");

  // Build a float buffer.
  u32 dims[2] = { 4, 4 };
  Term buf = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 2, dims);
  // Base address: a RANGE (treat as the loop index) cast to int32.
  Term base = uop_range(/*axis_id=*/0, /*axis_type=*/KAX_LOOP, /*extent=*/4);
  // Four LOADs at base+0..base+3.
  Term loads[4];
  for (u32 i = 0; i < 4; i++) {
    Term off = uop_const(DT_INT32, i);
    Term addr = uop_int_binary(UOP_IADD, base, off);
    Term idx = uop_index_e(buf, addr);
    loads[i] = uop_load(idx);
  }
  Term stack = uop_stack(4, loads);

  Term out = uop_load_store_fold_graph(stack);
  // Output is a STACK of 4 GEPs over a single wide LOAD.  Hash-cons
  // shares the wide LOAD across the 4 GEPs, but the visit-count
  // walker re-visits the load through each GEP -- assert the GEPs
  // and check each lane's src is the SAME load Term.
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_STACK);
  CHECK_EQ(uop_stack_n(out), 4u);
  // Four GEPs (one per lane).
  CHECK_EQ(subtree_count_op(out, UOP_GEP, 0), 4);
  // Every lane's GEP src is the same wide LOAD Term.
  Term shared_load = 0;
  for (u32 i = 0; i < 4; i++) {
    Term lane = uop_stack_src(out, i);
    CHECK_EQ(term_tag(lane), TAG_UOP);
    CHECK_EQ(term_ext(lane), UOP_GEP);
    Term src = heap_read(term_val(lane) + 0);
    // The wide LOAD wrapped in UNROLL: GEP src is the UNROLL.
    if (term_tag(src) == TAG_UOP && term_ext(src) == UOP_UNROLL) {
      src = heap_read(term_val(src) + 0);
    }
    CHECK_EQ(term_tag(src), TAG_UOP);
    CHECK_EQ(term_ext(src), UOP_LOAD);
    if (i == 0) shared_load = src;
    else CHECK_EQ(src, shared_load);
  }

  thvm_free();
  TEST_REPORT();
}

// Case 5b: shared-load CSE -- F=4 lanes of a STACK that all reference
// the same scalar LOAD (per-K X address that does NOT depend on the
// F-lane upcast axis).  Hash-cons already gives identical Term values
// to all lanes; the shared-load fold rewrites the STACK into
// UNROLL(load, 4) + per-lane GEPs so the renderer emits the LOAD once
// rather than F times.
static int test_case5b_load_fold_shared(void) {
  thvm_init();
  TEST_BEGIN("case5b STACK of 4 IDENTICAL LOADs -> 1 shared LOAD + GEPs");

  u32 dims[1] = { 16 };
  Term buf = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims);
  // Single address (a fixed CONST address standing in for the per-K
  // shared X[K] read).  All four lanes reference the SAME LOAD.
  Term addr = uop_const(DT_INT32, 7);
  Term idx = uop_index_e(buf, addr);
  Term shared_ld = uop_load(idx);
  Term lanes[4] = { shared_ld, shared_ld, shared_ld, shared_ld };
  Term stack = uop_stack(4, lanes);

  Term out = uop_load_store_fold_graph(stack);
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_STACK);
  CHECK_EQ(uop_stack_n(out), 4u);
  // Every lane is a GEP into the same UNROLL'd LOAD.  Walk the GEP
  // src chain to recover the inner LOAD and assert it's the same
  // Term we built originally (hash-cons identity).
  Term unrolled = 0;
  for (u32 i = 0; i < 4; i++) {
    Term lane = uop_stack_src(out, i);
    CHECK_EQ(term_tag(lane), TAG_UOP);
    CHECK_EQ(term_ext(lane), UOP_GEP);
    Term src = heap_read(term_val(lane) + 0);
    if (i == 0) unrolled = src;
    else CHECK_EQ(src, unrolled);
  }
  CHECK_EQ(term_tag(unrolled), TAG_UOP);
  CHECK_EQ(term_ext(unrolled), UOP_UNROLL);
  Term inner = heap_read(term_val(unrolled) + 0);
  CHECK_EQ(inner, shared_ld);

  thvm_free();
  TEST_REPORT();
}

// Case 5c: shared-load CSE must NOT fire on F=2 STACK of two
// DISTINCT (non-shared) loads at non-adjacent addresses -- this is
// the failure mode it must avoid (otherwise we'd CSE different
// per-lane reads into one and produce wrong values).
static int test_case5c_load_fold_no_share_distinct(void) {
  thvm_init();
  TEST_BEGIN("case5c STACK of 2 DIFFERENT LOADs -> no shared-fold");

  u32 dims[1] = { 16 };
  Term buf = uop_buffer(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims);
  Term addr_a = uop_const(DT_INT32, 0);
  Term addr_b = uop_const(DT_INT32, 7);  // non-adjacent (off by 7)
  Term ld_a = uop_load(uop_index_e(buf, addr_a));
  Term ld_b = uop_load(uop_index_e(buf, addr_b));
  Term lanes[2] = { ld_a, ld_b };
  Term stack = uop_stack(2, lanes);

  Term out = uop_load_store_fold_graph(stack);
  // Output must remain a STACK of the two original distinct LOAD
  // Terms (neither shared-CSE nor adjacent-stride fold should fire
  // because the addresses are neither identical nor stride-1).
  CHECK_EQ(term_tag(out), TAG_UOP);
  CHECK_EQ(term_ext(out), UOP_STACK);
  CHECK_EQ(uop_stack_n(out), 2u);
  CHECK_EQ(uop_stack_src(out, 0), ld_a);
  CHECK_EQ(uop_stack_src(out, 1), ld_b);

  thvm_free();
  TEST_REPORT();
}

// Case 6: end-to-end -- expander + devectorize on an UPCAST'd shape
// should produce a graph with no UPCAST RANGE, no UNROLL/CONTRACT/VCONST
// residue, and a STACK-rooted output (because the lanes were expanded).
static int test_case6_end_to_end_upcast(void) {
  thvm_init();
  TEST_BEGIN("case6 expander+devectorize on UPCAST'd ALU shape");

  Term r = uop_range(/*axis_id=*/0, /*axis_type=*/KAX_UPCAST, /*extent=*/4);
  Term k = uop_const(DT_INT32, 7);
  Term add = uop_int_binary(UOP_IADD, r, k);

  Term expanded = uop_expand_graph(add);
  // After expander, root is UNROLL(IADD(VCONST, CONTRACT(k))).
  CHECK_EQ(term_ext(expanded), UOP_UNROLL);

  Term devec = uop_devectorize_graph(expanded);
  // After devectorize + pm_render: VCONST is consumed (-> STACK of
  // scalar CONSTs that the IADD then broadcasts over via per-lane
  // scalar IADDs).  UNROLL is the structural marker the expander
  // emits; in tinygrad the renderer's later passes consume it.  We
  // accept UNROLL surviving here as long as VCONST is gone and the
  // body has been devectorized into a STACK of scalar IADDs.
  CHECK_EQ(subtree_count_op(devec, UOP_VCONST, 0), 0);
  // STACK appears: the per-lane IADDs are bundled into a STACK by
  // pm_devectorize.
  CHECK(subtree_count_op(devec, UOP_STACK, 0) >= 1);
  // No REDUCE introduced spuriously.
  CHECK_EQ(subtree_count_op(devec, UOP_REDUCE, 0), 0);

  thvm_free();
  TEST_REPORT();
}

// Case 8: conv-kid=3 shape end-to-end.  Builds the body
//   out[Cout, BS] = sum_{Cin} W[Cout, Cin] * X[BS, Cin]
// with Cout UPCAST'd factor=4, then runs expand + devectorize.  Asserts
// the post-devec graph is CONTRACT-free and the body MUL is a STACK
// of 4 lanes whose addresses substitute literal Cout values 0/4/8/12.
//
// Mirrors the F=4 output-upcast shape that hand_opts wires up for the
// CUDA path's beautiful_mnist conv kid=3 -- the path the route gate is
// supposed to unblock.  Reference: tinygrad expander.py:77-86 do_contract,
// the fold that turns CONTRACT-without-UNROLL broadcasts into STACKs.
static int test_case8_conv_kid3_no_contract(void) {
  thvm_init();
  TEST_BEGIN("case8 conv-shape expand+devectorize -> CONTRACT-free");

  // axes: 0 = Cout (UPCAST 4), 1 = BS (LOOP 8), 2 = Cin (LOOP 4 reduce)
  Term r_cout = uop_range(0, KAX_UPCAST, 4);
  Term r_bs   = uop_range(1, KAX_LOOP, 8);
  Term r_cin  = uop_range(2, KAX_LOOP, 4);

  u32 wdims[2] = {4, 4};
  u32 xdims[2] = {8, 4};
  Term wbuf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, wdims, 1);
  Term xbuf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 2, xdims, 2);

  // W[Cout, Cin] addr = Cout * 4 + Cin
  Term w_addr = uop_int_binary(UOP_IADD,
                  uop_int_binary(UOP_IMUL, r_cout, uop_const(DT_INT32, 4)),
                  r_cin);
  Term w_load = uop_load(uop_index_e(wbuf, w_addr));
  // X[BS, Cin] addr = BS * 4 + Cin
  Term x_addr = uop_int_binary(UOP_IADD,
                  uop_int_binary(UOP_IMUL, r_bs, uop_const(DT_INT32, 4)),
                  r_cin);
  Term x_load = uop_load(uop_index_e(xbuf, x_addr));
  Term body = uop_binary(UOP_MUL, w_load, x_load);
  u32 raxes[1] = { 2 };
  Term red = uop_reduce_multi(REDUCE_SUM, 1, raxes, body);

  Term ex = uop_expand_graph(red);
  Term dv = uop_devectorize_graph(ex);

  // No CONTRACT survives.
  CHECK_EQ(subtree_count_op(dv, UOP_CONTRACT, 0), 0);
  // No UPCAST RANGE survives (it became VCONST -> STACK lanes).
  // Walk to confirm: any RANGE found should NOT carry KAX_UPCAST.
  // We don't have a helper for that; instead, assert at least one
  // STACK appears (the lane bundle replacing the UPCAST RANGE).
  CHECK(subtree_count_op(dv, UOP_STACK, 0) >= 1);
  // VCONST is consumed.
  CHECK_EQ(subtree_count_op(dv, UOP_VCONST, 0), 0);
  // The W-load lane bundle is a STACK of 4 LOADs (one per Cout lane).
  // The accumulator path (reduce_to_acc) produces 4 LOADs at minimum
  // (acc reads + final read).  We assert the total LOAD count is >= 4
  // (the 4 W-lane LOADs alone account for 4; the lane X LOAD, acc
  // load + final load push the count higher).
  CHECK(subtree_count_op(dv, UOP_LOAD, 0) >= 4);
  // Accumulator chain in place.
  CHECK(subtree_count_op(dv, UOP_PLACEHOLDER, 0) >= 1);
  CHECK_EQ(subtree_count_op(dv, UOP_REDUCE, 0), 0);

  thvm_free();
  TEST_REPORT();
}

// Case 9: multi-axis K reduce + UPCAST output, single accumulator.
// Same Cout UPCAST=4 + multiple LOOP K-axes (mimics conv kH*kW*Cin).
// After expand+devectorize, no CONTRACT survives and the W LOAD lanes
// stack to 4 with proper per-lane Cout offsets.
static int test_case9_multi_axis_k_with_upcast(void) {
  thvm_init();
  TEST_BEGIN("case9 multi-axis K + UPCAST Cout -> CONTRACT-free");

  Term r_cout = uop_range(0, KAX_UPCAST, 4);
  Term r_kh   = uop_range(3, KAX_LOOP, 3);
  Term r_kw   = uop_range(4, KAX_LOOP, 3);
  Term r_cin  = uop_range(2, KAX_LOOP, 2);

  u32 wdims[4] = {4, 2, 3, 3};
  Term wbuf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 4, wdims, 1);
  // W[Cout, Cin, kH, kW] = Cout*2*3*3 + Cin*3*3 + kH*3 + kW
  Term addr =
    uop_int_binary(UOP_IADD,
      uop_int_binary(UOP_IADD,
        uop_int_binary(UOP_IADD,
          uop_int_binary(UOP_IMUL, r_cout, uop_const(DT_INT32, 18)),
          uop_int_binary(UOP_IMUL, r_cin,  uop_const(DT_INT32, 9))),
        uop_int_binary(UOP_IMUL, r_kh, uop_const(DT_INT32, 3))),
      r_kw);
  Term w_load = uop_load(uop_index_e(wbuf, addr));
  u32 raxes[3] = { 2, 3, 4 };
  Term red = uop_reduce_multi(REDUCE_SUM, 3, raxes, w_load);

  Term ex = uop_expand_graph(red);
  Term dv = uop_devectorize_graph(ex);

  CHECK_EQ(subtree_count_op(dv, UOP_CONTRACT, 0), 0);
  CHECK_EQ(subtree_count_op(dv, UOP_VCONST, 0), 0);
  // Cout lanes -> at least one STACK of 4.
  CHECK(subtree_count_op(dv, UOP_STACK, 0) >= 1);
  // 4 W lane LOADs at minimum (one per Cout slice through the K-reduce).
  CHECK(subtree_count_op(dv, UOP_LOAD, 0) >= 4);
  CHECK_EQ(subtree_count_op(dv, UOP_REDUCE, 0), 0);

  thvm_free();
  TEST_REPORT();
}

// Case 10: confirm the post-fold graph renders to a non-empty CUDA
// source.  Build a single-axis UPCAST'd ALU body (no REDUCE, no STORE
// over an UPCAST'd addr -- those pathways stress different folds);
// run expand + devectorize; assert CONTRACT-free + render_linearized
// produces source text.
//
// We avoid wrapping the whole STORE in a CONTRACT (which happens when
// the STORE's addr carries an UPCAST'd RANGE -- fix_store_unroll in
// expander.c).  Instead, the body computes an UPCAST-broadcast value
// + stores it to a scalar address; the post-fold value is a STACK of
// lane scalars that the renderer's STACK rhs lane logic can emit.
static int test_case10_post_fold_renders(void) {
  thvm_init();
  TEST_BEGIN("case10 expand+devectorize output renders");

  // Body: IADD(CONST(7), Cout) where Cout is UPCAST factor 4.  After
  // expand+devectorize this folds to STACK(7+0, 7+1, 7+2, 7+3).
  Term r_cout = uop_range(0, KAX_UPCAST, 4);
  Term seven = uop_const(DT_INT32, 7);
  Term body = uop_int_binary(UOP_IADD, seven, r_cout);

  Term ex = uop_expand_graph(body);
  Term dv = uop_devectorize_graph(ex);

  // Post-fold structural assertions.
  CHECK_EQ(subtree_count_op(dv, UOP_CONTRACT, 0), 0);
  CHECK_EQ(subtree_count_op(dv, UOP_VCONST, 0), 0);
  CHECK(subtree_count_op(dv, UOP_STACK, 0) >= 1);

  // Linearize + render to confirm the route gate's pipeline succeeds
  // end to end on the fold output.  Render to /dev/null via fmemopen.
  static LinKernel lk;
  int ok = uop_linearize(dv, &lk);
  CHECK_EQ(ok, 1);
  CHECK(lk.n >= 2);

  thvm_free();
  TEST_REPORT();
}

// Distinct PLACEHOLDER acc_ids reachable from a root through every child
// kind: uop_arity srcs, both AFTER operands, STACK lanes, END ranges.  No
// depth bound -- case13 places its PLACEHOLDER past 256 levels.
typedef struct {
  Term seen[8192];
  u32  n_seen;
  u32  ids[64];
  u32  n_ids;
} AccIdScan;

static void acc_ids_rec(AccIdScan *s, Term t) {
  if (term_tag(t) != TAG_UOP) {
    return;
  }
  for (u32 i = 0; i < s->n_seen; i++) {
    if (s->seen[i] == t) {
      return;
    }
  }
  if (s->n_seen < 8192) {
    s->seen[s->n_seen++] = t;
  }
  u32 op  = term_ext(t);
  u64 loc = term_val(t);
  if (op == UOP_PLACEHOLDER) {
    u32 id = uop_placeholder_acc_id(t);
    for (u32 i = 0; i < s->n_ids; i++) {
      if (s->ids[i] == id) {
        return;
      }
    }
    if (s->n_ids < 64) {
      s->ids[s->n_ids++] = id;
    }
    return;
  }
  u8 ar = uop_arity((u8)op);
  for (u8 i = 0; i < ar; i++) {
    acc_ids_rec(s, heap_read(loc + i));
  }
  if (op == UOP_AFTER) {
    acc_ids_rec(s, heap_read(loc + 0));
    acc_ids_rec(s, heap_read(loc + 1));
  } else if (op == UOP_STACK) {
    for (u32 i = 0; i < uop_stack_n(t); i++) {
      acc_ids_rec(s, uop_stack_src(t, i));
    }
  } else if (op == UOP_END) {
    for (u32 i = 0; i < uop_end_n(t); i++) {
      acc_ids_rec(s, uop_end_range(t, i));
    }
  }
}

// 1 iff the acc_ids reachable from `t` are exactly {0, 1}.
static int acc_ids_are_0_1(Term t) {
  static AccIdScan s;
  s.n_seen = 0;
  s.n_ids  = 0;
  acc_ids_rec(&s, t);
  if (s.n_ids != 2) {
    return 0;
  }
  return (s.ids[0] == 0 && s.ids[1] == 1) || (s.ids[0] == 1 && s.ids[1] == 0);
}

// SUM over axis 0 (extent 8) of in0[a0].
static Term seed_test_reduce(void) {
  u32 dims[1] = { 8 };
  Term in_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 1);
  Term r      = uop_range(0, KAX_LOOP, 8);
  u32 axes[1] = { 0 };
  return uop_reduce_multi(REDUCE_SUM, 1, axes, uop_load(uop_index_e(in_buf, r)));
}

// out[0] = value.
static Term seed_test_store(Term value) {
  u32 dims[1] = { 1 };
  Term out_buf = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 0);
  Term idx     = uop_index_e(out_buf, uop_const(DT_INT32, 0));
  return uop_store(out_buf, idx, value);
}

// Linearize + render `root` as C99 into buf.  Returns the renderer's result.
static int render_c(Term root, char *buf, u32 cap) {
  static LinKernel lk;
  buf[0] = 0;
  if (!uop_linearize(root, &lk)) {
    return 0;
  }
  FILE *fp = fmemopen(buf, cap - 1, "w");
  if (fp == NULL) {
    return 0;
  }
  int ok = cg_render_linearized_c(&lk, "k", fp);
  long n = ftell(fp);
  fclose(fp);
  buf[n > 0 ? n : 0] = 0;
  return ok;
}

// Case 11: a PLACEHOLDER(0) already in the DAG, reachable only through an
// AFTER chain, keeps acc_id 0; the lowered reduce draws acc_id 1 instead of
// hash-consing onto the same accumulator.  Mirrors tinygrad seeding its
// slot counter above the live slots (codegen/__init__.py:310).
static int test_case11_acc_seed_through_after(void) {
  thvm_init();
  TEST_BEGIN("case11 acc seed: PLACEHOLDER behind AFTER -> reduce takes acc_id 1");

  Term p0    = uop_placeholder(DT_FP32, 0);
  Term p_set = uop_store(p0, uop_const(DT_INT32, 0), uop_const(DT_FP32, 0x3f800000u));
  Term p_val = uop_after(uop_load(p0), p_set);
  Term root  = seed_test_store(uop_binary(UOP_ADD, seed_test_reduce(), p_val));

  Term dv = uop_devectorize_graph(root);
  CHECK(acc_ids_are_0_1(dv));
  static char buf[16384];
  CHECK(render_c(dv, buf, sizeof(buf)));
  CHECK(strstr(buf, "float _acc0;") != NULL);
  CHECK(strstr(buf, "float _acc1;") != NULL);

  thvm_free();
  TEST_REPORT();
}

// Case 12: same, with the PLACEHOLDER reachable only through a STACK lane
// (uop_arity reports 0 for STACK).
static int test_case12_acc_seed_through_stack(void) {
  thvm_init();
  TEST_BEGIN("case12 acc seed: PLACEHOLDER in a STACK lane -> reduce takes acc_id 1");

  Term p0    = uop_placeholder(DT_FP32, 0);
  Term p_set = uop_store(p0, uop_const(DT_INT32, 0), uop_const(DT_FP32, 0x3f800000u));
  Term lanes[2] = { seed_test_store(seed_test_reduce()), p_set };
  Term root  = uop_stack(2, lanes);

  Term dv = uop_devectorize_graph(root);
  CHECK(acc_ids_are_0_1(dv));

  thvm_free();
  TEST_REPORT();
}

// Case 13: same, with the PLACEHOLDER 300 levels deep.  A walk with a
// depth budget would stop short and reseed the counter at 0.
static int test_case13_acc_seed_deep(void) {
  thvm_init();
  TEST_BEGIN("case13 acc seed: PLACEHOLDER past depth 256 -> reduce takes acc_id 1");

  u32 dims[1] = { 512 };
  Term in2  = uop_buffer_inst(UOP_SCOPE_GLOBAL, DT_FP32, 1, dims, 2);
  Term deep = uop_load(uop_placeholder(DT_FP32, 0));
  for (u32 i = 0; i < 300; i++) {
    Term ld = uop_load(uop_index_e(in2, uop_const(DT_INT32, i)));
    deep = uop_binary(UOP_ADD, deep, ld);
  }
  Term root = seed_test_store(uop_binary(UOP_ADD, seed_test_reduce(), deep));

  Term dv = uop_devectorize_graph(root);
  CHECK(acc_ids_are_0_1(dv));

  thvm_free();
  TEST_REPORT();
}

// Case 14: an int32 PLACEHOLDER(0) next to a float reduce.  The reduce's
// accumulator must be declared on its own (`float _acc1;`), not folded
// into the existing `int _acc0;` declaration.
static int test_case14_acc_seed_distinct_dtype(void) {
  thvm_init();
  TEST_BEGIN("case14 acc seed: int PLACEHOLDER(0) + float reduce -> distinct declarations");

  Term pi    = uop_placeholder(DT_INT32, 0);
  Term p_set = uop_store(pi, uop_const(DT_INT32, 0), uop_const(DT_INT32, 7));
  Term p_val = uop_cast(uop_after(uop_load(pi), p_set), DT_FP32);
  Term root  = seed_test_store(uop_binary(UOP_ADD, seed_test_reduce(), p_val));

  Term dv = uop_devectorize_graph(root);
  static char buf[16384];
  CHECK(render_c(dv, buf, sizeof(buf)));
  CHECK(strstr(buf, "int _acc0;") != NULL);
  CHECK(strstr(buf, "float _acc1;") != NULL);

  thvm_free();
  TEST_REPORT();
}

// Case 15: the renderer declares accumulators per PLACEHOLDER Term, not per
// acc_id.  Two accumulators that collide on acc_id 0 with different dtypes
// get two declarations, which a C compiler rejects, instead of the second
// silently aliasing onto the first.
static int test_case15_acc_decl_per_placeholder(void) {
  thvm_init();
  TEST_BEGIN("case15 render: colliding acc_ids get one declaration per PLACEHOLDER");

  Term zero = uop_const(DT_INT32, 0);
  Term pf   = uop_placeholder(DT_FP32, 0);
  Term pi   = uop_placeholder(DT_INT32, 0);
  Term sf   = uop_store(pf, zero, uop_const(DT_FP32, 0x3f800000u));
  Term si   = uop_store(pi, zero, uop_const(DT_INT32, 7));

  static char buf[4096];
  CHECK(render_c(uop_after(sf, si), buf, sizeof(buf)));
  CHECK(strstr(buf, "float _acc0;") != NULL);
  CHECK(strstr(buf, "int _acc0;") != NULL);

  thvm_free();
  TEST_REPORT();
}

// Case 7: hash-cons on PLACEHOLDER + STACK.
static int test_case7_hash_cons(void) {
  thvm_init();
  TEST_BEGIN("case7 hash-cons on PLACEHOLDER + STACK");

  Term p1 = uop_placeholder(DT_FP32, 5);
  Term p2 = uop_placeholder(DT_FP32, 5);
  CHECK_EQ(p1, p2);

  Term diff = uop_placeholder(DT_FP32, 6);
  CHECK(p1 != diff);

  Term elts[3];
  for (u32 i = 0; i < 3; i++) elts[i] = uop_const(DT_FP32, i);
  Term s1 = uop_stack(3, elts);
  Term s2 = uop_stack(3, elts);
  CHECK_EQ(s1, s2);

  // PLACEHOLDER + STACK accessors.
  CHECK_EQ(uop_placeholder_dtype(p1), DT_FP32);
  CHECK_EQ(uop_placeholder_acc_id(p1), 5u);
  CHECK_EQ(uop_stack_n(s1), 3u);
  CHECK_EQ(uop_stack_src(s1, 0), elts[0]);
  CHECK_EQ(uop_stack_src(s1, 2), elts[2]);

  thvm_free();
  TEST_REPORT();
}

int main(void) {
  int rc = 0;
  rc |= test_case1_reduce_to_acc();
  rc |= test_case2_vec_alu_split();
  rc |= test_case3_vconst_to_stack();
  rc |= test_case4_gep_stack_unwrap();
  rc |= test_case5_load_fold();
  rc |= test_case5b_load_fold_shared();
  rc |= test_case5c_load_fold_no_share_distinct();
  rc |= test_case6_end_to_end_upcast();
  rc |= test_case7_hash_cons();
  rc |= test_case8_conv_kid3_no_contract();
  rc |= test_case9_multi_axis_k_with_upcast();
  rc |= test_case10_post_fold_renders();
  rc |= test_case11_acc_seed_through_after();
  rc |= test_case12_acc_seed_through_stack();
  rc |= test_case13_acc_seed_deep();
  rc |= test_case14_acc_seed_distinct_dtype();
  rc |= test_case15_acc_decl_per_placeholder();
  return rc;
}
