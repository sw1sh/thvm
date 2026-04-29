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
        TUOpMul, ..., TUOpReduce, TUOpKind, TUOpSrcs).
     3. UpValues on TTerm that make Plus / Times / Minus over
        tensor terms build UOp graphs automatically.
*)

BeginPackage["THVMLink`"];

TSet::usage = "TSet[dst, src] writes the bytes of `src` into `dst`'s backing buffer in place; `dst` keeps its TenDesc id so callers still holding it observe the new contents.  Equivalent to `TRealize[TAssign[dst, src]]; dst`.  Also installed as the WL Set UpValue on literal-TTerm LHSes, so `Evaluate[w] = expr` mutates `w` rather than rebinding the symbol.";

(* Forward-decl: these are defined in NN.wl (loads alphabetically
   after Tensor.wl).  Without this, the UpValues below resolve to
   phantoms in `THVMLink`Private`* with no DownValue. *)
{TTanh, TMatMul, TDot, TSoftmaxAxis};

Begin["`Private`"];

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
liftNumeric[n_ ? NumericQ, dtype_String] := TUOpConst[n, dtype]
liftNumeric[t_TTerm,       _]            := t

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

(* TTermShape -- runs the C-side `term_shape_in` resolver, which
   handles TEN, UOP (shape-inferred from children), and TVAR
   (consults the lam_shape side table).  Returns {} when the
   shape can't be determined; otherwise a list of dim extents.
   Use this when you want shape inference, not just "is this a
   concrete TEN". *)
TTermShape[t_TTerm] := Normal @ THVMLink`Private`$termShapeInFn[ttermRaw[t]]
TTermShape[i_Integer] := Normal @ THVMLink`Private`$termShapeInFn[i]

TTensorDType[t_ ? tensorIdQ]    := dtypeName[TTermExt[t]]
TTensorDType[t_TTerm]           := Missing["NotATensor", TTagName[TTermTag[t]]]

TTensorRefcount[t_ ? tensorIdQ] := $tensorRcFn[TTermVal[t]]
TTensorRefcount[t_TTerm]        := Missing["NotATensor", TTagName[TTermTag[t]]]

(* TTensorData returns a NumericArray whose type matches the
   tensor's dtype (Real32 for DT_F32, Integer32 for DT_I32).  Callers
   that want a plain List can wrap in `Normal`. *)
TTensorData[t_ ? tensorIdQ] := $tensorReadFn[TTermVal[t]]
TTensorData[t_TTerm]        := Missing["NotATensor", TTagName[TTermTag[t]]]

(* === UOp graph constructors === *)

TUOpConst[value_, dtype_String : "f32"] := (
    ensureInit[];
    TTerm[$uopConstFn[dtypeCode[dtype], N[value]]]
)

TUOpAdd[a_, b_]   := (ensureInit[]; TTerm[$uopBinaryFn[$UopAdd,   ttermRaw[a], ttermRaw[b]]])
TUOpMul[a_, b_]   := (ensureInit[]; TTerm[$uopBinaryFn[$UopMul,   ttermRaw[a], ttermRaw[b]]])
TUOpCmplt[a_, b_] := (ensureInit[]; TTerm[$uopBinaryFn[$UopCmplt, ttermRaw[a], ttermRaw[b]]])
TUOpCmpeq[a_, b_] := (ensureInit[]; TTerm[$uopBinaryFn[$UopCmpeq, ttermRaw[a], ttermRaw[b]]])

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

(* Build the BWD projection over a fresh dup-like grad cell holding
   [y, gy].  Returns a TAG_DP1 term with DUP_GRAD_FLAG set on its ext.
   The companion FWD projection (TAG_DP0) at the same loc just reads
   y; the BWD fires the gy-threaded chain rule.  gy must match y's
   shape -- TGrad below builds a default ones-at-y.shape seed. *)
TUOpGrad[y_, gy_] := (ensureInit[]; TTerm[$uopGradFn[ttermRaw[y], ttermRaw[gy]]])
TUOpGradWithTarget[y_, gy_, target_] := (ensureInit[];
    TTerm[$uopGradWithTargetFn[ttermRaw[y], ttermRaw[gy], ttermRaw[target]]])
TUOpFwd [y_, gy_] := (ensureInit[]; TTerm[$uopFwdFn [ttermRaw[y], ttermRaw[gy]]])

(* Build {fwd, bwd} pair sharing one cell [y, gy].  The dup-like
   discipline -- both projections reference the same heap loc, so the
   forward subgraph isn't duplicated. *)
TGradPair[y_, gy_] := With[{bwd = TUOpGrad[y, gy]},
    With[{loc = TTermVal[bwd]},
        {packTerm[0, $TagDP0, $DupGradFlag, loc], bwd}
    ]
]

TUOpLoad[src_] := (ensureInit[]; TTerm[$uopLoadFn[ttermRaw[src]]])

(* TAssign[dst_TEN, src_UOP_or_TEN] -- in-place buffer write.  Wnf
   fires it once `src` reduces to a TAG_TEN: backend memcpy of
   src.buf into dst.buf, returns dst.  Used by optimizer loops to
   mutate weight tensors without allocating fresh tids per step.
   Both children share the binary heap layout; reuses $uopBinaryFn
   under opcode $UopAssign. *)
TAssign[dst_, src_] := (ensureInit[];
    TTerm[$uopBinaryFn[$UopAssign, ttermRaw[dst], ttermRaw[src]]])

(* TConv2D[input, weights, bias] is defined in NN.wl alongside the
   other neural-network layers. *)

(* TGrad[y, target] / TGrad[y, target, gy]: the user-facing VJP.

   The chain rule threads gy down to each TEN leaf, emitting
   SUP^{leaf_tid}(zero_at_leaf.shape, gy_at_leaf.shape).  To extract
   the per-target gradient we need ONE DUP per distinct leaf-tid in y,
   projecting:
     - MATCH side  for the target's leaf SUP   (= gy, the per-element grad)
     - MISMATCH side for every other leaf SUP  (= zero, no contribution)

   We discover leaf tids by walking y once at construction time and
   nest the DUPs: TDup[t1, ..., TDup[t2, ..., TUOpGrad[y, gy]]] each
   projecting the appropriate side based on whether ti == target.tid.

   Default gy = ones at y's shape.  For scalar y (shape {1}) this is
   just CONST(1.0); for tensor y we EXPAND CONST(1.0) to y's shape so
   the chain rule's adjoint shape transforms (RESHAPE/PERMUTE/etc.)
   land at well-defined ranks. *)
gradLeafTids[t_TTerm] := Module[{tag = TTermTag[t]},
    Which[
        tag === $TagTEN, {TTermVal[t]},
        tag === $TagUOP, gradLeafTidsUop[TTermExt[t], TTermVal[t]],
        (* Higher-order TGrad: y can contain DUP projections (TDP0/DP1)
           whose cell body holds the actual UOp/TEN.  Walk cell[0] so
           we find leaves embedded in unfired sub-cells from a prior
           chain-rule pass. *)
        tag === $TagDP0 || tag === $TagDP1, gradLeafTids[THeapRead[TTermVal[t]]],
        True, {}
    ]
]
gradLeafTids[_] := {}

gradLeafTidsUop[opcode_, base_] := Module[{n = uopArity[opcode]},
    DeleteDuplicates @ Flatten @ Table[gradLeafTids[THeapRead[base + i]], {i, 0, n - 1}]
]

(* Default cotangent seed for TGrad: ones at y's shape.  CONST is a
   scalar (shape {1}); EXPAND lifts it to y.shape when y is non-scalar. *)
gradOnesSeed[y_TTerm] := Module[{shape = tUopShape[y], one},
    one = TUOpConst[1.0];
    If[ ListQ[shape] && Length[shape] > 0 && shape =!= {1},
        TUOpExpand[one, shape],
        one
    ]
]

TGrad[y_, target_TTerm] := TGrad[y, target, gradOnesSeed[y]]
TGrad[y_, target_TTerm, gy_TTerm] := Module[
    {targetTid, leafTids, matchProj, mismatchProj, targetShape, dupNest, zero,
     targetTag},
    targetTid    = TTermVal[target];
    targetTag    = TTermTag[target];
    targetShape  = TTensorShape[target];
    (* When target is a free variable (TVAR -- typically captured by a
       surrounding TLam that hasn't beta-reduced yet) leaf tids inside
       y can't be discovered statically.  Build a single TUOpGradWith-
       Target cell instead of the per-leaf DUP nest; the C-side chain
       rule does direct tid match against `target` (resolved through
       SUB at firing time, post-beta).  ListQ[targetShape] gates the
       fall-through to the zero-broadcast: TVAR target has no shape
       (TTensorShape returns Missing) so we just return the GRAD
       term directly -- the chain rule produces a tensor at y's
       shape, which is the natural gradient shape for the bound
       variable. *)
    If[ targetTag =!= $TagTEN,
        Return[TUOpGradWithTarget[y, gy, target]]
    ];
    leafTids     = gradLeafTids[y];
    matchProj    = {a, b} |-> b;
    mismatchProj = {a, b} |-> a;
    dupNest = Fold[
        {inner, tid} |-> TDup[tid, inner, If[ tid === targetTid, matchProj, mismatchProj]],
        TUOpGrad[y, gy],
        leafTids
    ];
    (* Broadcast a scalar zero into the result so it always lands at
       target.shape.  Two cases this fixes:
         (a) target absent from y (or non-differentiable op like CMPLT)
             -> chain rule yields scalar CONST(0); ADD broadcasts it
             across target.shape giving zeros at target.shape.
         (b) higher-order TGrad where the inner chain rule's matched
             value reduces to a constant or a sub-shape -> the ADD
             pads the shape declaration up to target.shape without
             changing matched values (cpu_op_add with numel==1 src
             broadcasts cleanly).
       For the common case where dupNest already lands at
       target.shape, the EXPAND of a scalar 0 + ADD is value-neutral. *)
    If[ ListQ[targetShape] && Length[targetShape] > 0,
        zero = TUOpExpand[TUOpConst[0.0], targetShape];
        TUOpAdd[dupNest, zero],
        dupNest
    ]
]

(* Multi-target VJP: build n unary TGrads sharing the y subgraph (and
   the gy seed) by heap-loc identity.  materialize's per-realize memo
   dedups every forward kernel emitted from those shared UOps across
   all n targets.  Returns a List of n TTerms in the same order as
   `targets`. *)
TGradMany[y_, {target_}]    := {TGrad[y, target]}
TGradMany[y_, targets_List] := With[{seed = gradOnesSeed[y]},
    TGrad[y, #, seed] & /@ targets
]

(* TRealize: heap-walk materialize (in-place rewrite UOPs to UOP_KERNELs)
   then TWnf to beta-reduce and fire the kernels.  No UOP_MATERIALIZE
   wrapper -- thvm_materialize is invoked directly. *)
(* TRealize: one-shot materialize + wnf wrapped in a per-step
   buffer-pool boundary (sub-item b of the per-step buffer pool
   arc).  Equivalent to TWnf[TMaterialize[expr]] except every CPU
   buffer alloc'd during materialize+wnf that ISN'T reachable from
   the result tensor's producer chain gets freed at exit. *)
TRealize[expr_] := (ensureInit[]; TTerm[$realizeFn[ttermRaw[expr]]])

(* TMaterialize = direct schedule + kernelize + linearize rewrite,
   no firing.  Useful for inspection (visualize the scheduled DAG
   before dispatch).  The return value is a UOP_KERNEL term whose
   first heap cell is the pre-allocated output TAG_TEN (empty) and
   second cell is a TAG_NUM carrying the KernelEntry id.  Subsequent
   TWnf fires the kernels bottom-up (once commit 4 lands). *)
TMaterialize[expr_] := (ensureInit[]; TTerm[$materializeFn[ttermRaw[expr]]])

(* TKernelCount / TKernelProgramCacheSize / TKernelInfo  --  defined
   in Kernel.wl alongside the rest of the kernel-introspection surface. *)

(* === TTensorCreate: implicit shape from data ===

   - If `data` is a NumericArray of supported dtype, share its buffer
     via the Shared-NumericArray passing mode (zero copy on CPU).
   - If `data` is a PackedArray or nested list of reals, we can't
     share directly (Mathematica's PackedArray isn't exposed via the
     NumericArray ABI); lift it to a NumericArray first.  The
     conversion is one internal copy; once in NumericArray form the
     subsequent C-side wrap is zero copy.
   - Shape and dtype are inferred from `data`; callers who want to
     override either should use the explicit `TTensor[shape, data,
     dtype]` form. *)

(* Detect the NumericArray subtype we can consume directly. *)
$sharedNATypes = {"Real32", "Integer32"};

sharedDTypeOf["Real32"]   := "f32"
sharedDTypeOf["Integer32"]:= "i32"
sharedDTypeOf[_]          := Missing["UnsupportedNAType"]

asSharableNA[na_NumericArray] /; MemberQ[$sharedNATypes, NumericArrayType[na]] := na
asSharableNA[na_NumericArray]           := NumericArray[Normal[na], "Real32"]

(* A plain list (possibly nested): let NumericArray pick a storage
   type based on element heads.  Preserve the shape; no Flatten. *)
asSharableNA[data_List] := With[{type =
    If[ AllTrue[Flatten[{data}], IntegerQ], "Integer32", "Real32"]
},
    NumericArray[data, type]
]

(* PackedArray: dispatch on element type. *)
asSharableNA[data_?Developer`PackedArrayQ] :=
    With[{t = Developer`PackedArrayType[data]},
        NumericArray[data,
            Switch[t, Integer, "Integer32", Real, "Real32", _, "Real32"]]
    ]

TTensorCreate[data_] := (
    ensureInit[];
    Module[{na = asSharableNA[data]},
        (* Infer target dtype from NumericArray type.  Reshape comes
           from the Dimensions of the data itself. *)
        With[{t = TTerm[$tensorFromNAFn[na]]},
            t
        ]
    ]
)

TUOpKind[u_] := Lookup[$uopNames, TTermExt[u], "UOP?" <> ToString[TTermExt[u]]]

TUOpSrcs[u_] := With[{loc = TTermVal[u], op = TTermExt[u]},
    Module[{n},
        n = Which[
            op === $UopMaterialize,                                                            1,
            op === $UopKernel,                                                                  2,
            op === $UopConst,                                                                   1,
            MemberQ[{$UopReshape, $UopPermute, $UopExpand,
                     $UopPad, $UopShrink, $UopFlip}, op],                                      1,
            MemberQ[{$UopAdd, $UopMul, $UopCmplt, $UopCmpeq}, op],                             2,
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

(* Frontend pre-broadcast: rank-0 / shape-{1} scalars lifted from
   a host-side numeric get EXPAND'd to the LHS shape BEFORE the
   binop UOP is constructed.  Same architectural slot as tinygrad's
   `Tensor._broadcasted` (tinygrad/tensor.py:3554-3571): both
   operands of an elementwise binop are normalised to a common
   shape at term-construction time, so the UOP graph downstream
   is rank-matched from the start.

   Without it, MUL[t:{N}, scalar:{1}] would fall to the
   elementwise numel-cycle broadcast, which produces a DIFFERENT
   KProgOp[] than the rank-matched form -- the kernel-program-
   cache hashes on KProgOp[] bytes, so the unbroadcasted form
   misses on every call (~60 ms cold-clang-JIT cost) while the
   pre-broadcasted form caches to ~0.1 ms.

   Why frontend, not a UOP-rewriting pass: a rewriter would walk
   every elementwise op + insert EXPAND post-hoc; the frontend
   does it once at construction.  Why not in the kernel codegen
   itself: same KProgOp[] hash divergence; the cache would still
   need a frontend-level normalisation upstream.  Tinygrad makes
   the same choice. *)
broadcastScalar[t_TTerm, targetShape_List] := With[{s = tUopShape[t]},
    If[ (s === {} || s === {1}) && targetShape =!= s,
        TUOpExpand[t, targetShape],
        t]]
broadcastScalar[other_, _] := other

TTerm /: Plus[t_TTerm ? tensorTermQ, rest__] := With[{
    targetShape = tUopShape[t],
    lifted = liftNumeric[#, broadcastDType[t, #]] & /@ {rest}
},
    pairFold[TUOpAdd,
        Prepend[broadcastScalar[#, targetShape] & /@ lifted, t]]
]

TTerm /: Times[t_TTerm ? tensorTermQ, rest__] := With[{
    targetShape = tUopShape[t],
    lifted = liftNumeric[#, broadcastDType[t, #]] & /@ {rest}
},
    pairFold[TUOpMul,
        Prepend[broadcastScalar[#, targetShape] & /@ lifted, t]]
]

TTerm /: Minus[t_TTerm ? tensorTermQ] := TUOpNeg[t]

TTerm /: Power[t_TTerm ? tensorTermQ, Rational[1, 2]] := TUOpSqrt[t]
TTerm /: Power[t_TTerm ? tensorTermQ, -1]             := TUOpRecip[t]
(* Integer exponent n >= 1: expand to repeated multiplication.  Required
   so plain WL `t^3` (e.g. inside an ElementwiseLayer's GELU function
   body) lowers to a TTerm chain rather than staying as Power[TTerm, 3].
   No bespoke UOP_POW; the chain is folded MUL ops the existing path
   already handles. *)
TTerm /: Power[t_TTerm ? tensorTermQ, n_Integer ? Positive] :=
    Fold[TUOpMul[#1, t] &, t, ConstantArray[t, n - 1]]

TTerm /: Sqrt[t_TTerm ? tensorTermQ] := TUOpSqrt[t]
TTerm /: Tanh[t_TTerm ? tensorTermQ] := TTanh[t]

(* Exp / Log: route through the EXP2 / LOG2 primitives via the
   constant log2(e) / ln(2) factors.  Mirror the tExp / TLog helpers
   in NN.wl but exposed on the standard WL function names so callers
   write `Exp[x]` / `Log[x]` against a TTerm naturally. *)
TTerm /: Exp[t_TTerm ? tensorTermQ] :=
    TUOpExp2[TUOpMul[t, TUOpConst[N[Log2[E]], inheritDType[t]]]]
TTerm /: Log[t_TTerm ? tensorTermQ] :=
    TUOpMul[TUOpLog2[t], TUOpConst[N[Log[2]], inheritDType[t]]]

(* Total[t]: REDUCE_SUM along axis 0 (matches WL's Total which sums
   the outermost level).  Total[t, axis] reduces along an arbitrary
   axis (1-indexed in WL convention; the runtime is 0-indexed so we
   subtract).  Total[t, All] sums every axis to a scalar via repeated
   reductions. *)
TTerm /: Total[t_TTerm ? tensorTermQ]                  := TUOpReduce[t, 0, "SUM"]
TTerm /: Total[t_TTerm ? tensorTermQ, axis_Integer]    := TUOpReduce[t, axis - 1, "SUM"]
TTerm /: Total[t_TTerm ? tensorTermQ, All]             := Fold[
    TUOpReduce[#1, 0, "SUM"] &, t,
    Range[Length[tUopShape[t]]]    (* one reduce per axis, all from axis 0 *)
]

TTerm /: Less[a_TTerm ? tensorTermQ, b_] :=
    TUOpCmplt[a,
        broadcastScalar[liftNumeric[b, inheritDType[a]], tUopShape[a]]]
TTerm /: Less[a_, b_TTerm ? tensorTermQ] :=
    TUOpCmplt[
        broadcastScalar[liftNumeric[a, inheritDType[b]], tUopShape[b]], b]

(* === Movement / linalg UpValues =============================
   Idiomatic WL forms route to existing TUOp* / T* primitives so
   user code reads as ordinary Mathematica rather than a soup of
   T-prefixed builders.

   - Transpose[t]        -> reverse-axis permute
   - Transpose[t, perm]  -> general permute (1-indexed -> 0-indexed)
   - Dot[a, b]           -> TMatMul (rank-2 @ rank-2) / TDot (rank-1)
   - ArrayReshape[t, sh] -> TUOpReshape *)

TTerm /: Transpose[t_TTerm ? tensorTermQ] :=
    With[{rank = Length @ tUopShape[t]},
        TUOpPermute[t, Reverse @ Range[0, rank - 1]]]
TTerm /: Transpose[t_TTerm ? tensorTermQ, perm_List] :=
    TUOpPermute[t, perm - 1]

TTerm /: Dot[a_TTerm ? tensorTermQ, b_TTerm ? tensorTermQ] :=
    With[{ra = Length @ tUopShape[a], rb = Length @ tUopShape[b]},
        Which[
            ra === 2 && rb === 2,  TMatMul[a, b],
            ra === 1 && rb === 1,  TDot[a, b],
            True, TMatMul[a, b]]]      (* fallback; refine when needed *)

TTerm /: ArrayReshape[t_TTerm ? tensorTermQ, shape_List] :=
    TUOpReshape[t, shape]

(* Layer-call UpValues: `Layer[opts][t_TTerm]` is still a TTerm
   UpValue -- TagSetDelayed on TTerm, with the layer bound as
   `l_SoftmaxLayer` so we can read its options.  WL's "Level"
   parameter is 1-indexed; thvm's TSoftmaxAxis is 0-indexed. *)
TTerm /: l_SoftmaxLayer[t_TTerm ? tensorTermQ] :=
    TSoftmaxAxis[t, NetExtract[l, "Parameters"]["Level"] - 1]

(* Set on a literal-TTerm LHS rewrites in place: realises src into a
   fresh TenDesc, memcpys those bytes into dst's backing buffer.  dst
   keeps its TenDesc id (so any caller still holding it sees the new
   contents).  Only fires when the LHS is the literal TTerm form --
   `Set[m, src]` where m is a Symbol whose VALUE is a TTerm doesn't
   match, because Set holds the LHS unevaluated.  Use one of:

       TSet[dst, src]                  (* preferred: short, evaluates dst *)
       Evaluate[dst] = src             (* forces dst to its TTerm value *)

   to invoke this without literally typing out the TTerm form. *)
TTerm /: Set[t_TTerm, src_] := TSet[t, src]

(* TSet[dst, src]: in-place buffer write.  Evaluates dst (so callers
   can pass a Module-bound symbol) before dispatching the literal-form
   TAssign + TRealize.  Returns dst so chains compose. *)
TSet[dst_TTerm, src_] := (TRealize[TAssign[dst, src]]; dst)

End[];

EndPackage[];
