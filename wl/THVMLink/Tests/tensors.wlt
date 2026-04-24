(* tensors.wlt -- WL surface for TTensor + TUOp constructors.
   Step 12 commit 2: graph building only, no materialize / dispatch yet.
*)

(* === TTensor === *)

VerificationTest[
    TInit[];
    t = TTensor[{4}, "f32"];
    {TTermTag[t], TTermExt[t], Length[TTensorShape[t]]},
    {$TagTEN, $DTF32, 1},
    TestID -> "TTensor/alloc-empty"
]

VerificationTest[
    TInit[];
    t = TTensor[{2, 3}, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}];
    {Head[TTensorData[t]], Normal @ TTensorData[t]},
    {NumericArray, {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}},
    TestID -> "TTensor/initial-data-roundtrip-as-NumericArray"
]

VerificationTest[
    TInit[];
    data = NumericArray[{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}, "Real32"];
    t    = TTensorCreate[data];
    (* Shape + dtype inferred from NumericArray; bytes shared on CPU. *)
    {TTensorShape[t], TTensorDType[t], Normal @ TTensorData[t]},
    {{2, 3}, "f32", {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}},
    TestID -> "TTensorCreate/shares-NumericArray"
]

VerificationTest[
    TInit[];
    (* Integer32 NumericArray -> TAG_TEN i32 *)
    data = NumericArray[{10, 20, 30}, "Integer32"];
    t    = TTensorCreate[data];
    {TTensorShape[t], TTensorDType[t], Normal @ TTensorData[t]},
    {{3}, "i32", {10, 20, 30}},
    TestID -> "TTensorCreate/integer32"
]

VerificationTest[
    TInit[];
    (* Nested list falls through to NumericArray conversion. *)
    t = TTensorCreate[{{1.0, 2.0}, {3.0, 4.0}}];
    {TTensorShape[t], Normal @ TTensorData[t]},
    {{2, 2}, {{1.0, 2.0}, {3.0, 4.0}}},
    TestID -> "TTensorCreate/list-is-lifted-to-NA"
]

VerificationTest[
    TInit[];
    t = TTensor[{4}, "f32"];
    TTensorRefcount[t],
    1,
    TestID -> "TTensor/refcount-starts-at-1"
]

(* === UOp constructors === *)

VerificationTest[
    TInit[];
    a = TTensor[{2}, {1.0, 2.0}];
    b = TTensor[{2}, {3.0, 4.0}];
    sum = TUOpAdd[a, b];
    {TTermTag[sum], TTermExt[sum], TUOpKind[sum]},
    {$TagUOP, $UopAdd, "ADD"},
    TestID -> "TUOpAdd/builds-TAG_UOP"
]

VerificationTest[
    TInit[];
    a = TTensor[{2}, {1.0, 2.0}];
    expr = TUOpAdd[TUOpNeg[a], a];
    srcs = TUOpSrcs[expr];
    {TUOpKind[srcs[[1]]], TTermTag[srcs[[2]]]},
    {"NEG", $TagTEN},
    TestID -> "TUOpAdd/can-mix-uop-and-tensor-sources"
]

VerificationTest[
    TInit[];
    c = TUOpConst[2.5, "f32"];
    {TUOpKind[c], TTermExt[c]},
    {"CONST", $UopConst},
    TestID -> "TUOpConst/dtype-in-ext"
]

VerificationTest[
    TInit[];
    a = TTensor[{2, 3}, "f32"];
    rs = TUOpReshape[a, {6}];
    (* TUOpSrcs returns just the src cell for movement ops in step 12;
       the dim NUMs live in heap[loc+1..] and are inspected directly. *)
    {TUOpKind[rs], TTermTag[TUOpSrcs[rs][[1]]],
     TTermTag[THeapRead[TTermVal[rs] + 1]]},
    {"RESHAPE", $TagTEN, $TagNUM},
    TestID -> "TUOpReshape/dims-as-NUM-cells"
]

VerificationTest[
    TInit[];
    a = TTensor[{4}, "f32"];
    r = TUOpReduce[a, 0, "SUM"];
    {TUOpKind[r], TTermTag[THeapRead[TTermVal[r] + 1]],
     TTermVal[THeapRead[TTermVal[r] + 1]]},
    {"REDUCE", $TagNUM, $ReduceSum},
    TestID -> "TUOpReduce/kind-and-axis-as-NUM"
]

VerificationTest[
    TInit[];
    a   = TTensor[{2}, {1.0, 2.0}];
    mat = TUOpMaterialize[TUOpAdd[a, a]];
    TUOpKind[mat],
    "MATERIALIZE",
    TestID -> "TUOpMaterialize/wraps-expr"
]
