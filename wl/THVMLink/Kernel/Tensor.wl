(* ::Package:: *)
(* Tensor.wl - tensor + UOp WL surface for THVMLink.

   Loaded from THVMLink.wl inside Begin["`Private`"]; all symbols
   stay in the main THVMLink` context so users see TTensor /
   TUOp* without a subcontext namespace.  Sibling-split purely for
   size -- THVMLink.wl now handles lifecycle + IC combinators +
   atomic term; Tensor.wl handles anything tensor-shaped.

   Three groups of definitions live here:
     1. TTensor constructors + inspection  (TTensor, TTensorShape,
        TTensorDType, TTensorData, TTensorRefcount).
     2. TUOp graph builders                 (TUOpConst, TUOpAdd,
        TUOpMul, ..., TUOpReduce, TUOpMaterialize, TUOpKind,
        TUOpSrcs).
     3. UpValues on TTerm that make Plus / Times / Minus over
        tensor terms build UOp graphs automatically.
*)

(* === predicates ===
   tensorTermQ[t]: true iff t is a TTerm whose tag makes it a
   tensor-shaped value (TAG_TEN or TAG_UOP).  Used to guard the
   numerical UpValues so we never intercept IC combinators
   (LAM, APP, DUP, ...) -- those still follow the normal IC
   reduction rules. *)

tensorTermQ[t_TTerm] := With[{tag = $termTagFn[ttermRaw[t]]},
    tag === $TagTEN || tag === $TagUOP
]
tensorTermQ[_]       := False

(* A TUOpConst wraps a plain numeric.  Used to lift a scalar on the
   right-hand side of Plus / Times against a tensor term. *)
liftNumeric[n_?NumericQ, dtype_String] := TUOpConst[n, dtype]
liftNumeric[t_TTerm,     _]            := t

(* Pick a dtype to broadcast into.  If any side is a TAG_TEN or
   TAG_UOP with a dtype, use that; else default to f32. *)
inheritDType[t_TTerm] := With[{tag = $termTagFn[ttermRaw[t]]},
    Switch[tag,
        $TagTEN, dtypeName[$termExtFn[ttermRaw[t]]],
        _,       "f32"
    ]
]
inheritDType[_] := "f32"

broadcastDType[a_, b_] := First[
    Select[{inheritDType[a], inheritDType[b]}, StringQ],
    "f32"
]

(* === TTensor constructors === *)

TTensor[shape_List]                       := TTensor[shape, "f32"]
TTensor[shape_List, dtype_String]         := (
    ensureInit[];
    TTerm[$tensorAllocFn[dtypeCode[dtype], shape]]
)
TTensor[shape_List, data_List]            := TTensor[shape, data, "f32"]
TTensor[shape_List, data_List, dtype_String] := With[{
    t = TTensor[shape, dtype]
},
    If[ dtype === "f32",
        $tensorWriteFn [TTermVal[t], N @ Flatten[data]],
        $tensorWriteIFn[TTermVal[t], Round @ Flatten[data]]
    ];
    t
]

(* Reading a tensor -- only valid once `t` is actually a TAG_TEN.
   If `t` is a UOp or IC term, we return a typed Missing rather
   than read random memory, so test failures surface cleanly
   instead of crashing WL. *)

tensorIdQ[t_] := TTermTag[t] === $TagTEN

TTensorShape[t_ ? tensorIdQ]    := $tensorShapeFn[TTermVal[t]]
TTensorShape[t_TTerm]           := Missing["NotATensor", TTagName[TTermTag[t]]]

TTensorDType[t_ ? tensorIdQ]    := dtypeName[TTermExt[t]]
TTensorDType[t_TTerm]           := Missing["NotATensor", TTagName[TTermTag[t]]]

TTensorRefcount[t_ ? tensorIdQ] := $tensorRcFn[TTermVal[t]]
TTensorRefcount[t_TTerm]        := Missing["NotATensor", TTagName[TTermTag[t]]]

TTensorData[t_ ? tensorIdQ] := With[{id = TTermVal[t], dt = TTermExt[t]},
    If[ dt === $DTF32, $tensorReadFn[id], $tensorReadIFn[id]]
]
TTensorData[t_TTerm] := Missing["NotATensor", TTagName[TTermTag[t]]]

(* === UOp graph constructors === *)

TUOpConst[value_, dtype_String : "f32"] := (
    ensureInit[];
    TTerm[$uopConstFn[dtypeCode[dtype], N[value]]]
)

TUOpAdd[a_, b_]   := (ensureInit[]; TTerm[$uopBinaryFn[$UopAdd,   ttermRaw[a], ttermRaw[b]]])
TUOpMul[a_, b_]   := (ensureInit[]; TTerm[$uopBinaryFn[$UopMul,   ttermRaw[a], ttermRaw[b]]])
TUOpCmplt[a_, b_] := (ensureInit[]; TTerm[$uopBinaryFn[$UopCmplt, ttermRaw[a], ttermRaw[b]]])

TUOpNeg[a_]   := (ensureInit[]; TTerm[$uopUnaryFn[$UopNeg,   ttermRaw[a]]])
TUOpRecip[a_] := (ensureInit[]; TTerm[$uopUnaryFn[$UopRecip, ttermRaw[a]]])
TUOpExp2[a_]  := (ensureInit[]; TTerm[$uopUnaryFn[$UopExp2,  ttermRaw[a]]])
TUOpLog2[a_]  := (ensureInit[]; TTerm[$uopUnaryFn[$UopLog2,  ttermRaw[a]]])
TUOpSqrt[a_]  := (ensureInit[]; TTerm[$uopUnaryFn[$UopSqrt,  ttermRaw[a]]])

TUOpReduce[src_, axis_Integer, kind_String] := (
    ensureInit[];
    TTerm[$uopReduceFn[reduceKindCode[kind], axis, ttermRaw[src]]]
)

TUOpReshape[src_, shape_List] := (ensureInit[]; TTerm[$uopReshapeFn[ttermRaw[src], shape]])
TUOpPermute[src_, axes_List]  := (ensureInit[]; TTerm[$uopPermuteFn[ttermRaw[src], axes]])
TUOpExpand [src_, shape_List] := (ensureInit[]; TTerm[$uopExpandFn [ttermRaw[src], shape]])
TUOpPad    [src_, ranges_List]:= (ensureInit[]; TTerm[$uopPadFn    [ttermRaw[src], Flatten[ranges]]])
TUOpShrink [src_, ranges_List]:= (ensureInit[]; TTerm[$uopShrinkFn [ttermRaw[src], Flatten[ranges]]])

TUOpFlip[src_, axes_List] := With[{mask = Total[2^# & /@ axes]},
    ensureInit[];
    TTerm[$uopFlipFn[ttermRaw[src], mask]]
]

TUOpMaterialize[expr_] := (ensureInit[]; TTerm[$uopMatFn[ttermRaw[expr]]])

TUOpKind[u_] := Lookup[$uopNames, TTermExt[u], "UOP?" <> ToString[TTermExt[u]]]

TUOpSrcs[u_] := With[{loc = TTermVal[u], op = TTermExt[u]},
    Module[{n},
        n = Which[
            op === $UopMaterialize,                                                            1,
            op === $UopKernel,                                                                  2,
            op === $UopConst,                                                                   1,
            MemberQ[{$UopReshape, $UopPermute, $UopExpand,
                     $UopPad, $UopShrink, $UopFlip}, op],                                      1,
            MemberQ[{$UopAdd, $UopMul, $UopCmplt}, op],                                        2,
            MemberQ[{$UopNeg, $UopRecip, $UopExp2, $UopLog2, $UopSqrt}, op],                   1,
            op === $UopReduce,                                                                  1,
            True,                                                                               1
        ];
        Table[THeapRead[loc + i], {i, 0, n - 1}]
    ]
]

(* === numerical UpValues ===

   Let users write ordinary WL arithmetic against tensor terms and
   have it build UOp graphs without any manual TUOpAdd/TUOpMul
   sprinkling.  Only fires when at least one side is a
   tensor-shaped TTerm (tensorTermQ); IC combinator terms are left
   alone.

   Scalars on the other side get lifted to UOP_CONST with the
   broadcasting dtype.  Variadic Plus/Times folds pairwise so
   an `a + b + c` becomes `TUOpAdd[TUOpAdd[a, b], c]`. *)

pairFold[op_, args_List] := Fold[op, First[args], Rest[args]]

TTerm /: Plus[t_TTerm ? tensorTermQ, rest__] := With[{
    lifted = liftNumeric[#, broadcastDType[t, #]] & /@ {rest}
},
    pairFold[TUOpAdd, Prepend[lifted, t]]
]

TTerm /: Times[t_TTerm ? tensorTermQ, rest__] := With[{
    lifted = liftNumeric[#, broadcastDType[t, #]] & /@ {rest}
},
    pairFold[TUOpMul, Prepend[lifted, t]]
]

TTerm /: Minus[t_TTerm ? tensorTermQ] := TUOpNeg[t]

TTerm /: Power[t_TTerm ? tensorTermQ, Rational[1, 2]] := TUOpSqrt[t]
TTerm /: Power[t_TTerm ? tensorTermQ, -1]             := TUOpRecip[t]

TTerm /: Less[a_TTerm ? tensorTermQ, b_] := TUOpCmplt[a, liftNumeric[b, broadcastDType[a, b]]]
TTerm /: Less[a_, b_TTerm ? tensorTermQ] := TUOpCmplt[liftNumeric[a, broadcastDType[a, b]], b]
