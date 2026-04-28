(* grad_edge.wlt -- broader edge-case grad tests:
   - large vectors / 2-D tensors
   - deep MUL chains at 1st order (a^8, a^10)
   - mixed movement-op compositions
   - CMPLT-mask gradient flow (ReLU-like patterns)
   - mixed shapes + scalar broadcasts
*)

(* === Larger vectors ============================================== *)

VerificationTest[
    TInit[];
    n = 64;
    a = TTensorCreate @ NumericArray[Range[1.0, n], "Real32"];
    (* d/da sum(a^2) = 2a *)
    g = TRealize @ TGrad[ TUOpReduce[TUOpMul[a, a], 0, "SUM"], a];
    Normal @ TTensorData[g],
    Range[2.0, 2*n, 2.0],
    TestID -> "grad/large-vector-square-sum"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[ConstantArray[1.0, {16, 16}], "Real32"];
    (* d/da sum(a^2) = 2a, all entries 2.0 *)
    g = TRealize @ TGrad[ TUOpReduce[TUOpReduce[TUOpMul[a, a], 0, "SUM"], 0, "SUM"], a];
    Normal @ TTensorData[g],
    ConstantArray[2.0, {16, 16}],
    TestID -> "grad/2d-square-sum"
]

(* === Deep MUL chains (a^N for large N) =========================== *)

(* d(a^8)/da = 8 a^7.  At a=1 vector -> all 8s. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 1.0, 1.0}, "Real32"];
    seed = TUOpExpand[TUOpConst[1.0, "f32"], {3}];
    pow8 = Nest[TUOpMul[#, a] &, a, 7];
    Normal @ TTensorData @ TRealize @ TGrad[pow8, a, seed],
    {8.0, 8.0, 8.0},
    TestID -> "grad/a8-vector-first-order"
]

(* d(a^10)/da = 10 a^9.  At a=1 -> 10. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0}, "Real32"];
    pow10 = Nest[TUOpMul[#, a] &, a, 9];
    Normal @ TTensorData @ TRealize @ TGrad[pow10, a],
    {10.0},
    TestID -> "grad/a10-scalar-first-order"
]

(* === ReLU-style mask via CMPLT then MUL ======================== *)

(* y = max(0, x) elementwise, computed as x * (x > 0).
   d/dx of sum(y) = mask = (x > 0). *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{-2.0, -1.0, 0.5, 1.5, 3.0}, "Real32"];
    zero = TUOpExpand[TUOpConst[0.0, "f32"], {5}];
    mask = TUOpCmplt[zero, x];   (* 1 where x > 0, else 0 *)
    relu = TUOpMul[x, mask];
    g = TRealize @ TGrad[ TUOpReduce[relu, 0, "SUM"], x];
    Normal @ TTensorData[g],
    {0.0, 0.0, 1.0, 1.0, 1.0},
    TestID -> "grad/relu-via-cmplt-mask"
]

(* === Mixed-shape chains: SHRINK on rank-2, then RESHAPE ========= *)

(* a is {2, 4}; SHRINK to {2, 2}; RESHAPE to {4}; SUM. d/da has 1s
   in the kept slice, 0s outside.  All entries kept (shrink slice is
   {{0,2},{1,3}}) so 4 of the 8 entries get gy=1. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0, 4.0}, {5.0, 6.0, 7.0, 8.0}}, "Real32"];
    expr = TUOpReduce[
        TUOpReshape[
            TUOpShrink[a, {{0, 2}, {1, 3}}],
            {4}],
        0, "SUM"];
    Normal @ TTensorData @ TRealize @ TGrad[expr, a],
    {{0.0, 1.0, 1.0, 0.0}, {0.0, 1.0, 1.0, 0.0}},
    TestID -> "grad/shrink-reshape-rank2"
]

(* === Scalar broadcast in ADD chain =============================== *)

(* y = a + scalar; d/da y_i = 1 for all i. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    s = TUOpConst[5.0, "f32"];
    expr = TUOpReduce[TUOpAdd[a, s], 0, "SUM"];
    Normal @ TTensorData @ TRealize @ TGrad[expr, a],
    {1.0, 1.0, 1.0},
    TestID -> "grad/add-scalar-broadcast"
]

(* y = a * scalar; d/da y_i = scalar for all i. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    s = TUOpConst[7.0, "f32"];
    expr = TUOpReduce[TUOpMul[a, s], 0, "SUM"];
    Normal @ TTensorData @ TRealize @ TGrad[expr, a],
    {7.0, 7.0, 7.0},
    TestID -> "grad/mul-scalar-broadcast"
]

(* === Higher-order with movement op in path ===================== *)

(* d^2/da^2 of sum(reshape(a*a, {2, 2})) = 2 elementwise. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    expr = TUOpReduce[TUOpReduce[
        TUOpReshape[TUOpMul[a, a], {2, 2}],
        0, "SUM"], 0, "SUM"];
    seed = TUOpExpand[TUOpConst[1.0, "f32"], {4}];
    Normal @ TTensorData @ TRealize @ TGrad[TGrad[expr, a], a, seed],
    {2.0, 2.0, 2.0, 2.0},
    TestID -> "grad/higher-order-reshape-in-path"
]

(* === EXPAND broadcast then back via REDUCE_SUM =================== *)

(* y = sum(EXPAND(a, {3, 4})) where a shape {4}.
   d/da[i] = 3 (each a[i] broadcast 3 times into the sum). *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    expr = TUOpReduce[TUOpReduce[TUOpExpand[a, {3, 4}], 0, "SUM"], 0, "SUM"];
    Normal @ TTensorData @ TRealize @ TGrad[expr, a],
    {3.0, 3.0, 3.0, 3.0},
    TestID -> "grad/expand-then-reduce-sum"
]

(* === Cumulative chain through 4 movement ops =================== *)

(* y = sum(FLIP(PERMUTE(RESHAPE(SHRINK(a*a, {{0, 4}}), {2, 2}), {1, 0}), {0})).
   d/da: 2a inside the kept SHRINK slice, 0 outside. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0, 5.0}, "Real32"];
    expr = TUOpReduce[TUOpReduce[
        TUOpFlip[
            TUOpPermute[
                TUOpReshape[
                    TUOpShrink[TUOpMul[a, a], {{0, 4}}],
                    {2, 2}],
                {1, 0}],
            {0}],
        0, "SUM"], 0, "SUM"];
    Normal @ TTensorData @ TRealize @ TGrad[expr, a],
    {2.0, 4.0, 6.0, 8.0, 0.0},
    TestID -> "grad/four-movement-op-chain"
]
