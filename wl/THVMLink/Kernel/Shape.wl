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

End[];

EndPackage[];
