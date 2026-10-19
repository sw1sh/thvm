(* tensors.wlt -- WL surface for TTensor + TUOp constructors.
   Step 12 commit 2: graph building only, no materialize / dispatch yet.
*)

(* === TTensor === *)

VerificationTest[
    TInit[];
    t = TTensor[{4}, "f32"];
    {TTermTag[t], TTermExt[t], Length[TTensorShape[t]]},
    {$TagTEN, $DTFp32, 1},
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
    Module[{a, b, c, k},
        a   = TTensor[{4}, {1.0, 2.0, 3.0, 4.0}];
        b   = TTensor[{4}, {5.0, 6.0, 7.0, 8.0}];
        c   = TTensor[{4}, {9.0, 10.0, 11.0, 12.0}];
        k   = TMaterialize[(a + b) * c];
        {TUOpKind[k], TKernelCount[]}
    ],
    {"KERNEL", 2},
    TestID -> "TMaterialize/compound-fuses-to-one-kernel"
]

VerificationTest[
    TInit[];
    a    = TTensor[{3}, {1.0, 2.0, 3.0}];
    (* Go through TUOpAdd directly -- `a + a` would be simplified to
       `2*a` by WL's Orderless Plus before our UpValue fires.
       Materialize dedups the duplicate input, so the kernel has
       n_inputs == 1.  Pre THVM_PHASE_C7_FREE_PROGRAM the test also
       asserted the ADD opcode and src-slot equality on the KProgOp;
       under FREE_PROGRAM=1 default, program[] is freed post-lift so
       n_inputs is the surviving dedup observation. *)
    k    = TMaterialize[TUOpAdd[a, a]];
    kid  = TTermVal @ THeapRead[TTermVal[k] + 1];
    info = TKernelInfo[kid];
    info["n_inputs"],
    1,
    TestID -> "TMaterialize/dedups-duplicate-inputs"
]

VerificationTest[
    TInit[];
    a    = TTensor[{3}, {1.0, 2.0, 3.0}];
    k    = TMaterialize[TUOpReduce[a, 0, "SUM"]];
    (* output_numel collapses the reduced axis to 1.  AxisTypes
       includes REDUCE (axis.c reads this from the lifted DAG's
       UOP_RANGE leaves with KAX_REDUCE / KAX_GROUP_REDUCE). *)
    kid  = TTermVal @ THeapRead[TTermVal[k] + 1];
    info = TKernelInfo[kid];
    axisTypes = First[TKernelOpts[kid]]["AxisTypes"];
    {info["output_numel"], MemberQ[axisTypes, "REDUCE" | "GROUP_REDUCE"]},
    {1, True},
    TestID -> "TMaterialize/reduce-output-shape"
]

(* Elementwise sugar broadcasts like numpy/tinygrad: right-aligned, size-1 axes grow. *)
VerificationTest[
    With[{m = TTensorCreate[{{1., 5.}, {3., 2.}}]},
        {Normal @ TRealize[m + TTensorCreate[{10., 20.}]], Normal @ TRealize[m * TTensorCreate[{{10.}, {100.}}]]}],
    {{{11., 25.}, {13., 22.}}, {{10., 50.}, {300., 200.}}},
    TestID -> "tensors/sugar-broadcasts"
]

(* Shapes that cannot broadcast give General::bcast and stay unevaluated. *)
VerificationTest[
    MatchQ[TTensorCreate[{{1., 5.}, {3., 2.}}] + TTensorCreate[{1., 2., 3.}], _Plus],
    True,
    {General::bcast, General::bcast},
    TestID -> "tensors/sugar-broadcast-incompatible"
]

(* The raw constructors refuse operands that would read past a buffer: different element
   counts, a reshape to another count, a non-permutation, an expand the source cannot take. *)
VerificationTest[
    With[{m = TTensorCreate[{{1., 5.}, {3., 2.}}], v = TTensorCreate[{1., 2., 3.}]},
        Head /@ {TUOpAdd[m, v], TUOpReshape[v, {2, 2}], TUOpPermute[m, {1, 1}], TUOpExpand[v, {3, 2}]}],
    {TUOpAdd, TUOpReshape, TUOpPermute, TUOpExpand},
    {TUOpAdd::shape, TUOpReshape::shape, TUOpPermute::shape, TUOpExpand::shape},
    TestID -> "tensors/uop-shape-checks"
]

