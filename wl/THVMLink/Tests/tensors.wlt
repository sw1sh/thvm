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

(* === TMaterialize: run the rewrite, inspect the scheduled DAG === *)

VerificationTest[
    TInit[];
    a   = TTensor[{4}, {1.0, 2.0, 3.0, 4.0}];
    b   = TTensor[{4}, {5.0, 6.0, 7.0, 8.0}];
    k   = TMaterialize[a + b];
    (* single-op kernel: one ADD over two 4-element inputs.
       TKernelCount includes the reserved slot 0, so the first real
       kernel brings it to 2. *)
    {TUOpKind[k], TKernelCount[]},
    {"KERNEL", 2},
    TestID -> "TMaterialize/emits-one-kernel"
]

VerificationTest[
    TInit[];
    a   = TTensor[{4}, {1.0, 2.0, 3.0, 4.0}];
    b   = TTensor[{4}, {5.0, 6.0, 7.0, 8.0}];
    c   = TTensor[{4}, {9.0, 10.0, 11.0, 12.0}];
    k   = TMaterialize[(a + b) * c];
    (* two kernels (ADD then MUL), no fusion in step-12 v1. *)
    {TUOpKind[k], TKernelCount[]},
    {"KERNEL", 3},
    TestID -> "TMaterialize/compound-two-kernels"
]

VerificationTest[
    TInit[];
    a    = TTensor[{3}, {1.0, 2.0, 3.0}];
    (* Go through TUOpAdd directly -- `a + a` would be simplified to
       `2*a` by WL's Orderless Plus before our UpValue fires.
       Program layout (sub-item c of UOP_LOAD arc): one LOAD per
       input + the main op.  For dedup'd 1-input ADD: [LOAD, ADD].
       The ADD lives at index n_inputs+1 (1-based). *)
    k    = TMaterialize[TUOpAdd[a, a]];
    kid  = TTermVal @ THeapRead[TTermVal[k] + 1];
    info = TKernelInfo[kid];
    {info["n_inputs"],
     info["program"][[info["n_inputs"] + 1, "src", 1]] ===
     info["program"][[info["n_inputs"] + 1, "src", 2]]},
    {1, True},
    TestID -> "TMaterialize/dedups-duplicate-inputs"
]

VerificationTest[
    TInit[];
    a    = TTensor[{3}, {1.0, 2.0, 3.0}];
    k    = TMaterialize[TUOpReduce[a, 0, "SUM"]];
    (* Kernel id sits in the second heap cell of the UOP_KERNEL.
       REDUCE lives at index n_inputs+1 (1-based) after the LOAD prefix. *)
    kid  = TTermVal @ THeapRead[TTermVal[k] + 1];
    info = TKernelInfo[kid];
    {info["program"][[info["n_inputs"] + 1, "opcode"]],
     info["output_numel"]},
    {"REDUCE", 1},
    TestID -> "TMaterialize/reduce-output-shape"
]
