(* ::Package:: *)
(* Shape.wl - shape arithmetic + scalar bit decoding.

   Lives in `THVMLink`Private` (shared with the rest of the package
   via BeginPackage["THVMLink`"] + Begin["`Private`"]).  Other files
   reach these helpers without qualification because they share the
   same private context. *)

BeginPackage["THVMLink`"];

Begin["`Private`"];

(* === scalar bit decoding ===
   Manual IEEE 754 single-precision decode -- WL's BinaryReadList
   wants a Stream (not a ByteArray), and writing through a temp
   file inside a render path is too heavy. *)

bitsToReal32[bits_Integer] := Block[{sign, exp, mant},
    sign = If[BitAnd[BitShiftRight[bits, 31], 1] === 1, -1, 1];
    exp  = BitAnd[BitShiftRight[bits, 23], 16^^FF];
    mant = BitAnd[bits, 16^^7FFFFF];
    Which[
        exp === 16^^FF && mant === 0,    sign * Infinity,
        exp === 16^^FF,                  Indeterminate,
        exp === 0 && mant === 0,         0.0 * sign,
        exp === 0,                       sign * (mant / 2.^23) * 2.^-126,
        True,                            sign * (1 + mant / 2.^23) * 2.^(exp - 127)
    ]
]

bitsToInt32[bits_Integer] := If[bits >= 2^31, bits - 2^32, bits]

(* For a TAG_NUM cell, decode its raw bits to a printable scalar
   string per the dtype encoded in `ext`.  Other tags return "". *)
scalarTextFromCell[cellTerm_] := With[{
    tag = TTermTag[cellTerm], ext = TTermExt[cellTerm], val = TTermVal[cellTerm]
},
    If[ tag === $TagNUM,
        Switch[ext,
            $DTF32, ToString[bitsToReal32[val]],
            $DTI32, ToString[bitsToInt32[val]],
            _,      ""
        ],
        ""
    ]
]

(* === shape arithmetic ===
   Mirror of the rules used in src/schedule/materialize.c so the
   diagrams' inferred shape matches what materialize would emit. *)

(* Quoted "{d0,d1,...}" string for label display. *)
shapeText[shape_List] :=
    "{" <> StringRiffle[ToString /@ shape, ","] <> "}"
shapeText[_]          := ""

(* Elementwise broadcast: pick the non-scalar side.  Mirrors the
   v0->numel == 1 check in materialize's op_output_shape. *)
broadcastShape[s1_List, s2_List] := If[(Times @@ s1) === 1, s2, s1]
broadcastShape[s1_,    _]        := s1

(* Reduction along an axis: drop that axis (collapse to {1} for
   rank<=1 inputs to mirror the C-side fallback). *)
dropAxis[shape_List, axis_Integer] := If[
    Length[shape] <= 1, {1},
    Delete[shape, axis + 1]
]
dropAxis[_, _] := {1}

(* TenDesc shape lookup -- TENS[id].view.shape via the loaded
   library function $tensorShapeFn (defined in THVMLink.wl).
   Returns Missing[] on out-of-range. *)
tenShapeOf[id_Integer] := Quiet @ Check[$tensorShapeFn[id], Missing[]]

(* tUopShape[t] -- recover the result shape of any TTerm by static
   walk of its UOP graph.  Mirrors materialize_in_env.c's output-
   shape branch but on the WL side, so layer dispatch can size
   intermediate tensors without going through TTensorShape (which
   only handles TAG_TEN).

   Returns a List of Integer dims, or $Failed for unsupported tags. *)
tUopShape[t_TTerm] := Module[{raw, tag, val, ext},
    raw = ttermRaw[t];
    tag = $termTagFn[raw];
    val = $termValFn[raw];
    ext = $termExtFn[raw];
    Switch[tag,
        $TagTEN, tenShapeOf[val],
        $TagUOP,
            Switch[ext,
                $UopKernel,
                    (* Output buffer is heap[val] = TAG_TEN. *)
                    tUopShape[TTerm[$heapReadFn[val]]],
                $UopConst,
                    {1},
                $UopAdd | $UopMul | $UopCmplt | $UopCmpeq,
                    broadcastShape[
                        tUopShape[TTerm[$heapReadFn[val]]],
                        tUopShape[TTerm[$heapReadFn[val + 1]]]
                    ],
                $UopNeg | $UopRecip | $UopExp2 | $UopLog2 | $UopSqrt,
                    tUopShape[TTerm[$heapReadFn[val]]],
                $UopReduce,
                    dropAxis[
                        tUopShape[TTerm[$heapReadFn[val]]],
                        $termValFn[$heapReadFn[val + 2]]
                    ],
                $UopReshape,
                    (* RESHAPE heap layout: [src, NUM(ndim), NUM(d0), ...];
                       dims live at val+2..val+1+ndim.  ndim is stored
                       explicitly; the previous numel-based termination
                       hack broke on shapes containing leading 1s. *)
                    Module[{ndim},
                        ndim = $termValFn[$heapReadFn[val + 1]];
                        Table[$termValFn[$heapReadFn[val + 2 + i]],
                              {i, 0, ndim - 1}]
                    ],
                $UopExpand,
                    (* EXPAND heap layout: [src, NUM(ndim), NUM(d0), ...];
                       dims live at val+2..val+1+ndim.  ndim is stored
                       explicitly so EXPAND can change rank. *)
                    Module[{ndim},
                        ndim = $termValFn[$heapReadFn[val + 1]];
                        Table[$termValFn[$heapReadFn[val + 2 + i]],
                              {i, 0, ndim - 1}]
                    ],
                (* SHRINK / PAD heap: [src, NUM(ndim), NUM(b0), NUM(e0), ...].
                   ndim cell at val+1; pairs at val+2..val+1+2*ndim.
                   SHRINK shrinks each axis to e_i - b_i; PAD grows by
                   b_i + e_i. *)
                $UopShrink,
                    Module[{cs, n},
                        cs = tUopShape[TTerm[$heapReadFn[val]]];
                        If[ cs === $Failed, Return[$Failed, Module]];
                        n = Length[cs];
                        Table[
                            $termValFn[$heapReadFn[val + 3 + 2 * (i - 1)]] -
                            $termValFn[$heapReadFn[val + 2 + 2 * (i - 1)]],
                            {i, 1, n}]
                    ],
                $UopPad,
                    Module[{cs, n},
                        cs = tUopShape[TTerm[$heapReadFn[val]]];
                        If[ cs === $Failed, Return[$Failed, Module]];
                        n = Length[cs];
                        Table[
                            cs[[i]] +
                            $termValFn[$heapReadFn[val + 2 + 2 * (i - 1)]] +
                            $termValFn[$heapReadFn[val + 3 + 2 * (i - 1)]],
                            {i, 1, n}]
                    ],
                (* PERMUTE heap: [src, NUM(ndim), NUM(p0), NUM(p1), ...].
                   out.dim[i] = src.dim[perm[i]]. *)
                $UopPermute,
                    Module[{cs, n},
                        cs = tUopShape[TTerm[$heapReadFn[val]]];
                        If[ cs === $Failed, Return[$Failed, Module]];
                        n = Length[cs];
                        Table[
                            cs[[1 + $termValFn[$heapReadFn[val + 1 + i]]]],
                            {i, 1, n}]
                    ],
                (* FLIP doesn't change shape. *)
                $UopFlip,
                    tUopShape[TTerm[$heapReadFn[val]]],
                _, $Failed
            ],
        _, $Failed
    ]
]
tUopShape[_] := $Failed

End[];

EndPackage[];