(* Input that is not a term or tensor data gives a message and stays unevaluated, instead of
   reaching the library and building a malformed term. *)
VerificationTest[
    {TRealize[foo], TRealize[3], TTensorCreate[{{1, 2}, {3}}], TTensorCreate[{1, 2}, "bogus"], TUOpConst[foo], TUOpReduce[TTensorCreate[{1., 2.}], 0, "PROD"]},
    {TRealize[foo], TRealize[3], TTensorCreate[{{1, 2}, {3}}], TTensorCreate[{1, 2}, "bogus"], TUOpConst[foo], TUOpReduce[TTensorCreate[{1., 2.}], 0, "PROD"]},
    {TRealize::term, TRealize::term, TTensorCreate::data, TTensorCreate::dtype, TUOpConst::args, TUOpReduce::args},
    SameTest -> (Length[#1] === Length[#2] && And @@ MapThread[MatchQ[Head[#1], Head[#2]] &, {#1, #2}] &),
    TestID -> "tensors/input-checks"
]

VerificationTest[
    TUOpReduce[TTensorCreate[{1., 2.}], 3, "SUM"],
    _TUOpReduce,
    {TUOpReduce::args},
    SameTest -> MatchQ,
    TestID -> "tensors/reduce-axis-past-rank"
]

(* TMatMul needs matching inner dimensions; TGather an index with one position per slice;
   TSet a source with the destination's element count.  Each refusal is a message. *)
VerificationTest[
    With[{a = TTensorCreate[{{1., 2.}, {3., 4.}}], x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}], d = TTensorCreate[{1., 2., 3.}]},
        Head /@ {TMatMul[a, TTensorCreate[{{1., 2., 3.}}]], TGather[x, 1, {1}], TSet[d, TTensorCreate[{5., 6.}]]}],
    {TMatMul, TGather, TSet},
    {TMatMul::dims, TGather::args, TSet::src},
    TestID -> "tensors/matmul-gather-set-checks"
]

VerificationTest[
    With[{x = TTensorCreate[{{1., 2., 3.}, {4., 5., 6.}}]}, Normal @ TRealize[TGather[x, 1, {2, 0}]]],
    {{3.}, {4.}},
    TestID -> "tensors/gather-per-row"
]

(* TUOpKind names a UOP's op, a grad projection "GRAD", and any other term by its tag. *)
VerificationTest[
    With[{x = TTensorCreate[{1., 2.}]}, {TUOpKind[x], TUOpKind[x + x], TUOpKind[TGrad[Total[x^2], x]]}],
    {"TEN", "ADD", "GRAD"},
    TestID -> "tensors/tuopkind-names"
]

(* A TKernel in arithmetic stands for its output term; kernel["Source", fmt] is TKernelSource. *)
VerificationTest[
    With[{x = TTensorCreate[{1., 2.}]}, With[{k = TKernel[TMaterialize[x^2]]}, {Normal @ TRealize[k + x], Normal @ TRealize[2. k]}]],
    {{2., 6.}, {2., 8.}},
    TestID -> "tensors/tkernel-arithmetic"
]

VerificationTest[
    (TRealize[TTensorCreate[{1., 2.}] * 3.]; StringQ[TKernel[TKernelCount[] - 1]["Source", "C"]]),
    True,
    TestID -> "tensors/tkernel-source-format"
]

(* tinygrad promotion: an integer tensor with a real scalar, or mixed with a float tensor,
   computes in f32; integer with integer stays integer. *)
VerificationTest[
    With[{i = TTensorCreate[{1, 2}], f = TTensorCreate[{0.5, 1.5}]},
        {Normal @ TRealize[i * 2.5], TTensorDType[TRealize[i * 2.5]], Normal @ TRealize[i * 3], TTensorDType[TRealize[i * 3]], Normal @ TRealize[i + f]}],
    {{2.5, 5.}, "f32", {3, 6}, "i32", {1.5, 3.5}},
    TestID -> "tensors/int-float-promotion"
]

(* A reduce over a graph of lambda variables has no known shape until the lambda is applied, so
   TUOpReduce accepts it. *)
VerificationTest[
    Normal[TRealize[TApp[TLam[w, TUOpReduce[TUOpMul[w, w], 0, "SUM"]], TTensorCreate[{1., 2., 3.}]]]],
    {14.},
    TestID -> "tensors/reduce-under-lambda"
]
