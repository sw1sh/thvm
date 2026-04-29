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
TLinear::usage          = "TLinear[x, W, b] computes x @ W + b where x has shape {..., M, K}, W has shape {K, N}, and b has shape {N} (or None for bias-free).  The bias is reshaped to {1,...,1,N} and EXPAND'd to the matmul output shape; without the explicit EXPAND the elementwise numel-cycle aligns only the first leading-axis row and writes garbage into the rest.  Standard nn.Linear analogue (tinygrad's Tensor.linear).";
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
TBatchNorm::usage        = "TBatchNorm[x, gamma, beta, mean, var] / TBatchNorm[x, gamma, beta, mean, var, eps] applies the inference-form batch-norm transform: y = gamma * (x - mean) / sqrt(var + eps) + beta along the channel axis.  x is rank-3 {C, H, W} (channels-first); gamma/beta/mean/var are all rank-1 {C}.  Default eps = 1e-5.  The trainer maintains running mean/var with TAssign on the WL side; this op is the pure forward shape used by both train and infer.";
TConv2D::usage           = "TConv2D[input, weights, bias] builds a stride-1, no-padding 2-D convolution as a kh*kw-unrolled chain of SHRINK + RESHAPE + EXPAND + MUL + REDUCE_SUM + ADD primitives.  input shape {C_in, H, W}; weights {C_out, C_in, kh, kw}; bias {C_out}; output {C_out, H-kh+1, W-kw+1}.  No bespoke CONV2D opcode -- autograd flows through primitives via the chain rule.  Phase 9 follow-up: replace the kh*kw partials with a single im2col + sgemm dispatch.";
TGlorot::usage           = "TGlorot[shape] returns a NumericArray of the given shape, filled with samples from N(0, sqrt(2 / fan_in)) (Glorot/Xavier-He init).  fan_in = product of all dims after the first.  Suitable for ReLU / linear layer weight init.";
TZeros::usage            = "TZeros[shape] returns a Real32 NumericArray of zeros at the given shape.  Convenience for bias init / running-stat init.";
TOnes::usage             = "TOnes[shape] returns a Real32 NumericArray of ones at the given shape.  Convenience for layer-norm gamma init / scale-1 placeholders.";
TZerosLike::usage        = "TZerosLike[t] returns a TTensor handle of zeros matching the shape and dtype of TTerm `t`.  Suitable for seeding Adam m/v moment buffers.";
TOneHot::usage           = "TOneHot[label, n] returns a Real32 NumericArray of length n with a 1.0 at index `label` (0-indexed) and 0.0 elsewhere.  Convenience for sparse-categorical-CE targets.";

TSparseCategoricalCrossEntropy::usage = "TSparseCategoricalCrossEntropy[logits, targetOneHot] computes the categorical cross-entropy loss given pre-softmax logits and a one-hot target.  Lowers to log(sum(exp(logits))) - sum(target * logits) along the last axis, then averages over the leading batch axis (rank-1 logits have no batch dim and skip the average).  Numerically naive (no max-subtract); for the f32 inputs the MNIST training pipeline produces the magnitudes stay well within range.";

TEmbedding::usage        = "TEmbedding[table, idx] returns row idx of a {V, D} table as a TTerm of shape {D}.  idx is a host-side Integer; lowers as TUOpShrink[table, {{idx, idx+1}, {0, D}}] + TUOpReshape to {D}.  Dynamic-idx gather (idx as a runtime tensor) needs a future UOP_GATHER opcode.";
TEmbeddingMatrix::usage  = "TEmbeddingMatrix[table, ids] gathers rows `ids` from a {V, D} table into a {Length[ids], D} matrix.  ids is a host-side List[Integer].  Lowers as one TEmbedding + TUOpReshape to {1, D} per id, then stitches along the leading axis via PAD + sum (same idiom as TMultiHeadAttention's per-head concat -- no STACK op in thvm).  Dynamic-id gather needs a future UOP_GATHER opcode.";
TGELU::usage             = "TGELU[x] applies the tanh-form GELU approximation: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3))).  Composes via existing TTanh -- no new opcode required.";
TCausalMask::usage       = "TCausalMask[seq] returns a {seq, seq} TTerm wrapping a fresh tensor whose entries are 0 at (i, j) with j <= i and -1e9 at j > i.  Add to attention scores before softmax to zero out future-token attention.";
TLayerNormAffine::usage  = "TLayerNormAffine[x, gamma, beta] applies TLayerNorm[x] then multiplies by gamma + adds beta along the last axis.  GPT-2's layer-norm carries learned gamma/beta; the bare TLayerNorm in this file normalises only.";
TMultiHeadAttention::usage = "TMultiHeadAttention[Q, K, V, n_heads, mask] computes multi-head scaled dot-product attention.  Q, K, V are {seq, dim} TTerms with dim = n_heads * d_head; mask is a {seq, seq} additive bias (use TCausalMask) or None.  Splits each projection to {n_heads, seq, d_head} via reshape + permute, runs scaled-dot per head, concatenates back to {seq, dim}.  Per-head loop today (batched sgemm is a Phase 12 follow-up).";

