(* ::Package:: *)
(* NN/Init.wl - host-side parameter init helpers for example scripts:
   Glorot / zeros / ones / zeros-like / ones-like and the one-hot encoders. *)

BeginPackage["THVMLink`"];

GeneralUtilities`SetUsage[TGlorot, "TGlorot[shape$] returns a fresh f32 TTerm tensor of the given shape$, filled with samples from N(0, sqrt(2 / fan_in)) (He init for ReLU). fan_in is the inputs per output unit: the first dim for a 2-D linear weight {in, out} (TLinear is input-first), or C_in * kh * kw (product of the dims after the first) for a conv weight {C_out, C_in, kh, kw}. Suitable for ReLU / linear / conv weight init."];
GeneralUtilities`SetUsage[TZeros, "TZeros[shape$] returns a fresh f32 TTerm tensor of zeros at the given shape$. Convenience for bias / running-stat init."];
GeneralUtilities`SetUsage[TOnes, "TOnes[shape$] returns a fresh f32 TTerm tensor of ones at the given shape$. Convenience for layer-norm gamma init / scale-1 placeholders."];
GeneralUtilities`SetUsage[TZerosLike, "TZerosLike[t$] returns a TTensor handle of zeros matching the shape and dtype of TTerm t$. Suitable for seeding Adam m/v moment buffers."];
GeneralUtilities`SetUsage[TOnesLike, "TOnesLike[t$] returns a TTensor handle of ones matching the shape and dtype of TTerm t$. The ones counterpart of TZerosLike."];
GeneralUtilities`SetUsage[TOneHot, "TOneHot[label$, n$] and TOneHot[label$, n$, dtype$] return a length-n$ TTerm with a 1.0 at index label$ (0-indexed) and 0.0 elsewhere.
TOneHot[labels$, n$] for a list of labels returns the {Length[labels$], n$} one-hot matrix (one row per label), the sequence-one-hot a fixed-window LM forward consumes; a label outside 0..n$-1 yields an all-zero padding row."];

Begin["`Private`"];

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

TOnesLike[t_TTerm] := TOnes[TTensorShape[t], TTensorDType[t]]

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

End[];

EndPackage[];
