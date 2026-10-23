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
SetUsage[TLinear, "TLinear[x$, W$, b$] computes x$ @ W$ + b$ (tinygrad's Tensor.linear): x$ is {$$, K}, W$ is {K, N} (a rank-1 W$ multiplies elementwise), and b$ is {N} or None for bias-free; leading axes of x$ broadcast."];

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
matMulShapesOkQ[{_, k_Integer}, {k_Integer, _}] := True
matMulShapesOkQ[_, _] := False
TMatMul::dims = "TMatMul multiplies an m by k matrix with a k by n one; the shapes are `1` and `2`.";

TMatMul[a_TTerm, b_TTerm] /; matMulShapesOkQ[tUopShape[a], tUopShape[b]] := nnCall[TMatMul, "dot", {a, b}]
TMatMul[a_TTerm, b_TTerm] := Null /; (Message[TMatMul::dims, tUopShape[a], tUopShape[b]]; False)

TLinear[x_TTerm, w_TTerm, b : (_TTerm | None) : None] := nnCall[TLinear, "linear", {x, w, b}]

End[];

EndPackage[];
