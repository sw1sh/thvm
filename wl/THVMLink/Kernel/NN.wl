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
     TMatVec[W, x]               -- W:{out,in} @ x:{in} or {1,in} -> {out}
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

TFromNet::usage         = "TFromNet[net, x] converts a Wolfram NeuralNetworks layer (or NetChain / NetGraph) into a TTerm UOp graph rooted at the input TTerm `x`.  The net must be initialised so its weights are concrete arrays.\n\nTFromNet[net, ids] for a token-encoder net (a List[Integer] of 1-indexed token ids) traverses the real NetChain / NetGraph and returns the {seq, vocab} logits TTerm -- e.g. GPT-2 inference, with the tied token-embedding LM head appended.\n\nTFromNet[net] (no input arg) infers the input shape from the net's InputPorts and returns a TLam whose bound variable carries that shape annotation; the lifted forward runs at that input shape and serves inference there.\n\nTFromNet[net, onehot] for a token-LM NetChain, where `onehot` is a {maxSeq, vocab} one-hot TTerm, returns the fixed-sequence forward applied to it: the variable-length id gather is replaced by onehot . tokenTable so the WHOLE forward has a FIXED shape (maxSeq is the one-hot's own sequence length -- there is no integer window knob).  Build it ONCE under TJit -- step = TJit[TRealize @ TWnf @ TFromNet[net, #] &] -- and generate by passing each step's fresh one-hot; TJit replays the cached dispatch and rebinds the one-hot (positions 0..maxSeq-1 are constant; causal masking keeps the padded tail inert).  TFromNet[net] on a token-LM NetChain returns a reusable forward fwd such that fwd[onehot] is that applied forward.";
TToNet::usage           = "TToNet[lam] does a best-effort reconstruction of a NetChain from the GRAPH of a TFromNet[net]-built LAM `lam` -- no net is stored.  Recognises the standard layer sub-DAG signatures (Linear / Elementwise(ReLU) / Softmax / Flatten), reading each layer's weights straight off the graph leaves; NetChain infers the input dimension from the first Linear layer's weight.  Returns a Failure for a body containing a conv / pool / unrecognised op (their im2col lowering does not round-trip cleanly).";
TNetParams::usage       = "TNetParams[lam] returns the trainable weight handles (float-leaf TEN TTerms) of a TFromNet[net] LAM, in the C leaf-walk's stable order -- no Sow-provenance, no registry.  The LAM's bound input is a TAG_VAR (a hole), so it is excluded for free, leaving exactly the weights baked into the forward; TGrad auto-differentiates every reachable float leaf and TNetTrain updates them in place.  Parameters come from the network's structure, as in tinygrad's get_parameters: an applied forward TFromNet[net, x] (concrete input) is an evaluation with no distinguished parameters -- lift with TFromNet[net] instead.";
TNetParamInfo::usage    = "TNetParamInfo[lam] returns the trainable weight handles of a TFromNet[net] LAM paired with best-effort provenance, as a list of <|\"Term\", \"Layer\", \"Param\"|> associations.  Without a stored net the Layer is Missing[\"NotStored\"] and Param is inferred from the handle's shape (rank-2+ -> \"Weights\", rank-1 -> \"Biases\"); TNetParams[lam] is the same handles without provenance.";
TNetInitialize::usage   = "TNetInitialize[lam] re-initialises the trainable weights of a TFromNet[net] LAM in place: Glorot for weight matrices, ones for normalisation scalings, zeros for biases.  Installed as the NetInitialize UpValue on TTerms.";
TFromLayer::usage       = "TFromLayer[layer, x] is the single-layer form of TFromNet.";
TLayerWeights::usage    = "TLayerWeights[layer] returns the NumericArrays of every learnable parameter a layer carries (Weights, Biases, ...), in the layer-specific declaration order.";
TLayerToTensors::usage  = "TLayerToTensors[layer] is the same as TLayerWeights but each NumericArray is wrapped in a fresh TTensorCreate handle so it can feed back into a UOp graph.";
TSum::usage             = "TSum[x] = TUOpReduce[x, 0, \"SUM\"].";
TSquare::usage          = "TSquare[x] = TUOpMul[x, x].";
TDot::usage             = "TDot[a, b] = TSum[TUOpMul[a, b]].";
TMatVec::usage          = "TMatVec[W, x] computes W @ x where W has shape {out, in} and x has shape {in} (rank-1) or {1, in} (rank-2).  Result has shape {out}.  EXPAND-broadcast then REDUCE_SUM along the inner axis.";
TMatMul::usage          = "TMatMul[A, B] computes A @ B where A has shape {M, K} and B has shape {K, N}.  Result has shape {M, N}.  Lowered as RESHAPE + EXPAND to a common {M, K, N} shape, MUL elementwise, then REDUCE_SUM along axis 1.  cpu_blas_dispatch recognises this KProgOp[] pattern and routes to cblas_sgemm.";
TLinear::usage          = "TLinear[x, W, b] computes x @ W + b where x has shape {..., M, K}, W has shape {K, N}, and b has shape {N} (or None for bias-free).  The bias is reshaped to {1,...,1,N} and EXPAND'd to the matmul output shape; without the explicit EXPAND the elementwise numel-cycle aligns only the first leading-axis row and writes garbage into the rest.  Standard nn.Linear analogue (tinygrad's Tensor.linear).";
TL2Loss::usage          = "TL2Loss[x] = TSum[TSquare[x]].";
TMSELoss::usage         = "TMSELoss[pred, target] = TL2Loss[pred - target].";
TReLU::usage            = "TReLU[x] = elementwise max(x, 0), implemented as MUL[x, CMPLT[0, x]] -- the CMPLT mask broadcasts a CONST(0) against x and yields 1 where x > 0, else 0.";
TTanh::usage            = "TTanh[x] = elementwise tanh = 2*sigmoid(2x) - 1, where sigmoid(x) = 1/(1 + e^(-x)).  The negated exponent keeps it finite for all x (saturating to +-1) and fully differentiable -- the older (e^(2x)-1)/(e^(2x)+1) form returned NaN once 2x left the f32 exp range, which the cubic argument of tanh-form GELU triggers.  No UOP_TANH primitive needed.";
TSigmoid::usage         = "TSigmoid[x] = 1 / (1 + e^(-x)), the numerically stable logistic sigmoid (the exponent is negated so large |x| saturates to 0 or 1 instead of overflowing to NaN).  Matches tinygrad's sigmoid lowering.";
TSoftmax::usage         = "TSoftmax[x] = exp(x - max(x)) / sum(exp(x - max(x))) over axis 0.  exp via the EXP2 + log2(e) chain.  Numerically stable (max-subtract) and grad-correct: every reduce is re-broadcast with an explicit TUOpExpand so the SUB/MUL chain rule keeps the softmax cross-coupling term.";
TLog::usage             = "TLog[x] = elementwise natural log, implemented as LOG2(x) * ln(2) since the runtime has UOP_LOG2 but no UOP_LOG.";
TCrossEntropyLoss::usage = "TCrossEntropyLoss[pred, target] = -sum(target * log(pred)).  Probability-form categorical cross-entropy.  Both inputs are TTerms with the same shape; target is typically a one-hot vector.  Forward only -- LOG2 has no grad rule yet.";
TMaxPool2d::usage        = "TMaxPool2d[x] / TMaxPool2d[x, k] runs a non-overlapping kxk max-pool over the trailing two axes of a rank-3 {C, H, W} or rank-4 batched {B, C, H, W} channels-first tensor.  Default k=2.";
TLayerNorm::usage        = "TLayerNorm[x] / TLayerNorm[x, eps] normalises along the last axis: y = (x - mean) / sqrt(var + eps).  Default eps=1e-5.  mean / var are scalar reductions broadcast back via the softmax-style reduce-broadcast pattern; the scheduler's relaxation pass collapses each reduce + its broadcast tail into one kernel where it can.";
TSoftmaxAxis::usage      = "TSoftmaxAxis[x, axis] = exp(x - max(x)) / sum(exp(x - max(x))) reduced along `axis`.  Generalises TSoftmax (which is hard-coded to axis 0).  The dropped axis is re-introduced as size 1 + EXPAND'd back so the elementwise mul has matching shapes; same softmax-style reduce-broadcast shape.  Max-subtract along the axis keeps it numerically stable: random-init nets (LeNet) overflow exp without it.";
TAttention::usage        = "TAttention[Q, K, V] computes scaled dot-product attention: softmax(Q @ K^T / sqrt(d_k)) @ V.  Q is {seq_q, d_k}; K is {seq_k, d_k}; V is {seq_k, d_v}.  Result is {seq_q, d_v}.  Pure assembly of TMatMul + TSoftmaxAxis + TMatMul; the two matmuls dispatch through cblas_sgemm.";
TBatchNorm::usage        = "TBatchNorm[x, gamma, beta, mean, var] / TBatchNorm[x, gamma, beta, mean, var, eps] applies the inference-form batch-norm transform: y = gamma * (x - mean) / sqrt(var + eps) + beta along the channel axis.  x is rank-3 {C, H, W} or rank-4 {B, C, H, W}; gamma/beta/mean/var are all rank-1 {C}.  Default eps = 1e-5.";
TBatchNormTrain::usage   = "TBatchNormTrain[x, gamma, beta] / TBatchNormTrain[x, gamma, beta, eps] computes batch-norm using statistics from x, then applies per-channel gamma/beta.  Supports rank-3 {C,H,W} and rank-4 {B,C,H,W}.";
TConv2D::usage           = "TConv2D[input, weights, bias] builds a stride-1, no-padding 2-D convolution.  input shape {C_in, H, W} or batched {B, C_in, H, W}; weights {C_out, C_in, kh, kw}; bias {C_out}.  Rank-3 routes through TConv2DIm2Col; rank-4 routes through TConv2DIm2ColBatched.";
TConv2DIm2Col::usage     = "TConv2DIm2Col[input, weights, bias] is the im2col + matmul lowering of a stride-1, no-padding 2-D convolution.  Same signature/output shape as TConv2D.  Builds the im2col matrix `xCol : {C_in*kh*kw, H_out*W_out}`, then `out = w_flat @ xCol` via TMatMul which dispatches through cblas_sgemm.  TConv2D defaults to this path.";
TConv2DIm2ColBatched::usage = "TConv2DIm2ColBatched[input, weights, bias] is the rank-4 batched im2col lowering for input {B,C,H,W}.  It builds xCol : {C*kh*kw, B*H_out*W_out}, runs one matmul, then reshapes back to {B,C_out,H_out,W_out}.";
TConv2DKhKw::usage       = "TConv2DKhKw[input, weights, bias] is the kh*kw partial-sum lowering -- original TConv2D body kept for reference.";
TGlorot::usage           = "TGlorot[shape] returns a fresh f32 TTerm tensor of the given shape, filled with samples from N(0, sqrt(2 / fan_in)) (He init for ReLU).  fan_in is the count of inputs per output unit: the FIRST dim for a 2-D linear weight {in, out} (TLinear is x . W, input-first), or C_in * kh * kw = product of the dims after the first for a conv weight {C_out, C_in, kh, kw} (output-first).  Suitable for ReLU / linear / conv weight init.";
TZeros::usage            = "TZeros[shape] returns a fresh f32 TTerm tensor of zeros at the given shape.  Convenience for bias init / running-stat init.";
TOnes::usage             = "TOnes[shape] returns a fresh f32 TTerm tensor of ones at the given shape.  Convenience for layer-norm gamma init / scale-1 placeholders.";
TZerosLike::usage        = "TZerosLike[t] returns a TTensor handle of zeros matching the shape and dtype of TTerm `t`.  Suitable for seeding Adam m/v moment buffers.";
TOneHot::usage           = "TOneHot[label, n] / TOneHot[label, n, dtype] returns a TTerm tensor of length n with a 1.0 at index `label` (0-indexed) and 0.0 elsewhere.  Convenience for sparse-categorical-CE targets.  TOneHot[labels_List, n] returns the {Length[labels], n} one-hot matrix (one row per label) -- the sequence-one-hot a fixed-window LM forward consumes; a label outside 0..n-1 yields an all-zero row (padding).";