Begin["`Private`"];

(* === Tensor-method helpers ============================== *)
(*
   These are thin aliases on top of the WL UpValues installed by
   Tensor.wl.  Plus / Times / Power[..., -1] / Power[..., 1/2] / Sqrt
   / Exp / Log / Total all operate on TTerm tensor values directly,
   so the helpers below read like ordinary math rather than UOp
   constructor soup.
*)

TSum[x_TTerm]                   := Total[x]
TSquare[x_TTerm]                := x * x
TDot[a_TTerm, b_TTerm]          := Total[a * b]
TL2Loss[x_TTerm]                := Total[x * x]
TMSELoss[pred_TTerm, tgt_TTerm] := TL2Loss[pred - tgt]
TReLU[x_TTerm]                  := x * (0 < x)

(* tanh(x) = (e^(2x) - 1) / (e^(2x) + 1). *)
TTanh[x_TTerm] := With[{u = Exp[2 * x]}, (u - 1) / (u + 1)]

(* TLog is kept as a public alias; new code should write Log[x]. *)
TLog[x_TTerm] := Log[x]

(* softmax(x)_i = exp(x_i) / sum(exp(x)).  The RECIP of the scalar sum
   is EXPLICITLY broadcast to e's shape via TUOpExpand rather than
   relying on the kernel-level numel-cycle broadcast in MUL: without
   the explicit EXPAND the chain rule for MUL has no notion of the
   implicit broadcast and drops the cross-coupling term in
       d(CE)/dz = probs - target. *)
TSoftmax[x_TTerm] := With[{e = Exp[x], shape = tUopShape[x]},
    e * TUOpExpand[1 / Total[e], shape]
]

(* CrossEntropy probabilities form: target is a probability
   distribution (typically one-hot), pred is the model's predicted
   distribution.  Loss = -sum_i target_i * log(pred_i). *)
TCrossEntropyLoss[pred_TTerm, target_TTerm] := -Total[target * Log[pred]]

