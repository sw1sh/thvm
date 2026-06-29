(* ::Package:: *)
(* NN/Linear.wl - tensor-method helpers and the linear / matmul layer
   primitives (Tinygrad's Tensor methods translated).

   These read like ordinary math over a TTerm because they sit on the
   Plus / Times / Total / Dot UpValues installed in Tensor.wl. *)

BeginPackage["WolframInstitute`THVMLink`", {"GeneralUtilities`"}];

SetUsage[TSum, "TSum[x$] = TUOpReduce[x$, 0, \"SUM\"]."];
SetUsage[TSquare, "TSquare[x$] = TUOpMul[x$, x$]."];
SetUsage[TDot, "TDot[a$, b$] = TSum[TUOpMul[a$, b$]]."];
SetUsage[TL2Loss, "TL2Loss[x$] = TSum[TSquare[x$]]."];
SetUsage[TMSELoss, "TMSELoss[pred$, target$] = TL2Loss[pred$ - target$]."];
SetUsage[TMatVec, "TMatVec[W$, x$] computes W$ @ x$ where W$ is {out, in} and x$ is {in} (rank-1) or {1, in} (rank-2), giving {out}. Expand-broadcast then sum-reduce along the inner axis."];
SetUsage[TMatMul, "TMatMul[A$, B$] computes A$ @ B$ where A$ is {M, K} and B$ is {K, N}, giving {M, N}. Lowered as reshape + expand to a common {M, K, N} shape, elementwise multiply, then sum-reduce along axis 1; this pattern is recognised and routed to cblas_sgemm."];
SetUsage[TLinear, "TLinear[x$, W$, b$] computes x$ @ W$ + b$ where x$ is {$$, M, K}, W$ is {K, N}, and b$ is {N} (or None for bias-free). The bias is reshaped to {1, $$, 1, N} and expanded to the matmul output shape. Standard nn.Linear analogue (tinygrad's Tensor.linear)."];

Begin["`Private`"];

TSum[x_TTerm] := Total[x]
TSquare[x_TTerm] := x * x
TDot[a_TTerm, b_TTerm] := Total[a * b]
TL2Loss[x_TTerm] := Total[x * x]
TMSELoss[pred_TTerm, tgt_TTerm] := TL2Loss[pred - tgt]

(* tUopShape (the static shape walk) rather than TTensorShape so `w` may
   be an unrealized UOP term -- e.g. a Transpose feeding the vector.matrix
   case of the Dot UpValue -- not just a realized leaf. *)
TMatVec[w_TTerm, x_TTerm] := With[{shapeW = tUopShape[w]},
    Module[{out, in, xb},
        out = shapeW[[1]];
        in  = shapeW[[2]];
        xb  = TUOpExpand[x, {out, in}];
        TUOpReduce[TUOpMul[w, xb], 1, "SUM"]
    ]
]

(* TMatMul[A, B] -- A:{M,K} @ B:{K,N} -> {M,N}.  Lowered to a common
   {M,K,N} shape so MUL is elementwise and REDUCE_SUM along axis 1
   collapses the K dimension.  Both reshapes are contiguity-preserving
   (A:{M,K} -> {M,K,1} only inserts a unit axis; B:{K,N} -> {1,K,N}
   likewise) so EXPAND just sets a broadcast stride; no extra
   materialization.

   cpu_blas_dispatch recognises this exact KProgOp[] (MUL +
   REDUCE_SUM with inner=N, where input buffers are M*K and K*N
   floats) and routes to cblas_sgemm. *)
TMatMul[a_TTerm, b_TTerm] := With[{shapeA = tUopShape[a], shapeB = tUopShape[b]},
    Module[{m, k, n, ab, bb},
        m = shapeA[[1]];
        k = shapeA[[2]];
        n = shapeB[[2]];
        ab = TUOpExpand[TUOpReshape[a, {m, k, 1}], {m, k, n}];
        bb = TUOpExpand[TUOpReshape[b, {1, k, n}], {m, k, n}];
        TUOpReduce[TUOpMul[ab, bb], 1, "SUM"]
    ]
]

(* TLinear[x, W, b] -- standard nn.Linear forward = x @ W + b.
   The bias is rank-1 {N}; reshape to {1, ..., 1, N} and EXPAND
   to the matmul's output shape.  Without the explicit EXPAND,
   {seq,N} + {N} via the elementwise numel-cycle aligns only the
   first row and writes garbage into the rest.  The TLayerNorm
   reduction-broadcast pattern uses the same EXPAND trick. *)
TLinear[x_TTerm, w_TTerm, b_TTerm] := With[{shapeX = tUopShape[x], shapeW = tUopShape[w]},
    Module[{out, bcast, prod, biasShape},
        prod = TMatMul[x, w];
        out = ReplacePart[shapeX, Length[shapeX] -> shapeW[[2]]];
        biasShape = ConstantArray[1, Length[out]];
        biasShape[[Length[out]]] = shapeW[[2]];
        bcast = TUOpExpand[TUOpReshape[b, biasShape], out];
        prod + bcast
    ]
]

End[];

EndPackage[];
