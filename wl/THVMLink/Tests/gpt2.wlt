(* ::Package:: *)
(* gpt2.wlt - per-layer parity tests for the GPT-2 building blocks.

   Each test lifts a GPT-2 layer through the same TFromNet / T* path the
   inference script uses, and asserts the lifted output matches the
   WOLFRAM layer's own output (or, where the Wolfram MXNet evaluator is
   unreliable in this environment, the unambiguous MLX.m / NeuralNetworks
   layer spec) to f32 tolerance.  That parity-vs-Wolfram is the
   correctness gate for the GPT-2 extension.

   The reference numbers come from the NeuralNetworks layers directly
   (EmbeddingLayer, NormalizationLayer, ElementwiseLayer); the multi-head
   causal-attention reference is the bare-dot per-head computation the
   Wolfram GPT-2 attention sub-NetGraph and the MLX backend both
   implement (Q.K^T scaled by 1/Sqrt[d_head], causal additive mask,
   row-softmax, .V, concat heads). *)

Needs["THVMLink`"];
Needs["NeuralNetworks`"];

rd[s_]    := Normal @ TTensorData @ TRealize[s];
md[a_, b_] := Max[Abs[Flatten[N[a] - N[b]]]];
tc[a_]    := TTensorCreate[NumericArray[a, "Real32"]];

SeedRandom[20240605];

(* ===== EmbeddingLayer: token-id gather ===== *)
With[{vocab = 24, dim = 8},
    embW = RandomReal[{-1, 1}, {vocab, dim}];
    el   = NetReplacePart[EmbeddingLayer[dim, vocab],
        {"Weights" -> embW, "Scales" -> None}];
    ids  = {3, 11, 1, 24, 7};                 (* 1-indexed Wolfram ids *)
    VerificationTest[
        md[el[ids], rd @ TEmbeddingMatrix[tc[embW], ids - 1]] < 1.*^-5,
        True,
        TestID -> "gpt2/embedding-vs-EmbeddingLayer"
    ]
];

(* ===== NormalizationLayer (LayerNorm): feature-axis normalise ===== *)
(* Build the layer the way GPT-2 carries it (Epsilon 1e-5, learned
   Scaling / Biases over the last axis) and compare to the lifted
   fromLayer[NormalizationLayer] output. *)
With[{seq = 4, dim = 16},
    nScale = RandomReal[{0.5, 1.5}, {dim}];
    nBias  = RandomReal[{-0.5, 0.5}, {dim}];
    xn     = RandomReal[{-2, 2}, {seq, dim}];
    (* manual GPT-2 LayerNorm: biased variance, eps 1e-5, last axis *)
    lnRow  = Function[row, Module[{m, v},
        m = Mean[row]; v = Mean[(row - m)^2];
        (row - m) / Sqrt[v + 0.00001] * nScale + nBias]];
    manLN  = lnRow /@ xn;
    tLN    = rd @ TLayerNormAffine[tc[xn], tc[nScale], tc[nBias]];
    VerificationTest[
        md[manLN, tLN] < 1.*^-4,
        True,
        TestID -> "gpt2/layernorm-vs-spec"
    ]
];

(* ===== GELU (tanh approximation) vs Wolfram ElementwiseLayer ===== *)
(* The argument range spans the cubic-overflow region (x up to 12) that
   used to drive the old (e^2x-1)/(e^2x+1) tanh to NaN. *)
With[{seq = 4, dim = 8},
    geluFn = 0.5*#*(1 + Tanh[0.7978845608028654*(# + 0.044715*#^3)]) &;
    xg     = RandomReal[{-6, 12}, {seq, dim}];
    VerificationTest[
        md[ElementwiseLayer[geluFn][xg], rd @ TGELU[tc[xg]]] < 1.*^-4,
        True,
        TestID -> "gpt2/gelu-vs-ElementwiseLayer"
    ]
];