(* TConv2D[input, weights, bias] -- stride-1, no-padding 2-D
   convolution, lowered into a chain of kh*kw partial sums:

       For each (ki, kj) in [0, kh) x [0, kw):
         x_slice = SHRINK(input,   all C_in,  [ki, ki + H_out), [kj, kj + W_out))
         w_slice = SHRINK(weights, all C_out, all C_in,
                                   [ki, ki + 1), [kj, kj + 1))
         x_b     = EXPAND(RESHAPE(x_slice, {1, C_in, H_out, W_out}),
                                 {C_out, C_in, H_out, W_out})
         w_b     = EXPAND(w_slice, {C_out, C_in, H_out, W_out})
         partial = REDUCE_SUM(x_b * w_b, axis = 1)
                                                     {C_out, H_out, W_out}
       sum the kh*kw partials, broadcast bias, return.

   Why kh*kw partials (rather than tinygrad's _pool unfold): the
   unfolded tensor would have shape {C_in, H_out, W_out, kh, kw}
   which for a 28x28 -> 24x24 conv with kh=kw=5 is 24*24*25 = 14400
   elements per channel; the partial-sum form only allocates a few
   {C_out, C_in, H_out, W_out} intermediates per kernel position,
   which fits the per-op-allocates-a-buffer materializer better.
   Phase 9 follow-up: replace with one im2col + sgemm dispatch. *)
TConv2D[input_TTerm, weights_TTerm, bias_TTerm] := Module[{
    inShape, wShape, cIn, cOut, h, wd, kh, kw, hOut, wOut,
    partials, xSlice, wSlice, xB, wB, summed, biasBcast
},
    inShape = tUopShape[input];
    wShape  = tUopShape[weights];
    {cIn, h, wd}        = inShape;
    {cOut, cIn, kh, kw} = wShape;
    hOut = h  - kh + 1;
    wOut = wd - kw + 1;
    partials = Flatten @ Table[
        xSlice = TUOpShrink[input,
            {{0, cIn}, {ki, ki + hOut}, {kj, kj + wOut}}];
        wSlice = TUOpShrink[weights,
            {{0, cOut}, {0, cIn}, {ki, ki + 1}, {kj, kj + 1}}];
        xB = TUOpExpand[
            TUOpReshape[xSlice, {1, cIn, hOut, wOut}],
            {cOut, cIn, hOut, wOut}];
        wB = TUOpExpand[wSlice, {cOut, cIn, hOut, wOut}];
        TUOpReduce[xB * wB, 1, "SUM"],
        {ki, 0, kh - 1}, {kj, 0, kw - 1}
    ];
    summed = Fold[Plus, First[partials], Rest[partials]];
    biasBcast = TUOpExpand[
        TUOpReshape[bias, {cOut, 1, 1}],
        {cOut, hOut, wOut}];
    summed + biasBcast
]

(* === host-side init helpers for example scripts ============== *)

TGlorot[shape_List] := NumericArray[
    RandomVariate[
        NormalDistribution[0., Sqrt[2.0 / If[Length[shape] >= 2,
            Times @@ Drop[shape, 1], shape[[1]]]]],
        shape],
    "Real32"]

TZeros[shape_List] := NumericArray[ConstantArray[0., shape], "Real32"]

TOnes[shape_List] := NumericArray[ConstantArray[1., shape], "Real32"]

TZerosLike[t_TTerm] := TTensorCreate @ TZeros[TTensorShape[t]]

TOneHot[label_Integer, n_Integer] := NumericArray[
    Table[If[i - 1 == label, 1.0, 0.0], {i, n}],
    "Real32"]

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

(* TLinear[x, W, b] -- standard nn.Linear forward = x @ W + b.
   The bias is rank-1 {N}; reshape to {1, ..., 1, N} and EXPAND
   to the matmul's output shape.  Without the explicit EXPAND,
   {seq,N} + {N} via the elementwise numel-cycle aligns only the
   first row and writes garbage into the rest.  The TLayerNorm
   reduction-broadcast pattern uses the same EXPAND trick. *)
TLinear[x_TTerm, w_TTerm, b_TTerm] := With[{
    shapeX = tUopShape[x],
    shapeW = tUopShape[w]
},
    Module[{out, bcast, prod, biasShape},
        prod = TMatMul[x, w];
        out = ReplacePart[shapeX, Length[shapeX] -> shapeW[[2]]];
        biasShape = ConstantArray[1, Length[out]];
        biasShape[[Length[out]]] = shapeW[[2]];
        bcast = TUOpExpand[TUOpReshape[b, biasShape], out];
        prod + bcast
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
    Module[{rank, axis, n, sumShape, broadcast, mean, centered, var},
        rank      = Length[shape];
        axis      = rank - 1;
        n         = shape[[rank]];
        sumShape  = ReplacePart[shape, rank -> 1];
        broadcast = TUOpExpand[TUOpReshape[#, sumShape], shape] &;
        mean      = broadcast[TUOpReduce[x, axis, "SUM"] / n];
        centered  = x - mean;
        var       = broadcast[TUOpReduce[centered * centered, axis, "SUM"] / n];
        centered / Sqrt[var + eps]
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
    Module[{e, sumShape},
        e        = Exp[x];
        sumShape = ReplacePart[shape, axis + 1 -> 1];
        e * TUOpExpand[1 / TUOpReshape[TUOpReduce[e, axis, "SUM"], sumShape], shape]
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
(* TBatchNorm[x, gamma, beta, mean, var, eps] -- inference form.
       y[c, h, w] = gamma[c] * (x[c, h, w] - mean[c]) / sqrt(var[c] + eps)
                  + beta[c]
   Refactored so the per-channel scale/shift collapse to scalar
   computations once each: scale = gamma / sqrt(var + eps),
   shift = beta - mean * scale.  Then both broadcast across {H, W}
   as one MUL + one ADD on the rank-3 tensor.  Rank-1 ops on {C}
   stay tiny; the rank-3 ops fuse cleanly with the JIT path. *)
TBatchNorm[x_TTerm, gamma_TTerm, beta_TTerm, mean_TTerm, var_TTerm] :=
    TBatchNorm[x, gamma, beta, mean, var, 1.0*^-5]
TBatchNorm[x_TTerm, gamma_TTerm, beta_TTerm, mean_TTerm, var_TTerm,
           eps_?NumericQ] := With[{shape = tUopShape[x]},
    Module[{c, h, w, scaleC, shiftC, scaleBcast, shiftBcast},
        c = shape[[1]];
        h = shape[[2]];
        w = shape[[3]];
        (* Per-channel scale + shift collapse to scalar computations
           over the rank-1 {C} parameter tensors -- Plus / Times /
           Sqrt UpValues lift the scalar `eps` through liftNumeric. *)
        scaleC     = gamma / Sqrt[var + eps];
        shiftC     = beta  - mean * scaleC;
        scaleBcast = TUOpExpand[TUOpReshape[scaleC, {c, 1, 1}], {c, h, w}];
        shiftBcast = TUOpExpand[TUOpReshape[shiftC, {c, 1, 1}], {c, h, w}];
        x * scaleBcast + shiftBcast
    ]
]

(* TSparseCategoricalCrossEntropy[logits, targetOneHot] -- categorical
   cross-entropy from logits.  loss = log(sum(exp(logits))) -
   sum(target * logits), reduced along the LAST axis (the class axis);
   for rank-2 batched inputs the per-sample loss is then averaged
   along axis 0.  Numerically naive (no max-subtract); fine for the
   activation magnitudes a typical NN forward produces. *)
TSparseCategoricalCrossEntropy[logits_TTerm, target_TTerm] := With[{
    shape = tUopShape[logits]
},
    Module[{rank, classAxis, perSample},
        rank      = Length[shape];
        classAxis = rank - 1;
        perSample = Log[TUOpReduce[Exp[logits], classAxis, "SUM"]]
                  - TUOpReduce[target * logits, classAxis, "SUM"];
        If[ rank <= 1,
            perSample,
            Total[perSample] / shape[[1]]    (* batch mean over axis 0 *)
        ]
    ]
]

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

(* === GPT-2 building blocks ==================================
   The five entries below are pure WL composition over the layers
   above (TLayerNorm, TMatMul, TSoftmaxAxis, TTanh) plus existing
   movement primitives.  Modeled on tinygrad's `extra/models/gpt2.py`
   surface but expressed in thvm's `T*` flavor.  No new C/IR work. *)

TEmbedding[table_TTerm, idx_Integer] := With[{shape = tUopShape[table]},
    Module[{d},
        d = shape[[2]];
        TUOpReshape[
            TUOpShrink[table, {{idx, idx + 1}, {0, d}}],
            {d}]
    ]
]

TEmbeddingMatrix[table_TTerm, ids_List] := With[{
    shape = tUopShape[table]
},
    Module[{d, len, rows, padded},
        d   = shape[[2]];
        len = Length[ids];
        rows = TUOpReshape[TEmbedding[table, #], {1, d}] & /@ ids;
        padded = MapIndexed[
            TUOpPad[#1, {{First[#2] - 1, len - First[#2]}, {0, 0}}] &,
            rows];
        Fold[Plus, First[padded], Rest[padded]]
    ]
]

TGELU[x_TTerm] := With[{
    sqrt2OverPi = Sqrt[2.0 / Pi]            (* ~ 0.7978845608... *),
    halfCubeC   = 0.044715
},
    (* x * x * x rather than x^3: there's no Power[t_TTerm, n_Integer]
       UpValue for n != 1/2, -1, so x^3 would stay unevaluated. *)
    0.5 * x * (1 + TTanh[sqrt2OverPi * (x + halfCubeC * (x * x * x))])
]

TCausalMask[seq_Integer] := TTensorCreate @ NumericArray[
    Table[ If[ j <= i, 0.0, -1.0*^9], {i, seq}, {j, seq}],
    "Real32"]

TLayerNormAffine[x_TTerm, gamma_TTerm, beta_TTerm] := With[{
    shape = tUopShape[x],
    norm  = TLayerNorm[x]
},
    Module[{rank, gammaB, betaB, broadcastShape},
        rank = Length[shape];
        broadcastShape = ConstantArray[1, rank];
        broadcastShape[[rank]] = shape[[rank]];
        (* Reshape rank-1 {D} -> {1,...,1,D} then EXPAND to full
           shape.  Without the explicit EXPAND, the elementwise
           numel-cycle in MUL/ADD reads gamma/beta in the wrong
           stride pattern across the leading axes and silently
           produces denormals.  Tested at seq=4, dim=16: bare
           {seq,dim}*{1,dim} returns garbage; EXPAND[{1,dim} ->
           {seq,dim}] is correct. *)
        gammaB = TUOpExpand[TUOpReshape[gamma, broadcastShape], shape];
        betaB  = TUOpExpand[TUOpReshape[beta,  broadcastShape], shape];
        norm * gammaB + betaB
    ]
]

TMultiHeadAttention[q_TTerm, k_TTerm, v_TTerm,
                    nHeads_Integer, mask_:None] := With[{
    shapeQ = tUopShape[q]
},
    Module[{seq, dim, dHead, qH, kH, vH, perHead, headOuts},
        seq   = shapeQ[[1]];
        dim   = shapeQ[[2]];
        dHead = dim / nHeads;
        (* {seq, dim} -> {seq, nHeads, dHead} -> {nHeads, seq, dHead} *)
        qH = TUOpPermute[TUOpReshape[q, {seq, nHeads, dHead}], {1, 0, 2}];
        kH = TUOpPermute[TUOpReshape[k, {seq, nHeads, dHead}], {1, 0, 2}];
        vH = TUOpPermute[TUOpReshape[v, {seq, nHeads, dHead}], {1, 0, 2}];
        perHead[h_] := Module[{qSlice, kSlice, vSlice, scores, scoresM, attn},
            qSlice = TUOpReshape[
                TUOpShrink[qH, {{h, h + 1}, {0, seq}, {0, dHead}}],
                {seq, dHead}];
            kSlice = TUOpReshape[
                TUOpShrink[kH, {{h, h + 1}, {0, seq}, {0, dHead}}],
                {seq, dHead}];
            vSlice = TUOpReshape[
                TUOpShrink[vH, {{h, h + 1}, {0, seq}, {0, dHead}}],
                {seq, dHead}];
            scores = TMatMul[qSlice, TUOpPermute[kSlice, {1, 0}]] /
                     Sqrt[N @ dHead];
            scoresM = If[ mask === None, scores, scores + mask];
            attn   = TSoftmaxAxis[scoresM, 1];
            TMatMul[attn, vSlice]
        ];
        headOuts = perHead /@ Range[0, nHeads - 1];
        headStitch[headOuts, seq, nHeads, dHead]
    ]
]

(* Stitch a list of nHeads {seq, dHead} TTerms into {seq, dim=
   nHeads*dHead} via per-head PAD into the corresponding column
   slice + sum.  No STACK op in thvm. *)
headStitch[heads_List, seq_, nHeads_, dHead_] := With[{
    padded = Table[
        TUOpPad[heads[[h + 1]],
            {{0, 0}, {h * dHead, (nHeads - h - 1) * dHead}}],
        {h, 0, nHeads - 1}]
},
    Fold[Plus, First[padded], Rest[padded]]
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
    TConv2D[x, w, b]
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
