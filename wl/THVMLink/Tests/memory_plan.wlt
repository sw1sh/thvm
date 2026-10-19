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
    (* Diamond of four realizes (one TRealize would fuse it into a single
       kernel): k1 = a + b; k2 = k1 c; k3 = k1 d; k4 = k2 + k3, so the
       depths are 0, 1, 1, 2. *)
    TInit[];
    TReset[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{3.0, 4.0}, "Real32"];
    c = TTensorCreate @ NumericArray[{5.0, 6.0}, "Real32"];
    d = TTensorCreate @ NumericArray[{7.0, 8.0}, "Real32"];
    k1 = TRealize @ TUOpAdd[a, b];
    k2 = TRealize @ TUOpMul[k1, c];
    k3 = TRealize @ TUOpMul[k1, d];
    k4 = TRealize @ TUOpAdd[k2, k3];
    #["depth"] & /@ First[TMemoryPlan[]]["Kernels"],
    {0, 1, 1, 2},
    TestID -> "memory-plan/diamond-depths"
]

VerificationTest[
    (* A chain of 400 realizes, each consuming the last one's output: the
       depth walk is iterative, so a chain deeper than $RecursionLimit allows
       a recursive walk still gives every kernel its depth. *)
    TInit[];
    TReset[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0}, "Real32"];
    Do[x = TRealize @ TUOpAdd[x, x], {400}];
    Max[#["depth"] & /@ First[TMemoryPlan[]]["Kernels"]],
    _Integer?(# >= 399 &),
    SameTest -> MatchQ,
    TestID -> "memory-plan/long-chain-depth"
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
    (* TMemoryPlanGantt returns a Legended-wrapped Graphics. *)
    TInit[];
    TReset[];
    a   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b   = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    res = TRealize @ TUOpAdd[a, b];
    Head @ TMemoryPlanGantt[TMemoryPlan[]],
    Legended,
    TestID -> "memory-plan/gantt-head-is-legended"
]

VerificationTest[
    (* TMemoryPlanGantt with "TopN" -> 5 still returns Legended;
       smoke-test the option pass-through. *)
    TInit[];
    TReset[];
    a   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b   = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    res = TRealize @ TUOpAdd[a, b];
    Head @ TMemoryPlanGantt[TMemoryPlan[], "TopN" -> 5],
    Legended,
    TestID -> "memory-plan/gantt-topn-option"
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

VerificationTest[
    (* the Gantt draws one bar per buffer (the bar map was parsed as one Function) *)
    TReset[];
    a = TTensorCreate[Range[1., 64.]];
    TMaterialize[Total[a^2] * Total[a]];
    g = TMemoryPlanGantt[TMemoryPlan[]];
    {Length[Cases[g, _Rectangle, Infinity]] > 0, FreeQ[g, _Function]},
    {True, True},
    TestID -> "memory-plan/gantt-draws-bars"
]