(* No GELU output is NaN / Inf even deep in the cubic-overflow tail. *)
VerificationTest[
    AllTrue[Flatten[rd @ TGELU[tc[RandomReal[{-40, 60}, {3, 5}]]]],
        Element[#, Reals] &],
    True,
    TestID -> "gpt2/gelu-no-overflow-nan"
];

(* ===== TTanh stability: matches Tanh and never overflows ===== *)
With[{xt = RandomReal[{-50, 50}, {6}]},
    VerificationTest[
        md[Tanh[xt], rd @ TTanh[tc[xt]]] < 1.*^-4,
        True,
        TestID -> "gpt2/tanh-stable-vs-Tanh"
    ]
];

(* ===== Multi-head causal self-attention vs the MLX / GPT-2 spec ===== *)
(* Reference = bare-dot per-head attention: scores = Q.K^T / Sqrt[d_head],
   add the causal additive mask, row-softmax, .V, concat the heads back
   to {seq, dim}.  This is exactly the Wolfram GPT-2 attention
   sub-NetGraph (bare-dot ScoringNet, Mask -> Causal, 1/Sqrt[d_head]
   pre-scale on Q) and MLX.m's manual-attention branch. *)
With[{seq = 5, nh = 3, dh = 4},
    dim = nh * dh;
    q = RandomReal[{-1, 1}, {seq, dim}];
    k = RandomReal[{-1, 1}, {seq, dim}];
    v = RandomReal[{-1, 1}, {seq, dim}];
    manHeads = Table[
        With[{qh = q[[All, (h - 1)*dh + 1 ;; h*dh]],
              kh = k[[All, (h - 1)*dh + 1 ;; h*dh]],
              vh = v[[All, (h - 1)*dh + 1 ;; h*dh]]},
            Module[{sc, mask, sm},
                sc   = qh . Transpose[kh] / Sqrt[N @ dh];
                mask = Table[If[j <= i, 0., -10.^9], {i, seq}, {j, seq}];
                sm   = Map[Function[r, Exp[r - Max[r]] / Total[Exp[r - Max[r]]]],
                           sc + mask];
                sm . vh]],
        {h, nh}];
    manMHA = Table[Flatten[manHeads[[All, i]]], {i, seq}];
    tMHA   = rd @ TMultiHeadAttention[tc[q], tc[k], tc[v], nh, TCausalMask[seq]];
    VerificationTest[
        md[manMHA, tMHA] < 1.*^-4,
        True,
        TestID -> "gpt2/attention-vs-causal-mha-spec"
    ]
];

(* ===== Full GPT-2 self-attention block from the building blocks ===== *)
(* The hand-assembled TGPT2SelfAttention wrapper was deleted (GPT-2 is a pure
   graph via TFromNet[net, ids] now); this asserts the SAME block assembled
   directly from the kept building blocks (TLinear projections + causal
   TMultiHeadAttention + output TLinear) still matches the manual spec.  The
   per-head 1/Sqrt[d_head] scale is the TMultiHeadAttention default (Q not
   pre-scaled here). *)
With[{seq = 4, nh = 2, dh = 5},
    dim = nh * dh;
    wQ = RandomReal[{-0.2, 0.2}, {dim, dim}]; bQ = RandomReal[{-0.1, 0.1}, {dim}];
    wK = RandomReal[{-0.2, 0.2}, {dim, dim}]; bK = RandomReal[{-0.1, 0.1}, {dim}];
    wV = RandomReal[{-0.2, 0.2}, {dim, dim}]; bV = RandomReal[{-0.1, 0.1}, {dim}];
    wO = RandomReal[{-0.2, 0.2}, {dim, dim}]; bO = RandomReal[{-0.1, 0.1}, {dim}];
    x  = RandomReal[{-1, 1}, {seq, dim}];
    proj  = Function[{w, b}, x . w + ConstantArray[b, seq]];
    qP = proj[wQ, bQ]; kP = proj[wK, bK]; vP = proj[wV, bV];
    manHeads = Table[
        With[{qh = qP[[All, (h - 1)*dh + 1 ;; h*dh]],
              kh = kP[[All, (h - 1)*dh + 1 ;; h*dh]],
              vh = vP[[All, (h - 1)*dh + 1 ;; h*dh]]},
            Module[{sc, mask, sm},
                sc   = qh . Transpose[kh] / Sqrt[N @ dh];
                mask = Table[If[j <= i, 0., -10.^9], {i, seq}, {j, seq}];
                sm   = Map[Function[r, Exp[r - Max[r]] / Total[Exp[r - Max[r]]]],
                           sc + mask];
                sm . vh]],
        {h, nh}];
    manAttn = Table[Flatten[manHeads[[All, i]]], {i, seq}];
    manOut  = manAttn . wO + ConstantArray[bO, seq];
    (* graph assembled from the building blocks *)
    xt  = tc[x];
    qt  = TLinear[xt, tc[wQ], tc[bQ]];
    kt  = TLinear[xt, tc[wK], tc[bK]];
    vt  = TLinear[xt, tc[wV], tc[bV]];
    att = TMultiHeadAttention[qt, kt, vt, nh, TCausalMask[seq]];
    tOut = rd @ TLinear[att, tc[wO], tc[bO]];
    VerificationTest[
        md[manOut, tOut] < 1.*^-3,
        True,
        TestID -> "gpt2/self-attention-block-from-building-blocks"
    ]
];

(* ===== single-query attention over a KV cache == full-sequence row ===== *)
(* The decode-time KV-cache step queries ONE new token (seq_q = 1) against
   the cached K / V of every past position (seq_k = cacheLen) with a {1,
   seq_k} additive mask.  This must reproduce the full-sequence causal MHA's
   row at that position bit-for-bit -- the correctness gate for incremental
   GPT-2 generation.  Cross-attention shape (seq_q != seq_k) exercises the
   generalised TMultiHeadAttention. *)
With[{seq = 6, nh = 3, dh = 4},
    dim  = nh * dh;
    q    = RandomReal[{-1, 1}, {seq, dim}];
    k    = RandomReal[{-1, 1}, {seq, dim}];
    v    = RandomReal[{-1, 1}, {seq, dim}];
    full = rd @ TMultiHeadAttention[tc[q], tc[k], tc[v], nh, TCausalMask[seq]];
    (* one query row p (1-indexed) attending over the whole {seq, dim} cache,
       masked to positions <= p -- equals the full result's row p. *)
    queryStep[p_] := With[{
        qRow = tc[q[[{p}]]],                                  (* {1, dim} *)
        mRow = tc[Table[If[j <= p, 0., -1.*^9], {1}, {j, seq}]] (* {1, seq} *)
    },
        First @ rd @ TMultiHeadAttention[qRow, tc[k], tc[v], nh, mRow]];
    VerificationTest[
        Max[Table[md[full[[p]], queryStep[p]], {p, seq}]] < 1.*^-4,
        True,
        TestID -> "gpt2/cached-single-query-attention-vs-full-row"
    ]
];
