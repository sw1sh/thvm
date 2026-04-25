(* memory_plan.wlt -- TMemoryPlan / TMemoryPlanReport (mp2 of the
   visualization arc).  Verifies the data layer: topo depth on a
   small DAG, buf collation across alias TenDescs, status
   classification.  Gantt rendering is mp3. *)

VerificationTest[
    TInit[];
    TReset[];
    a   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b   = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    res = TRealize @ TUOpAdd[a, b];
    plan = TMemoryPlan[];
    Head[plan],
    TMemoryPlan,
    TestID -> "memory-plan/head-is-tmemoryplan"
]

VerificationTest[
    TInit[];
    TReset[];
    a   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b   = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    res = TRealize @ TUOpAdd[a, b];
    payload = First @ TMemoryPlan[];
    {KeyExistsQ[payload, "Kernels"],
     KeyExistsQ[payload, "Tens"],
     KeyExistsQ[payload, "Bufs"]},
    {True, True, True},
    TestID -> "memory-plan/payload-keys"
]

VerificationTest[
    (* Diamond: k1 = ADD(a, b); k2 = MUL(k1_out, c);
                k3 = MUL(k1_out, d).
       k1 has depth 0 (its inputs are leaves); k2/k3 have depth 1. *)
    TInit[];
    TReset[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{3.0, 4.0}, "Real32"];
    c = TTensorCreate @ NumericArray[{5.0, 6.0}, "Real32"];
    d = TTensorCreate @ NumericArray[{7.0, 8.0}, "Real32"];
    k1Term = TUOpAdd[a, b];
    res2   = TRealize @ TUOpAdd[TUOpMul[k1Term, c], TUOpMul[k1Term, d]];
    plan = First @ TMemoryPlan[];
    depths = #["depth"] & /@ plan["Kernels"];
    {Min[depths], Max[depths]},
    {0, _Integer?(# >= 1 &)},
    SameTest -> (#1[[1]] === #2[[1]] && MatchQ[#1[[2]], #2[[2]]] &),
    TestID -> "memory-plan/diamond-depth-0-and-positive"
]

VerificationTest[
    (* Reshape alias: TUOpReshape on a contig source produces an
       alias TenDesc that shares buf_id with the source.  The
       collation step must emit ONE Bufs entry covering both
       tids in alias_tids. *)
    TInit[];
    TReset[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    res = TRealize @ TUOpReshape[a, {2, 2}];
    plan = First @ TMemoryPlan[];
    aliasGroups = Select[plan["Bufs"], Length[#["alias_tids"]] >= 2 &];
    Length[aliasGroups] >= 1,
    True,
    TestID -> "memory-plan/reshape-alias-collapses-into-one-buf"
]

VerificationTest[
    (* TMemoryPlanReport returns a Column expression. *)
    TInit[];
    TReset[];
    a   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b   = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    res = TRealize @ TUOpAdd[a, b];
    Head @ TMemoryPlanReport[TMemoryPlan[]],
    Column,
    TestID -> "memory-plan/report-head-is-column"
]

VerificationTest[
    (* TMemoryPlanGantt returns a Graphics expression. *)
    TInit[];
    TReset[];
    a   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b   = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    res = TRealize @ TUOpAdd[a, b];
    Head @ TMemoryPlanGantt[TMemoryPlan[]],
    Graphics,
    TestID -> "memory-plan/gantt-head-is-graphics"
]

VerificationTest[
    (* TMemoryPlanGantt with "BarHeight" -> "Uniform" still
       returns Graphics; smoke-test the option pass-through. *)
    TInit[];
    TReset[];
    a   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b   = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    res = TRealize @ TUOpAdd[a, b];
    Head @ TMemoryPlanGantt[TMemoryPlan[], "BarHeight" -> "Uniform"],
    Graphics,
    TestID -> "memory-plan/gantt-uniform-bar-height-option"
]

VerificationTest[
    (* MakeBoxes summary: rendering TMemoryPlan via ToBoxes
       should NOT throw and should produce an InterpretationBox-
       containing structure (the BoxForm`ArrangeSummaryBox
       wrapper). *)
    TInit[];
    TReset[];
    a   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b   = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    res = TRealize @ TUOpAdd[a, b];
    !FreeQ[ToBoxes[TMemoryPlan[]], InterpretationBox],
    True,
    TestID -> "memory-plan/makeboxes-renders-summary"
]
