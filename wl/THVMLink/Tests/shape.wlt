(* shape.wlt -- WL-side shape inference (`tUopShape`) over UOP
   graphs.  Mirrors materialize_in_env.c's output-shape branch so
   layer dispatchers can size operations on intermediate UOPs
   without going through TTensorShape (which only handles TAG_TEN). *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}, "Real32"];
    WolframInstitute`THVMLink`Private`tUopShape[x],
    {2, 3},
    TestID -> "shape/ten-passthrough"
]

VerificationTest[
    TInit[];
    x = TUOpConst[3.14, "f32"];
    WolframInstitute`THVMLink`Private`tUopShape[x],
    {1},
    TestID -> "shape/const-rank1"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    WolframInstitute`THVMLink`Private`tUopShape[TUOpAdd[a, b]],
    {3},
    TestID -> "shape/add-broadcast-equal-shapes"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, "Real32"];
    WolframInstitute`THVMLink`Private`tUopShape[TUOpReshape[x, {2, 3}]],
    {2, 3},
    TestID -> "shape/reshape-1d-to-2d"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}, "Real32"];
    WolframInstitute`THVMLink`Private`tUopShape[TUOpReduce[x, 0, "SUM"]],
    {3},
    TestID -> "shape/reduce-axis0-rank2"
]

VerificationTest[
    TInit[];
    input   = TTensorCreate @ NumericArray[ConstantArray[0.5, {1, 5, 5}], "Real32"];
    weights = TTensorCreate @ NumericArray[ConstantArray[1.0, {2, 1, 3, 3}], "Real32"];
    bias    = TTensorCreate @ NumericArray[{0.0, 0.0}, "Real32"];
    WolframInstitute`THVMLink`Private`tUopShape[TConv2D[input, weights, bias]],
    {2, 3, 3},
    TestID -> "shape/conv2d-1ch-2outch-3x3-kernel"
]

(* Composition: shape walks through nested UOPs without realising. *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}, "Real32"];
    chain = TUOpReduce[TUOpReshape[x, {2, 3}], 0, "SUM"];
    WolframInstitute`THVMLink`Private`tUopShape[chain],
    {3},
    TestID -> "shape/composed-reshape-then-reduce"
]
