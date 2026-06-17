(* ::Package:: *)
(* NN/Net.wl - the Wolfram NeuralNetworks bridge: lift a NetChain / NetGraph /
   layer into a TTerm UOp graph (TFromNet), recover a NetChain from a lifted
   LAM (TToNet), and the param-handle / init accessors.  Includes the GPT-2 /
   token-LM forward + KV-cache decode assembly that the bridge drives. *)

BeginPackage["WolframInstitute`THVMLink`"];

GeneralUtilities`SetUsage[TFromNet, "TFromNet[net$, x$] converts a Wolfram NeuralNetworks layer (or NetChain / NetGraph) into a TTerm UOp graph rooted at the input TTerm x$; net$ must be initialised so its weights are concrete arrays.
TFromNet[net$, ids$] for a token-encoder net traverses the NetChain / NetGraph over a 1-indexed token-id list and returns the {seq, vocab} logits TTerm (e.g. GPT-2 inference, with the tied token-embedding LM head appended).
TFromNet[net$] infers the input shape from the net's InputPorts and returns a TLam whose bound variable carries that shape annotation; the lifted forward runs and serves inference at that shape. On a token-LM NetChain it instead returns a reusable forward fwd$ with fwd$[onehot$] the applied forward.
TFromNet[net$, onehot$] for a token-LM NetChain applies the fixed-sequence forward to a {maxSeq, vocab} one-hot TTerm: the variable-length id gather becomes onehot$ . tokenTable so the whole forward has a fixed shape (maxSeq is the one-hot's own sequence length). Build it once under TJit and pass each step's fresh one-hot; TJit replays the cached dispatch and rebinds the one-hot, causal masking keeping the padded tail inert."];
GeneralUtilities`SetUsage[TToNet, "TToNet[lam$] best-effort reconstructs a NetChain from the graph of a TFromNet[net]-built LAM (no net is stored). It recognises the standard layer sub-DAG signatures (Linear / Elementwise ReLU / Softmax / Flatten), reading each layer's weights off the graph leaves, and infers the input dimension from the first Linear weight. Returns a Failure for a body with a conv / pool / unrecognised op, whose im2col lowering does not round-trip cleanly."];
GeneralUtilities`SetUsage[TNetParams, "TNetParams[lam$] returns the trainable weight handles (float-leaf TTerms) of a TFromNet[net] LAM, in the C leaf-walk's stable order. The LAM's bound input is a hole and is excluded, leaving exactly the weights baked into the forward; TGrad differentiates every reachable float leaf and TNetTrain updates them in place. As in tinygrad's get_parameters, an applied forward TFromNet[net, x] has a concrete input baked in and no distinguished parameters, so lift with TFromNet[net] instead."];
GeneralUtilities`SetUsage[TNetParamInfo, "TNetParamInfo[lam$] returns the trainable weight handles of a TFromNet[net] LAM paired with best-effort provenance, as a list of <|\"Term\", \"Layer\", \"Param\"|> associations. Without a stored net the Layer is Missing[\"NotStored\"] and Param is inferred from the handle's shape (rank-2+ gives \"Weights\", rank-1 gives \"Biases\"); TNetParams[lam$] returns the same handles without provenance."];
GeneralUtilities`SetUsage[TNetInitialize, "TNetInitialize[lam$] re-initialises the trainable weights of a TFromNet[net] LAM in place: Glorot for weight matrices, ones for normalisation scalings, zeros for biases. Installed as the NetInitialize UpValue on TTerms."];
GeneralUtilities`SetUsage[TFromLayer, "TFromLayer[layer$, x$] is the single-layer form of TFromNet."];
GeneralUtilities`SetUsage[TLayerWeights, "TLayerWeights[layer$] returns the NumericArrays of every learnable parameter layer$ carries (Weights, Biases, $$), in the layer-specific declaration order."];
GeneralUtilities`SetUsage[TLayerToTensors, "TLayerToTensors[layer$] is like TLayerWeights but wraps each NumericArray in a fresh TTensorCreate handle so it can feed back into a UOp graph."];
Begin["`Private`"];

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
|>

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
   class axis) -- TSoftmax's default. *)
