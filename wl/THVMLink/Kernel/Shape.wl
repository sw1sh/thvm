(* ::Package:: *)
(* Shape.wl - shape arithmetic + scalar bit decoding.

   Lives in `WolframInstitute`THVMLink`Private` (shared with the rest of the package
   via BeginPackage["WolframInstitute`THVMLink`"] + Begin["`Private`"]).  Other files
   reach these helpers without qualification because they share the
   same private context. *)

BeginPackage["WolframInstitute`THVMLink`"];

Begin["`Private`"];

(* === scalar bit decoding ===
   Manual IEEE 754 single-precision decode: WL's BinaryReadList
   wants a Stream (not a ByteArray), and writing through a temp
   file inside a render path is too heavy. *)

bitsToReal32[bits_Integer] := Block[{sign, exp, mant},
    sign = If[BitAnd[BitShiftRight[bits, 31], 1] === 1, -1, 1];
    exp = BitAnd[BitShiftRight[bits, 23], 16^^FF];
    mant = BitAnd[bits, 16^^7FFFFF];
    Which[
        exp === 16^^FF && mant === 0, sign * Infinity,
        exp === 16^^FF, Indeterminate,
        exp === 0 && mant === 0, 0.0 * sign,
        exp === 0, sign * (mant / 2.^23) * 2.^-126,
        True, sign * (1 + mant / 2.^23) * 2.^(exp - 127)
    ]
]

bitsToInt32[bits_Integer] := If[bits >= 2^31, bits - 2^32, bits]

(* For a TAG_NUM cell, decode its raw bits to a printable scalar
   string per the dtype encoded in `ext`.  Other tags return "". *)
scalarTextFromCell[cellTerm_] := With[{tag = TTermTag[cellTerm], ext = TTermExt[cellTerm], val = TTermVal[cellTerm]},
    If[ tag === $TagNUM,
        Switch[ext,
            $DTFp32, ToString[bitsToReal32[val]],
            $DTInt32, ToString[bitsToInt32[val]],
            _, ""
        ],
        ""
    ]
]

(* === shape arithmetic ===
   Mirror of the rules used in src/schedule/materialize.c so the
   diagrams' inferred shape matches what materialize would emit. *)

(* Quoted "{d0,d1,...}" string for label display. *)
shapeText[shape_List] := "{" <> StringRiffle[ToString /@ shape, ","] <> "}"
shapeText[_] := ""

(* Elementwise broadcast: pick the non-scalar side.  Mirrors the
   v0->numel == 1 check in materialize's op_output_shape. *)
broadcastShape[s1_List, s2_List] := If[(Times @@ s1) === 1, s2, s1]
broadcastShape[s1_, _] := s1

(* Reduction along one or more axes: drop them all.  A multi-axis REDUCE
   folds N axes in one shot, so the shape mirror must drop every reduced
   axis (mirroring src/uop/reduce.c).  Positions are deleted in a single
   Delete so an earlier drop does not shift the index of a later one.
   Out-of-range axes are ignored; a full reduce collapses to rank-0 {}
   (a genuine scalar). *)
dropAxes[shape_List, axes_List] := Delete[shape, List /@ (Select[axes, 0 <= # < Length[shape] &] + 1)]
dropAxes[_, _] := {}

(* TenDesc shape lookup: TENS[id].view.shape via the loaded
   library function $tensorShapeFn (defined in THVMLink.wl).
   Returns Missing[] on out-of-range. *)
tenShapeOf[id_Integer] := Quiet @ Check[$tensorShapeFn[id], Missing[]]

(* tUopShape[t]: the result shape of any TTerm, as a List of Integer dims
   (or $Failed when it cannot be determined).  Delegates straight to the
   runtime's term_shape_in ($termShapeInFn), the single, authoritative,
   per-term-CACHED shape inference.  The C inference caches per term, so
   construction over a deep graph is O(N) and stays reclim-safe at any
   real depth.

   The C wrapper returns a length-0 list for BOTH a rank-0 scalar and a
   failure (the ok flag is not exposed), so a lone {} is disambiguated by
   tag: a TEN / UOP scalar is a genuine {} shape, but a TAG_VAR with no
   TLamShape annotation is unshapeable -> $Failed. *)
tUopShape[t_TTerm] := With[{raw = ttermRaw[t]},
    With[{s = Quiet @ Check[Normal @ $termShapeInFn[raw], $Failed]},
        Which[
            ! ListQ[s] || ! VectorQ[s, IntegerQ], $Failed,
            s =!= {}, s,
            $termTagFn[raw] === $TagVAR, $Failed,
            True, s
        ]
    ]
]
tUopShape[_] := $Failed

(* tUopDType[t]: the dtype NAME ("f32" / "bf16" / ...) the runtime infers
   for a TTerm -- a realized TEN's stored dtype, or an unrealized UOP's
   inferred dtype (term_dtype_in walks the graph without materializing).
   term_dtype_in always succeeds (defaulting to f32), so this returns a
   usable dtype for any tensor-valued term. *)
tUopDType[t_TTerm] := dtypeName[$termDtypeInFn[ttermRaw[t]]]
tUopDType[_] := "f32"

End[];

EndPackage[];
