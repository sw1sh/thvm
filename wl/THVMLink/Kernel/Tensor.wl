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

(* TUOpConv2DLowered[input, weights, bias] -- builds 2-D
   convolution forward from primitive UOPs only.  Public
   entry point TUOpConv2D below dispatches to this.

   Per-kernel-position partial-sum strategy:
       For each (ki, kj) in [0, kh) x [0, kw):
         x_slice = SHRINK(input, all C_in, [ki, ki+H_out), [kj, kj+W_out))
                                                              {C_in, H_out, W_out}
         w_slice = SHRINK(weights, all C_out, all C_in, [ki, ki+1), [kj, kj+1))
                                                       {C_out, C_in, 1, 1}
         x_b     = EXPAND(RESHAPE(x_slice, {1, C_in, H_out, W_out}),
                          {C_out, C_in, H_out, W_out})
         w_b     = EXPAND(w_slice, {C_out, C_in, H_out, W_out})
         partial = REDUCE_SUM(MUL(x_b, w_b), axis = 1)
                                                  {C_out, H_out, W_out}
       sum all kh*kw partials, then add bias broadcast {C_out, 1, 1}
       expanded to {C_out, H_out, W_out}.

   Why kh*kw partial sums (rather than the tinygrad _pool unfold):
   the unfolded tensor would have shape {C_in, H_out, W_out, kh, kw}
   which for LeNet's 28x28 -> 24x24 with kh=kw=5 is 24*24*25 = 14400
   elements per channel; the partial-sum form only allocates a few
   {C_out, C_in, H_out, W_out} intermediates per kernel position,
   which fits the per-op-allocates-a-buffer materializer better. *)
TUOpConv2DLowered[input_, weights_, bias_] := Module[{
    inShape, wShape, cIn, cOut, h, wd, kh, kw, hOut, wOut,
    partials, xSlice, wSlice, xB, wB, summed, biasBroadcast
},
    ensureInit[];
    (* Use tUopShape (handles UOP chains) rather than TTensorShape
       (TAG_TEN only) -- LeNet's second conv takes the Pool output,
       which is a UOP_REDUCE chain, not a materialised tensor. *)
    inShape = tUopShape[input];
    wShape  = tUopShape[weights];
    {cIn, h, wd}        = inShape;
    {cOut, cIn, kh, kw} = wShape;
    hOut = h  - kh + 1;
    wOut = wd - kw + 1;
    partials = Flatten @ Table[
        xSlice = TUOpShrink[input,
            {{0, cIn}, {ki, ki + hOut}, {kj, kj + wOut}}];
        wSlice = TUOpShrink[weights,
            {{0, cOut}, {0, cIn}, {ki, ki + 1}, {kj, kj + 1}}];
        xB = TUOpExpand[
            TUOpReshape[xSlice, {1, cIn, hOut, wOut}],
            {cOut, cIn, hOut, wOut}];
        wB = TUOpExpand[wSlice, {cOut, cIn, hOut, wOut}];
        TUOpReduce[TUOpMul[xB, wB], 1, "SUM"],
        {ki, 0, kh - 1}, {kj, 0, kw - 1}
    ];
    summed = Fold[TUOpAdd, First @ partials, Rest @ partials];
    biasBroadcast = TUOpExpand[
        TUOpReshape[bias, {cOut, 1, 1}],
        {cOut, hOut, wOut}];
    TUOpAdd[summed, biasBroadcast]
]

(* TUOpConv2D[input, weights, bias] -- public conv2d entry point.
   Dispatches to the lowered primitive chain so autograd flows
   through the chain rule (no bespoke CONV2D grad branch).  All
   existing call sites pick up the lowering transparently. *)
TUOpConv2D[input_, weights_, bias_] := TUOpConv2DLowered[input, weights, bias]

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
    one = TUOpConst[1.0, "f32"];
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
        zero = TUOpExpand[TUOpConst[0.0, "f32"], targetShape];
        TUOpAdd[dupNest, zero],
        dupNest
    ]
]

(* Multi-target VJP: build n unary TGrads sharing the y subgraph (and
   the gy seed) by heap-loc identity, apply the user's body to them
   positionally.  materialize's per-realize memo dedups every forward
   kernel emitted from those shared UOps across all n targets. *)
TGradMany[y_, {target_}, body_]    := body[TGrad[y, target]]
TGradMany[y_, targets_List, body_] := With[{seed = gradOnesSeed[y]},
    body @@ (TGrad[y, #, seed] & /@ targets)
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

(* === kernel-entry introspection === *)

TKernelCount[]    := (ensureInit[]; $kernelCountFn[])

(* Number of distinct KProgOp[] arrays cached.  After a TRealize,
   `TKernelProgramCacheSize[] <= TKernelCount[] - 1`: programs that
   share structure share one cache entry. *)
TKernelProgramCacheSize[] := (ensureInit[]; THVMLink`Private`$kernelProgramCacheSizeFn[])

(* TKernelInfo[kid] returns an Association with the kernel's
   linearized program + shape metadata, useful for tests and
   THeapDiagram visualization overlays. *)
TKernelInfo[kid_Integer] := Module[{raw = $kernelInfoFn[kid], n, nOps},
    n    = raw[[1]];
    nOps = raw[[2]];
    <|
      "n_inputs"    -> n,
      "n_ops"       -> nOps,
      "output_numel"-> raw[[3]],
      "output_dtype"-> dtypeName[raw[[4]]],
      "program"     -> Table[
          With[{base = 4 + (i - 1) * 6},
            <|
              "opcode" -> Lookup[$uopNames, raw[[base + 1]], "?"],
              "n_src"  -> raw[[base + 2]],
              "src"    -> { raw[[base + 3]], raw[[base + 4]] },
              "arg"    -> raw[[base + 5]],
              "numel"  -> raw[[base + 6]]
            |>
          ],
          {i, nOps}
      ]
    |>
]

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

TTerm /: Less[a_TTerm ? tensorTermQ, b_] := TUOpCmplt[a, liftNumeric[b, inheritDType[a]]]
TTerm /: Less[a_, b_TTerm ? tensorTermQ] := TUOpCmplt[liftNumeric[a, inheritDType[b]], b]

End[];

EndPackage[];