fromLayer[SoftmaxLayer, _, x_TTerm] := TSoftmax[x]

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
    If[ ! MemberQ[{3, 4}, Length[shape]],
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
    If[ MatchQ[applied, _TTerm], applied,
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
$linearTransposedCache = <||>

(* Lookup's default arg is NOT held, so the classic
   Lookup[cache, key, cache[key] = ...] idiom recomputes (and overwrites)
   the slot on every call -- defeating the cache.  Guard on KeyExistsQ so
   the transpose runs only on the first sighting of a layer. *)
linearTransposedTensor[layer_LinearLayer] := With[{key = layer},
    If[ KeyExistsQ[$linearTransposedCache, key],
        $linearTransposedCache[key],
        $linearTransposedCache[key] = TTensorCreate @ NumericArray[
            Transpose @ Normal @ NetExtract[layer, "Weights"], "Real32"]
    ]]

linearBiasTensor[layer_LinearLayer] := With[{key = Hold[layer, "b"]},
    If[ KeyExistsQ[$linearTransposedCache, key],
        $linearTransposedCache[key],
        $linearTransposedCache[key] = TTensorCreate @ NetExtract[layer, "Biases"]
    ]]

fromLayer[NetMapOperator, layer_, x_TTerm] := Module[{inner},
    inner = NetExtract[layer, "Net"];
    If[ MatchQ[inner, _LinearLayer],
        TLinear[x, linearTransposedTensor[inner], linearBiasTensor[inner]],
        (* Fallback: try generic dispatch with the same x. *)
        TFromLayer[inner, x]
    ]
]

(* CatenateLayer[Level -> n]: concatenate a list of equal-rank tensors along the
   1-indexed axis `n`.  GPT-2's attention NetGraph catenates its 12 per-head
   {seq, 64} outputs into {seq, 768} at Level -> 2 (the feature axis).  thvm has
   no STACK / CAT op, so place each input into its column slice of the output via
   TUOpPad (zero-pad before/after along the concat axis) and sum -- the headStitch
   idiom generalised to arbitrary widths.  The concat axis is a feature axis
   (literal), so the PAD is safe even when another axis is symbolic. *)
fromLayer[CatenateLayer, layer_, xs_List] := Module[
    {axis, rank, widths, offsets, total},
    axis    = NetExtract[layer, "Level"];          (* 1-indexed *)
    rank    = Length @ tUopShape[First[xs]];
    widths  = tUopShape[#][[axis]] & /@ xs;
    offsets = Prepend[Accumulate[Most[widths]], 0];
    total   = Total[widths];
    Total @ MapThread[
        {t, off, w} |-> TUOpPad[t,
            Table[If[ a === axis, {off, total - off - w}, {0, 0}], {a, rank}]],
        {xs, offsets, widths}]
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

(* Multi-head self-attention NetGraph recognition.  A Wolfram transformer
   encodes multi-head attention STRUCTURALLY as N parallel per-head sub-NetGraphs
   (each a q/k/v LinearLayer projection 768->64 + a single-head AttentionLayer)
   feeding a CatenateLayer + an output LinearLayer.  Walking that literally lifts
   144 single-head attentions + a PAD catenate per block -- a kernel explosion.
   Instead RECOGNISE the pattern and emit ONE TMultiHeadAttention over the full
   {dim, dim} Q/K/V projections (the N per-head {dHead, dim} weights stacked
   row-wise, so column block h is head h -- the layout TMultiHeadAttention's
   reshape-to-heads split expects).  Bit-identical to the per-head walk; a handful
   of fusable kernels instead of ~2000.  Self-attention only: fires when the
   graph's Input + Query ports collapse to a single TTerm (getInput fan-out). *)
mhaGraphNodes[g_] := With[{c = Quiet @ NetExtract[g, All]},
    Which[ AssociationQ[c], Values[c], ListQ[c], c, True, {}]]

mhaHeadQ[h_] := MatchQ[h, _NetGraph] &&
    MemberQ[Head /@ mhaGraphNodes[h], AttentionLayer]

mhaSelfAttnGraphQ[g_] := MatchQ[g, _NetGraph] && Module[{ns = mhaGraphNodes[g]},
    Length[ns] >= 3
        && MatchQ[Last[ns], _NetMapOperator]
        && MatchQ[ns[[-2]], _CatenateLayer]
        && AllTrue[ns[[1 ;; -3]], mhaHeadQ]]

fromLayer[NetGraph, g_, x_TTerm] /; mhaSelfAttnGraphQ[g] := Module[
    {ns, heads, nH, dim, dH, stackW, stackB, Wq, bq, Wk, bk, Wv, bv, outProj,
     Wo, bo, scaleFn, scale, maskSpec, lin, q, k, v, attn, seq, b, kC, vC},
    ns      = mhaGraphNodes[g];
    heads   = ns[[1 ;; -3]];
    outProj = Last[ns];
    nH      = Length[heads];
    (* stack the per-head {dHead, dim} projections row-wise -> {dim, dim} *)
    stackW[port_] := Join @@ (Normal @ NetExtract[#, {port, "Net", "Weights"}] & /@ heads);
    stackB[port_] := Join @@ (Normal @ NetExtract[#, {port, "Net", "Biases"}] & /@ heads);
    Wq = stackW["query"]; bq = stackB["query"];
    Wk = stackW["key"];   bk = stackB["key"];
    Wv = stackW["value"]; bv = stackB["value"];
    Wo = Normal @ NetExtract[outProj, {"Net", "Weights"}];
    bo = Normal @ NetExtract[outProj, {"Net", "Biases"}];
    dim = Length[Wq]; dH = dim / nH;
    (* the per-head scaling ElementwiseLayer (a #&) carries the 1/Sqrt[dHead]
       externally; apply it as the attention scale.  Default to 1/Sqrt[dHead]. *)
    scaleFn = Quiet @ NetExtract[First[heads], {"scaling", "Function"}];
    scale   = If[ MatchQ[scaleFn, _Function], N @ scaleFn[1.0], 1 / Sqrt[N @ dH]];
    maskSpec = Quiet @ Lookup[
        NetExtract[First[heads], {"attention", "Parameters"}], "Mask", None];
    lin[t_, w_, bb_] := TLinear[t, TTensorCreate[N @ Transpose @ w], TTensorCreate[N @ bb]];
    q = lin[x, Wq, bq]; k = lin[x, Wk, bk]; v = lin[x, Wv, bv];
    attn = If[ AssociationQ[$decodeAttn],
        (* decode fork: one {nCtx, dim} cache per block (not 144 per-head) *)
        b = $decodeAttn["idx"]; $decodeAttn["idx"] = b + 1;
        kC = $decodeAttn["k"][[b]]; vC = $decodeAttn["v"][[b]];
        TDecodeAttend[q, kC, vC, k, v, nH,
            $decodeAttn["posVid"], $decodeAttn["lenVid"], scale],
        seq = First @ tUopShape[q];
        TMultiHeadAttention[q, k, v, nH,
            If[ MatchQ[maskSpec, "Causal" | Causal],
                If[ symLeadingQ[q],
                    TCausalMaskSym[symVid[seq], TKVarHi[symVid[seq]]],
                    TCausalMask[seq]],
                None],
            scale]];
    lin[attn, Wo, bo]
]

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
               by ONE source (GPT-2 self-attention: Input + Query both <- the
               pre-norm stream).  That source is either a single shared
               predecessor (distinct == 1) or the NetGraph's OWN input port fanned
               to every port (distinct == 0, e.g. the per-head sub-NetGraphs whose
               Input + Query both read the attention graph's input).  Pass that one
               term so the sub-net fans it to every port -- NOT a multi-element
               List its inner 0-pred nodes would mis-read.  Genuine multi-input
               combine layers (ThreadingLayer / AttentionLayer) take the
               residual-pad branch below instead. *)
            arity > 1 && Length[distinct] <= 1
                && MatchQ[Head[node], NetGraph | NetMapOperator | NetChain],
                If[ Length[distinct] === 1, First[distinct], input],
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

(* DECODE context: when set, the AttentionLayer dispatch
   below routes through TDecodeAttend (the single-query KV-cache fork) instead
   of TMultiHeadAttention -- the ONLY cache-aware op in the block; everything
   else (LayerNorm / GELU MLP / residual) is ordinary {1, dim}.  $decodeAttn is
   an Association <|"k" -> {kCache..}, "v" -> {vCache..}, "posVid", "lenVid",
   "scale", "idx"|>; `idx` is a mutable per-step block counter advanced by each
   attention hit, so the b-th MultiHead AttentionLayer the fold visits picks the
   b-th block's cache.  tokenLmDecodeStep Block-scopes it; outside decode it is
   None and the attention dispatch is the ordinary symbolic/literal MHA. *)
$decodeAttn = None

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
   when not going through TFromNet).

   DECODE fork: when $decodeAttn is set, q/k/v arrive as the {1, dim}
   single-token projections; route them through TDecodeAttend over THIS
   block's KV cache ($decodeAttn["k"/"v"][[idx]]) instead of
   TMultiHeadAttention, advancing the block counter. *)
fromLayer[AttentionLayer, layer_, qkv_List] /;
        Length[qkv] === 3 && AssociationQ[$decodeAttn] :=
    Module[{q, k, v, params, mask, rescale, nHeads, dim, scale, qShape,
            portNames, qi, ki, vi, b, kCache, vCache},
        portNames = Replace[Information[layer, "InputPortNames"],
            Except[_List] -> {"Query", "Key", "Value"}];
        qi = FirstPosition[portNames, "Query", {1}][[1]];
        ki = FirstPosition[portNames, "Key",   {2}][[1]];
        vi = FirstPosition[portNames, "Value", {3}][[1]];
        q = qkv[[qi]];  k = qkv[[ki]];  v = qkv[[vi]];
        params  = Quiet @ NetExtract[layer, "Parameters"];
        rescale = Lookup[params, "ScoreRescaling", None];
        qShape  = Lookup[params, "$QueryShape", Missing[]];
        nHeads  = If[ ListQ[qShape] && Length[qShape] >= 2, Last[qShape], 1];
        dim     = tUopShape[q][[2]];
        scale   = Switch[ rescale,
            None,            1.0,
            "DimensionSqrt", 1 / Sqrt[N[dim / nHeads]],
            _,               1 / Sqrt[N[dim / nHeads]]];
        (* advance to this block's cache (1-indexed mutable counter) *)
        b = $decodeAttn["idx"]; $decodeAttn["idx"] = b + 1;
        kCache = $decodeAttn["k"][[b]];
        vCache = $decodeAttn["v"][[b]];
        TDecodeAttend[q, kCache, vCache, k, v, nHeads,
            $decodeAttn["posVid"], $decodeAttn["lenVid"], scale]
    ]

fromLayer[AttentionLayer, layer_, qkv_List] /; Length[qkv] === 3 :=
    Module[{q, k, v, params, mask, rescale, qShape, nHeads, seq, dim,
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
        mask      = Lookup[params, "Mask", None];
        rescale   = Lookup[params, "ScoreRescaling", None];
        (* nHeads = the head axis of the layer's $QueryShape ({seq, nHeads});
           a single-head AttentionLayer (MultiHead -> False, $QueryShape ->
           {Automatic}) falls through to nHeads = 1.  Routing it through
           TMultiHeadAttention (rather than TAttention) is what respects the
           layer's ScoreRescaling AND Mask: GPT-2's per-head AttentionLayer is
           ScoreRescaling -> None (the 1/Sqrt[d] is an external scaling layer on
           the query) + Mask -> Causal, so TAttention would both double-scale and
           drop the causal mask. *)
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
    If[ ! AssociationQ[ports] || Length[ports] =!= 1, Return[$Failed]];
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

(* tokenLmDecodeStep[net, oneHotRow, kCaches, vCaches, posVid, lenVid] -- the
   single-token DECODE forward.  Process ONE new token (a {1, vocab} one-hot)
   through GPT-2's blocks, each block APPENDING its new K/V into the persistent
   per-block cache (kCaches[[b]], vCaches[[b]]) and ATTENDING the single query
   over the cached prefix -> {1, vocab} logits.
   Mirrors tokenLmForward exactly EXCEPT:
     (a) posRow: the SINGLE positional row at the current position, a kvar-BEGIN
         {1, dim} slice of the position table at TKVarPack[posVid] (the
         shrink-decode path).  x = oneHotRow . tokT + posRow ({1, dim}).
     (b) the 12 blocks fold ordinarily (LayerNorm / GELU MLP / residual are all
         literal {1, dim}), but the attention layer in each block routes through
         TDecodeAttend over THAT block's cache -- arranged by Block-scoping
         $decodeAttn (the per-block cache + kvars + a mutable block counter that
         fromLayer[AttentionLayer] advances) around the fold.
     (c) head: x . Transpose[tokT] -> {1, vocab}.
   After a prefill, the step's logits match the full forward's logits at that
   position. *)
tokenLmDecodeStep[net_, oneHotRow_TTerm, kCaches_List, vCaches_List,
                  posVid_Integer, lenVid_Integer] :=
    tokenLmDecodeStep[net, oneHotRow, kCaches, vCaches, posVid, lenVid,
        Automatic]

(* posRow variant (TJit decode driver): the 7th argument supplies the positional
   row explicitly as a {1, dim} TTerm INPUT instead of building the in-graph
   kvar-begin SHRINK (posT) of the position table.  Used so the TJit closure can
   REBIND the positional row per replay step (the in-graph shrink bakes the
   capture-time row and never moves under replay -> positionless echo).  When
   posRow is Automatic the in-graph shrink path is used (the non-JIT
   TDecodeNext).  posRow feeds the `x = embed + posRow` ADD kernel, a
   raw-input-into-a-dispatch site that input-replace rebinds (unlike a
   raw-input-into-an-ASSIGN-src). *)
tokenLmDecodeStep[net_, oneHotRow_TTerm, kCaches_List, vCaches_List,
                  posVid_Integer, lenVid_Integer, posRow_] :=
    Module[{tokTable, posTable, dim, restLayers, outDims, needsHead, tokT, posT,
            posLeaf, x, hidden},
        tokTable   = tokenEmbeddingArray[net];
        posTable   = positionEmbeddingArray[net];
        dim        = Last[Dimensions[tokTable]];
        restLayers = DeleteCases[
            Drop[Table[net[[i]], {i, Length[net]}], 1], _SequenceLastLayer];
        outDims    = Cases[Flatten @ {First @ Values @ Quiet @ Information[net, "OutputPorts"]}, _Integer];
        needsHead  = outDims === {} || Last[outDims] === dim;
        tokT       = TUOpCopy[TTensorCreateHost[tokTable]];
        (* (a) the SINGLE positional row at the current position.  With an
           explicit posRow input, use it directly; otherwise size the table at
           the kvar's static upper bound, then SHRINK the {pos, pos+1} row at
           TKVarPack[posVid] -> {1, dim}; view_apply_shrink decodes the kvar
           begin to the runtime row. *)
        posT    = If[ posRow === Automatic,
            posLeaf = TUOpCopy[TTensorCreateHost[posTable[[1 ;; TKVarHi[posVid]]]]];
            TUOpShrink[posLeaf,
                {{TKVarPack[posVid], TKVarPack[posVid] + 1}, {0, dim}}],
            posRow];
        x       = oneHotRow . tokT + posT;
        (* (b) fold the blocks with the per-block decode caches in scope.  Block
           localises the counter mutation; idx starts at 1 (the first attention
           layer the fold meets picks block 1). *)
        hidden  = Block[{$decodeAttn = <|
                "k" -> kCaches, "v" -> vCaches,
                "posVid" -> posVid, "lenVid" -> lenVid, "idx" -> 1|>},
            Fold[TFromLayer[#2, #1] &, x, restLayers]];
        (* (c) tied LM head -> {1, vocab} *)
        If[ needsHead, hidden . Transpose[tokT], hidden]
    ]

(* TDecodeStep[net, oneHotRow, state] -- the public per-step decode entry.
   `state` is <|"kCaches" -> {..12..}, "vCaches" -> {..12..}, "posVid", "lenVid"|>
   (the persistent per-block KV caches + the two position kvars, both set per
   step via TKVarSet before TRealize).  Returns the {1, vocab} logits TTerm. *)
TDecodeStep[net_, oneHotRow_TTerm, state_Association] :=
    tokenLmDecodeStep[net, oneHotRow,
        state["kCaches"], state["vCaches"], state["posVid"], state["lenVid"]]

(* TDecodeStep[net, oneHotRow, posRow, state] -- the posRow-as-INPUT decode
   forward for the TJit driver: identical to the 3-arg form except the {1, dim}
   positional row is supplied as a rebindable INPUT (`posRow`) rather than an
   in-graph kvar shrink, so a TJit capture replays it per step.  posVid still
   drives the cache APPEND offset (which rebinds at fire); lenVid the prefix
   length.  Pass posRow = TTensorCreate[N@{posTable[[pos + 1]]}] each step. *)
TDecodeStep[net_, oneHotRow_TTerm, posRow_TTerm, state_Association] :=
    tokenLmDecodeStep[net, oneHotRow,
        state["kCaches"], state["vCaches"], state["posVid"], state["lenVid"],
        posRow]

(* TDecodeInit[net] -- allocate an incremental-decode session.
   One zeroed {nCtx, dim} KV cache per block (one per AttentionLayer the decode
   fold visits, since fromLayer[AttentionLayer] advances $decodeAttn["idx"] once
   per attention hit and indexes kCaches[[idx]]), the two position kvars, the
   running write position 0, and the JIT-driver slots (the position table, the
   vocab size, and a lazily-captured TJit closure `fn`).  nCtx = the position
   table's length (max context), dim = the embedding width.  Returns the `state`
   Association TDecodeStep / TDecodeNext consume. *)
TDecodeInit::tokennet = "`1` is not a token-LM net (needs a token + a position EmbeddingLayer).";
TDecodeInit[net_] := Module[{tokTable, posTable, dim, nCtx, nBlocks},
    tokTable = tokenEmbeddingArray[net];
    posTable = positionEmbeddingArray[net];
    If[ tokTable === None || posTable === None,
        Message[TDecodeInit::tokennet, net]; Return[$Failed]];
    dim      = Last[Dimensions[tokTable]];
    nCtx     = First[Dimensions[posTable]];
    nBlocks  = Count[allLayers[net], _AttentionLayer];
    <|"kCaches" -> Table[TRealize[TTensorCreate[ConstantArray[0., {nCtx, dim}]]], {nBlocks}],
      "vCaches" -> Table[TRealize[TTensorCreate[ConstantArray[0., {nCtx, dim}]]], {nBlocks}],
      "posVid"   -> TKVarAlloc[1, nCtx],
      "lenVid"   -> TKVarAlloc[1, nCtx],
      "pos"      -> 0,
      "posTable" -> posTable,
      "vocab"    -> First[Dimensions[tokTable]],
      "fn"       -> None|>]

(* posRow at position `pos` (0-indexed): the {1, dim} positional row, a HOST
   slice of the session's position table -- the per-step input the captured TJit
   closure rebinds (replacing the in-graph kvar shrink, which would freeze the
   capture-time position -> positionless echo). *)
decodePosRow[state_Association, pos_Integer] :=
    TTensorCreate[N @ {Normal @ state["posTable"][[pos + 1]]}]

(* TDecodeNext[net, state, tokenId] -- one decode step.  Sets the position kvars
   (posVid drives the cache append offset, rebinding at fire; lenVid the prefix
   length), feeds `tokenId` as a {1, vocab} one-hot, and runs the decode forward
   through a TJit closure captured ONCE over {oneHotRow, posRow} on the first
   call and replayed after -- CONSTANT memory (no per-step re-lift), output
   token-identical to a fresh forward.  Returns the updated state with `pos`
   advanced, the greedy next-token `token` (argmax, 0-indexed), and the `logits`.
   Chain with NestList over a prompt then its own `token` to generate.  (Per-step
   latency is currently gated by the host cache-read gather -- the symbolic cache
   reads replay as JIT_OP_GATHER -- so it is constant-memory, not yet faster than
   the fixed-window forward; routing those reads through on-device kernels is the
   open speed follow-up.) *)
TDecodeNext[net_, state_Association, tokenId_Integer] := Module[
    {oneHotRow, posRow, fn, logits, st},
    TKVarSet[state["posVid"], state["pos"]];
    TKVarSet[state["lenVid"], state["pos"] + 1];
    oneHotRow = TTensorCreate[N @ {Normal @ SparseArray[{tokenId + 1 -> 1.}, state["vocab"]]}];
    posRow    = decodePosRow[state, state["pos"]];
    fn = state["fn"];
    If[ fn === None,
        fn = TJit[{oh, pr} |-> TRealize @ TDecodeStep[net, oh, pr, state]]];
    st = If[ state["fn"] === None, <|state, "fn" -> fn|>, state];
    logits = First @ Normal @ TTensorData @ fn[oneHotRow, posRow];
    <|st, "pos" -> st["pos"] + 1,
      "token"  -> First[Ordering[logits, -1]] - 1,
      "logits" -> logits|>]

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
    oneHot |-> TFromNet[net, oneHot]

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
paramInfoOf[handles_List] := (t |-> <|
    "Term" -> t, "Layer" -> Missing["NotStored"],
    "Param" -> paramKindFromShape[TTensorShape[t]]|>) /@ handles
TNetParamInfo[lam_TTerm] := paramInfoOf[TNetParams[lam]]

(* TNetInitialize[lam]: re-initialise the trainable weights IN PLACE --
   Glorot for weight matrices, zeros for biases -- straight into each
   parameter tensor's buffer, derived from the graph's float leaves.
   Installed as the NetInitialize UpValue on TTerms. *)
initParams[handles_List] := (
    (t |-> With[{shape = TTensorShape[t]},
        TSet[t, If[ Length[shape] >= 2, TGlorot[shape], TZeros[shape]]]]) /@ handles;
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
   / TLinear / TReLU / TSoftmax above); we peel those signatures from the
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
                uIsTen[inR] && ! uIsTen[inL], w = inR; inner = inL,
                uIsTen[inL] && ! uIsTen[inR], w = inL; inner = inR,
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
        (* SoftmaxLayer: TSoftmax root = MUL[exp(x-max), EXPAND RECIP sum] *)
        uIsUop[t, $UopMul]
            && uIsUop[uChild[t, 1], $UopExpand]
            && uIsUop[uChild[uChild[t, 1], 0], $UopRecip],
            {SoftmaxLayer[], softmaxInner[t]},
        True, $Failed
    ]
]

(* the pre-softmax logits term: TSoftmax is exp(x-max)/sum; the numerator
   EXP2 chain's argument SUB's first operand is the logits x. *)
softmaxInner[t_] := Module[{num, scaled, sub},
    num = uChild[t, 0];                         (* EXP2[(x-max)*log2e] *)
    If[ ! uIsUop[num, $UopExp2], Return[$Failed]];
    scaled = uChild[num, 0];                     (* MUL[(x-max), log2e] *)
    If[ ! uIsUop[scaled, $UopMul], Return[$Failed]];
    sub = uChild[scaled, 0];                      (* SUB = ADD[x, NEG max] *)
    If[ ! uIsUop[sub, $UopAdd], Return[$Failed]];
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
