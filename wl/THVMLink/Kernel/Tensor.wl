(* ::Package:: *)
(* Tensor.wl - tensor + UOp WL surface for THVMLink.

   Loaded from THVMLink.wl inside Begin["`Private`"]; all symbols
   stay in the main THVMLink` context so users see TTensor /
   TUOp* without a subcontext namespace.  Sibling-split purely for
   size: THVMLink.wl handles lifecycle + IC combinators +
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

GeneralUtilities`SetUsage[TSet, "TSet[dst$, src$] writes the bytes of src$ into dst$'s backing buffer in place, keeping dst$'s TenDesc id so callers still holding it see the new contents; equivalent to TRealize[TAssign[dst$, src$]]; dst$.
Also installed as the WL Set UpValue on literal-TTerm left-hand sides, so Evaluate[w$] = expr$ mutates w$ rather than rebinding the symbol.
Returns dst$."];

GeneralUtilities`SetUsage[TSetData, "TSetData[dst$, na$] writes the raw bytes of NumericArray (or list/PackedArray) na$ directly into dst$'s existing backing buffer in place, allocating no fresh TenDesc.
This is the fast per-step feed path: build the input tensor once, then re-feed each step via TSetData instead of TSet[dst$, TTensorCreate[na$]], which churns a new TenDesc and ASSIGN graph every step.
na$'s dtype and element count must match dst$. Returns dst$."];

(* === C-side TenDesc side-table accessors ===
   Live here (rather than MemoryPlan.wl) because they're the core
   tensor-introspection bridge.  Kernel.wl owns the parallel
   KernelEntry accessors; MemoryPlan.wl owns per-backend buf
   accessors. *)
GeneralUtilities`SetUsage[TTensTable, "TTensTable[] returns a list of {producer_kid, buf_id, dtype, view_numel, view_contiguous, refcount, backend_id} per TenDesc.
backend_id is 1 for CPU, 2 for Metal, 0 for unbound."];
GeneralUtilities`SetUsage[TTensCount, "TTensCount[] returns the number of allocated TenDescs, excluding the reserved slot 0."];
GeneralUtilities`SetUsage[TTotalBufBytes, "TTotalBufBytes[] returns the sum of live CPU buffer bytes (refcount greater than 0)."];

GeneralUtilities`SetUsage[TRealToFP16, "TRealToFP16[reals$] packs a list of Reals into a NumericArray of UnsignedInteger16 raw fp16 bytes.
Pair with TFP16ToReal to round-trip; see TTensor[shape$, na$, \"f16\"] for the tensor surface."];
GeneralUtilities`SetUsage[TRealToBf16, "TRealToBf16[reals$] packs a list of Reals into a NumericArray of UnsignedInteger16 raw bfloat16 bytes."];
GeneralUtilities`SetUsage[TFP16ToReal, "TFP16ToReal[na$] unpacks a UnsignedInteger16 NumericArray of raw fp16 bytes into a Real list."];
GeneralUtilities`SetUsage[TBf16ToReal, "TBf16ToReal[na$] unpacks a UnsignedInteger16 NumericArray of raw bfloat16 bytes into a Real list."];

GeneralUtilities`SetUsage[TRealToFP8E4M3, "TRealToFP8E4M3[reals$] packs a list of Reals into a NumericArray of UnsignedInteger8 raw fp8e4m3 bytes.
Pair with TFP8E4M3ToReal to round-trip."];
GeneralUtilities`SetUsage[TRealToFP8E5M2, "TRealToFP8E5M2[reals$] packs a list of Reals into a NumericArray of UnsignedInteger8 raw fp8e5m2 bytes."];
GeneralUtilities`SetUsage[TFP8E4M3ToReal, "TFP8E4M3ToReal[na$] unpacks a UnsignedInteger8 NumericArray of raw fp8e4m3 bytes into a Real list."];
GeneralUtilities`SetUsage[TFP8E5M2ToReal, "TFP8E5M2ToReal[na$] unpacks a UnsignedInteger8 NumericArray of raw fp8e5m2 bytes into a Real list."];

GeneralUtilities`SetUsage[TTensorViewDebug, "TTensorViewDebug[tid$] returns the TenDesc's View internals as {ndim, dims$$, strides$$, offset, contiguous, nviews, buf_id, producer_kid}.
Returns {-1} when THVM_WL_TENSOR_VIEW_DEBUG is unset. Diagnostic only."];

GeneralUtilities`SetUsage[TPackInt4, "TPackInt4[ints$] packs a list of Integers in [-8, 7] into a UnsignedInteger8 NumericArray of packed nibbles (2 elements per byte, low nibble first)."];
GeneralUtilities`SetUsage[TPackUInt4, "TPackUInt4[ints$] packs a list of Integers in [0, 15] into a UnsignedInteger8 NumericArray of packed nibbles."];
GeneralUtilities`SetUsage[TUnpackInt4, "TUnpackInt4[na$, numel$] unpacks numel$ signed nibbles from a packed-byte NumericArray into a list of Integers."];
GeneralUtilities`SetUsage[TUnpackUInt4, "TUnpackUInt4[na$, numel$] unpacks numel$ unsigned nibbles into a list of Integers."];

GeneralUtilities`SetUsage[TUOpCast, "TUOpCast[src$, dtype$] returns a UOP node that value-preservingly casts src$ to the named dtype$.
Backward gradient (under TGrad) casts back to src$'s dtype, matching tinygrad's Ops.CAST rule."];
GeneralUtilities`SetUsage[TUOpBitcast, "TUOpBitcast[src$, dtype$] returns a UOP node that bit-level reinterprets src$ as the named dtype$; source and destination must share itemsize.
Backward gradient is zero (BITCAST has no value-preserving gradient)."];

(* Forward-decl: these are defined in NN.wl (loads alphabetically
   after Tensor.wl).  Without this, the UpValues below resolve to
   phantoms in `THVMLink`Private`* with no DownValue. *)
{TTanh, TMatMul, TDot, TSoftmax};

Begin["`Private`"];

TTensTable[]     := (ensureInit[]; Partition[Normal @ $tensTableFn[], 7])
TTensCount[]     := (ensureInit[]; $tensCountFn[])
TTotalBufBytes[] := (ensureInit[]; $totalBufBytesFn[])

(* === predicates ===
   tensorTermQ[t]: true iff t is a TTerm whose tag makes it a
   tensor-shaped value (TAG_TEN, TAG_UOP, TAG_DP0/DP1 with the
   DUP_GRAD_FLAG bit set on its ext, a chain-rule projection from
   TUOpGradWithTarget that fires to a tensor, or a TAG_VAR carrying a
   TLamShape shape annotation, i.e. the bound variable of a tensor
   lambda).  The VAR case is what lets the numeric UpValue sugar
   (Plus / Dot / Transpose / ...) build a UOP graph inside a TLamShape
   body, so the APP-LAM interaction can JIT-materialize the forward;
   without it `x + 1` over a bound `x` falls through to the generic IC
   OP2, which strands at realize because OP2 is numeric, not tensor.
   A plain TLam binder (no annotation) stays non-tensor so ordinary IC
   combinators (LAM, APP, regular DUP, ...) follow IC reduction. *)

tensorTermQ[t_TTerm] := With[{
    raw = ttermRaw[t], tag = $termTagFn[ttermRaw[t]]
},
    Or[
        tag === $TagTEN, tag === $TagUOP,
        And[ Or[tag === $TagDP0, tag === $TagDP1],
             BitAnd[$termExtFn[raw], $DupGradFlag] =!= 0 ],
        And[ tag === $TagVAR, TTermShape[t] =!= {} ]
    ]
]
tensorTermQ[_]       := False

(* A TUOpConst wraps a plain numeric.  Used to lift a scalar on the
   right-hand side of Plus / Times against a tensor term. *)
liftNumeric[n_ ? NumericQ, dtype_String] := TUOpConst[n, dtype]
liftNumeric[t_TTerm,       _]            := t

(* Pick a dtype to broadcast into.  Mirrors C-side term_dtype_in
   (src/schedule/uop_meta.c): TEN reads the TenDesc dtype; UOP
   walks the graph (CONST holds dtype on its NUM cell, CAST/BITCAST
   on the NUM at loc+1, every other elementwise/movement/reduce op
   inherits from src[0]).  TAG_DP0/DP1 with DUP_GRAD_FLAG (a
   TUOpGradWithTarget chain-rule projection) inherits from y at
   heap[loc].  Used by gradOnesSeed and the Plus/Times/Less
   UpValues so a right-hand-side scalar lifts to the surrounding
   tensor's dtype instead of defaulting to f32 (which silently
   bit-misinterprets under f64 / f16 / bf16 kernels). *)
(* Iterative single-path descent (NOT recursion): the dtype always comes from
   src[0] (or the kernel output / DUP body), so we follow that one edge in a
   loop to a TEN / CONST / CAST leaf.  A recursive walk would blow
   $RecursionLimit on a deep residual chain (e.g. GPT-2's 12 blocks), the
   same depth issue tUopShape (Shape.wl) avoids the same way.  `result` stays
   Null while descending; any terminal case sets it and ends the loop. *)
inheritDType[t_TTerm] := Module[{raw, tag, val, ext, result = Null},
    raw = ttermRaw[t];
    While[ result === Null,
        tag = $termTagFn[raw];
        Which[
            tag === $TagTEN,
                result = dtypeName[$termExtFn[raw]],
            tag === $TagUOP,
                val = $termValFn[raw];
                ext = $termExtFn[raw];
                Switch[ext,
                    $UopKernel,
                        raw = $heapReadFn[val],
                    $UopConst,
                        result = dtypeName[$termExtFn[$heapReadFn[val]]],
                    $UopCast | $UopBitcast,
                        result = dtypeName[$termValFn[$heapReadFn[val + 1]]],
                    $UopAdd  | $UopMul   | $UopCmplt | $UopCmpeq |
                    $UopNeg  | $UopRecip | $UopExp2  | $UopLog2  | $UopSqrt |
                    $UopReshape | $UopPermute | $UopExpand | $UopPad |
                    $UopShrink  | $UopFlip    | $UopReduce |
                    $UopLoad    | $UopAssign  | $UopCopy,
                        raw = $heapReadFn[val],
                    _, result = "f32"
                ],
            (tag === $TagDP0 || tag === $TagDP1)
                && BitAnd[$termExtFn[raw], $DupGradFlag] =!= 0,
                raw = $heapReadFn[$termValFn[raw]],
            True, result = "f32"
        ]
    ];
    result
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

(* Reading a tensor: only valid once `t` is actually a TAG_TEN.
   If `t` is a UOp or IC term, we return a typed Missing rather
   than read random memory, so test failures surface cleanly
   instead of crashing WL. *)

tensorIdQ[t_] := TTermTag[t] === $TagTEN

TTensorShape[t_ ? tensorIdQ]    := $tensorShapeFn[TTermVal[t]]
TTensorShape[t_TTerm]           := Missing["NotATensor", TTagName[TTermTag[t]]]

(* TTermShape: runs the C-side `term_shape_in` resolver, which
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

(* markGradLeaf / gradLeafQ: INTERNAL grad-leaf mark on TenDesc, NOT a
   user-facing flag.  TGrad auto-marks every reachable float leaf right
   before the walk (tinygrad spec: no requires_grad anywhere; .grad is
   filled for every in-scope non-CONST float leaf, and the optimizer's
   param list decides what updates).  The C leaf rule (grad_leaf_sup,
   target == 0 path) consults this mark to drive the accumulation walk;
   TGrad sets/clears it transiently and never exposes it on the surface. *)
markGradLeaf[t_ ? tensorIdQ, on_:True] := (
    $tensorSetReqGradFn[TTermVal[t], If[ on, 1, 0]]; t)
markGradLeaf[t_TTerm, ___]             := t

gradLeafQ[t_ ? tensorIdQ] := $tensorReqGradFn[TTermVal[t]] === 1
gradLeafQ[t_TTerm]        := False

(* TGradOf[t]: read TenDesc.grad (the walk-once chain-rule
   accumulator).  Returns a TTerm wrapping the lazy grad term, or
   Missing["NoGrad"] when nothing has accumulated.  TClearGrad[t]
   zeros it (PyTorch zero_grad). *)
TGradOf[t_ ? tensorIdQ] := With[{g = $tensorGradFn[TTermVal[t]]},
    If[ g === 0, Missing["NoGrad"], TTerm[g]]]
TGradOf[t_TTerm]        := Missing["NotATensor", TTagName[TTermTag[t]]]

TClearGrad[t_ ? tensorIdQ] := ($tensorClearGradFn[TTermVal[t]]; t)
TClearGrad[t_TTerm]        := Missing["NotATensor", TTagName[TTermTag[t]]]

(* Debug-only: dump View internals as {ndim, dims..., strides..., offset,
   contiguous, nviews, buf_id, producer_kid}.  No-ops (returns {-1}) when
   THVM_WL_TENSOR_VIEW_DEBUG is unset.  Used to isolate output-view
   stride bugs from kernel-body bugs. *)
TTensorViewDebug[t_ ? tensorIdQ] := Normal @ $tensorViewDbgFn[TTermVal[t]]
TTensorViewDebug[t_Integer]      := Normal @ $tensorViewDbgFn[t]

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

TUOpCast[src_, dtype_String]    := (ensureInit[]; TTerm[$uopCastFn   [ttermRaw[src], dtypeCode[dtype]]])
TUOpBitcast[src_, dtype_String] := (ensureInit[]; TTerm[$uopBitcastFn[ttermRaw[src], dtypeCode[dtype]]])

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

(* === Symbolic-shape (kvar) surface ===
   Build a forward over a runtime-variable dimension so a single materialized
   graph runs at any length without re-lift (tinygrad symbolic shapes).  The
   axis is sized at its upper bound `hi` (buffers / dispatch shape are worst
   case); TKVarSet binds the actual loop bound before TRealize.  Usage messages
   are declared public in THVMLink.wl. *)
TKVarAlloc[lo_Integer, hi_Integer] := (ensureInit[]; $kvarAllocFn[lo, hi])
TKVarHi[vid_Integer]               := (ensureInit[]; $kvarHiFn[vid])
TKVarSet[vid_Integer, v_Integer]   := (ensureInit[]; $kvarSetRuntimeFn[vid, v])
TSymbolicAxis[t_, axis_Integer, vid_Integer] := (ensureInit[]; TTerm[$markSymbolicAxisFn[ttermRaw[t], axis, vid]])

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
   shape; TGrad below builds a default ones-at-y.shape seed. *)
TUOpGrad[y_, gy_] := (ensureInit[]; TTerm[$uopGradFn[ttermRaw[y], ttermRaw[gy]]])
TUOpGradWithTarget[y_, gy_, target_] := (ensureInit[];
    TTerm[$uopGradWithTargetFn[ttermRaw[y], ttermRaw[gy], ttermRaw[target]]])
TUOpFwd [y_, gy_] := (ensureInit[]; TTerm[$uopFwdFn [ttermRaw[y], ttermRaw[gy]]])

(* Build {fwd, bwd} pair sharing one cell [y, gy].  The dup-like
   discipline: both projections reference the same heap loc, so the
   forward subgraph isn't duplicated. *)
TGradPair[y_, gy_] := With[{bwd = TUOpGrad[y, gy]},
    With[{loc = TTermVal[bwd]},
        {packTerm[0, $TagDP0, $DupGradFlag, loc], bwd}
    ]
]

TUOpLoad[src_] := (ensureInit[]; TTerm[$uopLoadFn[ttermRaw[src]]])

(* Generic transfer to the realize backend (device -1): the legacy
   device-agnostic COPY.  TUOpCopy[src, "metal"|"cpu"] carries an EXPLICIT
   target device in the graph (tinygrad's `.to(device)` / Ops.COPY with a
   DEVICE src), so the node records where its data lives regardless of the
   realize backend. *)
TUOpCopy[src_TTerm] := (ensureInit[]; TTerm[$uopCopyFn[ttermRaw[src], -1]])
TUOpCopy[src_TTerm, device_String] :=
    (ensureInit[]; TTerm[$uopCopyFn[ttermRaw[src], deviceCode[device]]])

(* TDevice[t]: the device t's data lives on ("cpu"/"metal"/...) or None
   for the default.  Ported from tinygrad's UOp.device (uop/ops.py:756). *)
TDevice[t_TTerm] := (ensureInit[]; deviceName[$termDeviceFn[ttermRaw[t]]])

(* TToDevice[t, "metal"|"cpu"]: move t to a device, tinygrad's
   Tensor.to(device).  A lazy graph "lives on" a device iff it is COMPUTED
   there, so this pushes a device-targeted UOP_COPY down to every tensor leaf
   (weights, host inputs, realized tensors); TRealize then routes the whole
   realize to that device and computes the forward there, uploading each leaf
   once.  A single COPY around the root would instead compute the whole graph
   on CPU and move only the result, so to run an imported forward on the GPU
   you just write TToDevice[net[x], "metal"] (no need to wrap the input
   separately).  A bare realized tensor is the degenerate one-leaf upload.
   Mirrors tinygrad's model.to(device) moving the parameters. *)
TToDevice[t_TTerm, device_String] :=
    (ensureInit[]; TTerm[$uopToDeviceFn[ttermRaw[t], deviceCode[device]]])

(* === INDEX-layer UOp constructors ===
   INDEX layer + BUFFER/STORE/AFTER/OPT.  Used by Rewrite.wl and the
   cross-validation .wlt suite to build canonical DAGs (RANGE-based
   matmul, softmax-shape reduces) directly in WL without going
   through Python. *)
TUOpRange[axisId_Integer, axisType_Integer, extent_Integer] := (
    ensureInit[]; TTerm[$uopRangeFn[axisId, axisType, extent]])

TUOpBuffer[scope_Integer, dtype_Integer, dims_List, instance_Integer:0] := (
    ensureInit[]; TTerm[$uopBufferFn[scope, dtype, dims, instance]])

TUOpIndexE[buf_, addr_] := (
    ensureInit[]; TTerm[$uopIndexEFn[ttermRaw[buf], ttermRaw[addr]]])

(* Wraps the various integer-binary opcodes (UOP_IADD..UOP_IAND).
   Match the names used by py_uop_int_binary callers. *)
TUOpIAdd[a_, b_] := (ensureInit[]; TTerm[$uopIntBinaryFn[$UopIAdd, ttermRaw[a], ttermRaw[b]]])
TUOpISub[a_, b_] := (ensureInit[]; TTerm[$uopIntBinaryFn[$UopISub, ttermRaw[a], ttermRaw[b]]])
TUOpIMul[a_, b_] := (ensureInit[]; TTerm[$uopIntBinaryFn[$UopIMul, ttermRaw[a], ttermRaw[b]]])
TUOpIDiv[a_, b_] := (ensureInit[]; TTerm[$uopIntBinaryFn[$UopIDiv, ttermRaw[a], ttermRaw[b]]])
TUOpIMod[a_, b_] := (ensureInit[]; TTerm[$uopIntBinaryFn[$UopIMod, ttermRaw[a], ttermRaw[b]]])
TUOpILt [a_, b_] := (ensureInit[]; TTerm[$uopIntBinaryFn[$UopILt,  ttermRaw[a], ttermRaw[b]]])
TUOpIAnd[a_, b_] := (ensureInit[]; TTerm[$uopIntBinaryFn[$UopIAnd, ttermRaw[a], ttermRaw[b]]])

TUOpIWhere[cond_, t_, e_] := (ensureInit[];
    TTerm[$uopIWhereFn[ttermRaw[cond], ttermRaw[t], ttermRaw[e]]])

TUOpInvalid[] := (ensureInit[]; TTerm[$uopInvalidFn[]])

TUOpOpt[target_, kind_Integer, factor_Integer] := (
    ensureInit[]; TTerm[$uopOptFn[ttermRaw[target], kind, factor]])

TUOpStore[buf_, addr_, value_] := (
    ensureInit[]; TTerm[$uopStoreFn[ttermRaw[buf], ttermRaw[addr], ttermRaw[value]]])

TUOpAfter[node_, afterNode_] := (
    ensureInit[]; TTerm[$uopAfterFn[ttermRaw[node], ttermRaw[afterNode]]])

(* Integer constant atom for stride coefficients.  The DAG-side
   classifiers (uop_dag_classify_matmul_shape) pattern-match on
   UOP_CONST(DT_INT32, bits) for IMUL stride coefficients; bare
   TAG_NUM atoms won't match. *)
TUOpIConst[v_Integer] := (ensureInit[]; TTerm[$termIConstFn[v]])

(* TAssign[dst_TEN, src_UOP_or_TEN]: in-place buffer write.  Wnf
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
(* uop_leaf_tids (src/uop/leaf_tids.c) is a generic UOP-DAG walk
   that returns the distinct TAG_TEN-leaf tids reachable from a
   root term.  Iterative + visited bitmap, sub-ms even for deep
   graphs.  TGrad / TGradMany are the callers. *)
uopLeafTids[t_TTerm] := (ensureInit[]; Normal @ $uopLeafTidsFn[ttermRaw[t]])
uopLeafTids[_]       := {}

(* Default cotangent seed for TGrad: ones at y's shape and dtype.
   CONST is a scalar (shape {1}); EXPAND lifts it to y.shape when
   y is non-scalar.  Dtype matches y's so f64 / f16 / bf16 kernels
   read the seed buffer with the right itemsize; a plain f32 seed
   would bit-misinterpret under f64 (giving e-314 garbage from the
   chain rule's gy * src multiply). *)
gradOnesSeed[y_TTerm] := Module[{shape = tUopShape[y], one},
    one = TUOpConst[1.0, inheritDType[y]];
    If[ ListQ[shape] && Length[shape] > 0 && shape =!= {1},
        TUOpExpand[one, shape],
        one
    ]
]

(* Float-dtype TEN leaves reachable from `y`, as TTerms.  thvm TEN
   leaves are buffers (never UOP_CONST), so the tinygrad "non-CONST
   float leaf" filter is just the float-dtype test here.  uopLeafTids
   gives the in-scope leaf tids; the TenDesc table maps each tid to its
   dtype so we can pack the TAG_TEN term and drop the integer ones. *)
$gradFloatCodes := $gradFloatCodes = dtypeCode /@
    {"f16", "bf16", "f32", "f64", "fp8e4m3", "fp8e5m2"}
gradFloatLeafTerms[y_TTerm] := Module[{tids, tbl, n},
    tids = uopLeafTids[y];
    tbl  = TTensTable[];
    n    = Length[tbl];
    Select[
        Map[packTerm[0, $TagTEN, tbl[[#, 3]], #] &,
            Select[tids, 1 <= # <= n &]],
        MemberQ[$gradFloatCodes, TTermExt[#]] &]
]

(* TGrad[y]: PyTorch loss.backward().  Auto-mark every reachable float
   leaf, fire ONE backward walk seeded with ones-at-y (no explicit
   target), and let the C leaf rule (grad_leaf_sup, target == 0 path)
   accumulate each leaf's summed cotangent into its TenDesc.grad.  Read
   back per-tensor with TGradOf (the param.grad analogue).  No
   requires_grad flag: .grad is filled for EVERY in-scope non-CONST
   float leaf (tinygrad spec).  Unmark afterward so a later TGrad over a
   different loss starts from a clean mark set.  Returns y for chaining. *)
TGrad[y_TTerm] := Module[{leaves = gradFloatLeafTerms[y]},
    markGradLeaf /@ leaves;
    TWnf[TUOpGrad[y, gradOnesSeed[y]]];
    markGradLeaf[#, False] & /@ leaves;
    y
]

TGrad[y_, target_TTerm] := TGrad[y, target, gradOnesSeed[y]]
TGrad[y_, target_TTerm, gy_TTerm] :=
    tGradWithLeaves[y, target, gy, uopLeafTids[y]]

(* Shared-leaves form: caller supplies leafTids so the list-form TGrad
   can compute them ONCE per forward graph instead of once per target.
   uopLeafTids walks the full UOP DAG; for an 8-weight LeNet
   that's the dominant per-step cost (13s -> sub-second). *)
tGradWithLeaves[y_, target_TTerm, gy_TTerm, leafTids_List] := (
    (* Target-aware chain rule: leaf rule emits gy at matching tids
       and scalar zero elsewhere: no SUPs, no DUP-nest projection.
       Avoids the exponential cell blow-up in deep DAGs (LeNet:
       10 leafTids x deep forward DAG would require 10 nested
       DUP-SUP commute fires per leaf and cross-product fires).
       Shape padding (when chain rule yields a scalar/sub-shape
       result) lives in C-side interact_grad so the EXPAND+ADD
       wrap is elided when the result already matches target.shape.
       leafTids retained in the signature for caller compatibility
       (the list-form TGrad) but no longer consulted. *)
    TUOpGradWithTarget[y, gy, target]
)

(* List form: d(y)/d(target_i) for every target, returned as a List of
   TTerms in `targets` order.  A single-element list delegates to the
   target-aware unary TGrad; the multi-element form below is the
   requires_grad single walk. *)
TGrad[y_, {target_}]    := {TGrad[y, target]}

(* Multi-target VJP via the auto-grad-all-float-leaves single walk.
   Mark every reachable float leaf, fire ONE target-free backward
   (TUOpGrad, the target==0 cell), and let interact_grad's leaf rule
   accumulate each leaf's summed cotangent into TenDesc.grad (uop_grad.c
   grad_leaf_sup target==0 path).  Read the per-target accumulator back
   via TGradOf for exactly the requested `targets` (tinygrad
   Tensor.gradient[targets]: .grad is filled for all leaves, the
   caller picks which to return).

   The single walk computes every shared intermediate cotangent ONCE and
   threads it to all leaves: a forward value reached by multiple
   backward consumers is one node (grad_fwd_of shares it in non-SUP/DUP
   mode), so the backward stops re-deriving the upstream stack per
   consumer.

   The float leaves are unmarked afterward so the global GRAD_REQ_NCOUNT
   mark state is restored for any later TGrad. *)
TGrad[y_, targets_List] := Module[{leaves, grads},
    leaves = gradFloatLeafTerms[y];
    markGradLeaf /@ leaves;
    TClearGrad /@ leaves;                         (* fresh grad per walk *)
    TWnf[TUOpGrad[y, gradOnesSeed[y]]];          (* single backward walk *)
    grads = Map[
        With[{g = TGradOf[#]},
             If[ MissingQ[g],
                 TZeros[TTensorShape[#], TTensorDType[#]],
                 g]] &,
        targets];
    markGradLeaf[#, False] & /@ leaves;          (* restore global state *)
    grads
]

(* TRealize: heap-walk materialize (in-place rewrite UOPs to UOP_KERNELs)
   then TWnf to beta-reduce and fire the kernels.  No UOP_MATERIALIZE
   wrapper; thvm_materialize is invoked directly.  Equivalent to
   TWnf[TMaterialize[expr]] except every CPU buffer alloc'd during
   materialize+wnf that ISN'T reachable from the result tensor's
   producer chain gets freed at exit (per-step buffer-pool boundary).

   Single root: TRealize[expr]      -> one realized TTerm
   Multi-root:  TRealize[{t1, ...}] -> List of realized TTerms
   The list form bundles every root into ONE materialize+wnf pass
   (thvm_realize_many walks the whole bundle in a single thvm_materialize),
   so kernels shared across roots dedup: bufferize_classify sees a node
   reached by multiple roots as multi-consumer and emits it ONCE.  It
   returns the list of N realized roots (pinned TEN handles, via
   term_ctr_n/at), so callers can thread each into downstream expressions.
   Looping TRealize per root instead (TRealize /@) re-emits every shared
   sub-DAG per root: each isolated pass sees its upstream as single-
   consumer and inlines it, paying ~N x the kernels (e.g. the N param
   gradients of one backward, which share the upstream cotangent chain). *)
TRealize[exprs_List] := Module[{raw, n},
    ensureInit[];
    raw = $realizeManyFn[Developer`ToPackedArray[ttermRaw /@ exprs, Integer]];
    n   = $termCtrNFn[raw];
    Table[TTerm[$termCtrAtFn[raw, i]], {i, 0, n - 1}]
]
TRealize[expr_] := (ensureInit[]; TTerm[$realizeFn[ttermRaw[expr]]])

(* TMaterialize = direct schedule + kernelize + linearize rewrite,
   no firing.  Useful for inspection (visualize the scheduled DAG
   before dispatch).  The return value is a UOP_KERNEL term whose
   first heap cell is the pre-allocated output TAG_TEN (empty) and
   second cell is a TAG_NUM carrying the KernelEntry id.  Subsequent
   TWnf fires the kernels bottom-up. *)
TMaterialize[expr_] := (ensureInit[]; TTerm[$materializeFn[ttermRaw[expr]]])

(* TKernelCount / TKernelProgramCacheSize / TKernelInfo are defined
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
$sharedNATypes = {
    "Real32", "Real64",
    "Integer8",  "UnsignedInteger8",
    "Integer16", "UnsignedInteger16",
    "Integer32", "UnsignedInteger32",
    "Integer64", "UnsignedInteger64"
};

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

(* dtype label -> NumericArray storage type.  Default (no dtype
   given) means "infer from data": IntegerQ -> i32, otherwise f32.
   Byte-aligned integer dtypes map directly; the float and FP8
   families ride on raw-bytes carriers. *)
naTypeFor["bool"] := "UnsignedInteger8"
naTypeFor["i8"]   := "Integer8"
naTypeFor["u8"]   := "UnsignedInteger8"
naTypeFor["i16"]  := "Integer16"
naTypeFor["u16"]  := "UnsignedInteger16"
naTypeFor["i32"]  := "Integer32"
naTypeFor["u32"]  := "UnsignedInteger32"
naTypeFor["i64"]  := "Integer64"
naTypeFor["u64"]  := "UnsignedInteger64"
naTypeFor["f16"]  := "UnsignedInteger16"   (* raw bytes carrier *)
naTypeFor["bf16"] := "UnsignedInteger16"   (* raw bytes carrier *)
naTypeFor["f32"]  := "Real32"
naTypeFor["f64"]  := "Real64"
naTypeFor[_]      := "Real32"

(* f16 / bf16 round-trip helpers.  Use TRealToFP16 / TRealToBf16 to
   pack a Real list into a NumericArray of raw narrow-float bytes;
   the inverse TFP16ToReal / TBf16ToReal unpacks back to Reals. *)
TRealToFP16[xs_List]              := (ensureInit[]; $fp16PackFn  [N @ xs, $DTFp16])
TRealToBf16[xs_List]              := (ensureInit[]; $fp16PackFn  [N @ xs, $DTBf16])
TFP16ToReal[na_NumericArray]      := (ensureInit[]; $fp16UnpackFn[na,    $DTFp16])
TBf16ToReal[na_NumericArray]      := (ensureInit[]; $fp16UnpackFn[na,    $DTBf16])
TRealToFP8E4M3[xs_List]           := (ensureInit[]; $fp8PackFn  [N @ xs, $DTFp8E4M3])
TRealToFP8E5M2[xs_List]           := (ensureInit[]; $fp8PackFn  [N @ xs, $DTFp8E5M2])
TFP8E4M3ToReal[na_NumericArray]   := (ensureInit[]; $fp8UnpackFn[na,    $DTFp8E4M3])
TFP8E5M2ToReal[na_NumericArray]   := (ensureInit[]; $fp8UnpackFn[na,    $DTFp8E5M2])
TPackInt4   [xs_List]                       := (ensureInit[]; $int4PackFn  [Round @ Flatten[{xs}], $DTInt4])
TPackUInt4  [xs_List]                       := (ensureInit[]; $int4PackFn  [Round @ Flatten[{xs}], $DTUInt4])
TUnpackInt4 [na_NumericArray, numel_Integer]:= (ensureInit[]; $int4UnpackFn[na, $DTInt4,  numel])
TUnpackUInt4[na_NumericArray, numel_Integer]:= (ensureInit[]; $int4UnpackFn[na, $DTUInt4, numel])

TTensorCreate[data_]                       := (
    ensureInit[];
    TTerm[$tensorFromNAFn[asSharableNA[data]]])

(* Host-pinned tensor: ALWAYS CPU-resident (zero-copy NA wrap) even
   when the active backend is non-CPU.  Pair with TUOpCopy to defer the
   device upload to materialize time.  Used by the TFromNet token-LM
   lift so weights stay host leaves and only the realize device upload
   is lazy. *)
TTensorCreateHost[data_]                   := (
    ensureInit[];
    TTerm[$tensorFromNAHostFn[asSharableNA[data]]])

TTensorCreate[data_, dtype_String]         := (
    ensureInit[];
    Module[{na, useTyped, normalized, shape, flatNa, t, logicalShape},
        useTyped = MemberQ[{"f16", "bf16", "fp8e4m3", "fp8e5m2",
                            "i4", "u4", "bool"}, dtype];
        logicalShape = Dimensions[data];
        If[ logicalShape === {}, logicalShape = {Length @ Flatten[{data}]}];
        Which[
            (* f16 / bf16: data may already be a packed UnsignedInteger16
               NumericArray, OR a nested list of Reals to be packed
               first.  Packing flattens, so we capture the source
               shape and re-impose it after the round-trip. *)
            (dtype === "f16" || dtype === "bf16") &&
                MatchQ[data, _NumericArray]
                && NumericArrayType[data] === "UnsignedInteger16",
                na = data,
            dtype === "f16" || dtype === "bf16",
                normalized = If[ MatchQ[data, _NumericArray], Normal[data], data];
                shape = Dimensions[normalized];
                flatNa = If[ dtype === "f16",
                    TRealToFP16[N @ Flatten[normalized]],
                    TRealToBf16[N @ Flatten[normalized]]];
                na = If[ shape === {},
                    flatNa,
                    NumericArray[ArrayReshape[Normal[flatNa], shape], "UnsignedInteger16"]],
            dtype === "fp8e4m3" || dtype === "fp8e5m2",
                normalized = If[ MatchQ[data, _NumericArray], Normal[data], data];
                shape = Dimensions[normalized];
                flatNa = If[ dtype === "fp8e4m3",
                    TRealToFP8E4M3[N @ Flatten[normalized]],
                    TRealToFP8E5M2[N @ Flatten[normalized]]];
                na = If[ shape === {},
                    flatNa,
                    NumericArray[ArrayReshape[Normal[flatNa], shape], "UnsignedInteger8"]],
            dtype === "i4" || dtype === "u4",
                normalized = If[ MatchQ[data, _NumericArray], Normal[data], data];
                shape = Dimensions[normalized];
                (* Storage: ceil(numel / 2) bytes packed, 1D. *)
                na = If[ dtype === "i4",
                    TPackInt4 [Round @ Flatten[normalized]],
                    TPackUInt4[Round @ Flatten[normalized]]];
                (* Stash logical shape on the tensor; tensor_from_na_typed
                   keeps the byte-flat NA shape, but TTensorShape returns
                   the logical shape we set after alloc.  Re-impose by
                   reshaping the freshly-allocated tensor's view below. *)
                ,
            (* Default int / float path: coerce through the NA carrier. *)
            True,
                normalized = If[ MatchQ[data, _NumericArray], Normal[data], data];
                na = NumericArray[normalized, naTypeFor[dtype]]
        ];
        If[ useTyped,
            TTerm[$tensorFromNATypedFn[na, dtypeCode[dtype], logicalShape]],
            TTerm[$tensorFromNAFn[na]]
        ]
    ])

TUOpKind[u_] := Lookup[$uopNames, TTermExt[u], "UOP?" <> ToString[TTermExt[u]]]

TUOpSrcs[u_] := With[{loc = TTermVal[u], op = TTermExt[u]},
    With[{n = Switch[op,
            $UopKernel,                                                              2,
            $UopAdd | $UopMul | $UopCmplt | $UopCmpeq,                              2,
            _,                                                                       1
        ]},
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
   KProgOp[] than the rank-matched form.  The kernel-program-
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

(* numpy-style broadcast of a set of shapes: right-align, pad missing
   leading axes with 1, take the per-axis max.  The common shape of an
   elementwise binop; using the FIRST operand's shape is wrong when a
   later operand outranks it (e.g. a rank-0 scalar reduce result like
   TDot combined with a shape-{1} target). *)
broadcastShapes[shapes : {___List}] := Module[{maxr, padded},
    maxr = Max[Append[Length /@ shapes, 0]];
    If[ maxr === 0,
        {},
        padded = (s |-> PadLeft[s, maxr, 1]) /@ shapes;
        MapThread[Max, padded]]
]

TTerm /: Plus[t_TTerm ? tensorTermQ, rest__] := Module[{lifted, allTerms, targetShape},
    lifted = (r |-> liftNumeric[r, broadcastDType[t, r]]) /@ {rest};
    allTerms = Prepend[lifted, t];
    targetShape = broadcastShapes[tUopShape /@ allTerms];
    pairFold[TUOpAdd, (x |-> broadcastScalar[x, targetShape]) /@ allTerms]
]

TTerm /: Times[t_TTerm ? tensorTermQ, rest__] := Module[{lifted, allTerms, targetShape},
    lifted = (r |-> liftNumeric[r, broadcastDType[t, r]]) /@ {rest};
    allTerms = Prepend[lifted, t];
    targetShape = broadcastShapes[tUopShape /@ allTerms];
    pairFold[TUOpMul, (x |-> broadcastScalar[x, targetShape]) /@ allTerms]
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
   - Dot[a, b]           -> TDot (vec.vec) / TMatVec (mat.vec, vec.mat) /
                            TMatMul (mat.mat + batched)
   - ArrayReshape[t, sh] -> TUOpReshape *)

TTerm /: Transpose[t_TTerm ? tensorTermQ] :=
    With[{rank = Length @ tUopShape[t]},
        TUOpPermute[t, Reverse @ Range[0, rank - 1]]]
TTerm /: Transpose[t_TTerm ? tensorTermQ, perm_List] :=
    TUOpPermute[t, perm - 1]

TTerm /: Dot[a_TTerm ? tensorTermQ, b_TTerm ? tensorTermQ] :=
    With[{ra = Length @ tUopShape[a], rb = Length @ tUopShape[b]},
        Which[
            ra === 1 && rb === 1,  TDot[a, b],               (* inner product -> scalar *)
            ra === 2 && rb === 1,  TMatVec[a, b],            (* matrix . vector -> {m} *)
            ra === 1 && rb === 2,  TMatVec[Transpose[b], a], (* vector . matrix -> {n} *)
            True,                  TMatMul[a, b]]]           (* matrix . matrix (+ batched) *)

TTerm /: ArrayReshape[t_TTerm ? tensorTermQ, shape_List] :=
    TUOpReshape[t, shape]

(* ArrayReduce[Total, t, axes]: SUM-reduce the given 1-indexed axes,
   highest first so the remaining axis indices stay valid as each one
   collapses.  Total[t] / Total[t, axis] above cover the all-axis and
   single-axis cases; this is the explicit multi-axis form. *)
TTerm /: ArrayReduce[Total, t_TTerm ? tensorTermQ, axes_] :=
    Fold[TUOpReduce[#1, #2 - 1, "SUM"] &, t,
        Reverse @ Sort @ DeleteDuplicates @ Flatten @ {axes}]

(* Normal[t]: read the realized tensor's data back as an ordinary nested
   list, i.e. Normal[TTensorData[t]], so `Normal @ TRealize @ expr`
   stands in for `Normal @ TTensorData @ TRealize @ expr`. *)
TTerm /: Normal[t_TTerm ? tensorTermQ] := Normal[TTensorData[t]]

(* Layer-call UpValues: `Layer[opts][t_TTerm]` is still a TTerm
   UpValue, TagSetDelayed on TTerm, with the layer bound as
   `l_SoftmaxLayer` so we can read its options.  WL's "Level"
   parameter is 1-indexed; thvm's TSoftmax is 0-indexed. *)
TTerm /: l_SoftmaxLayer[t_TTerm ? tensorTermQ] :=
    TSoftmax[t, NetExtract[l, "Parameters"]["Level"] - 1]

(* Set on a literal-TTerm LHS rewrites in place: realises src into a
   fresh TenDesc, memcpys those bytes into dst's backing buffer.  dst
   keeps its TenDesc id (so any caller still holding it sees the new
   contents).  Only fires when the LHS is the literal TTerm form:
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

(* TSetData[dst, na]: in-place host upload.  Writes the NumericArray's
   bytes straight into dst's existing buffer via tensor_write_na, with
   NO fresh TenDesc and no ASSIGN graph: the fast per-step feed path.
   `asSharableNA` lifts a plain list / PackedArray to a NumericArray
   first (one copy), matching TTensorCreate's input handling so dtype
   inference is consistent.  Returns dst. *)
TSetData[dst_TTerm, na_] := (
    ensureInit[];
    $tensorWriteNAFn[TTermVal[dst], asSharableNA[na]];
    dst)

End[];

EndPackage[];
