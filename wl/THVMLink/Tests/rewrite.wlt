(* rewrite.wlt -- cross-validate WL KOpt rules against the C-side
   apply_opt_dag implementation.

   For each KOpt + each test fixture, apply the rewrite via two
   independent paths and assert the resulting TTermExpr snapshots
   are structurally equal:

     WL: TTermExpr[root] -> KOpt<X>[args][snapshot] -> WL output
     C : root -> TUOpDagApplyKOpt[root, op, axis, arg] -> TTermExpr  -> C output

     wlOut === cOut  must hold.

   When any pair drifts, either the WL rule or the C implementation
   has a bug -- both surface as a failing VerificationTest.
*)

(* === fixtures =================================================== *)

(* Canonical 16x16 matmul:
     STORE(C, m*16+n, REDUCE(MUL(A[m*16+k], B[k*16+n]), SUM, k))
   Matches what tests/test_apply_opt_dag.c builds for the C-side
   xvalid suite. *)

buildMatmul16[] := Module[{a, b, c, m, n, k, kc, addrA, addrB, addrC,
                           loadA, loadB, mul, red},
    TInit[];
    a = TUOpBuffer[$UopScopeGlobal, $DTFp32, {16, 16}, 1];
    b = TUOpBuffer[$UopScopeGlobal, $DTFp32, {16, 16}, 2];
    c = TUOpBuffer[$UopScopeGlobal, $DTFp32, {16, 16}, 0];
    m = TUOpRange[0, $KaxLoop,   16];
    n = TUOpRange[1, $KaxLoop,   16];
    k = TUOpRange[2, $KaxReduce, 16];
    kc = TUOpIConst[16];
    addrA = TUOpIAdd[TUOpIMul[m, kc], k];
    addrB = TUOpIAdd[TUOpIMul[k, kc], n];
    addrC = TUOpIAdd[TUOpIMul[m, kc], n];
    loadA = TUOpIndexE[a, addrA];
    loadB = TUOpIndexE[b, addrB];
    mul = TUOpMul[loadA, loadB];
    red = TUOpReduce[mul, 2, "SUM"];
    TUOpStore[c, addrC, red]
]

(* === xvalid driver =============================================== *)
(* The C-side spec is passed as `op[axis, arg]` (a head-keyed triple,
   e.g. `$KopTC[0, 8]` -- the 9[0,8] form Integer-headed but pattern-
   matchable).  Lets the test corpus read like ML-style data. *)

xvalid[wlOp_, op_[axis_, arg_], fixture_] := With[{root = fixture[]},
  With[{wlSnap = TTermExpr[root]},
    TTermExpr[TUOpDagApplyKOpt[root, op, axis, arg]] === wlOp[wlSnap]
  ]
]

(* === KOpt cross-validation tests ================================ *)
(* Naming convention: TestID = "xvalid-<kopname>-<fixture>-<args>". *)

(* --- TC ----------------------------------------------------------- *)

VerificationTest[
    xvalid[KOptTC[8],   $KopTC[0, 8],  buildMatmul16],
    True,
    TestID -> "xvalid-tc-matmul-factor8"]

VerificationTest[
    xvalid[KOptTC[16],  $KopTC[0, 16], buildMatmul16],
    True,
    TestID -> "xvalid-tc-matmul-factor16"]

VerificationTest[
    xvalid[KOptTC[32],  $KopTC[0, 32], buildMatmul16],
    True,
    TestID -> "xvalid-tc-matmul-factor32"]

(* --- GLOBAL ------------------------------------------------------- *)

VerificationTest[
    xvalid[KOptGlobal[0], $KopGlobal[0, 0], buildMatmul16],
    True,
    TestID -> "xvalid-global-matmul-axis-m"]

VerificationTest[
    xvalid[KOptGlobal[1], $KopGlobal[1, 0], buildMatmul16],
    True,
    TestID -> "xvalid-global-matmul-axis-n"]

(* --- SWAP --------------------------------------------------------- *)

VerificationTest[
    xvalid[KOptSwap[0, 1], $KopSwap[0, 1], buildMatmul16],
    True,
    TestID -> "xvalid-swap-matmul-mn"]

(* --- splits: UPCAST / UNROLL / LOCAL / GROUP / GROUPTOP ----------
   All take (axis, factor); use factor 4 on axis 0 (extent 16).
   The split rule rewrites RANGE(0, LOOP, 16) into IADD(IMUL(outer,
   4), inner); xvalid asserts WL and C produce the same shape. *)

VerificationTest[
    xvalid[KOptUpcast[0, 4],   $KopUpcast[0, 4],   buildMatmul16],
    True,
    TestID -> "xvalid-upcast-matmul-axis0-factor4"]

VerificationTest[
    xvalid[KOptUnroll[0, 4],   $KopUnroll[0, 4],   buildMatmul16],
    True,
    TestID -> "xvalid-unroll-matmul-axis0-factor4"]

VerificationTest[
    xvalid[KOptLocal[0, 4],    $KopLocal[0, 4],    buildMatmul16],
    True,
    TestID -> "xvalid-local-matmul-axis0-factor4"]

(* KOP_GROUP / KOP_GROUPTOP xvalid tests retired with commit 07b68af8
   (KOP_GROUP/GROUPTOP consumers removed from C-side apply_opt_dag).
   The opcode #defines stay for opcode-number ordering; no producer
   emits them.  The WL surface KOptGroup / KOptGroupTop also stays
   for back-compat but no longer mirrors a live C path. *)

(* === composition tests ==========================================
   Apply a KOpt SEQUENCE in the same order via WL and C.  The
   C-side composition is iterative kernel_apply_opt; the WL side is
   RightComposition of the operator-form rules.  cSeq elements use
   the same `op[axis, arg]` head-keyed shape as xvalid.
*)

composeC[root_, kopts_List] := Fold[
    Function[{r, spec},
        Replace[spec, op_[axis_, arg_] :> TUOpDagApplyKOpt[r, op, axis, arg]]],
    root, kopts]

xvalidSeq[wlSeq_List, cSeq_List, fixture_] := With[{root = fixture[]},
  With[{wlSnap = TTermExpr[root]},
    TTermExpr[composeC[root, cSeq]] ===
      (RightComposition @@ wlSeq)[wlSnap]
  ]
]

VerificationTest[
    xvalidSeq[
        {KOptTC[8], KOptGlobal[0], KOptGlobal[1]},
        {$KopTC[0, 8], $KopGlobal[0, 0], $KopGlobal[1, 0]},
        buildMatmul16],
    True,
    TestID -> "xvalid-compose-tc-global-global"]
