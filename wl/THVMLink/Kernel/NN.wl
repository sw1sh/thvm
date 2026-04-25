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
     TLeNet[]                    -- NetInitialize'd LeNet architecture
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
TL2Loss::usage          = "TL2Loss[x] = TSum[TSquare[x]].";
TMSELoss::usage         = "TMSELoss[pred, target] = TL2Loss[pred - target].";
TReLU::usage            = "TReLU[x] = elementwise max(x, 0), implemented as MUL[x, CMPLT[0, x]] -- the CMPLT mask broadcasts a CONST(0) against x and yields 1 where x > 0, else 0.";
TTanh::usage            = "TTanh[x] = elementwise tanh, implemented as (u - 1)/(u + 1) where u = exp(2x) = EXP2(x * 2 * log2 e).  Uses only existing UOPs (no UOP_TANH primitive).  Loses precision for |x| > ~10 due to exp overflow; that's accepted for now since hidden activations rarely sit there.";
TSoftmax::usage         = "TSoftmax[x] = exp(x) / sum(exp(x)) over the last axis.  exp via the EXP2 + log2(e) chain.  Numerically naive (no max-subtract stabilisation) -- input magnitudes >~80 will overflow exp.  Forward only.";
TLog::usage             = "TLog[x] = elementwise natural log, implemented as LOG2(x) * ln(2) since the runtime has UOP_LOG2 but no UOP_LOG.";
TCrossEntropyLoss::usage = "TCrossEntropyLoss[pred, target] = -sum(target * log(pred)).  Probability-form categorical cross-entropy.  Both inputs are TTerms with the same shape; target is typically a one-hot vector.  Forward only -- LOG2 has no grad rule yet.";
TLeNet::usage           = "TLeNet[] returns a NetInitialize'd NetChain of the canonical LeNet-5 architecture (Conv 6@5x5 -> ReLU -> MaxPool 2x2 -> Conv 16@5x5 -> ReLU -> MaxPool 2x2 -> Flatten -> Linear 120 -> ReLU -> Linear 10 -> Softmax) with random weights.  Used as a fallback for NetModel[\"LeNet\"], whose weights come back as `Automatic` under the local Mathematica paclet version mismatch.";

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
   class-score vector at the SoftmaxLayer position).  RECIP of the
   scalar sum broadcasts back across the input shape via the
   binary-elementwise UOP_MUL rule. *)
TSoftmax[x_TTerm] := With[{e = tExp[x]},
    TUOpMul[e, TUOpRecip[TUOpReduce[e, 0, "SUM"]]]
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
    TUOpReduce[
        TUOpReduce[
            TUOpReshape[x, {c, h/kh, kh, w/kw, kw}],
            2, "MAX"],
        3, "MAX"]
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

(* Canonical LeNet-5 architecture (input 1x28x28 grayscale, 10
   output classes), NetInitialize'd so weights are concrete
   NumericArrays.  NetModel["LeNet"] would normally provide the
   trained weights, but those load as `Automatic` under the local
   paclet version mismatch -- so we re-initialise from the
   architecture instead.  Training from scratch is the goal anyway
   (TOptim["Adam"] on MNIST). *)
TLeNet[] := NetInitialize @ NetChain[{
    ConvolutionLayer[6, {5, 5}, "Stride" -> {1, 1}],
    ElementwiseLayer[Ramp],
    PoolingLayer[{2, 2}, "Stride" -> {2, 2}, "Function" -> Max],
    ConvolutionLayer[16, {5, 5}, "Stride" -> {1, 1}],
    ElementwiseLayer[Ramp],
    PoolingLayer[{2, 2}, "Stride" -> {2, 2}, "Function" -> Max],
    FlattenLayer[],
    LinearLayer[120],
    ElementwiseLayer[Ramp],
    LinearLayer[10],
    SoftmaxLayer[]
}, "Input" -> {1, 28, 28}]

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
