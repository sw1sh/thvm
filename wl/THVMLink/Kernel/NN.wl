(* ::Package:: *)
(* NN.wl - converters from Wolfram NeuralNetworks layers to the
   tensor UOp graph.

   Tinygrad-style: a layer is just a snapshot of weights + a forward
   recipe.  Instead of inventing our own layer constructors we lean
   on the built-in `LinearLayer` / `ConvolutionLayer` / `Element-`
   `wiseLayer` / `NetChain`, lift their parameters into TTensors,
   and emit the same TUOp* combinators users write by hand.  Autograd
   then flows through whatever ops `interact_grad` already supports
   (currently ADD, MUL, NEG, REDUCE_SUM -- step 14 will widen this).

   Public surface
     TFromNet[net, x]            -- entry point.  Dispatches on
                                    Head[net] (LinearLayer, NetChain,
                                    ElementwiseLayer, Convolution-
                                    Layer, ...).  Returns a TTerm.
     TFromLayer[layer, x]        -- alias / single-layer form.
     TLayerWeights[layer]        -- NumericArrays of every parameter
                                    a layer carries, in declaration
                                    order.  Used by tests / training
                                    loops to read or replace weights.
     TLayerToTensors[layer]      -- the same parameters but already
                                    wrapped as TTensor handles.

   Tensor-level helpers (Tinygrad's Tensor methods translated)
     TSum[x]                     -- UOP_REDUCE SUM along axis 0
     TSquare[x]                  -- x * x
     TDot[a, b]                  -- sum(a * b)
     TMatVec[W, x]               -- W:{out,in} @ x:{1,in} -> {out}
     TL2Loss[x]                  -- sum(x*x)
     TMSELoss[pred, target]      -- sum((pred - target)^2)
     TReLU[x]                    -- elementwise max(x, 0)
     TTanh[x]                    -- elementwise tanh, via EXP2
     TSoftmax[x]                 -- softmax over the last axis
     TLog[x]                     -- elementwise natural log via LOG2
     TCrossEntropyLoss[pred, target]
                                 -- -sum(target * log(pred))
*)

BeginPackage["THVMLink`"];

TFromNet::usage         = "TFromNet[net, x] converts a Wolfram NeuralNetworks layer (or NetChain / NetGraph) into a TTerm UOp graph rooted at the input TTerm `x`.  The net must be initialised so its weights are concrete arrays.";
TFromLayer::usage       = "TFromLayer[layer, x] is the single-layer form of TFromNet.";
TLayerWeights::usage    = "TLayerWeights[layer] returns the NumericArrays of every learnable parameter a layer carries (Weights, Biases, ...), in the layer-specific declaration order.";
TLayerToTensors::usage  = "TLayerToTensors[layer] is the same as TLayerWeights but each NumericArray is wrapped in a fresh TTensorCreate handle so it can feed back into a UOp graph.";
TSum::usage             = "TSum[x] = TUOpReduce[x, 0, \"SUM\"].";
TSquare::usage          = "TSquare[x] = TUOpMul[x, x].";
TDot::usage             = "TDot[a, b] = TSum[TUOpMul[a, b]].";
TMatVec::usage          = "TMatVec[W, x] computes W @ x where W has shape {out, in} and x has shape {1, in}.  Result has shape {out}.  EXPAND-broadcast then REDUCE_SUM along the inner axis.";
TMatMul::usage          = "TMatMul[A, B] computes A @ B where A has shape {M, K} and B has shape {K, N}.  Result has shape {M, N}.  Lowered as RESHAPE + EXPAND to a common {M, K, N} shape, MUL elementwise, then REDUCE_SUM along axis 1.  cpu_blas_dispatch recognises this KProgOp[] pattern and routes to cblas_sgemm.";
TL2Loss::usage          = "TL2Loss[x] = TSum[TSquare[x]].";
TMSELoss::usage         = "TMSELoss[pred, target] = TL2Loss[pred - target].";
TReLU::usage            = "TReLU[x] = elementwise max(x, 0), implemented as MUL[x, CMPLT[0, x]] -- the CMPLT mask broadcasts a CONST(0) against x and yields 1 where x > 0, else 0.";
TTanh::usage            = "TTanh[x] = elementwise tanh, implemented as (u - 1)/(u + 1) where u = exp(2x) = EXP2(x * 2 * log2 e).  Uses only existing UOPs (no UOP_TANH primitive).  Loses precision for |x| > ~10 due to exp overflow; that's accepted for now since hidden activations rarely sit there.";
TSoftmax::usage         = "TSoftmax[x] = exp(x) / sum(exp(x)) over the last axis.  exp via the EXP2 + log2(e) chain.  Numerically naive (no max-subtract stabilisation) -- input magnitudes >~80 will overflow exp.  Forward only.";
TLog::usage             = "TLog[x] = elementwise natural log, implemented as LOG2(x) * ln(2) since the runtime has UOP_LOG2 but no UOP_LOG.";
TCrossEntropyLoss::usage = "TCrossEntropyLoss[pred, target] = -sum(target * log(pred)).  Probability-form categorical cross-entropy.  Both inputs are TTerms with the same shape; target is typically a one-hot vector.  Forward only -- LOG2 has no grad rule yet.";
TMaxPool2d::usage        = "TMaxPool2d[x] / TMaxPool2d[x, k] runs a non-overlapping kxk max-pool over the trailing two axes of a rank-3 channels-first tensor {C, H, W}.  Default k=2.  Lowered as RESHAPE {C, H, W} -> {C, H/k, k, W/k, k} + two REDUCE_MAX over the inserted k-axes.  H and W must be divisible by k.";
TLayerNorm::usage        = "TLayerNorm[x] / TLayerNorm[x, eps] normalises along the last axis: y = (x - mean) / sqrt(var + eps).  Default eps=1e-5.  mean / var are scalar reductions broadcast back via the softmax-style reduce-broadcast pattern; the scheduler's relaxation pass collapses each reduce + its broadcast tail into one kernel where it can.";
TSoftmaxAxis::usage      = "TSoftmaxAxis[x, axis] = exp(x) / sum(exp(x)) reduced along `axis`.  Generalises TSoftmax (which is hard-coded to axis 0).  The dropped axis is re-introduced as size 1 + EXPAND'd back so the elementwise mul has matching shapes; same softmax-style reduce-broadcast shape.";
TAttention::usage        = "TAttention[Q, K, V] computes scaled dot-product attention: softmax(Q @ K^T / sqrt(d_k)) @ V.  Q is {seq_q, d_k}; K is {seq_k, d_k}; V is {seq_k, d_v}.  Result is {seq_q, d_v}.  Pure assembly of TMatMul + TSoftmaxAxis + TMatMul; the two matmuls dispatch through cblas_sgemm.";

Begin["`Private`"];

(* === Tensor-method helpers ============================== *)

TSum[x_TTerm]                   := TUOpReduce[x, 0, "SUM"]
TSquare[x_TTerm]                := TUOpMul[x, x]
TDot[a_TTerm, b_TTerm]          := TSum[TUOpMul[a, b]]
TL2Loss[x_TTerm]                := TSum[TSquare[x]]
TMSELoss[pred_TTerm, tgt_TTerm] := TL2Loss[TUOpAdd[pred, TUOpNeg[tgt]]]
TReLU[x_TTerm]                  := TUOpMul[x, TUOpCmplt[TUOpConst[0.0, "f32"], x]]

(* tanh(x) = (e^(2x) - 1) / (e^(2x) + 1).
   We have EXP2 (= 2^x) but not exp (= e^x).
   exp(y) = 2^(y * log2 e), so e^(2x) = EXP2(x * 2 * log2 e). *)
TTanh[x_TTerm] := With[{
    twoLog2E = TUOpConst[N[2 * Log2[E]], "f32"],
    one      = TUOpConst[1.0, "f32"]
},
    With[{u = TUOpExp2[TUOpMul[x, twoLog2E]]},
        TUOpMul[
            TUOpAdd[u, TUOpNeg[one]],
            TUOpRecip[TUOpAdd[u, one]]
        ]
    ]
]

(* exp(x) via EXP2: exp(x) = 2^(x * log2 e). *)
tExp[x_TTerm] := TUOpExp2[TUOpMul[x, TUOpConst[N[Log2[E]], "f32"]]]

(* softmax(x)_i = exp(x_i) / sum(exp(x)).  Sum reduces over axis 0
   (rank-1 inputs only for v1 -- the LeNet path produces a rank-1
   class-score vector at the SoftmaxLayer position).  The RECIP of
   the scalar sum is EXPLICITLY broadcast to e's shape via
   TUOpExpand rather than relying on the kernel-level numel-cycle
   broadcast in MUL.  Without the explicit EXPAND the chain rule
   for MUL has no notion of the implicit broadcast and propagates
   the cotangent for the RECIP-branch as `e * lifted_gy` (shape
   {N}) instead of `sum(e * lifted_gy)` (shape {1}), losing the
   probs-i cross-coupling term that makes
       d(CE)/dz = probs - target
   come out correctly with one-hot targets. *)
TSoftmax[x_TTerm] := With[{e = tExp[x], shape = tUopShape[x]},
    TUOpMul[e,
        TUOpExpand[
            TUOpRecip[TUOpReduce[e, 0, "SUM"]],
            shape]]
]

(* log(x) = log2(x) * ln(2).  Runtime has UOP_LOG2 but no UOP_LOG. *)
TLog[x_TTerm] := TUOpMul[TUOpLog2[x], TUOpConst[N[Log[2]], "f32"]]

(* CrossEntropy "probabilities" form: target is a probability
   distribution (typically one-hot), pred is the model's predicted
   distribution.  Loss = -sum_i target_i * log(pred_i).  Returns a
   scalar TTerm (rank-{1}). *)
TCrossEntropyLoss[pred_TTerm, target_TTerm] :=
    TUOpNeg[TUOpReduce[TUOpMul[target, TLog[pred]], 0, "SUM"]]

TMatVec[w_TTerm, x_TTerm] := With[{shapeW = TTensorShape[w]},
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
TMatMul[a_TTerm, b_TTerm] := With[{
    shapeA = tUopShape[a],
    shapeB = tUopShape[b]
},
    Module[{m, k, n, ab, bb},
        m = shapeA[[1]];
        k = shapeA[[2]];
        n = shapeB[[2]];
        ab = TUOpExpand[TUOpReshape[a, {m, k, 1}], {m, k, n}];
        bb = TUOpExpand[TUOpReshape[b, {1, k, n}], {m, k, n}];
        TUOpReduce[TUOpMul[ab, bb], 1, "SUM"]
    ]
]

(* TMaxPool2d[x, k] -- non-overlapping kxk max-pool over the last
   two axes of {C, H, W}.  Reshape {C, H, W} -> {C, H/k, k, W/k, k}
   is contiguity-preserving (memory layout of {C, H, W} row-major
   IS {C, H/k, k, W/k, k} row-major when H = (H/k)*k and W likewise),
   so EXPAND/PERMUTE aren't needed -- a plain RESHAPE plus two
   REDUCE_MAX on the inserted k-axes does it.  Reduce axis 2 first
   (so axis 3 in the rank-5 shape -- the second k -- shifts down to
   axis 3 in the resulting rank-4 shape, ready for the second reduce). *)
TMaxPool2d[x_TTerm] := TMaxPool2d[x, 2]
TMaxPool2d[x_TTerm, k_Integer] := With[{shape = tUopShape[x]},
    Module[{c, h, w},
        c = shape[[1]];
        h = shape[[2]];
        w = shape[[3]];
        TUOpReduce[
            TUOpReduce[
                TUOpReshape[x, {c, h/k, k, w/k, k}],
                2, "MAX"],
            3, "MAX"]
    ]
]

(* TLayerNorm[x, eps] -- normalize along the last axis.
       mean    = sum(x)         / N        scalar (per "row" if rank > 1)
       var     = sum((x-mean)^2) / N
       y       = (x - mean) / sqrt(var + eps)

   For a rank-1 input {N}, both reductions collapse to true scalars.
   For higher rank, reductions drop the last axis and we reshape +
   EXPAND back to broadcast across that axis (same shape pattern as
   TSoftmaxAxis below).

   Two REDUCEs (mean + var) means the Phase-3 single-REDUCE relaxation
   doesn't fire; layernorm currently materializes mean as its own
   kernel.  The variance + normalise tail then fuses into one kernel
   via the same softmax-style relaxation when var's broadcast chain
   ends in an EXPAND (which it does -- (x - mean) feeds into a
   square-sum-recip-sqrt-broadcast-mul). *)
TLayerNorm[x_TTerm] := TLayerNorm[x, 1.0*^-5]
TLayerNorm[x_TTerm, eps_?NumericQ] := With[{shape = tUopShape[x]},
    Module[{rank, axis, n, sumShape, recipN, mean, centered, var, denom},
        rank     = Length[shape];
        axis     = rank - 1;                         (* last axis *)
        n        = shape[[rank]];
        sumShape = ReplacePart[shape, rank -> 1];    (* for re-broadcast *)
        recipN   = TUOpConst[N[1.0/n], "f32"];
        (* mean = (sum(x) / N), broadcast back to input shape *)
        mean = TUOpExpand[
            TUOpReshape[
                TUOpMul[TUOpReduce[x, axis, "SUM"],
                        TUOpExpand[recipN, ReplacePart[shape, rank -> 1]]],
                sumShape],
            shape];
        centered = TUOpAdd[x, TUOpNeg[mean]];
        var = TUOpExpand[
            TUOpReshape[
                TUOpMul[TUOpReduce[TUOpMul[centered, centered], axis, "SUM"],
                        TUOpExpand[recipN, ReplacePart[shape, rank -> 1]]],
                sumShape],
            shape];
        denom = TUOpRecip[TUOpSqrt[
            TUOpAdd[var, TUOpExpand[TUOpConst[N[eps], "f32"], shape]]]];
        TUOpMul[centered, denom]
    ]
]

(* TSoftmaxAxis[x, axis] -- softmax along an arbitrary axis of a
   rank-N tensor.  Generalises TSoftmax (axis = 0 only).  The
   reduced axis is re-introduced as a unit dim via TUOpReshape so
   TUOpExpand can broadcast back.

   For axis = 0 on a rank-1 input the math reduces to TSoftmax: the
   sumShape is {1}, the reshape is contig, the expand fans the
   scalar back to {N}.  Higher-rank cases (e.g. attention's row-wise
   softmax over the last axis of a {seq_q, seq_k} score matrix) use
   the same code path. *)
TSoftmaxAxis[x_TTerm, axis_Integer] := With[{shape = tUopShape[x]},
    Module[{e, sumShape, r, recipBroadcast},
        e        = tExp[x];
        sumShape = ReplacePart[shape, axis + 1 -> 1];
        r        = TUOpReduce[e, axis, "SUM"];
        recipBroadcast =
            TUOpExpand[TUOpRecip[TUOpReshape[r, sumShape]], shape];
        TUOpMul[e, recipBroadcast]
    ]
]

(* TAttention[Q, K, V] -- scaled dot-product attention.
       Q : {seq_q, d_k}
       K : {seq_k, d_k}
       V : {seq_k, d_v}
       output = softmax(Q K^T / sqrt(d_k)) V    : {seq_q, d_v}

   K^T comes from a TUOpPermute axes={1,0}; the resulting
   non-contiguous view feeds straight into TMatMul, which materialises
   the shape via the standard RESHAPE+EXPAND pattern.  Both matmuls
   dispatch through cblas_sgemm.  Softmax-along-last-axis is the
   row-wise normalisation each query needs. *)
TAttention[q_TTerm, k_TTerm, v_TTerm] := With[{
    shapeQ = tUopShape[q],
    shapeK = tUopShape[k]
},
    Module[{seqQ, seqK, dk, scale, kT, scores, attn},
        seqQ  = shapeQ[[1]];
        seqK  = shapeK[[1]];
        dk    = shapeK[[2]];
        scale = TUOpConst[N[1.0/Sqrt[dk]], "f32"];
        (* K^T : {seq_k, d_k} -> {d_k, seq_k}.  Non-contiguous view;
           TMatMul's RESHAPE forces materialization through the view-
           strided fast path the CPU/Metal interpreters already handle. *)
        kT     = TUOpPermute[k, {1, 0}];
        scores = TUOpMul[TMatMul[q, kT],
                         TUOpExpand[scale, {seqQ, seqK}]];
        attn   = TSoftmaxAxis[scores, 1];
        TMatMul[attn, v]
    ]
]

(* === Wolfram-layer parameter access ===================== *)

(* Parameter names for each layer kind, in canonical declaration
   order.  Adding a new layer means adding an entry here + a
   fromLayer[<head>, ...] forward rule below. *)
$layerParams[LinearLayer]      = {"Weights", "Biases"}
$layerParams[ConvolutionLayer] = {"Weights", "Biases"}
$layerParams[ElementwiseLayer] = {}
$layerParams[ReshapeLayer]     = {}
$layerParams[FlattenLayer]     = {}
$layerParams[PoolingLayer]     = {}
$layerParams[SoftmaxLayer]     = {}
$layerParams[_]                = {}

TLayerWeights[layer_] :=
    NetExtract[layer, #] & /@ $layerParams[Head[layer]]

TLayerToTensors[layer_] :=
    TTensorCreate /@ TLayerWeights[layer]

(* === forward dispatch =================================== *)

(* Promote a rank-1 input {in} to {1, in} so TMatVec's EXPAND has
   the rank it needs.  Returns x unchanged if it's already at least
   rank 2.  Uses tUopShape so it works on intermediate UOP terms
   (post-Flatten in a chain), and TUOpReshape for the rank bump --
   no TTensorData copy required. *)
asRowVec[x_TTerm] := With[{shape = tUopShape[x]},
    If[ Length[shape] === 1,
        TUOpReshape[x, {1, shape[[1]]}],
        x
    ]
]

fromLayer[LinearLayer, layer_, x_TTerm] := Module[{w, b, x2},
    {w, b} = TLayerToTensors[layer];
    x2 = asRowVec[x];
    TUOpAdd[TMatVec[w, x2], b]
]

(* ElementwiseLayer is dispatch-by-function.  Tinygrad-style we map
   the Wolfram function symbol back to one of our supported UOp
   constructors.  Functions whose gradient interact_grad doesn't yet
   handle (e.g. Ramp = ReLU, sigmoid via Exp) raise a Message. *)
$elementwiseDispatch = <|
    Identity       -> TIdentity,
    (#1 #1 &)      -> TSquare,
    (-#1 &)        -> TUOpNeg,
    Ramp           -> TReLU,
    Tanh           -> TTanh
|>;

(* ReshapeLayer[shape]: target shape via `NetExtract[layer, "Output"]`.
   Note that for a rank-1 target (e.g. ReshapeLayer[{6}]) NetExtract
   returns a bare Integer; wrap into {n} so TUOpReshape always sees
   a dim list.  Forward only; no grad rule for RESHAPE yet. *)
fromLayer[ReshapeLayer, layer_, x_TTerm] :=
    With[{out = NetExtract[layer, "Output"]},
        TUOpReshape[x, If[ListQ[out], out, {out}]]
    ]

(* FlattenLayer[]: collapse the entire input to rank-1.  Sizes the
   output via tUopShape so it works on intermediate UOP terms in a
   chain (an uninitialised FlattenLayer reports {Automatic}, useless
   for sizing).  Forward only. *)
fromLayer[FlattenLayer, _, x_TTerm] :=
    With[{shape = tUopShape[x]},
        TUOpReshape[x, {Times @@ shape}]
    ]

(* SoftmaxLayer: forward only, last-axis softmax via TSoftmax helper. *)
fromLayer[SoftmaxLayer, _, x_TTerm] := TSoftmax[x]

(* PoolingLayer non-overlapping (Stride == KernelSize), 2-D Max only.
   Channels-first input shape {C, H, W} ->
       reshape {C, H, W} -> {C, H/kh, kh, W/kw, kw}
       REDUCE axis 2 (kh) -> {C, H/kh, W/kw, kw}
       REDUCE axis 3 (kw, now innermost) -> {C, H/kh, W/kw}
   Refuses overlapping pooling, non-Max functions, or non-rank-3
   inputs with a Failure -- those need PERMUTE / multi-axis grad
   support not yet present. *)
fromLayer[PoolingLayer, layer_, x_TTerm] := Module[{
    kSize, stride, fn, shape, c, h, w, kh, kw
},
    fn     = NetExtract[layer, "Function"];
    kSize  = NetExtract[layer, "KernelSize"];
    stride = NetExtract[layer, "Stride"];
    If[ fn =!= Max,
        Return @ Failure["NotImplemented",
            <|"Message" -> "PoolingLayer Function != Max not yet supported",
              "fn" -> fn|>]];
    If[ kSize =!= stride,
        Return @ Failure["NotImplemented",
            <|"Message" -> "PoolingLayer overlapping (Stride != KernelSize) not yet supported",
              "KernelSize" -> kSize, "Stride" -> stride|>]];
    shape = tUopShape[x];
    If[ Length[shape] =!= 3,
        Return @ Failure["NotImplemented",
            <|"Message" -> "PoolingLayer expects rank-3 channels-first input {C, H, W}",
              "InputShape" -> shape|>]];
    {c, h, w} = shape;
    {kh, kw}  = kSize;
    If[ Mod[h, kh] =!= 0 || Mod[w, kw] =!= 0,
        Return @ Failure["NotImplemented",
            <|"Message" -> "PoolingLayer non-overlap requires H, W divisible by kernel",
              "InputShape" -> shape, "KernelSize" -> kSize|>]];
    If[ kh =!= kw,
        Return @ Failure["NotImplemented",
            <|"Message" -> "PoolingLayer non-square kernel sizes need a separate path",
              "KernelSize" -> kSize|>]];
    TMaxPool2d[x, kh]
]

fromLayer[ElementwiseLayer, layer_, x_TTerm] := Module[{f, op},
    f  = NetExtract[layer, "Function"];
    op = Lookup[$elementwiseDispatch, f, None];
    If[ op === None,
        Message[TFromNet::eltunsupported, f]; $Failed,
        op[x]
    ]
]

(* ConvolutionLayer 2-D, stride 1, no padding/dilation.  Forward
   only -- interact_grad has no rule for UOP_CONV2D yet.  Inputs
   are channels-first {C_in, H, W}; weights {C_out, C_in, kh, kw};
   bias {C_out}; output {C_out, H-kh+1, W-kw+1}.

   Refuses (with Failure) configurations the kernel doesn't yet
   support: non-1 stride, any padding, non-1 dilation. *)
fromLayer[ConvolutionLayer, layer_, x_TTerm] := Module[{
    w, b, stride, pad, dil
},
    stride = NetExtract[layer, "Stride"];
    pad    = NetExtract[layer, "PaddingSize"];
    dil    = NetExtract[layer, "Dilation"];
    If[ stride =!= {1, 1},
        Return @ Failure["NotImplemented",
            <|"Message" -> "ConvolutionLayer Stride != {1,1} not yet supported",
              "Stride" -> stride|>]];
    If[ pad =!= {{0,0},{0,0}} && pad =!= 0,
        Return @ Failure["NotImplemented",
            <|"Message" -> "ConvolutionLayer PaddingSize != 0 not yet supported",
              "PaddingSize" -> pad|>]];
    If[ dil =!= {1, 1},
        Return @ Failure["NotImplemented",
            <|"Message" -> "ConvolutionLayer Dilation != {1,1} not yet supported",
              "Dilation" -> dil|>]];
    {w, b} = TLayerToTensors[layer];
    TUOpConv2D[x, w, b]
]

TIdentity[x_] := x

(* === entry points ====================================== *)

TFromLayer[layer_, x_TTerm] := fromLayer[Head[layer], layer, x]

TFromNet[chain_NetChain, x_TTerm] := Fold[
    TFromLayer[#2, #1] &,
    x,
    Table[chain[[i]], {i, Length[chain]}]
]

TFromNet[layer_, x_TTerm] := TFromLayer[layer, x]

TFromNet::eltunsupported = "ElementwiseLayer with function `1` has no UOp equivalent yet (interact_grad would need a corresponding grad rule).";
TFromNet::convtbd        = "ConvolutionLayer conversion not yet implemented (`1`).  Step 14 task: needs movement-op support in materialize/interpret + the matching grad rules.";

End[];

EndPackage[];