TSparseCategoricalCrossEntropy::usage = "TSparseCategoricalCrossEntropy[logits, intLabels] computes the categorical cross-entropy loss given pre-softmax logits and integer class labels (one int per sample, NOT one-hot) -- the same convention as tinygrad / Keras.  intLabels' shape is logits' shape with the last (class) axis dropped.  Lowers to log(sum(exp(logits))) - logits[label] along the last axis, then averages over the leading batch axis.  Defers to TCategoricalCrossEntropy, so it inherits the stable max-subtract logsumexp.  For one-hot targets, use TCategoricalCrossEntropy.";
TCategoricalCrossEntropy::usage = "TCategoricalCrossEntropy[logits, targetOneHot] computes the categorical cross-entropy loss given pre-softmax logits and a one-hot target.  Lowers to the stable logsumexp form max(logits) + log(sum(exp(logits - max(logits)))) - sum(target * logits) along the last axis, then averages over the leading batch axis (rank-1 logits have no batch dim and skip the average).  The max-subtract keeps random-init conv logits from overflowing exp and is grad-transparent.  For integer class labels, use TSparseCategoricalCrossEntropy.";

TEmbedding::usage        = "TEmbedding[table, idx] returns row idx of a {V, D} table as a TTerm of shape {D}.  idx is a host-side Integer; lowers as TUOpShrink[table, {{idx, idx+1}, {0, D}}] + TUOpReshape to {D}.  Dynamic-idx gather (idx as a runtime tensor) needs a future UOP_GATHER opcode.";
TEmbeddingMatrix::usage  = "TEmbeddingMatrix[table, ids] gathers rows `ids` from a {V, D} table into a {Length[ids], D} matrix.  ids is a host-side List[Integer].  Lowers as one TEmbedding + TUOpReshape to {1, D} per id, then stitches along the leading axis via PAD + sum (same idiom as TMultiHeadAttention's per-head concat -- no STACK op in thvm).  Dynamic-id gather needs a future UOP_GATHER opcode.";
TGELU::usage             = "TGELU[x] applies the tanh-form GELU approximation: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3))).  Composes via existing TTanh -- no new opcode required.";
TCausalMask::usage       = "TCausalMask[seq] returns a {seq, seq} TTerm wrapping a fresh tensor whose entries are 0 at (i, j) with j <= i and -1e9 at j > i.  Add to attention scores before softmax to zero out future-token attention.";
TCausalMaskSym::usage    = "TCausalMaskSym[vid, hi] returns the SYMBOLIC-sequence causal mask: a {S, S} TTerm (S the kvar dimension `vid`, upper bound `hi`) whose entries are 0 at (i, j) with j <= i and a large negative bias at j > i.  Built EAGER from a symbolic ramp (TUOpExpand of a {hi} ramp marked symbolic on axis 0) so a symbolic-seq TMultiHeadAttention can add it to scores before softmax.  Use in place of TCausalMask[seq] when the sequence axis is a kvar; pass the resulting TTerm as TMultiHeadAttention's `mask` argument.";
TLayerNormAffine::usage  = "TLayerNormAffine[x, gamma, beta] applies TLayerNorm[x] then multiplies by gamma + adds beta along the last axis.  GPT-2's layer-norm carries learned gamma/beta; the bare TLayerNorm in this file normalises only.";
TMultiHeadAttention::usage = "TMultiHeadAttention[Q, K, V, n_heads, mask] computes multi-head scaled dot-product attention.  Q is {seq_q, dim}; K, V are {seq_k, dim} (read independently, so seq_k may differ from seq_q -- cross-attention, or a single-query step over a KV cache); dim = n_heads * d_head; mask is a {seq_q, seq_k} additive bias (use TCausalMask) or None.  Splits each projection to {n_heads, seq, d_head} via reshape + permute, runs scaled-dot per head, concatenates back to {seq_q, dim}.  Per-head loop today (batched sgemm is a Phase 12 follow-up).\n\nTMultiHeadAttention[Q, K, V, n_heads, mask, scale] uses the explicit `scale` factor in place of the default 1/Sqrt[d_head] (pass 1.0 when the caller has already pre-scaled Q, as GPT-2's NetGraph does).";
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

(* tanh(x) = 2 * sigmoid(2x) - 1 = 2 / (1 + e^(-2x)) - 1.
   Tinygrad's lowering (mixin/elementwise.py tanh -> sigmoid): sigmoid is
   1 / (1 + e^(-x)), whose exponent is negated so it never returns
   Inf/Inf = NaN.  For x -> +inf, e^(-2x) -> 0 and tanh -> +1; for
   x -> -inf, e^(-2x) -> +inf and 2/inf - 1 -> -1.  The naive
   (e^(2x)-1)/(e^(2x)+1) form overflows to NaN once 2x leaves the f32 exp
   range (~88), which GPT-2's tanh-form GELU hits because its argument
   grows cubically.  This form saturates cleanly AND is fully
   differentiable (no sign / abs), so it stays grad-correct for training. *)
TSigmoid[x_TTerm] := 1 / (1 + Exp[-x])
TTanh[x_TTerm]    := 2 * TSigmoid[2 * x] - 1

(* TLog is kept as a public alias; new code should write Log[x]. *)
TLog[x_TTerm] := Log[x]

(* softmax(x)_i = exp(x_i) / sum(exp(x)).  Every reduce result is
   re-broadcast to x's shape with an EXPLICIT TUOpExpand (the
   reduced axis re-introduced as a unit dim first), never the
   kernel-level numel-cycle broadcast in MUL/SUB: the chain rule
   for MUL/SUB has no notion of the implicit broadcast and drops
   the cross-coupling term in d(CE)/dz = probs - target.  The
   max-subtract is grad-transparent (softmax is shift-invariant and
   the EXPAND grad rule's REDUCE-along-broadcast-axes makes the
   subtracted term's contribution cancel) so we keep it for
   numerical stability -- random-init networks (LeNet) overflow exp
   without it. *)
TSoftmax[x_TTerm] := With[{shape = tUopShape[x], sumShape = ReplacePart[tUopShape[x], 1 -> 1]},
    Module[{m, xc, e, s},
        m  = TUOpExpand[TUOpReshape[TUOpReduce[x, 0, "MAX"], sumShape], shape];
        xc = x - m;
        e  = Exp[xc];
        s  = TUOpExpand[TUOpReshape[TUOpReduce[e, 0, "SUM"], sumShape], shape];
        e / s
    ]
]

(* CrossEntropy probabilities form: target is a probability
   distribution (typically one-hot), pred is the model's predicted
   distribution.  Loss = -sum_i target_i * log(pred_i). *)
(* Cross-entropy with eps clamp: when softmax peaks sharply, the
   non-winning class probabilities round to zero and Log[0] -> -inf.
   Adding a small eps before the log keeps the loss finite without
   meaningfully changing the gradient on well-behaved distributions
   (eps below the ULP of probability slots > eps stays inert under
   round-to-nearest-even).  Standard in all production training
   pipelines. *)
TCrossEntropyLoss[pred_TTerm, target_TTerm] :=
    - Total[target * Log[pred + TUOpConst[1.*^-7]]]

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
(* M1: route through im2col + sgemm; TConv2DKhKw keeps the kh*kw path. *)
TConv2D[input_TTerm, weights_TTerm, bias_TTerm] := With[{
    inShape = tUopShape[input]
},
    Which[
        Length[inShape] === 3, TConv2DIm2Col[input, weights, bias],
        Length[inShape] === 4, TConv2DIm2ColBatched[input, weights, bias],
        True, Failure["NotImplemented",
            <|"Message" -> "TConv2D expects rank-3 {C,H,W} or rank-4 {B,C,H,W}",
              "InputShape" -> inShape|>]
    ]
]

