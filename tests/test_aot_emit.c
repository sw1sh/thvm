// tests/test_aot_emit.c
//
// Smoke test for the Phase 2 emitter.  Each iter of Phase 2 grows
// what this test verifies in the emitted source string.
//
// iter 1 (now): just confirm the emitter runs, returns a non-NULL
// string, and the string contains the symbol names + dispatch
// switch we expect.  Doesn't compile or run the source -- that
// lands in iter 4 (clang + dlopen pipeline).

#include "../src/thvm.c"
#include "test.h"

static u32 def_a_simple(u32 slot) {
  // Register a trivial NUM body into a known def slot.  Body shape
  // isn't relevant for iter 1 (the emitter ignores it); we just
  // need DEFS[slot] != 0.  Using NUM keeps thvm_book_from_dynamic
  // out of the LAM-cloning code path that needs a heap-side LAM.
  Term body = term_new(0, TAG_NUM, DT_INT32, 42);
  thvm_def_register(slot, body);
  return slot;
}

int main(void) {
  thvm_init();
  setvbuf(stdout, NULL, _IONBF, 0);

  TEST_BEGIN("emit returns non-NULL on a valid def");
  {
    u32 id = def_a_simple(1);
    char *src = thvm_aot_emit_program(id, "trivial");
    CHECK(src != NULL);
    CHECK(strlen(src) > 0);
    free(src);
  }

  TEST_BEGIN("emit returns NULL on an invalid def_id");
  {
    char *src = thvm_aot_emit_program(DEFS_CAP + 5, "nope");
    CHECK_EQ((u64)src, (u64)NULL);
  }

  TEST_BEGIN("emit returns NULL on an unbound def slot");
  {
    char *src = thvm_aot_emit_program(99, "unbound_99");
    CHECK_EQ((u64)src, (u64)NULL);
  }

  TEST_BEGIN("emit output contains the expected symbol structure");
  {
    u32 id = def_a_simple(2);
    char *src = thvm_aot_emit_program(id, "treesum_x");
    CHECK(src != NULL);
    CHECK(strstr(src, "FN_treesum_x")                   != NULL);
    CHECK(strstr(src, "par_treesum_x_entry")            != NULL);
    CHECK(strstr(src, "aot_program_treesum_x_dispatch") != NULL);
    CHECK(strstr(src, "aot_program_treesum_x_register") != NULL);
    free(src);
  }

  TEST_BEGIN("emit handles App-of-Mat: if-tree dispatch on args[0]");
  {
    // Build a TLam[a, TMatChain[<|0 -> NUM(11), 1 -> NUM(22)|>,
    //                            default][a]] body DIRECTLY in BOOK_HEAP
    // and stash it in DEFS[3].  Skipping thvm_def_register because
    // its internal cloner (thvm_book_from_dynamic) expects the
    // input Term to live in HEAP, not BOOK_HEAP.

    // chain: outer MAT(ext=0) -> [handler=NUM(11), inner MAT]
    //         inner MAT(ext=1) -> [handler=NUM(22), default=NUM(99)]
    Term default_arm = term_new(0, TAG_NUM, DT_INT32, 99);
    Term h_inner     = term_new(0, TAG_NUM, DT_INT32, 22);
    u64 inner_loc    = book_alloc(2);
    book_set(inner_loc + 0, h_inner);
    book_set(inner_loc + 1, default_arm);
    Term mat_inner   = term_new(0, TAG_MAT, 1, inner_loc);

    Term h_outer     = term_new(0, TAG_NUM, DT_INT32, 11);
    u64 outer_loc    = book_alloc(2);
    book_set(outer_loc + 0, h_outer);
    book_set(outer_loc + 1, mat_inner);
    Term mat_outer   = term_new(0, TAG_MAT, 0, outer_loc);

    // App(mat_outer, TVar(lam_loc))
    u64 lam_loc      = book_alloc(1);
    Term var_a       = term_new(0, TAG_VAR, 0, lam_loc);
    u64 app_loc      = book_alloc(2);
    book_set(app_loc + 0, mat_outer);
    book_set(app_loc + 1, var_a);
    Term app         = term_new(0, TAG_APP, 0, app_loc);
    book_set(lam_loc, app);
    Term lam         = term_new(0, TAG_LAM, 0, lam_loc);

    DEFS[3] = lam;   // direct stash; bypasses the dynamic cloner
    char *src = thvm_aot_emit_program(3, "appmat");
    CHECK(src != NULL);
    // The emit should mention args[0] dispatch + both match values.
    // Phase 3 iter B: dv-fast-path -- hot inputs (CTR/NUM) skip the
    // wnf call entirely; non-WHNF tags (DP*/REF/...) fall through to
    // wnf(dv).  Match both lines so a future tweak to the spacing
    // doesn't silently regress the optimisation.
    CHECK(strstr(src, "Term dv = t->args[0];") != NULL);
    CHECK(strstr(src, "if (term_tag(dv) != TAG_CTR && term_tag(dv) != TAG_NUM) dv = cnf(dv);") != NULL);
    CHECK(strstr(src, "term_val(dv) == 0u")    != NULL);
    CHECK(strstr(src, "term_ext(dv) == 0u")    != NULL);
    CHECK(strstr(src, "term_val(dv) == 1u")    != NULL);
    CHECK(strstr(src, "term_ext(dv) == 1u")    != NULL);
    CHECK(strstr(src, "default arm")           != NULL);
    // iter 3: arms now emit their handlers as real value exprs.
    // The 0-arm handler is NUM(11), the 1-arm handler is NUM(22),
    // default is NUM(99).  Each should appear as a `term_new(0,
    // TAG_NUM, ..., N)` construction inside the arm.
    CHECK(strstr(src, "TAG_NUM, 5, 11u")       != NULL);
    CHECK(strstr(src, "TAG_NUM, 5, 22u")       != NULL);
    CHECK(strstr(src, "TAG_NUM, 5, 99u")       != NULL);
    free(src);
  }

  TEST_BEGIN("emit recognises saturated self-call -> R_CALL");
  {
    // Build  TLam[a, TLam[b, TMatChain[<|0 -> TVar(b)|>,
    //                           TApp[TApp[TRef[5], TVar(b)], TVar(a)]
    //                       ][a]]]
    // i.e., args[0]=a, args[1]=b ; default arm is self_call(b, a).
    // Self-id is 5 (def slot we'll register into), arity 2.
    // Expected: emit `aot_make_call(aot_make_task(FN_selfcall,
    // t->ret, t->args[1], t->args[0], 0, 0))` in the default arm.

    u64 lam_b_loc = book_alloc(1);
    Term var_b    = term_new(0, TAG_VAR, 0, lam_b_loc);
    u64 lam_a_loc = book_alloc(1);
    Term var_a    = term_new(0, TAG_VAR, 0, lam_a_loc);

    // App(App(REF[5], TVar(b)), TVar(a))
    Term ref      = term_new(0, TAG_REF, 5, 0);
    u64 app1_loc  = book_alloc(2);
    book_set(app1_loc + 0, ref);
    book_set(app1_loc + 1, var_b);
    Term app1     = term_new(0, TAG_APP, 0, app1_loc);
    u64 app2_loc  = book_alloc(2);
    book_set(app2_loc + 0, app1);
    book_set(app2_loc + 1, var_a);
    Term app2     = term_new(0, TAG_APP, 0, app2_loc);

    // mat: 0-arm handler = TVar(b) ; default = self-call expr
    u64 mat_loc   = book_alloc(2);
    book_set(mat_loc + 0, var_b);
    book_set(mat_loc + 1, app2);
    Term mat      = term_new(0, TAG_MAT, 0, mat_loc);

    // outer: App(mat, TVar(a))
    u64 outer_app = book_alloc(2);
    book_set(outer_app + 0, mat);
    book_set(outer_app + 1, var_a);
    Term outer    = term_new(0, TAG_APP, 0, outer_app);
    book_set(lam_b_loc, outer);
    Term lam_b    = term_new(0, TAG_LAM, 0, lam_b_loc);
    book_set(lam_a_loc, lam_b);
    Term lam_a    = term_new(0, TAG_LAM, 0, lam_a_loc);

    DEFS[5] = lam_a;
    char *src = thvm_aot_emit_program(5, "selfcall");
    CHECK(src != NULL);
    CHECK(strstr(src, "aot_make_call(aot_make_task(") != NULL);
    CHECK(strstr(src, "FN_selfcall, t->ret")          != NULL);
    // Iter 10 inlines TVar refs.  The two args (b, then a) appear
    // as `t->args[1]` / `t->args[0]` directly inside the
    // aot_make_task call list, no v_K indirection.
    CHECK(strstr(src, "t->args[1]") != NULL);
    CHECK(strstr(src, "t->args[0]") != NULL);
    free(src);
  }

  TEST_BEGIN("emit recognises sibling-pair TCtr2 -> R_SPLIT + cont");
  {
    // Build a tree-sum-build-shaped def:
    //   TLam[d, TLam[x,
    //     TMatChain[<|0 -> TCtr[1, TVar(x)]|>,                   (* leaf *)
    //                TLam[d_1,
    //                  TCtr[2,                                    (* node *)
    //                       App(App(TRef[6], TVar(d_1)), TVar(x)),
    //                       App(App(TRef[6], TVar(d_1)), TVar(x))]
    //                ]
    //              ][d]]]
    // Self-id = 6, arity = 2.  Default arm has TLam[d_1, TCtr2(node, ...)].
    // Both TCtr2 children are saturated self-calls -> SPLIT.

    u64 lam_d_loc   = book_alloc(1);
    Term var_d      = term_new(0, TAG_VAR, 0, lam_d_loc);
    u64 lam_x_loc   = book_alloc(1);
    Term var_x      = term_new(0, TAG_VAR, 0, lam_x_loc);
    u64 lam_d1_loc  = book_alloc(1);
    Term var_d1     = term_new(0, TAG_VAR, 0, lam_d1_loc);

    // 0-arm: leaf{x} = TCtr[1, x]
    u64 leaf_loc    = book_alloc(2);
    book_set(leaf_loc + 0, term_new(0, TAG_NUM, DT_INT32, 1));
    book_set(leaf_loc + 1, var_x);
    Term leaf       = term_new(0, TAG_CTR, 1, leaf_loc);

    // sibling call: App(App(REF[6], TVar(d_1)), TVar(x))
    Term ref        = term_new(0, TAG_REF, 6, 0);
    u64 a1_loc      = book_alloc(2);
    book_set(a1_loc + 0, ref);
    book_set(a1_loc + 1, var_d1);
    Term a1         = term_new(0, TAG_APP, 0, a1_loc);
    u64 a2_loc      = book_alloc(2);
    book_set(a2_loc + 0, a1);
    book_set(a2_loc + 1, var_x);
    Term call       = term_new(0, TAG_APP, 0, a2_loc);

    // node{call, call} = TCtr[2, call, call]  (use same call for both
    // sides -- still recognised as two self-calls)
    u64 node_loc    = book_alloc(3);
    book_set(node_loc + 0, term_new(0, TAG_NUM, DT_INT32, 2));
    book_set(node_loc + 1, call);
    book_set(node_loc + 2, call);
    Term node_ctr   = term_new(0, TAG_CTR, 2, node_loc);

    // default arm: TLam[d_1, node]
    book_set(lam_d1_loc, node_ctr);
    Term def_lam    = term_new(0, TAG_LAM, 0, lam_d1_loc);

    // mat: 0 -> leaf, default -> TLam[d_1, node]
    u64 mat_loc     = book_alloc(2);
    book_set(mat_loc + 0, leaf);
    book_set(mat_loc + 1, def_lam);
    Term mat        = term_new(0, TAG_MAT, 0, mat_loc);

    // outer: App(mat, TVar(d))
    u64 outer_loc   = book_alloc(2);
    book_set(outer_loc + 0, mat);
    book_set(outer_loc + 1, var_d);
    Term outer      = term_new(0, TAG_APP, 0, outer_loc);

    // wrap in two LAMs
    book_set(lam_x_loc, outer);
    Term lam_x      = term_new(0, TAG_LAM, 0, lam_x_loc);
    book_set(lam_d_loc, lam_x);
    Term lam_d      = term_new(0, TAG_LAM, 0, lam_d_loc);

    DEFS[6] = lam_d;
    char *src = thvm_aot_emit_program(6, "treebuild");
    CHECK(src != NULL);

    // 1 cont expected.
    CHECK(strstr(src, "1 cont(s)")                     != NULL);
    CHECK(strstr(src, "#define CONT_treebuild_0")      != NULL);
    CHECK(strstr(src, "par_treebuild_cont_0(")         != NULL);
    // The cont's body wraps the two child results in CTR(label=2).
    CHECK(strstr(src, "aot_make_ctr2(2u, t->args[0], t->args[1])")
                                                       != NULL);
    // Entry default arm should emit alloc + split.
    CHECK(strstr(src, "aot_alloc_cont(CONT_treebuild_0")  != NULL);
    CHECK(strstr(src, "aot_make_split(")               != NULL);
    CHECK(strstr(src, "aot_make_task(FN_treebuild,")   != NULL);
    // Dispatch should reference the cont.
    CHECK(strstr(src, "case CONT_treebuild_0")         != NULL);
    free(src);
  }

  TEST_BEGIN("emit recognises sibling-pair TOp2 -> R_SPLIT + fold cont");
  {
    // Build a sum-style def -- destructure node{l, r} and add the
    // two recursive sums:
    //   TLam[t,
    //     TMatChain[<|0 -> TVar(t),                  (* leaf-arm: v *)
    //                 1 -> TLam[l, TLam[r,            (* node-arm: l+r *)
    //                   TOp2[+, App(TRef[7], TVar(l)),
    //                           App(TRef[7], TVar(r))]
    //                 ]]
    //               |>, TLam[ig, TEra[]]            (* default *)
    //              ][t]]
    // Self-id = 7, arity = 1.  CTR-arm peels two LAMs (l, r) and
    // binds them to term_ctr_at(dv, 0)/1.  Body is OP2(ADD,
    // self(l), self(r)) -> SPLIT + OP2-fold cont.

    u64 lam_t_loc   = book_alloc(1);
    Term var_t      = term_new(0, TAG_VAR, 0, lam_t_loc);
    u64 lam_l_loc   = book_alloc(1);
    Term var_l      = term_new(0, TAG_VAR, 0, lam_l_loc);
    u64 lam_r_loc   = book_alloc(1);
    Term var_r      = term_new(0, TAG_VAR, 0, lam_r_loc);
    u64 lam_ig_loc  = book_alloc(1);

    // sibling calls: self(l), self(r).  arity 1, so single App.
    Term ref        = term_new(0, TAG_REF, 7, 0);
    u64 c0_loc      = book_alloc(2);
    book_set(c0_loc + 0, ref);
    book_set(c0_loc + 1, var_l);
    Term call_l     = term_new(0, TAG_APP, 0, c0_loc);
    u64 c1_loc      = book_alloc(2);
    book_set(c1_loc + 0, ref);
    book_set(c1_loc + 1, var_r);
    Term call_r     = term_new(0, TAG_APP, 0, c1_loc);

    // OP2(ADD, call_l, call_r).  OP_ADD = 0.
    u64 op_loc      = book_alloc(2);
    book_set(op_loc + 0, call_l);
    book_set(op_loc + 1, call_r);
    Term op2        = term_new(0, TAG_OP2, OP_ADD, op_loc);

    // CTR-arm handler: TLam[l, TLam[r, op2]]
    book_set(lam_r_loc, op2);
    Term lam_r      = term_new(0, TAG_LAM, 0, lam_r_loc);
    book_set(lam_l_loc, lam_r);
    Term ctr_handler = term_new(0, TAG_LAM, 0, lam_l_loc);

    // default-arm handler: TLam[ig, TEra[]]
    Term era         = term_new(0, TAG_ERA, 0, 0);
    book_set(lam_ig_loc, era);
    Term def_handler = term_new(0, TAG_LAM, 0, lam_ig_loc);

    // chain: outer MAT(0 -> TVar(t), inner)
    //         inner MAT(1 -> ctr_handler, default = def_handler)
    u64 inner_loc   = book_alloc(2);
    book_set(inner_loc + 0, ctr_handler);
    book_set(inner_loc + 1, def_handler);
    Term mat_inner  = term_new(0, TAG_MAT, 1, inner_loc);

    u64 outer_mat   = book_alloc(2);
    book_set(outer_mat + 0, var_t);
    book_set(outer_mat + 1, mat_inner);
    Term mat        = term_new(0, TAG_MAT, 0, outer_mat);

    // outer App: App(mat, TVar(t))
    u64 outer_loc   = book_alloc(2);
    book_set(outer_loc + 0, mat);
    book_set(outer_loc + 1, var_t);
    Term outer      = term_new(0, TAG_APP, 0, outer_loc);
    book_set(lam_t_loc, outer);
    Term lam_t      = term_new(0, TAG_LAM, 0, lam_t_loc);

    DEFS[7] = lam_t;
    char *src = thvm_aot_emit_program(7, "treesum");
    CHECK(src != NULL);

    // Cont 0 should be the OP2-fold cont.
    CHECK(strstr(src, "1 cont(s)")                != NULL);
    CHECK(strstr(src, "#define CONT_treesum_0")   != NULL);
    CHECK(strstr(src, "lv = (u32)term_val(t->args[0])") != NULL);
    CHECK(strstr(src, "rv = (u32)term_val(t->args[1])") != NULL);
    CHECK(strstr(src, "lv + rv")                  != NULL);
    CHECK(strstr(src, "aot_alloc_cont(CONT_treesum_0") != NULL);
    CHECK(strstr(src, "aot_make_split(")          != NULL);
    CHECK(strstr(src, "FN_treesum, aot_enc_ret")  != NULL);
    free(src);
  }

  TEST_BEGIN("emit handles TVar arm body resolving to a peeled LAM arg");
  {
    // Build  TLam[a, TLam[b, TMatChain[<|0 -> TVar(b)|>, TVar(b)][a]]]
    // ie.  args[0] = a, args[1] = b ; both arms return b.
    // Result: arm bodies should emit `Term v_X = t->args[1];`.
    u64 lam_b_loc    = book_alloc(1);
    Term var_b       = term_new(0, TAG_VAR, 0, lam_b_loc);

    u64 mat_loc      = book_alloc(2);
    book_set(mat_loc + 0, var_b);   // handler is TVar(b)
    book_set(mat_loc + 1, var_b);   // default is TVar(b)
    Term mat         = term_new(0, TAG_MAT, 0, mat_loc);

    u64 lam_a_loc    = book_alloc(1);
    Term var_a       = term_new(0, TAG_VAR, 0, lam_a_loc);
    u64 app_loc      = book_alloc(2);
    book_set(app_loc + 0, mat);
    book_set(app_loc + 1, var_a);
    Term app         = term_new(0, TAG_APP, 0, app_loc);
    book_set(lam_b_loc, app);

    Term lam_b       = term_new(0, TAG_LAM, 0, lam_b_loc);
    book_set(lam_a_loc, lam_b);
    Term lam_a       = term_new(0, TAG_LAM, 0, lam_a_loc);

    DEFS[4] = lam_a;
    char *src = thvm_aot_emit_program(4, "tvar_body");
    CHECK(src != NULL);
    // Phase 3 iter B: dv-fast-path -- hot inputs (CTR/NUM) skip the
    // wnf call entirely; non-WHNF tags (DP*/REF/...) fall through to
    // wnf(dv).  Match both lines so a future tweak to the spacing
    // doesn't silently regress the optimisation.
    CHECK(strstr(src, "Term dv = t->args[0];") != NULL);
    CHECK(strstr(src, "if (term_tag(dv) != TAG_CTR && term_tag(dv) != TAG_NUM) dv = cnf(dv);") != NULL);
    // Iter 10 inlines TVar resolution -- the arm body returns
    // `t->args[1]` directly via aot_make_value(t->args[1]) instead
    // of binding to a v_K named temp first.
    CHECK(strstr(src, "aot_make_value(t->args[1])") != NULL);
    free(src);
  }

  TEST_REPORT();
}
