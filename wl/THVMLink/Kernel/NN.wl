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

Begin["`Private`"];

(* === Tensor-method helpers ============================== *)

TSum[x_TTerm]                   := TUOpReduce[x, 0, "SUM"]
TSquare[x_TTerm]                := TUOpMul[x, x]
TDot[a_TTerm, b_TTerm]          := TSum[TUOpMul[a, b]]
TL2Loss[x_TTerm]                := TSum[TSquare[x]]
TMSELoss[pred_TTerm, tgt_TTerm] := TL2Loss[TUOpAdd[pred, TUOpNeg[tgt]]]
TReLU[x_TTerm]                  := TUOpMul[x, TUOpCmplt[TUOpConst[0.0, "f32"], x]]

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
$layerParams[_]                = {}

TLayerWeights[layer_] :=
    NetExtract[layer, #] & /@ $layerParams[Head[layer]]

TLayerToTensors[layer_] :=
    TTensorCreate /@ TLayerWeights[layer]

(* === forward dispatch =================================== *)

(* Promote a rank-1 input {in} to {1, in} via a fresh allocation
   so TMatVec's EXPAND has the rank it needs.  Returns x unchanged
   if it's already at least rank 2.  Backward through this
   reallocation is broken, but EXPAND already has no grad rule, so
   LinearLayer is forward-only either way. *)
asRowVec[x_TTerm] := With[{shape = TTensorShape[x]},
    If[ Length[shape] === 1,
        TTensor[{1, shape[[1]]}, Normal @ TTensorData[x], TTensorDType[x]],
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
    Ramp           -> TReLU
|>;

fromLayer[ElementwiseLayer, layer_, x_TTerm] := Module[{f, op},
    f  = NetExtract[layer, "Function"];
    op = Lookup[$elementwiseDispatch, f, None];
    If[ op === None,
        Message[TFromNet::eltunsupported, f]; $Failed,
        op[x]
    ]
]

(* ConvolutionLayer: 1-D, stride 1, no padding/dilation.  Forward
   only -- backward needs movement-op grad rules.

   For a length-N input with K-length kernel + C output channels,
   we lay it out as a sliding-window matmul:
     y[c, i] = b[c] + sum_k W[c, 0, k] * x[i + k]
   Implemented for the 1-channel-input case only. *)
fromLayer[ConvolutionLayer, layer_, x_TTerm] := Module[{
    w, b, kdims, kSize, outCh, inCh, info
},
    {w, b} = TLayerToTensors[layer];
    kdims  = Dimensions[Normal @ NetExtract[layer, "Weights"]];
    outCh  = kdims[[1]];
    inCh   = kdims[[2]];
    kSize  = kdims[[3]];
    info   = "Conv1d[outCh=" <> ToString[outCh] <>
             ", inCh="  <> ToString[inCh]  <>
             ", k="     <> ToString[kSize] <> "]";
    Message[TFromNet::convtbd, info];
    $Failed
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