TConv2DKhKw[input_TTerm, weights_TTerm, bias_TTerm] := Module[{
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

(* === tinygrad-`_pool` STRIDED-VIEW conv lowering ============
   TConv2DIm2ColPool / TConv2DIm2ColBatchedPool build the windowed
   im2col view `xCol : {cIn*kh*kw, ...}` purely from movement ops
   (RESHAPE / EXPAND / SHRINK / PERMUTE) -- a strided VIEW over the
   input, exactly like tinygrad's `_pool` unfold, rather than a
   materialised im2col matrix.  The EXPAND-then-merge-RESHAPE that
   produces the windowing isn't a single `View` (a stride-0 broadcast
   axis can't merge with a real-stride axis), so the view-system keeps
   it as a ShapeTracker chain; the dispatch pre-mat
   (cpu_dispatch_kernel / metal_dispatch_kernel) gathers it once into a
   contiguous matmul operand.  No PAD-and-sum: no kh*kw partial-sum
   kernels and no giant zero-padded `repeat` intermediate.

   Construction (stride 1, no padding, rank-3 input {cIn,H,W}, weights
   {cOut,cIn,kh,kw}; verified: (ky*(H+1)+hh) mod H = ky+hh within the
   used range since ky+hh <= H-1, and matches TConv2DKhKw to f32
   tolerance):
     Rh = Ceiling[kh*(H+1)/H]; Rw = Ceiling[kw*(W+1)/W];
     x1   = Reshape(Expand(Reshape(x,{cIn,1,H,1,W}),{cIn,Rh,H,Rw,W}),
                    {cIn,Rh*H,Rw*W});
     x2   = Shrink(x1, .., {0,kh*(H+1)}, {0,kw*(W+1)});
     x3   = Reshape(x2, {cIn,kh,H+1,kw,W+1});
     x4   = Shrink(x3, .., {0,kh},{0,Hout},{0,kw},{0,Wout});
     xCol6= Permute(x4, {0,1,3,2,4});           -> {cIn,kh,kw,Hout,Wout}
     xCol = Reshape(xCol6, {cIn*kh*kw, Hout*Wout});
   then `outFlat = wFlat @ xCol` -> cblas_sgemm, reshape + bias.

   FORWARD is bit-for-bit correct, and BACKWARD is too (matches
   TConv2DKhKw / the PAD-and-sum path to f32 tolerance): the grad of
   the chained SHRINK -> PAD over the strided view used to differentiate
   to ZERO w.r.t. the conv input, via two lowering bugs now fixed in
   src/schedule -- (a) a REDUCE over a broadcast (stride-0) axis whose
   RANGE never appears in the body load returned the reduce identity
   instead of `extent*body` (kernel_lift); (b) rangeify bailed on any
   SHRINK/PAD of rank > 4 before the higher-rank-capable paths
   (via_rngs bake-in / RESHAPE_V fusion) had a chance.  This path is
   TConv2D's default lowering now; THVM_CONV_POOL=0 reverts to
   PAD-and-sum.  (`input` is pushed onto a contiguous buffer boundary
   first -- see the body -- so the strided-view chain composes
   correctly even when `input` is itself a movement / compute DAG.) *)
TConv2DIm2ColPool[inputArg_TTerm, weights_TTerm, bias_TTerm] := Module[{
    input, inShape, wShape, cIn, cOut, h, wd, kh, kw, hOut, wOut, kSpat,
    rh, rw, x1, x2, x3, x4, xCol6, xCol, wFlat, outFlat, outShaped,
    biasBcast
},
    input = inputArg;
    inShape = tUopShape[input];
    wShape  = tUopShape[weights];
    {cIn, h, wd}      = inShape;
    cOut = wShape[[1]];
    kh   = wShape[[3]];
    kw   = wShape[[4]];
    hOut  = h  - kh + 1;
    wOut  = wd - kw + 1;
    kSpat = kh * kw;
    rh = Ceiling[kh * (h + 1) / h];
    rw = Ceiling[kw * (wd + 1) / wd];
    x1 = TUOpReshape[
        TUOpExpand[TUOpReshape[input, {cIn, 1, h, 1, wd}],
                   {cIn, rh, h, rw, wd}],
        {cIn, rh * h, rw * wd}];
    x2 = TUOpShrink[x1, {{0, cIn}, {0, kh * (h + 1)}, {0, kw * (wd + 1)}}];
    x3 = TUOpReshape[x2, {cIn, kh, h + 1, kw, wd + 1}];
    x4 = TUOpShrink[x3,
        {{0, cIn}, {0, kh}, {0, hOut}, {0, kw}, {0, wOut}}];
    xCol6 = TUOpPermute[x4, {0, 1, 3, 2, 4}];        (* {cIn,kh,kw,hOut,wOut} *)
    xCol  = TUOpReshape[xCol6, {cIn * kSpat, hOut * wOut}];
    wFlat = TUOpReshape[weights, {cOut, cIn * kSpat}];
    outFlat   = TMatMul[wFlat, xCol];               (* {cOut, hOut*wOut} *)
    outShaped = TUOpReshape[outFlat, {cOut, hOut, wOut}];
    biasBcast = TUOpExpand[
        TUOpReshape[bias, {cOut, 1, 1}],
        {cOut, hOut, wOut}];
    outShaped + biasBcast
]

(* Rank-4 batched analogue of TConv2DIm2ColPool: input {B,cIn,H,W}.

   The matmul keeps a 4-D output `{cOut, B, hOut, wOut}` instead of the
   flattened `{cOut, B*hOut*wOut}` -- so the contiguous wOut axis stays a
   separate output axis the renderer's conv template can land on the
   thread lanes (coalesced conv-input reads) and register-block the cOut
   ("M") axis.  Mechanically a TMatMul with extra trailing axes on the
   RHS operand: wFlat:{cOut,K} broadcasts over (B,hOut,wOut),
   xCol4:{K,B,hOut,wOut} broadcasts over cOut, MUL elementwise, REDUCE
   the K axis.  Both reshapes are contiguity-preserving so EXPAND just
   sets broadcast strides -- no extra materialization. *)
TConv2DIm2ColBatchedPool[inputArg_TTerm, weights_TTerm, bias_TTerm] := Module[{
    input, inShape, wShape, batch, cIn, cOut, h, wd, kh, kw, hOut, wOut, kSpat,
    kFlat, rh, rw, x1, x2, x3, x4, xCol6, xCol4, wFlat, wExp, xExp, out4,
    outShaped, biasBcast
},
    input = inputArg;
    inShape = tUopShape[input];
    wShape  = tUopShape[weights];
    {batch, cIn, h, wd} = inShape;
    cOut = wShape[[1]];
    kh   = wShape[[3]];
    kw   = wShape[[4]];
    hOut  = h  - kh + 1;
    wOut  = wd - kw + 1;
    kSpat = kh * kw;
    kFlat = cIn * kSpat;
    rh = Ceiling[kh * (h + 1) / h];
    rw = Ceiling[kw * (wd + 1) / wd];
    x1 = TUOpReshape[
        TUOpExpand[TUOpReshape[input, {batch, cIn, 1, h, 1, wd}],
                   {batch, cIn, rh, h, rw, wd}],
        {batch, cIn, rh * h, rw * wd}];
    x2 = TUOpShrink[x1,
        {{0, batch}, {0, cIn}, {0, kh * (h + 1)}, {0, kw * (wd + 1)}}];
    x3 = TUOpReshape[x2, {batch, cIn, kh, h + 1, kw, wd + 1}];
    x4 = TUOpShrink[x3,
        {{0, batch}, {0, cIn}, {0, kh}, {0, hOut}, {0, kw}, {0, wOut}}];
    xCol6 = TUOpPermute[x4, {1, 2, 4, 0, 3, 5}];     (* {cIn,kh,kw,B,hOut,wOut} *)
    xCol4 = TUOpReshape[xCol6, {kFlat, batch, hOut, wOut}];
    wFlat = TUOpReshape[weights, {cOut, kFlat}];
    wExp  = TUOpExpand[TUOpReshape[wFlat, {cOut, kFlat, 1, 1, 1}],
                       {cOut, kFlat, batch, hOut, wOut}];
    xExp  = TUOpExpand[TUOpReshape[xCol4, {1, kFlat, batch, hOut, wOut}],
                       {cOut, kFlat, batch, hOut, wOut}];
    out4      = TUOpReduce[TUOpMul[wExp, xExp], 1, "SUM"]; (* {cOut,B,hOut,wOut} *)
    outShaped = TUOpPermute[out4, {1, 0, 2, 3}];           (* {B,cOut,hOut,wOut} *)
    biasBcast = TUOpExpand[
        TUOpReshape[bias, {1, cOut, 1, 1}],
        {batch, cOut, hOut, wOut}];
    outShaped + biasBcast
]

(* TConv2DIm2Col / TConv2DIm2ColBatched route to the strided-view
   `_pool` bodies above by DEFAULT now: forward is bit-for-bit correct
   and backward is correct too (the reduce-over-broadcast-axis and
   rank-5+ SHRINK/PAD lowering bugs that used to zero the conv-input
   gradient are fixed in src/schedule).  The `_pool` lowering keeps the
   im2col operand a ShapeTracker VIEW chain instead of a materialised
   matrix -- the dispatch pre-mat gathers it once into the matmul
   operand; no kh*kw partial-sum kernels and no giant zero-padded
   `repeat` intermediate.  TConv2DKhKw stays as the explicit
   partial-sum reference body.
   THVM_CONV_POOL=0 forces the legacy PAD-and-sum im2col path
   (escape hatch for bisection). *)
convPoolEnabled[] := Environment["THVM_CONV_POOL"] =!= "0"

(* TConv2DIm2Col[input, weights, bias] -- im2col + matmul lowering.
   Same input/weight/bias signature and output shape as TConv2D.

   Builds the im2col matrix `xCol : {cIn*kh*kw, hOut*wOut}` so the
   convolution becomes one MatMul:
       out_flat[cOut, p] = sum_q w_flat[cOut, q] * xCol[q, p]
   where q indexes (cIn, ki, kj) and p indexes (i_out, j_out).

   xCol slots are filled by SHRINK'ing each (ki, kj) spatial patch
   from the input and PAD'ing the patch into its kh*kw-axis slot of
   a zero {cIn, kh*kw, hOut*wOut} tensor.  Slots don't overlap, so
   summing all kh*kw padded patches (Fold[Plus]) yields xCol_3d.
   Reshape collapses the (kh*kw) axis into the cIn axis.

   The final TMatMul dispatches via cpu_blas_dispatch's MUL +
   REDUCE_SUM pattern -> cblas_sgemm.  Net effect: one sgemm per
   conv layer (vs kh*kw partial-sum kernels in TConv2D), which is
   the ceiling lift needed to scale beautiful-mnist past BS=1. *)
TConv2DIm2Col[input_TTerm, weights_TTerm, bias_TTerm] /; convPoolEnabled[] :=
    TConv2DIm2ColPool[input, weights, bias]

TConv2DIm2Col[input_TTerm, weights_TTerm, bias_TTerm] := Module[{
    inShape, wShape, cIn, cOut, h, wd, kh, kw, hOut, wOut, kSpat,
    patches, summed, xCol, wFlat, outFlat, outShaped, biasBcast
},
    inShape = tUopShape[input];
    wShape  = tUopShape[weights];
    {cIn, h, wd}      = inShape;
    cOut = wShape[[1]];
    kh   = wShape[[3]];
    kw   = wShape[[4]];
    hOut  = h  - kh + 1;
    wOut  = wd - kw + 1;
    kSpat = kh * kw;
    patches = Flatten @ Table[
        With[{slot = ki * kw + kj},
            TUOpPad[
                TUOpReshape[
                    TUOpShrink[input,
                        {{0, cIn}, {ki, ki + hOut}, {kj, kj + wOut}}],
                    {cIn, 1, hOut * wOut}],
                {{0, 0}, {slot, kSpat - 1 - slot}, {0, 0}}]
        ],
        {ki, 0, kh - 1}, {kj, 0, kw - 1}
    ];
    (* Use Fold[TUOpAdd, ...] instead of Total[]: Mathematica's
       Plus[] dispatch on n-ary TTerm args goes pathological at
       n>~16 (Orderless attribute triggers an expensive arg sort
       that runs for minutes on a 25-element list).  TUOpAdd
       direct fold builds the same left-leaning tree in 1ms. *)
    summed    = Fold[TUOpAdd, First[patches], Rest[patches]];
    xCol      = TUOpReshape[summed,  {cIn * kSpat, hOut * wOut}];
    wFlat     = TUOpReshape[weights, {cOut, cIn * kSpat}];
    outFlat   = TMatMul[wFlat, xCol];
    outShaped = TUOpReshape[outFlat, {cOut, hOut, wOut}];
    biasBcast = TUOpExpand[
        TUOpReshape[bias, {cOut, 1, 1}],
        {cOut, hOut, wOut}];
    outShaped + biasBcast
]

TConv2DIm2ColBatched[input_TTerm, weights_TTerm, bias_TTerm] /; convPoolEnabled[] :=
    TConv2DIm2ColBatchedPool[input, weights, bias]

TConv2DIm2ColBatched[input_TTerm, weights_TTerm, bias_TTerm] := Module[{
    inShape, wShape, batch, cIn, cOut, h, wd, kh, kw, hOut, wOut, kSpat,
    patches, xPatch, summed, xCol, wFlat, outFlat, outObp, outShaped,
    biasBcast
},
    inShape = tUopShape[input];
    wShape  = tUopShape[weights];
    {batch, cIn, h, wd} = inShape;
    cOut = wShape[[1]];
    kh   = wShape[[3]];
    kw   = wShape[[4]];
    hOut  = h  - kh + 1;
    wOut  = wd - kw + 1;
    kSpat = kh * kw;
    patches = Flatten @ Table[
        With[{slot = ki * kw + kj},
            xPatch = TUOpPermute[
                TUOpShrink[input,
                    {{0, batch}, {0, cIn}, {ki, ki + hOut}, {kj, kj + wOut}}],
                {1, 0, 2, 3}];
            TUOpPad[
                TUOpReshape[xPatch, {cIn, 1, batch * hOut * wOut}],
                {{0, 0}, {slot, kSpat - 1 - slot}, {0, 0}}]
        ],
        {ki, 0, kh - 1}, {kj, 0, kw - 1}
    ];
    summed    = Fold[TUOpAdd, First[patches], Rest[patches]];
    xCol      = TUOpReshape[summed,  {cIn * kSpat, batch * hOut * wOut}];
    wFlat     = TUOpReshape[weights, {cOut, cIn * kSpat}];
    outFlat   = TMatMul[wFlat, xCol];
    outObp    = TUOpReshape[outFlat, {cOut, batch, hOut, wOut}];
    outShaped = TUOpPermute[outObp, {1, 0, 2, 3}];
    biasBcast = TUOpExpand[
        TUOpReshape[bias, {1, cOut, 1, 1}],
        {batch, cOut, hOut, wOut}];
    outShaped + biasBcast
]

(* === host-side init helpers for example scripts ============== *)

TGlorot[shape_List, dtype_String : "f32"] := With[{
    (* fan_in = number of inputs feeding each output unit.  thvm's two
       weight layouts put that dim in different places: a linear weight
       is {in, out} (TLinear is x . W, input-first) so fan_in is the
       FIRST dim; a conv weight is {C_out, C_in, kh, kw} (output-first)
       so fan_in is C_in * kh * kw = product of the dims after the first. *)
    fanIn = Which[
        Length[shape] == 2, shape[[1]],
        Length[shape] >= 3, Times @@ Drop[shape, 1],
        True,               shape[[1]]
    ]
},
    TTensorCreate[
        RandomVariate[NormalDistribution[0., Sqrt[2.0 / fanIn]], shape],
        dtype
    ]
]

TZeros[shape_List, dtype_String : "f32"] :=
    TTensorCreate[ConstantArray[0., shape], dtype]

TOnes[shape_List, dtype_String : "f32"] :=
    TTensorCreate[ConstantArray[1., shape], dtype]

TZerosLike[t_TTerm] := TZeros[TTensorShape[t], TTensorDType[t]]

TOneHot[label_Integer, n_Integer, dtype_String : "f32"] :=
    TTensorCreate[Table[If[i - 1 == label, 1.0, 0.0], {i, n}], dtype]

(* Batched form: a list of (0-indexed) labels -> a {Length[labels], n}
   one-hot matrix, one row per label.  The sequence-one-hot encoder a
   fixed-window LM forward consumes (onehot . tokenTable replaces the
   variable-length gather).  Built via SparseArray (a C-level fill of the
   one nonzero per row), not a dense Table[If[...]] -- the latter evaluates
   the test n times per row, ~1.5x slower for a {seq, vocab} one-hot.  A
   label outside 0..n-1 contributes no entry, leaving that row all-zero
   (the fixed window's padding rows). *)
TOneHot[labels_List, n_Integer, dtype_String : "f32"] :=
    TTensorCreate[
        Normal @ SparseArray[
            MapIndexed[If[0 <= #1 < n, {First[#2], #1 + 1} -> 1.0, Nothing] &, labels],
            {Length[labels], n}],
        dtype]

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
    Module[{rank, b, c, h, w},
        rank = Length[shape];
        Which[
            rank === 3,
                c = shape[[1]];
                h = shape[[2]];
                w = shape[[3]];
                TUOpReduce[
                    TUOpReduce[
                        TUOpReshape[x, {c, h/k, k, w/k, k}],
                        2, "MAX"],
                    3, "MAX"],
            rank === 4,
                b = shape[[1]];
                c = shape[[2]];
                h = shape[[3]];
                w = shape[[4]];
                TUOpReduce[
                    TUOpReduce[
                        TUOpReshape[x, {b, c, h/k, k, w/k, k}],
                        3, "MAX"],
                    4, "MAX"],
            True,
                Failure["NotImplemented",
                    <|"Message" -> "TMaxPool2d expects rank-3 or rank-4 input",
                      "InputShape" -> shape|>]
        ]
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
    Module[{m, xc, e, sumShape},
        sumShape = ReplacePart[shape, axis + 1 -> 1];
        m        = TUOpExpand[TUOpReshape[TUOpReduce[x, axis, "MAX"], sumShape], shape];
        xc       = x - m;
        e        = Exp[xc];
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
    Module[{rank, b, c, h, w, scaleC, shiftC, bcastShape, scaleBcast, shiftBcast},
        rank = Length[shape];
        If[ rank === 3,
            c = shape[[1]];
            h = shape[[2]];
            w = shape[[3]];
            bcastShape = {c, 1, 1},
            If[ rank === 4,
                b = shape[[1]];
                c = shape[[2]];
                h = shape[[3]];
                w = shape[[4]];
                bcastShape = {1, c, 1, 1},
                Return @ Failure["NotImplemented",
                    <|"Message" -> "TBatchNorm expects rank-3 or rank-4 input",
                      "InputShape" -> shape|>]
            ]
        ];
        scaleC     = gamma / Sqrt[var + eps];
        shiftC     = beta  - mean * scaleC;
        scaleBcast = TUOpExpand[TUOpReshape[scaleC, bcastShape], shape];
        shiftBcast = TUOpExpand[TUOpReshape[shiftC, bcastShape], shape];
        x * scaleBcast + shiftBcast
    ]
]

TBatchNormTrain[x_TTerm, gamma_TTerm, beta_TTerm] :=
    TBatchNormTrain[x, gamma, beta, 1.0*^-5]
TBatchNormTrain[x_TTerm, gamma_TTerm, beta_TTerm, eps_?NumericQ] :=
    With[{shape = tUopShape[x]},
    Module[{rank, b, c, h, w, reduceCount, bcastShape, mean, centered, var,
            broadcast},
        rank = Length[shape];
        If[ rank === 3,
            c = shape[[1]];
            h = shape[[2]];
            w = shape[[3]];
            reduceCount = h * w;
            bcastShape = {c, 1, 1};
            mean = TUOpReduce[TUOpReduce[x, 2, "SUM"], 1, "SUM"] / reduceCount,
            If[ rank === 4,
                b = shape[[1]];
                c = shape[[2]];
                h = shape[[3]];
                w = shape[[4]];
                reduceCount = b * h * w;
                bcastShape = {1, c, 1, 1};
                mean = TUOpReduce[
                    TUOpReduce[
                        TUOpReduce[x, 3, "SUM"],
                        2, "SUM"],
                    0, "SUM"] / reduceCount,
                Return @ Failure["NotImplemented",
                    <|"Message" -> "TBatchNormTrain expects rank-3 or rank-4 input",
                      "InputShape" -> shape|>]
            ]
        ];
        broadcast[t_] := TUOpExpand[TUOpReshape[t, bcastShape], shape];
        centered = x - broadcast[mean];
        var = If[ rank === 3,
            TUOpReduce[TUOpReduce[centered * centered, 2, "SUM"], 1, "SUM"]
                / reduceCount,
            TUOpReduce[
                TUOpReduce[
                    TUOpReduce[centered * centered, 3, "SUM"],
                    2, "SUM"],
                0, "SUM"] / reduceCount
        ];
        centered / Sqrt[broadcast[var] + eps] * broadcast[gamma]
            + broadcast[beta]
    ]
]

(* TCategoricalCrossEntropy[logits, targetOneHot] -- categorical
   cross-entropy from logits with a one-hot target.  loss =
   log(sum(exp(logits))) - sum(target * logits), reduced along the
   LAST axis (the class axis); for rank-2 batched inputs the
   per-sample loss is then averaged along axis 0.  Numerically naive
   (no max-subtract); fine for the activation magnitudes a typical
   NN forward produces. *)
TCategoricalCrossEntropy[logits_TTerm, target_TTerm] := With[{
    shape = tUopShape[logits]
},
    Module[{rank, classAxis, sumShape, mFlat, mBcast, perSample},
        rank      = Length[shape];
        classAxis = rank - 1;
        sumShape  = ReplacePart[shape, classAxis + 1 -> 1];
        (* Stable logsumexp: log(sum(exp(z))) = m + log(sum(exp(z - m)))
           with m = max along the class axis.  Grad-transparent (the
           softmax sums to 1, so the dm/dz terms cancel) and required:
           without it random-init conv logits overflow exp -> NaN. *)
        mFlat     = TUOpReduce[logits, classAxis, "MAX"];
        mBcast    = TUOpExpand[TUOpReshape[mFlat, sumShape], shape];
        perSample = mFlat
                  + Log[TUOpReduce[Exp[logits - mBcast], classAxis, "SUM"]]
                  - TUOpReduce[target * logits, classAxis, "SUM"];
        If[ rank <= 1,
            perSample,
            Total[perSample] / shape[[1]]    (* batch mean over axis 0 *)
        ]
    ]
]

(* TSparseCategoricalCrossEntropy[logits, intLabels] -- same loss as
   TCategoricalCrossEntropy but the target is INTEGER class labels
   (one int per sample, NOT one-hot), matching tinygrad / Keras.
   `intLabels` shape = logits shape with the last (class) axis dropped:
       logits {C}        -> labels scalar {}        (rank-1)
       logits {B, C}     -> labels {B}              (rank-2)
       logits {B, T, C}  -> labels {B, T}           (rank-3)
   Builds a 0/1 one-hot mask inline via (arange(C) == labels) -- same
   pattern tinygrad uses (no UOP_GATHER yet), then defers to the same
   logsumexp - sum(mask * logits) reduction. *)
TSparseCategoricalCrossEntropy[logits_TTerm, intLabels_TTerm] := With[{
    shape = tUopShape[logits]
},
    Module[{rank, classAxis, nClasses, ramp, rampShape, labelShape,
            mask, labels},
        rank      = Length[shape];
        classAxis = rank - 1;
        nClasses  = shape[[rank]];
        (* Build [0, 1, ..., C-1] as a {C} f32 ramp, then reshape to
           {1, ..., 1, C} so it broadcasts against the labels along
           every non-class axis. *)
        ramp      = TTensorCreate @ NumericArray[
                        Range[0, nClasses - 1], "Real32"];
        rampShape = Append[ConstantArray[1, rank - 1], nClasses];
        ramp      = TUOpReshape[ramp, rampShape];
        ramp      = TUOpExpand[ramp, shape];
        (* Cast labels to f32, reshape to {..., 1}, expand to logits'
           full shape so the cmpeq is a pure elementwise broadcast. *)
        labelShape = Append[Drop[shape, -1], 1];
        labels     = TUOpCast[intLabels, "f32"];
        labels     = TUOpReshape[labels, labelShape];
        labels     = TUOpExpand[labels, shape];
        mask       = TUOpCmpeq[ramp, labels];  (* {..., C} 0/1 f32 *)
        TCategoricalCrossEntropy[logits, mask]
    ]
]

(* Idiomatic: `q . Transpose[k]` lowers via the Dot + Transpose
   UpValues; `SoftmaxLayer[2][...]` lowers via the Layer-call
   UpValue installed in Tensor.wl; `/ Sqrt[N @ dk]` and `... . v`
   ride the Times / Power / Dot UpValues.  Reads as the textbook
   formula. *)
TAttention[q_TTerm, k_TTerm, v_TTerm] := With[{dk = tUopShape[k][[2]]},
    SoftmaxLayer[2][(q . Transpose[k]) / Sqrt[N @ dk]] . v]

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
        Total[padded]
    ]
]

TGELU[x_TTerm] := With[{
    sqrt2OverPi = Sqrt[2.0 / Pi]            (* ~ 0.7978845608... *),
    halfCubeC   = 0.044715
},
    0.5 * x * (1 + TTanh[sqrt2OverPi * (x + halfCubeC * x^3)])
]

TCausalMask[seq_Integer] := TTensorCreate @ NumericArray[
    Table[ If[ j <= i, 0.0, -1.0*^9], {i, seq}, {j, seq}],
    "Real32"]

(* A kvar-PACKED shape dim is 2^31 + vid: the leading axis of a
   symbolic-sequence tensor reads back >= 2^31 from tUopShape.  This is
   the discriminator the symbolic-seq attention branch keys off. *)
$kvarPackBase = 2^31;
symDimQ[d_]   := IntegerQ[d] && d >= $kvarPackBase
symVid[d_]    := d - $kvarPackBase            (* recover vid from a packed dim *)

(* True when a TTerm's leading axis is a kvar-packed (symbolic) dim.
   Safe on a $Failed shape -- symLeadingQ short-circuits to False rather
   than indexing into $Failed -- so it can guard the symbolic dispatch. *)
symLeadingQ[t_TTerm] := With[{s = tUopShape[t]},
    ListQ[s] && Length[s] >= 1 && symDimQ[First[s]]]

(* TCausalMaskSym[vid, hi] -- the symbolic-sequence causal mask, built
   EAGER from a symbolic ramp (the validated construction in
   symbolic.wlt's "symbolic/causal-attention-eager").  Each reduce /
   broadcast-EXPAND is realized to a contiguous leaf before the next op
   consumes it, so the doubly-symbolic {S,S} fused-broadcast addressing
   never arises.  mask[i,j] = 0 for j <= i, -30 for j > i. *)
TCausalMaskSym[vid_Integer, hi_Integer] := Module[
    {sym, er, ramp, ri, rj},
    sym  = $kvarPackBase + vid;
    er[t_] := TRealize[t];
    ramp = TSymbolicAxis[TTensorCreate[N @ Range[0, hi - 1]], 0, vid];
    ri   = er @ TUOpExpand[TUOpReshape[ramp, {sym, 1}], {sym, sym}];
    rj   = er @ TUOpExpand[TUOpReshape[ramp, {1, sym}], {sym, sym}];
    er @ TUOpMul[
        TUOpCmplt[TUOpAdd[ri, TUOpNeg[rj]], TUOpConst[0.]], TUOpConst[-30.]]
]

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
    dHead = tUopShape[q][[2]] / nHeads
},
    TMultiHeadAttention[q, k, v, nHeads, mask, 1 / Sqrt[N @ dHead]]
]

(* Symbolic-SEQUENCE multi-head attention.  Selected when Q's leading
   (sequence) axis is kvar-PACKED (>= 2^31): the seq length S is a
   symbolic dimension, so the whole construction runs EAGER -- every
   reduce / binary-op / broadcast-EXPAND realized to a contiguous leaf
   before the next op -- to sidestep the doubly-symbolic {S,S}
   fused-broadcast addressing.  This is the validated construction in
   symbolic.wlt's "symbolic/multihead-causal-attention-eager".

   Q,K,V are {S, dim}; `mask` is a SYMBOLIC {S,S} additive bias
   (TCausalMaskSym) or None; `scale` and the n_heads / d_head split
   match the integer path's semantics exactly.  SPLIT per head via a
   kvar reshape {S,dim}->{S,nHeads,dHead} + PERMUTE {1,0,2} + per-head
   SHRINK; per-head eager causal attention; PAD-free concat via a
   symbolic one-hot column selector (TUOpPad over a kvar axis zeros the
   data -- a known addressing bug -- so concat must avoid PAD). *)
TMultiHeadAttention[q_TTerm, k_TTerm, v_TTerm,
                    nHeads_Integer, mask_, scale_?NumericQ] /;
                    symLeadingQ[q] := With[{
    shapeQ = tUopShape[q]
},
    Module[{vid, sym, hi, dim, dHead, er, splitHead, perHead, heads,
            buildSlot},
        vid   = symVid[shapeQ[[1]]];
        sym   = shapeQ[[1]];          (* already kvar-packed *)
        hi    = TKVarHi[vid];         (* static buffer extent for axis 0 *)
        dim   = shapeQ[[2]];
        dHead = dim / nHeads;
        er[t_] := TRealize[t];        (* realize -> contiguous leaf *)
        (* {S,dim} -> {S,nHeads,dHead} -> PERMUTE {1,0,2} -> SHRINK head h *)
        splitHead[t_, h_] := er @ TUOpReshape[
            TUOpShrink[
                TUOpPermute[TUOpReshape[t, {sym, nHeads, dHead}], {1, 0, 2}],
                {{h, h + 1}, {0, sym}, {0, dHead}}],
            {sym, dHead}];
        perHead[h_] := Module[
            {qh, kh, vh, qe, ke, scores, masked, m, me, e, sm, se, w, we, ve},
            qh = splitHead[q, h];
            kh = splitHead[k, h];
            vh = splitHead[v, h];
            (* scores = scale * einsum ik,jk->ij (reduce over dHead axis) *)
            qe = er @ TUOpExpand[TUOpReshape[qh, {sym, 1, dHead}],
                {sym, sym, dHead}];
            ke = er @ TUOpExpand[TUOpReshape[kh, {1, sym, dHead}],
                {sym, sym, dHead}];
            scores = er @ TUOpMul[
                er @ TUOpReduce[er @ TUOpMul[qe, ke], 2, "SUM"],
                TUOpConst[N @ scale]];
            masked = If[ mask === None, scores, er @ TUOpAdd[scores, mask]];
            (* softmax over the key axis (axis 1), EXP2 + log2(e) chain *)
            m  = er @ TUOpReduce[masked, 1, "MAX"];
            me = er @ TUOpExpand[TUOpReshape[m, {sym, 1}], {sym, sym}];
            e  = er @ TUOpExp2[er @ TUOpMul[
                er @ TUOpAdd[masked, TUOpNeg[me]], TUOpConst[N @ Log2[E]]]];
            sm = er @ TUOpReduce[e, 1, "SUM"];
            se = er @ TUOpExpand[TUOpReshape[sm, {sym, 1}], {sym, sym}];
            w  = er @ TUOpMul[e, TUOpRecip[se]];           (* {S,S} weights *)
            (* out = einsum ij,jk->ik (reduce over key axis 1) *)
            we = er @ TUOpExpand[TUOpReshape[w, {sym, sym, 1}],
                {sym, sym, dHead}];
            ve = er @ TUOpExpand[TUOpReshape[vh, {1, sym, dHead}],
                {sym, sym, dHead}];
            er @ TUOpReduce[er @ TUOpMul[we, ve], 1, "SUM"]];  (* {S,dHead} *)
        heads = perHead /@ Range[0, nHeads - 1];
        (* PAD-free concat: place each head's {S,dHead} into its column
           slot of {S,nHeads,dHead} via a symbolic one-hot selector, sum
           over heads, reshape -> {S,dim}. *)
        buildSlot[hh_, h_] := Module[{he, ohS},
            he = er @ TUOpExpand[TUOpReshape[hh, {sym, 1, dHead}],
                {sym, nHeads, dHead}];
            ohS = TSymbolicAxis[
                TTensorCreate[
                    Table[N @ Boole[s == h], {hi}, {s, 0, nHeads - 1}, {1}]],
                0, vid];
            er @ TUOpMul[he, er @ TUOpExpand[ohS, {sym, nHeads, dHead}]]];
        er @ TUOpReshape[
            er @ Total @ Table[buildSlot[heads[[h + 1]], h], {h, 0, nHeads - 1}],
            {sym, dim}]
    ]
]

TMultiHeadAttention[q_TTerm, k_TTerm, v_TTerm,
                    nHeads_Integer, mask_, scale_?NumericQ] := With[{
    shapeQ = tUopShape[q],
    shapeK = tUopShape[k]
},
    Module[{seqQ, seqK, dim, dHead, headView, sliceHead, perHead, headOuts},
        seqQ  = shapeQ[[1]];
        seqK  = shapeK[[1]];      (* K / V carry their own length: seqK may
                                     differ from seqQ for cross-attention and
                                     for a single-query step over a KV cache. *)
        dim   = shapeQ[[2]];
        dHead = dim / nHeads;
        (* {s, dim} -> {s, nHeads, dHead} -> {nHeads, s, dHead}.
           Transpose with 1-indexed perm via the WL UpValue. *)
        headView[t_, s_] := Transpose[ArrayReshape[t, {s, nHeads, dHead}],
                                  {2, 1, 3}];
        sliceHead[hView_, h_, s_] := ArrayReshape[
            TUOpShrink[hView, {{h, h + 1}, {0, s}, {0, dHead}}],
            {s, dHead}];
        perHead[h_] := With[{
            qS = sliceHead[headView[q, seqQ], h, seqQ],
            kS = sliceHead[headView[k, seqK], h, seqK],
            vS = sliceHead[headView[v, seqK], h, seqK]
        },
            Module[{scores, scoresM},
                scores  = (qS . Transpose[kS]) * scale;   (* {seqQ, seqK} *)
                scoresM = If[ mask === None, scores, scores + mask];
                TSoftmaxAxis[scoresM, 1] . vS]];          (* {seqQ, dHead} *)
        headOuts = perHead /@ Range[0, nHeads - 1];
        headStitch[headOuts, nHeads, dHead]
    ]
]

(* GPT-2 inference is a PURE GRAPH now: TFromNet[gpt2, ids] traverses the
   real NetModel NetChain / NetGraph (embedding -> 12 pre-norm transformer
   blocks -> final LayerNorm) via the fromLayer cases above and appends the
   tied token-embedding LM head, yielding the {seq, vocab} logits TTerm.  The
   per-layer building blocks (TEmbeddingMatrix, TLayerNormAffine, TGELU,
   TLinear, TMultiHeadAttention, TCausalMask) are validated in gpt2.wlt; the
   hand-assembled TGPT2FromArrays / TGPT2Block / TGPT2SelfAttention / TGPT2MLP
   forward (and its raw-array extraction in Examples/gpt2_weights.wl) were
   deleted -- the graph traversal reproduces them token-for-token. *)

(* Stitch a list of nHeads {seq_q, dHead} TTerms into {seq_q, dim=
   nHeads*dHead} via per-head PAD into the corresponding column
   slice + sum.  No STACK op in thvm.  The row (seq_q) axis is left
   untouched, so the stitch needs only nHeads and dHead. *)
headStitch[heads_List, nHeads_, dHead_] := Total @ Table[
    TUOpPad[heads[[h + 1]],
        {{0, 0}, {h * dHead, (nHeads - h - 1) * dHead}}],
    {h, 0, nHeads - 1}]

(* === Wolfram-layer parameter access ===================== *)

(* Parameter names for each layer kind, in canonical declaration
   order.  Adding a new layer means adding an entry here + a
   fromLayer[<head>, ...] forward rule below. *)
$layerParams[LinearLayer]        = {"Weights", "Biases"}
$layerParams[ConvolutionLayer]   = {"Weights", "Biases"}
$layerParams[NormalizationLayer] = {"Scaling", "Biases"}
$layerParams[BatchNormalizationLayer] = {"Scaling", "Biases"}
$layerParams[EmbeddingLayer]     = {"Weights"}
$layerParams[ElementwiseLayer]   = {}
$layerParams[ReshapeLayer]       = {}
$layerParams[FlattenLayer]       = {}
$layerParams[PoolingLayer]       = {}
$layerParams[SoftmaxLayer]       = {}
$layerParams[DropoutLayer]       = {}
$layerParams[ThreadingLayer]     = {}
$layerParams[_]                  = {}

TLayerWeights[layer_] :=
    NetExtract[layer, #] & /@ $layerParams[Head[layer]]

(* Each weight is a plain TTensorCreate leaf baked into the forward.  The
   trainable weights are identified from the GRAPH afterwards (TNetParams =
   the body's float-leaf TEN terms), matching tinygrad's removal of
   requires_grad: TGrad auto-grads every reachable float leaf, and the param
   list decides what updates -- no Sow-provenance, no registry. *)
TLayerToTensors[layer_] := TTensorCreate /@ TLayerWeights[layer]

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

(* LinearLayer weight from TLayerToTensors is {out, in} (Wolfram
   convention).  Rank-preserving: a single {in} vector goes through
   the TMatVec path (-> {out}); a batched {B, in} input goes through
   the batched TLinear with the transposed weight {in, out} (-> {B, out}),
   keeping the leading batch axis instead of collapsing it. *)
fromLayer[LinearLayer, layer_, x_TTerm] := Module[{wArr, bArr, w},
    wArr = NetExtract[layer, "Weights"];
    bArr = NetExtract[layer, "Biases"];
    w    = TTensorCreate[wArr];
    (* A biasless LinearLayer (Biases -> None) is just the matmul -- e.g.
       GPT-2's tied classifier head in the "LanguageModeling" net.  Feeding
       TTensorCreate[None] as a bias poisons the lift, so branch on it. *)
    If[ bArr === None,
        If[ Length[tUopShape[x]] <= 1,
            TMatVec[w, asRowVec[x]],
            TMatMul[x, Transpose[w]]
        ],
        With[{b = TTensorCreate[bArr]},
            If[ Length[tUopShape[x]] <= 1,
                TUOpAdd[TMatVec[w, asRowVec[x]], b],
                TLinear[x, Transpose[w], b]
            ]
        ]
    ]
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

(* FlattenLayer[]: collapse a single example {C, H, W} to rank-1.  On a
   batched rank-4 feature map {B, C, H, W} keep the leading batch axis
   ({B, C, H, W} -> {B, C*H*W}) so the batch survives into the linear
   head; otherwise the whole map (batch included) would fuse into one
   vector.  Sizes the output via tUopShape so it works on intermediate
   UOP terms in a chain (an uninitialised FlattenLayer reports
   {Automatic}, useless for sizing).  Forward only. *)
fromLayer[FlattenLayer, _, x_TTerm] :=
    With[{shape = tUopShape[x]},
        If[ Length[shape] >= 4,
            TUOpReshape[x, {First[shape], Times @@ Rest[shape]}],
            TUOpReshape[x, {Times @@ shape}]
        ]
    ]

(* SoftmaxLayer: forward only, stable softmax along the LAST axis (the
   class axis).  TSoftmax is hard-coded to axis 0, which on a batched
   {B, classes} input normalises across the batch; TSoftmaxAxis carries
   the same max-subtract along an arbitrary axis. *)
fromLayer[SoftmaxLayer, _, x_TTerm] :=
    TSoftmaxAxis[x, Length[tUopShape[x]] - 1]

(* PoolingLayer non-overlapping (Stride == KernelSize), 2-D Max only.
   Channels-first input {C, H, W} (or batched {B, C, H, W}); TMaxPool2d
   pools the trailing two axes either way:
       {..., H, W} -> {..., H/kh, kh, W/kw, kw}
       REDUCE the kh axis, then the (now innermost) kw axis -> {..., H/kh, W/kw}
   Refuses overlapping pooling, non-Max functions, or inputs that are
   neither rank-3 nor rank-4 with a Failure -- those need PERMUTE /
   multi-axis grad support not yet present. *)
fromLayer[PoolingLayer, layer_, x_TTerm] := Module[{
    kSize, stride, fn, shape, h, w, kh, kw
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
    If[ !MemberQ[{3, 4}, Length[shape]],
        Return @ Failure["NotImplemented",
            <|"Message" -> "PoolingLayer expects rank-3 {C, H, W} or rank-4 {B, C, H, W} input",
              "InputShape" -> shape|>]];
    {h, w}    = shape[[-2 ;;]];
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

(* ElementwiseLayer dispatch.  Tries an explicit symbol-based
   lookup first (fast path for `Tanh`, `Identity`, etc.).  If the
   layer carries an arbitrary Function body (e.g. GPT-2's tanh-form
   GELU `0.5 (#1 (1 + Tanh[0.797885 (#1 + 0.044715 #1^3)])) &`),
   apply the function directly to the TTerm input -- the WL Plus
   / Times / Power[t, n_Integer] / Tanh UpValues installed in
   Tensor.wl construct the equivalent UOp graph automatically. *)
fromLayer[ElementwiseLayer, layer_, x_TTerm] := Module[{f, op, applied},
    f  = NetExtract[layer, "Function"];
    op = Lookup[$elementwiseDispatch, f, None];
    If[ op =!= None, Return @ op[x]];
    applied = f[x];
    If[ Head[applied] === TTerm, applied,
        Message[TFromNet::eltunsupported, f]; $Failed
    ]
]

(* NormalizationLayer: GPT-2-style learned scale/shift along the
   feature axis.  NetExtract gives us "Scaling" (gamma) and
   "Biases" (beta), both rank-1 {dim}.  Maps cleanly to
   TLayerNormAffine.  Wolfram's NormalizationLayer also supports
   per-channel norm with axis spec; we ignore that and assume
   last-axis until a concrete test asks for it. *)
fromLayer[NormalizationLayer, layer_, x_TTerm] := Module[{gamma, beta},
    {gamma, beta} = TLayerToTensors[layer];
    TLayerNormAffine[x, gamma, beta]
]

(* BatchNormalizationLayer: normalise per channel using THIS batch's
   statistics (training form), then the learned per-channel scale/shift.
   Scaling = gamma, Biases = beta are the trainable params; the layer's
   MovingMean / MovingVariance (inference running stats) are not used
   here, so a forward built this way always normalises with batch stats
   -- correct for training, and used for batch-wise evaluation too. *)
fromLayer[BatchNormalizationLayer, layer_, x_TTerm] := Module[{gamma, beta},
    {gamma, beta} = TLayerToTensors[layer];
    TBatchNormTrain[x, gamma, beta]
]

(* DropoutLayer at inference is identity. *)
fromLayer[DropoutLayer, _, x_TTerm] := x

(* SequenceLastLayer: take the last element along the leading (sequence)
   axis, {seq, ...} -> {...}.  A last-row gather, lowered like TEmbedding:
   shrink axis 0 to its final slot and drop the resulting unit axis.  (The
   fixed-sequence LM lift drops this layer rather than folding it -- its
   padded window reads logits at the running length, not the last padded
   row -- so this case serves the variable-length TFromNet[net, ids] path,
   where the last position IS the last real token.) *)
fromLayer[SequenceLastLayer, _, x_TTerm] := With[{shape = tUopShape[x]},
    With[{ranges = ReplacePart[{0, #} & /@ shape, 1 -> {shape[[1]] - 1, shape[[1]]}]},
        TUOpReshape[TUOpShrink[x, ranges], Rest[shape]]
    ]
]

(* EmbeddingLayer takes integer indices and returns embedded
   vectors.  Static-int input (a List[Integer]) lowers via
   TEmbeddingMatrix.  TTerm input would need UOP_GATHER (Phase 12+
   primitive); flag with Failure for now.

   Wolfram's EmbeddingLayer is 1-INDEXED: `emb[{1}]` returns the
   first row of Weights.  Our TEmbeddingMatrix (matching tinygrad /
   numpy / PyTorch) is 0-indexed.  Subtract 1 to translate. *)
fromLayer[EmbeddingLayer, layer_, ids_List] := Module[{w},
    {w} = TLayerToTensors[layer];
    TEmbeddingMatrix[w, ids - 1]
]
fromLayer[EmbeddingLayer, _, _TTerm] :=
    Failure["NotImplemented",
        <|"Message" -> "EmbeddingLayer with TTerm input requires UOP_GATHER (Phase 12+).  Pass List[Integer] ids on the host side instead."|>]

(* ThreadingLayer is multi-input by definition; it appears inside
   NetGraph only.  fromLayer dispatched directly with a single x
   doesn't make sense -- the NetGraph traversal calls
   fromLayer[ThreadingLayer, layer, {x1, x2, ...}] with a List of
   predecessor outputs.  We extract the function and apply it. *)
fromLayer[ThreadingLayer, layer_, xs_List] := Module[{f, isPlus, isTimes},
    f = NetExtract[layer, "Function"];
    (* HoldPattern keeps the Slot-pattern literal from evaluating as a real
       Function -- a bare Function[Plus[Slot[_]..]] trips Function::slot. *)
    isPlus  = f === Plus  || MatchQ[f, HoldPattern[Function[Plus[Slot[_]..]]]];
    isTimes = f === Times || MatchQ[f, HoldPattern[Function[Times[Slot[_]..]]]];
    Which[
        (* Plus / Times forms: Total[xs] / (Times @@ xs) expand to
           Plus @@ xs / Times @@ xs, which the WL Plus / Times
           UpValues on TTerm catch. *)
        isPlus,  Total[xs],
        isTimes, Times @@ xs,
        True,    Failure["NotImplemented",
                     <|"Message" -> "ThreadingLayer Function not supported",
                       "Function" -> f|>]
    ]
]

(* NetMapOperator[innerNet] applies innerNet to each element along
   the leading axis of the input.  For NetMapOperator[LinearLayer]
   on {seq, in} input: produces {seq, out}; this is exactly TLinear
   on the whole batched tensor (no per-position loop needed -- the
   matmul broadcasts along the seq axis automatically).  Wolfram's
   LinearLayer carries Weights of shape {out, in} and Biases {out};
   the matmul we want is `x . W^T + b`, so transpose the weights
   via TUOpPermute when handing them to TLinear (which wants
   {in, out}). *)
(* Per-layer cache of pre-transposed weight tensors.  Wolfram's
   LinearLayer stores Weights in {out, in} order; TLinear wants
   {in, out}.  A runtime TUOpPermute forces strided materialisation
   of the WHOLE weight buffer on every call (~200ms for a 768x3072
   matmul); pre-transposing once host-side and caching the resulting
   contig TTerm keeps the per-call cost down to the actual sgemm. *)
$linearTransposedCache = <||>;

linearTransposedTensor[layer_LinearLayer] := With[{key = layer},
    Lookup[$linearTransposedCache, key,
        $linearTransposedCache[key] = TTensorCreate @ NumericArray[
            Transpose @ Normal @ NetExtract[layer, "Weights"], "Real32"]
    ]]

linearBiasTensor[layer_LinearLayer] := With[{key = Hold[layer, "b"]},
    Lookup[$linearTransposedCache, key,
        $linearTransposedCache[key] =
            TTensorCreate @ NetExtract[layer, "Biases"]]]

fromLayer[NetMapOperator, layer_, x_TTerm] := Module[{inner},
    inner = NetExtract[layer, "Net"];
    If[ Head[inner] === LinearLayer,
        TLinear[x, linearTransposedTensor[inner], linearBiasTensor[inner]],
        (* Fallback: try generic dispatch with the same x. *)
        TFromLayer[inner, x]
    ]
]

(* SequenceIndicesLayer: produces a sequence of integer position
   indices [1..len] for use as Embedding lookup ids.  Wolfram's
   layer is 1-indexed (matching its EmbeddingLayer convention), so
   `seqIdx[{7,13,21,30}]` returns `{1,2,3,4}`.  In our
   compositional model the host knows the prompt length, so this
   is just `Range[1, len]`. *)
fromLayer[SequenceIndicesLayer, _, ids_List] /; AllTrue[ids, IntegerQ] :=
    Range[1, Length[ids]]
fromLayer[SequenceIndicesLayer, _, _] :=
    Failure["NotImplemented",
        <|"Message" -> "SequenceIndicesLayer needs host-side List[Integer] input"|>]

(* NetGraph: topological evaluation of TOP-LEVEL subnodes.
   `NetExtract[g, All]` returns an Association of name -> sub-net
   (or a List for positional NetGraphs); we treat the keys as the
   set of top-level nodes.  Edges come from `LayersGraph`, which
   is FLATTENED across nested sub-NetGraphs / NetMapOperators;
   we project each flat-path endpoint down to its TOP-LEVEL prefix
   (the first segment of the path that is a top-level node) and
   keep edges that cross top-level boundaries. *)
fromLayer[NetGraph, g_, input_] := Module[{
    childAssoc, topNodes, lg, rawEdges, projectTop,
    topEdges, sortGraph, sorted, results, predsOf, getInput
},
    childAssoc = NetExtract[g, All];
    childAssoc = If[ AssociationQ[childAssoc],
        childAssoc,
        AssociationThread[Range[Length[childAssoc]], childAssoc]];
    topNodes = Keys[childAssoc];

    (* projectTop: given a multi-segment vertex name like
       {attention, 1, key, Net}, return its top-level prefix
       `attention`.  For a single-segment path {dropout}, the
       projection is `dropout`.  Returns Missing if no segment is
       in topNodes (shouldn't happen). *)
    projectTop[path_List] :=
        SelectFirst[path, MemberQ[topNodes, #] &, Missing[]];

    lg       = Information[g, "LayersGraph"];
    rawEdges = EdgeList[lg];
    topEdges = DeleteDuplicates @ DeleteCases[
        Cases[rawEdges,
            DirectedEdge[s_List, d_List] :>
                {projectTop[s], projectTop[d]}],
        {a_, a_} | {_Missing, _} | {_, _Missing}];

    sortGraph = Graph[topNodes, DirectedEdge @@@ topEdges];
    sorted    = TopologicalSort[sortGraph];

    predsOf[n_] := Cases[topEdges, {src_, n} :> src];

    results = <||>;
    (* getInput: build the input(s) for a node.
       - Zero predecessors -> the node reads from the NetGraph's
         input port directly.
       - One predecessor matching the node's input arity -> a single
         TTerm.
       - Multi-input layers (ThreadingLayer, AttentionLayer):
         predecessor count may be < arity because the NetGraph's
         own input port also feeds the node (residual pattern); we
         pad the front of the predecessor list with the input.  Plus
         and Times (the only ThreadingLayer functions we map) are
         commutative, so the order is harmless.  For non-commutative
         multi-input layers a Phase 12+ extension would need to
         consult the NetGraph's connection spec. *)
    getInput[n_] := Module[{
        node = childAssoc[n],
        preds = predsOf[n],
        arity, predTerms, distinct
    },
        arity = Length @ Replace[
            Information[node, "InputPortNames"],
            _Missing -> {Input}];
        predTerms = results /@ preds;
        (* GPT-2 self-attention: a sub-NetGraph with several input ports
           (Input + Query) all fed by the SAME predecessor (the pre-norm
           residual stream).  Collapse identical predecessors to one term so
           the sub-NetGraph receives a single input it fans to every port --
           NOT a multi-element List its inner 0-pred nodes would mis-read. *)
        distinct = DeleteDuplicates[predTerms];
        Which[
            (* a sub-NetGraph / NetMapOperator whose multiple ports are all fed
               by ONE predecessor (GPT-2 self-attention: Input + Query both <-
               the pre-norm stream).  Parallel edges collapse to a single pred;
               pass that one term so the sub-net fans it to every port.  Genuine
               multi-input combine layers (ThreadingLayer / AttentionLayer) take
               the residual-pad branch below instead. *)
            arity > 1 && Length[distinct] === 1
                && MatchQ[Head[node], NetGraph | NetMapOperator | NetChain],
                First[distinct],
            arity === 1 && Length[predTerms] === 0, input,
            arity === 1, First[predTerms],
            (* arity > 1 with too few predecessors -> input fills the front
               (residual-add convention for ThreadingLayer / AttentionLayer). *)
            arity > Length[predTerms],
                Join[ConstantArray[input, arity - Length[predTerms]], predTerms],
            True, predTerms
        ]];

    Do[
        results[n] = TFromLayer[childAssoc[n], getInput[n]],
        {n, sorted}
    ];
    results[Last[sorted]]
]

(* AttentionLayer: Wolfram's built-in.  For GPT-2 it's a
   single-head dot-product attention with optional masking.  We
   map it to TAttention (Q @ K^T / sqrt(d_k) softmax @ V).  Note
   that GPT-2's NetModel uses 12 separate AttentionLayers (one
   per head) inside a NetGraph that catenates their outputs --
   the top-level NetGraph dispatch above handles the fan-out
   plus catenate, and each per-head AttentionLayer dispatches
   here.  Multi-input: takes {Q, K, V} as a List of TTerms.

   Mask handling: AttentionLayer can carry a "Mask" option;
   we don't read it here -- callers supply the mask via the
   q,k,v packing (or use TMultiHeadAttention[..., mask] directly
   when not going through TFromNet). *)
fromLayer[AttentionLayer, layer_, qkv_List] /; Length[qkv] === 3 :=
    Module[{q, k, v, params, multiHead, mask, rescale, qShape, nHeads, seq, dim,
            scale, portNames, qi, ki, vi},
        (* The NetGraph traversal hands the three predecessors in the layer's
           InputPortNames order (GPT-2: {Key, Value, Query}).  Reorder to our
           {q, k, v} convention by name so the scaled Query projection lands on
           Q, not on K/V (a scramble that silently corrupts the attention). *)
        portNames = Replace[Information[layer, "InputPortNames"],
            Except[_List] -> {"Query", "Key", "Value"}];
        qi = FirstPosition[portNames, "Query", {1}][[1]];
        ki = FirstPosition[portNames, "Key",   {2}][[1]];
        vi = FirstPosition[portNames, "Value", {3}][[1]];
        q = qkv[[qi]];  k = qkv[[ki]];  v = qkv[[vi]];
        params    = Quiet @ NetExtract[layer, "Parameters"];
        multiHead = TrueQ @ Lookup[params, "MultiHead", False];
        mask      = Lookup[params, "Mask", None];
        rescale   = Lookup[params, "ScoreRescaling", None];
        If[ !multiHead, Return @ TAttention[q, k, v]];
        (* Multi-head: Q/K/V arrive as flat {seq, dim}; nHeads = the
           head axis of the layer's $QueryShape ({seq, nHeads}). *)
        qShape  = Lookup[params, "$QueryShape", Missing[]];
        nHeads  = If[ ListQ[qShape] && Length[qShape] >= 2, Last[qShape], 1];
        {seq, dim} = tUopShape[q];
        scale   = Switch[ rescale,
            None,           1.0,
            "DimensionSqrt", 1 / Sqrt[N[dim / nHeads]],
            _,              1 / Sqrt[N[dim / nHeads]]];
        TMultiHeadAttention[q, k, v, nHeads,
            If[ MatchQ[mask, "Causal" | Causal],
                If[ symLeadingQ[q],
                    TCausalMaskSym[symVid[seq], TKVarHi[symVid[seq]]],
                    TCausalMask[seq]],
                None],
            scale]
    ]
fromLayer[AttentionLayer, _, _] :=
    Failure["NotImplemented",
        <|"Message" -> "AttentionLayer expects {Q, K, V} TTerm triple via NetGraph multi-input dispatch"|>]

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

(* Catch-all: a layer head with no fromLayer rule fails LOUDLY rather than
   leaving an unevaluated fromLayer[...] to masquerade as a tensor and
   realize to garbage downstream.  Least specific (h_ first arg), so WL
   orders it after every concrete fromLayer case above. *)
fromLayer[h_, layer_, _] := (Message[TFromNet::unsupportedlayer, h]; $Failed)

(* Entry-level dispatch.  Accepts a TTerm (the usual case), a
   List[Integer] (EmbeddingLayer ids), or a List[TTerm] (multi-
   input layers like ThreadingLayer that NetGraph traversal feeds
   their predecessor outputs to). *)
TFromLayer[layer_, x_] := fromLayer[Head[layer], layer, x]

(* A nested NetChain (e.g. GPT-2's transformer sub-chain inside the top
   NetChain) folds like the top-level chain. *)
fromLayer[NetChain, inner_, x_] := TFromNet[inner, x]

TFromNet[chain_NetChain, x_TTerm] := Fold[
    TFromLayer[#2, #1] &,
    x,
    Table[chain[[i]], {i, Length[chain]}]
]

(* Token-encoder form: a List[Integer] of (1-indexed) token ids flows into the
   first layer (an EmbeddingLayer or a NetGraph whose embedding consumes the
   ids), which returns a TTerm; the rest of the chain folds over TTerms as
   usual.  Used for GPT-2 inference: TFromNet[gpt2, ids] -> {seq, vocab}
   logits, no hand-assembled forward.

   GPT-2's NetModel outputs the {seq, dim} hidden state (its LM head is the
   tied token-embedding projection, not a layer).  When the folded output is
   {seq, dim} and the net carries a {vocab, dim} token EmbeddingLayer, append
   the tied head logits = hidden . tokenEmbedding^T -> {seq, vocab}. *)
TFromNet[chain_NetChain, ids_List] := Module[{hidden, tokEmb},
    hidden = Fold[
        TFromLayer[#2, #1] &,
        ids,
        Table[chain[[i]], {i, Length[chain]}]];
    tokEmb = tokenEmbeddingArray[chain];
    If[ MatchQ[hidden, _TTerm] && tokEmb =!= None
            && Length[tUopShape[hidden]] === 2
            && Last[tUopShape[hidden]] === Last[Dimensions[tokEmb]],
        hidden . Transpose @ TTensorCreate[tokEmb],
        hidden
    ]
]

(* every leaf layer of a net, depth-first.  Descends ONLY container heads
   (NetChain / NetGraph / NetMapOperator) via NetExtract[_, All]; a leaf layer
   (EmbeddingLayer / LinearLayer / ...) is returned as-is (NetExtract[All] on a
   leaf would expose its parameter arrays, not sub-layers). *)
allLayers[net_] := If[
    MatchQ[Head[net], NetChain | NetGraph | NetMapOperator],
    Module[{kids = Quiet @ NetExtract[net, All]},
        If[ AssociationQ[kids], kids = Values[kids]];
        If[ ListQ[kids], Join @@ (allLayers /@ kids), {net}]],
    {net}
]

(* the {vocab, dim} token-embedding weight of a token-LM net: the EmbeddingLayer
   with the LARGEST vocab axis (GPT-2's token embed {50257,768} vs the smaller
   positional {1024,768}), or None.  Returns the layer's NATIVE NumericArray
   (NOT Normal'd to a nested list) so TTensorCreate shares its buffer zero-copy
   on CPU -- a Normal + NumericArray repack would copy the {50257,768} table
   (~154MB) twice. *)
tokenEmbeddingArray[net_] := Module[{embs, weights},
    embs = Cases[allLayers[net], _EmbeddingLayer];
    weights = Cases[Quiet @ NetExtract[#, "Weights"] & /@ embs,
        w_ /; MatchQ[Dimensions[w], {_, _}]];
    If[ weights === {}, None,
        First @ SortBy[weights, -First[Dimensions[#]] &]]
]

(* the {n_ctx, dim} positional-embedding weight: the EmbeddingLayer with the
   SMALLEST vocab axis (GPT-2's positional {1024,768} vs the token {50257,768}),
   or None when the net has fewer than two embedding tables.  Native
   NumericArray, like tokenEmbeddingArray (zero-copy share). *)
positionEmbeddingArray[net_] := Module[{embs, weights},
    embs = Cases[allLayers[net], _EmbeddingLayer];
    weights = Cases[Quiet @ NetExtract[#, "Weights"] & /@ embs,
        w_ /; MatchQ[Dimensions[w], {_, _}]];
    If[ Length[weights] < 2, None,
        First @ SortBy[weights, First[Dimensions[#]] &]]
]

TFromNet[layer_, x_TTerm] := TFromLayer[layer, x]
TFromNet[layer_, ids_List] := TFromLayer[layer, ids]

(* netInputShape[net]: returns the input port shape as a List of
   integers, or $Failed if the net has zero or multiple input ports
   or the port shape is symbolic / unspecified.  LinearLayer's
   "Input -> 4" reports as the bare integer 4; wrap as {4}.  When the
   port is a NetEncoder (e.g. NetModel["LeNet"]'s Image input), the
   net-level port reports the encoder, not the array shape -- fall back
   to the first layer's concrete input-port shape, which is the array
   the encoder feeds (the array TFromNet actually lifts over). *)
shapeFromPort[raw_] := Which[
    IntegerQ[raw],            {raw},
    VectorQ[raw, IntegerQ],   List @@ raw,
    True,                      $Failed
]

netInputShape[net_] := Module[{ports, raw, shape},
    ports = Quiet @ Information[net, "InputPorts"];
    If[!AssociationQ[ports] || Length[ports] =!= 1, Return[$Failed]];
    raw   = First[Values[ports]];
    shape = shapeFromPort[raw];
    If[ shape =!= $Failed, Return[shape]];
    If[ MatchQ[net, _NetChain] && Length[net] >= 1,
        With[{l1Ports = Quiet @ Information[NetExtract[net, 1], "InputPorts"]},
            If[ AssociationQ[l1Ports] && Length[l1Ports] === 1,
                Return[shapeFromPort[First[Values[l1Ports]]]]
            ]
        ]
    ];
    $Failed
]

(* TFromNet[net]: no input argument -- builds a TLam whose bound
   variable carries the net's input shape, body = TFromNet[net, x].
   Returns $Failed (with TFromNet::noinput) if the input shape can't
   be inferred.  TLamShape is HoldAll, so we substitute the shape
   list in via With so the `_List` pattern matches before the body
   captures `x`.

   A single baked body bakes the input shape into every movement-op
   dim (RESHAPE / EXPAND / SHRINK / the rank-3-vs-rank-4 conv / Flatten
   branch choice), so it runs only at the lifted input shape and serves
   inference there.  Nothing stores the originating net: the trainable
   weights come from the graph via gradFloatLeafTerms (TNetParams), and to
   train over a different batch size pass the NetChain to TNetTrain, which
   lifts the batched forward directly. *)
(* default fixed sequence length for the token-LM lift below. *)
(* Token-LM fixed-sequence forward over a {maxSeq, vocab} ONE-HOT input.
   The variable-length id gather is replaced by `onehot . tokenTable` so the
   WHOLE graph -- embedding, blocks, tied head -- has a FIXED shape; lifting
   it is a constant ~20s (the 12-block traversal, not compute) so it is meant
   to be built ONCE and reused via TJit (capture once + replay, rebinding the
   one-hot per step).  Token embedding = onehot . tokenTable; positional =
   the first maxSeq rows of the positional table (positions 0..maxSeq-1 are
   constant, no per-step gather); tied LM head = hidden . tokenTable^T.
   Causal masking in the blocks plus reading the logits at the current length
   keep the padded tail inert.  `onehot` is any {maxSeq, vocab} TTerm -- a
   concrete one-hot (the applied forward) or a bound VAR (the TLam form). *)
tokenLmForward[net_, onehot_, maxSeq_Integer] :=
    Module[{tokTable, posTable, dim, restLayers, outDims, needsHead, tokT, posT, x, hidden},
        tokTable   = tokenEmbeddingArray[net];
        posTable   = positionEmbeddingArray[net];
        dim        = Last[Dimensions[tokTable]];
        (* Drop layer 1 (the embedding -- replaced by the one-hot matmul
           below) and any SequenceLastLayer: the fixed window emits ALL
           positions' logits so the per-step read can pick the running
           length, whereas SequenceLastLayer would collapse to the last
           (padded) row.  Same spirit as replacing the variable-length
           gather with the one-hot: position selection moves to read time. *)
        restLayers = DeleteCases[
            Drop[Table[net[[i]], {i, Length[net]}], 1], _SequenceLastLayer];
        (* Append the tied token-embedding LM head ONLY when the folded net
           still outputs the {seq, dim} hidden state (the base GPT-2 model,
           whose head is the tied projection, not a layer).  An LM net that
           already carries the classifier -- NetModel[{..,"Task"->
           "LanguageModeling"}] or NetDrop[lm,-1] -- outputs {seq, vocab}
           logits, so a manual head would double-project.  Detect host-side
           from the output port. *)
        outDims    = Cases[Flatten @ {First @ Values @ Quiet @ Information[net, "OutputPorts"]}, _Integer];
        needsHead  = outDims === {} || Last[outDims] === dim;
        (* Native NumericArrays as CPU host leaves (TTensorCreateHost wraps
           the WL buffer zero-copy) wrapped in UOP_COPY: the device upload is
           deferred to realize time (identity on CPU, staged upload on GPU).
           Mirrors tinygrad Ops.COPY. *)
        tokT   = TUOpCopy[TTensorCreateHost[tokTable]];
        (* Positional embedding.  Integer-maxSeq path: the first maxSeq
           rows, a fixed {maxSeq, dim} const.  SYMBOLIC-onehot path: size
           the table at the kvar's static upper bound `hi` and mark axis 0
           symbolic so the {S,dim} + {S,dim} add aligns with the symbolic
           `onehot . tokT` (onehot is already symbolic on its leading axis,
           so the matmul output is outer-symbolic {S,dim}). *)
        posT   = If[ symLeadingQ[onehot],
            With[{vid = symVid[First @ tUopShape[onehot]]},
                (* Mark axis 0 symbolic on the host LEAF, THEN wrap in COPY:
                   TSymbolicAxis must see a buffer-backed leaf -- applied to a
                   UOP_COPY node it collapses the shape to rank 0. *)
                TUOpCopy[
                    TSymbolicAxis[
                        TTensorCreateHost[posTable[[1 ;; TKVarHi[vid]]]],
                        0, vid]]],
            TUOpCopy[TTensorCreateHost[posTable[[1 ;; maxSeq]]]]];
        x      = onehot . tokT + posT;
        hidden = Fold[TFromLayer[#2, #1] &, x, restLayers];
        If[ needsHead, hidden . Transpose[tokT], hidden]
    ]

(* True when `oneHot` is a {seq, vocab} one-hot over net's token vocabulary
   -- the input shape that selects the fixed-sequence LM forward. *)
tokenLmOneHotQ[net_, oneHot_TTerm] := With[{emb = tokenEmbeddingArray[net]},
    emb =!= None && MatchQ[tUopShape[oneHot], {_Integer, First[Dimensions[emb]]}]]
tokenLmOneHotQ[_, _] := False

(* TFromNet[net, oneHot]: the fixed-sequence forward APPLIED to a {maxSeq,
   vocab} one-hot.  maxSeq is the one-hot's own sequence length -- no integer
   knob.  Build it ONCE under TJit (TJit[TRealize@TWnf@TFromNet[net, #] &])
   and generate by passing each step's fresh one-hot; TJit replays + rebinds
   it.  Takes precedence over the generic NetChain/x_TTerm fold (its guard is
   more specific). *)
TFromNet[net_NetChain, oneHot_TTerm] /; tokenLmOneHotQ[net, oneHot] :=
    tokenLmForward[net, oneHot, First[tUopShape[oneHot]]]

(* TFromNet[net] for a token-LM NetChain: a reusable forward you apply to a
   one-hot -- fwd = TFromNet[net]; fwd[onehot] builds at the one-hot's shape.
   (A non-NetChain token net -- a bare EmbeddingLayer -- has no blocks and
   still guides to the 2-arg id graph form.) *)
TFromNet[net_NetChain] /; tokenEmbeddingArray[net] =!= None :=
    Function[oneHot, TFromNet[net, oneHot]]

TFromNet[net_] /; tokenEmbeddingArray[net] =!= None := (
    Message[TFromNet::tokennet, net]; $Failed
)

TFromNet[net_] := With[{shape = netInputShape[net]},
    If[shape === $Failed,
        Message[TFromNet::noinput, net]; $Failed,
        Module[{x}, TLamShape[shape, x, TFromNet[net, x]]]
    ]
]

(* === lam-graph accessors (no stored net) ===
   bound-var loc and body of a TFromNet-built LAM.  The body's float-leaf
   TEN terms (gradFloatLeafTerms, Tensor.wl) ARE the trainable weights baked
   into the forward; the bound input VAR is a TAG_VAR, never a TAG_TEN, so it
   is excluded for free. *)
lamBodyTerm[lam_TTerm]   := THeapRead[TTermVal[lam]]

(* TNetParams[lam]: the trainable weight handles derived from the GRAPH --
   the float-leaf TEN terms of the lifted LAM's body, in the C leaf-walk's
   stable order.  These are the very tensors baked into the forward, so TSet
   updates and TGrad cotangents flow back into it.  Same handles the old
   Sow-registry returned (8 for LeNet); no net stored.

   A network's parameters come from its STRUCTURE, not from analysing a
   realized graph: the LAM's bound input is a TAG_VAR (a hole, never a float
   TEN), so it is excluded for free, leaving exactly the weights -- the same
   way tinygrad's get_parameters walks the model object's Tensor attributes
   and never sees the input, which is only ever an argument (nn/state.py:112).
   An APPLIED forward TFromNet[net, x] (x a concrete TEN) is an evaluation,
   not a network: its input is baked in as a float leaf indistinguishable
   from a weight, so it has no well-defined parameter set.  Lift with
   TFromNet[net] (the LAM) and the input stays a bound variable. *)
TNetParams::applied = "TNetParams expects a network (the TFromNet[net] LAM, whose input is a bound variable).  `1` is an applied forward TFromNet[net, x] with a concrete input baked in as a leaf, so it has no distinguished parameters; lift with TFromNet[net] instead.";
TNetParams[lam_TTerm] := If[ TTermTag[lam] === $TagLAM,
    gradFloatLeafTerms[lamBodyTerm[lam]],
    (Message[TNetParams::applied, lam]; $Failed)]

(* TNetParamInfo[lam]: the same handles paired with provenance.  Without the
   net we cannot recover the originating {Layer, Param}; we infer a minimal
   Param from each handle's shape (rank-2+ weight matrix -> "Weights"; rank-1
   -> "Biases") so TNetInitialize still routes Glorot / zeros sensibly, and
   leave Layer as Missing.  Documented best-effort provenance. *)
paramKindFromShape[shape_List] := If[Length[shape] >= 2, "Weights", "Biases"]
paramInfoOf[handles_List] := Function[t,
    <|"Term" -> t, "Layer" -> Missing["NotStored"],
      "Param" -> paramKindFromShape[TTensorShape[t]]|>] /@ handles
TNetParamInfo[lam_TTerm] := paramInfoOf[TNetParams[lam]]

(* TNetInitialize[lam]: re-initialise the trainable weights IN PLACE --
   Glorot for weight matrices, zeros for biases -- straight into each
   parameter tensor's buffer, derived from the graph's float leaves.
   Installed as the NetInitialize UpValue on TTerms. *)
initParams[handles_List] := (
    Function[t,
        With[{shape = TTensorShape[t]},
            TSet[t,
                If[ Length[shape] >= 2, TGlorot[shape], TZeros[shape]]]]
    ] /@ handles;
)
TNetInitialize[lam_TTerm] := (initParams[TNetParams[lam]]; lam)
TTerm /: NetInitialize[lam_TTerm] := TNetInitialize[lam]

(* A lifted LAM runs only at its lifted input shape (the movement-op dims and
   the conv / Flatten rank branch bake the input shape in), so it serves
   inference at that shape.  Re-batching a lifted conv lam net-free needs
   shape re-inference on the baked DAG -- a follow-up; to train over varying
   batch sizes, pass the NetChain to TNetTrain, which lifts the batched
   forward directly. *)

(* === TToNet[lam]: best-effort graph -> NetChain reconstruction ===========
   No net is stored, so this re-derives a NetChain from the lifted body's UOP
   structure for the standard layer signatures (Linear / ReLU-Elementwise /
   Softmax).  Each layer T-constructor lowers to a fixed sub-DAG (see fromLayer
   / TLinear / TReLU / TSoftmaxAxis above); we peel those signatures from the
   output back to the bound VAR.  Conv / Pool lower to a deep im2col movement
   chain that does not round-trip cleanly; a body containing one (or any
   unrecognised op) returns a clear Failure (best-effort, per the API).
   NetChain re-infers the input dimension from the first Linear layer's
   weight, so no stored input shape is needed.  Not on the training path. *)

(* heap accessors over a UOP/TEN term *)
uTag[t_]    := $termTagFn[ttermRaw[t]]
uExt[t_]    := $termExtFn[ttermRaw[t]]
uChild[t_, i_] := TTerm[$heapReadFn[$termValFn[ttermRaw[t]] + i]]
uIsUop[t_, op_] := uTag[t] === $TagUOP && uExt[t] === op
uIsTen[t_]  := uTag[t] === $TagTEN

(* strip a RESHAPE / EXPAND / PERMUTE wrapper chain down to the core term *)
unwrapMove[t_] := If[ uTag[t] === $TagUOP
        && MemberQ[{$UopReshape, $UopExpand, $UopPermute}, uExt[t]],
    unwrapMove[uChild[t, 0]], t]

(* host array of a TEN term *)
tenArray[t_] := Normal @ TTensorData[t]

(* peelLayer[t]: {layer, innerTerm} for a recognised trailing layer, else $Failed.
   Linear:  ADD[REDUCE[MUL[EXPAND RESHAPE inner, EXPAND RESHAPE PERMUTE W]], EXPAND RESHAPE b]
   ReLU:    MUL[CMPLT[EXPAND CONST(0), inner], inner]
   Tanh:    (TTanh chain) -- recognised by its TANH-free sigmoid assembly is
            hard to invert, so only the explicit cases above + Softmax/Flatten. *)
peelLayer[t_] := Module[{red, mul, wMove, bMove, w, bias, wArr, inL, inR, inner},
    Which[
        (* LinearLayer: ADD[REDUCE[MUL[EXPAND RESHAPE in, EXPAND RESHAPE PERMUTE W]],
                             EXPAND RESHAPE bias] *)
        uIsUop[t, $UopAdd]
            && uIsUop[(red = uChild[t, 0]), $UopReduce]
            && uIsUop[(mul = uChild[red, 0]), $UopMul]
            && uIsTen[(bMove = unwrapMove[uChild[t, 1]])],
            inL = unwrapMove[uChild[mul, 0]];
            inR = unwrapMove[uChild[mul, 1]];
            (* one operand unwraps to the weight TEN, the other to the input
               (the prior layer's output, movement-unwrapped for the next peel) *)
            Which[
                uIsTen[inR] && !uIsTen[inL], w = inR; inner = inL,
                uIsTen[inL] && !uIsTen[inR], w = inL; inner = inR,
                True, Return[$Failed]];
            bias = bMove;
            (* unwrapMove strips the PERMUTE too, so `w` is the ORIGINAL
               {out,in} layer weight TEN -- exactly what LinearLayer wants. *)
            wArr = tenArray[w];
            {LinearLayer[First[Dimensions[wArr]],
                "Weights" -> wArr, "Biases" -> tenArray[bias]],
             inner},
        (* ReLU: MUL[CMPLT[EXPAND CONST(0), inner], inner] *)
        uIsUop[t, $UopMul]
            && uIsUop[uChild[t, 0], $UopCmplt],
            {ElementwiseLayer[Ramp], unwrapMove[uChild[t, 1]]},
        (* SoftmaxLayer: TSoftmaxAxis root = MUL[exp(x-max), EXPAND RECIP sum] *)
        uIsUop[t, $UopMul]
            && uIsUop[uChild[t, 1], $UopExpand]
            && uIsUop[uChild[uChild[t, 1], 0], $UopRecip],
            {SoftmaxLayer[], softmaxInner[t]},
        True, $Failed
    ]
]

(* the pre-softmax logits term: TSoftmaxAxis is exp(x-max)/sum; the numerator
   EXP2 chain's argument SUB's first operand is the logits x. *)
softmaxInner[t_] := Module[{num, scaled, sub},
    num = uChild[t, 0];                         (* EXP2[(x-max)*log2e] *)
    If[ !uIsUop[num, $UopExp2], Return[$Failed]];
    scaled = uChild[num, 0];                     (* MUL[(x-max), log2e] *)
    If[ !uIsUop[scaled, $UopMul], Return[$Failed]];
    sub = uChild[scaled, 0];                      (* SUB = ADD[x, NEG max] *)
    If[ !uIsUop[sub, $UopAdd], Return[$Failed]];
    uChild[sub, 0]                                (* x = the logits *)
]

TToNet[lam_TTerm] := Module[{body, layers, t, step, guard},
    body   = lamBodyTerm[lam];
    layers = {};
    t      = body;
    guard  = 0;
    While[ uTag[t] =!= $TagVAR && guard < 256,
        guard += 1;
        step = peelLayer[t];
        If[ step === $Failed,
            Return @ Failure["TToNetUnsupported",
                <|"Message" -> "TToNet: graph layer not reconstructible "
                    <> "(conv / pool / unrecognised op); best-effort recovery "
                    <> "supports Linear / Elementwise / Softmax / Flatten only."|>]];
        AppendTo[layers, First[step]];
        t = Last[step]
    ];
    If[ uTag[t] =!= $TagVAR,
        Return @ Failure["TToNetUnsupported",
            <|"Message" -> "TToNet: did not reach the input variable."|>]];
    NetChain[Reverse[layers]]
]
TToNet[_] := $Failed

TFromNet::unsupportedlayer = "No TFromNet lift rule for layer head `1`.  Add a fromLayer[`1`, ...] case (forward, plus a grad rule if it is to be trained).";
TFromNet::eltunsupported = "ElementwiseLayer with function `1` has no UOp equivalent yet (interact_grad would need a corresponding grad rule).";
TFromNet::convtbd        = "ConvolutionLayer conversion not yet implemented (`1`).  Step 14 task: needs movement-op support in materialize/interpret + the matching grad rules.";
TFromNet::noinput        = "TFromNet[`1`]: cannot infer input shape from the net's InputPorts.  Provide an explicit input via TFromNet[net, x] or supply a net with a single concrete-shape input port.";
TFromNet::tokennet       = "TFromNet[`1`] is a token-encoder net (its input is a token-id list, not a fixed-shape tensor).  Lift it over an explicit id list instead: TFromNet[net, ids] returns the {seq, vocab} logits TTerm graph.";

(* TNetTrain / TNetPredict are defined in Train.wl (the inert-loop training
   layer), which loads after NN.wl and depends on TFromNet / TToNet /
   TNetParams above. *)

End[];

EndPackage[];
