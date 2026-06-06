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
            $DTFp32, ToString[bitsToReal32[val]],
            $DTInt32, ToString[bitsToInt32[val]],
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

(* Reduction along an axis: drop that axis.  A rank-1 reduce collapses
   to rank-0 {} (a genuine scalar), mirroring the C-side REDUCE shape
   inference (uop_meta.c since e3a5082f drops the reduced axis even for
   a 1-D source).  An out-of-range axis / rank-0 source stays scalar. *)
dropAxis[shape_List, axis_Integer] := If[
    0 <= axis < Length[shape],
    Delete[shape, axis + 1],
    {}
]
dropAxis[_, _] := {}

(* TenDesc shape lookup -- TENS[id].view.shape via the loaded
   library function $tensorShapeFn (defined in THVMLink.wl).
   Returns Missing[] on out-of-range. *)
tenShapeOf[id_Integer] := Quiet @ Check[$tensorShapeFn[id], Missing[]]

(* tUopShape[t] -- recover the result shape of any TTerm by static
   walk of its UOP graph.  Mirrors materialize_in_env.c's output-
   shape branch but on the WL side, so layer dispatch can size
   intermediate tensors without going through TTensorShape (which
   only handles TAG_TEN).

   Driven ITERATIVELY: an explicit post-order over the term DAG with a
   raw-keyed memo, NOT recursion -- a deep residual chain (e.g. GPT-2's
   12 pre-norm blocks) would otherwise recurse the WL stack past
   $RecursionLimit (1024).  The per-tag formulas (shapeOfNode below) are
   the same as the prior recursive form; only the driver is a loop.  The
   memo also stops a shared sub-DAG being recomputed.

   Returns a List of Integer dims, or $Failed for unsupported tags. *)
tUopShape[t_TTerm] := Module[{root, stack, memo, raw, pending},
    root  = ttermRaw[t];
    memo  = <||>;
    stack = {root};
    While[ stack =!= {},
        raw = Last[stack];
        If[ KeyExistsQ[memo, raw], stack = Most[stack]; Continue[]];
        pending = Select[shapeChildRaws[raw], ! KeyExistsQ[memo, #] &];
        If[ pending =!= {},
            stack = Join[stack, pending],
            memo[raw] = shapeOfNode[raw, memo];
            stack = Most[stack]]
    ];
    Lookup[memo, root, $Failed]
]
tUopShape[_] := $Failed

(* The child term raws whose shapes shapeOfNode[raw] consumes -- empty for
   leaves (TEN / VAR / CONST) and for the movement ops that store their own
   output dims explicitly (RESHAPE / EXPAND).  Kept in lockstep with the
   shapeOfNode cases below. *)
shapeChildRaws[raw_] := If[ $termTagFn[raw] =!= $TagUOP, {},
    With[{val = $termValFn[raw], ext = $termExtFn[raw]},
        Switch[ext,
            $UopKernel, {$heapReadFn[val]},
            $UopAdd | $UopMul | $UopCmplt | $UopCmpeq,
                {$heapReadFn[val], $heapReadFn[val + 1]},
            $UopNeg | $UopRecip | $UopExp2 | $UopLog2 | $UopSqrt | $UopFlip
                | $UopReduce | $UopShrink | $UopPad | $UopPermute | $UopCopy,
                {$heapReadFn[val]},
            _, {}
        ]
    ]
]

(* per-tag output shape from already-computed child shapes in `memo` (raw-keyed;
   the tUopShape driver populates it in post-order, so every child read here is
   present).  `memo` is read-only. *)
shapeOfNode[raw_, memo_] := Module[{tag = $termTagFn[raw], val = $termValFn[raw]},
    Switch[tag,
        $TagTEN, tenShapeOf[val],
        (* TAG_VAR: bound by an enclosing TLam.  Read the lam_shape side table
           via the runtime's term_shape_in (which follows VAR -> SUB -> shape
           annotation).  Returns {} when the lam wasn't shape-annotated;
           surface that as $Failed so callers can react. *)
        $TagVAR,
            With[{s = Quiet @ Check[Normal @ $termShapeInFn[raw], $Failed]},
                If[ s === {} || !ListQ[s] || !VectorQ[s, IntegerQ], $Failed, s]
            ],
        $TagUOP,
            Switch[$termExtFn[raw],
                (* Output buffer is heap[val] = TAG_TEN. *)
                $UopKernel, memo[$heapReadFn[val]],
                $UopConst,  {1},
                $UopAdd | $UopMul | $UopCmplt | $UopCmpeq,
                    With[{a = memo[$heapReadFn[val]], b = memo[$heapReadFn[val + 1]]},
                        If[ a === $Failed || b === $Failed, $Failed, broadcastShape[a, b]]],
                $UopNeg | $UopRecip | $UopExp2 | $UopLog2 | $UopSqrt | $UopFlip
                    | $UopCopy,
                    memo[$heapReadFn[val]],
                (* REDUCE heap: [src, NUM(kind), NUM(n_axes), NUM(axis_0), ...].
                   n_axes at val+2; axes at val+3..val+2+n_axes.  A multi-axis
                   REDUCE folds N axes in one node, so drop ALL N (descending,
                   so each Delete leaves the lower indices valid). *)
                $UopReduce,
                    With[{srcShape = memo[$heapReadFn[val]]},
                        If[ srcShape === $Failed, $Failed,
                            Module[{nAxes, axes},
                                nAxes = $termValFn[$heapReadFn[val + 2]];
                                axes  = Table[$termValFn[$heapReadFn[val + 3 + i]],
                                              {i, 0, nAxes - 1}];
                                Fold[dropAxis, srcShape, ReverseSort[axes]]]]],
                (* RESHAPE / EXPAND heap: [src, NUM(ndim), NUM(d0), ...]; dims at
                   val+2..val+1+ndim.  ndim is stored explicitly, so the shape is
                   read straight off the heap (no child shape needed). *)
                $UopReshape | $UopExpand,
                    Module[{ndim = $termValFn[$heapReadFn[val + 1]]},
                        Table[$termValFn[$heapReadFn[val + 2 + i]], {i, 0, ndim - 1}]],
                (* SHRINK / PAD heap: [src, NUM(ndim), NUM(b0), NUM(e0), ...].
                   SHRINK -> e_i - b_i per axis; PAD grows by b_i + e_i. *)
                $UopShrink,
                    With[{cs = memo[$heapReadFn[val]]},
                        If[ cs === $Failed, $Failed,
                            Table[
                                $termValFn[$heapReadFn[val + 3 + 2 (i - 1)]] -
                                $termValFn[$heapReadFn[val + 2 + 2 (i - 1)]],
                                {i, 1, Length[cs]}]]],
                $UopPad,
                    With[{cs = memo[$heapReadFn[val]]},
                        If[ cs === $Failed, $Failed,
                            Table[
                                cs[[i]] +
                                $termValFn[$heapReadFn[val + 2 + 2 (i - 1)]] +
                                $termValFn[$heapReadFn[val + 3 + 2 (i - 1)]],
                                {i, 1, Length[cs]}]]],
                (* PERMUTE heap: [src, NUM(ndim), NUM(p0), ...]; out.dim[i] =
                   src.dim[perm[i]]. *)
                $UopPermute,
                    With[{cs = memo[$heapReadFn[val]]},
                        If[ cs === $Failed, $Failed,
                            Table[cs[[1 + $termValFn[$heapReadFn[val + 1 + i]]]],
                                  {i, 1, Length[cs]}]]],
                _, $Failed
            ],
        _, $Failed
    ]
]

End[];

EndPackage[];
