(* ::Package:: *)
(* NN/Embedding.wl - gather / take-along-axis, table embedding lookup, and
   the sinusoidal timestep / position embedding. *)

BeginPackage["THVMLink`"];

GeneralUtilities`SetUsage[TGather, "TGather[x$, axis$, index$] selects one element per slice along axis$ (0-indexed): out[$$, 0, $$] = x$[$$, index$, $$], keeping axis$ at size 1. It lowers tinygrad-style to sum((arange == index$) * x$) over axis$ -- the CMPEQ one-hot the cross-entropy uses, multiplied by x$ and sum-reduced. index$ is an integer TTerm or host list giving one index per slice (x$'s shape with axis$ -> 1, or the flat per-slice list). The gradient scatters the cotangent back to the selected positions, so it trains. See TTakeAlongAxis for the numpy (index$, axis$) order."];
GeneralUtilities`SetUsage[TTakeAlongAxis, "TTakeAlongAxis[x$, index$, axis$] is TGather[x$, axis$, index$] with the numpy take_along_axis argument order: select x$ at integer index$ along axis$, keeping the axis at size 1. This is the per-sample selection a policy gradient uses to pick each action's log-prob from {batch, n_actions} logits$ with a {batch} action list (axis$ = 1)."];
GeneralUtilities`SetUsage[TEmbedding, "TEmbedding[table$, idx$] returns row idx$ of a {V, D} table$ as a TTerm of shape {D}. idx$ is a host-side integer; it lowers to a shrink + reshape to {D}. Dynamic-idx gather (idx$ as a runtime tensor) needs a future gather opcode."];
GeneralUtilities`SetUsage[TEmbeddingMatrix, "TEmbeddingMatrix[table$, ids$] gathers rows ids$ from a {V, D} table$ into a {Length[ids$], D} matrix. ids$ is a host-side list of integers. It lowers to one TEmbedding + reshape to {1, D} per id, then stitches along the leading axis via pad + sum (thvm has no stack op). Dynamic-id gather needs a future gather opcode."];
GeneralUtilities`SetUsage[TSinusoidalEmbedding, "TSinusoidalEmbedding[t$, dim$] returns the {dim$} sinusoidal timestep / position embedding of scalar t$ in the diffusers cos-first order (flip_sin_to_cos): concat[cos(t$ * freqs), sin(t$ * freqs)] with freqs[i] = exp(-log(10000) * i / (dim$/2)). The weight-free part of a diffusion-model time embedder. t$ is a host number (or list), dim$ the embedding width."];

Begin["`Private`"];

(* === gather / take-along-axis ===
   tinygrad builds gather from a one-hot select + sum (mixin gather:
   onehot.where(x, 0).sum), which on thvm is the CMPEQ one-hot the
   cross-entropy already uses, multiplied by x and sum-reduced along the
   gathered axis.  Selects one element per slice along `axis` and keeps
   that axis at size 1; the cotangent scatters back to the selected
   positions, so it is grad-correct for policy-gradient log-prob selection. *)
gatherIndexTensor[index_TTerm, keepShape_] := TUOpReshape[TUOpCast[index, "f32"], keepShape]
gatherIndexTensor[index_List, keepShape_]  := TTensorCreate[ArrayReshape[N @ Flatten[{index}], keepShape]]

TGather[x_TTerm, axis_Integer, index_] := Module[
    {shape = tUopShape[x], rank, n, keepShape, arange, idx, oh},
    rank      = Length[shape];
    n         = shape[[axis + 1]];
    keepShape = ReplacePart[shape, axis + 1 -> 1];
    arange    = TUOpExpand[
        TUOpReshape[
            TTensorCreate[N @ Range[0, n - 1]],
            ReplacePart[ConstantArray[1, rank], axis + 1 -> n]],
        shape];
    idx       = TUOpExpand[gatherIndexTensor[index, keepShape], shape];
    oh        = TUOpCmpeq[arange, idx];
    TUOpReshape[TUOpReduce[x * oh, axis, "SUM"], keepShape]
]

TTakeAlongAxis[x_TTerm, index_, axis_Integer] := TGather[x, axis, index]

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

(* TSinusoidalEmbedding[t, dim]: diffusers get_timestep_embedding, cos-first
   (flip_sin_to_cos): freqs[i] = exp(-log(10000) * i / (dim/2)) for i in
   [0, dim/2); embedding = concat[cos(t*freqs), sin(t*freqs)].  Weight-free, so
   it is host-computed and wrapped as a TTerm leaf. *)
TSinusoidalEmbedding[t_ ? NumericQ, dim_Integer] := With[{half = dim / 2},
    With[{freqs = Exp[-Log[10000.] Range[0, half - 1] / half]},
        With[{args = t freqs},
            TTensorCreate[N @ Join[Cos[args], Sin[args]]]]]]

End[];

EndPackage[];
