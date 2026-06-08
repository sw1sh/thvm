(* symbolic.wlt -- symbolic-shape (kvar) dimensions.  A tensor axis marked
   symbolic with TSymbolicAxis is sized at its upper bound but loops the value
   bound by TKVarSet, so ONE materialized graph runs at any length with no
   re-lift (tinygrad symbolic shapes).  The on-ramp to symbolic-sequence
   inference: see docs/plans/decode_roadmap.md. *)

(* A {16,4} tensor of ones with axis 0 reinterpreted as a symbolic dim S;
   summing over S yields S in every output cell. *)
VerificationTest[
    TInit[];
    vid = TKVarAlloc[1, 16];
    t   = TTensorCreate[ConstantArray[1., {16, 4}]];
    ts  = TSymbolicAxis[t, 0, vid];
    r   = TUOpReduce[ts, 0, "SUM"];
    TKVarSet[vid, 5];
    Normal @ TTensorData @ TRealize @ r,
    {5., 5., 5., 5.},
    TestID -> "symbolic/reduce-over-symbolic-axis"
]

(* The SAME materialized graph realized at three lengths by rebinding the
   symbolic dim -- no re-lift between calls (the generation pattern). *)
VerificationTest[
    TInit[];
    vid = TKVarAlloc[1, 16];
    t   = TTensorCreate[ConstantArray[1., {16, 4}]];
    ts  = TSymbolicAxis[t, 0, vid];
    r   = TUOpReduce[ts, 0, "SUM"];
    {TKVarSet[vid, 5];  Normal @ TTensorData @ TRealize @ r,
     TKVarSet[vid, 7];  Normal @ TTensorData @ TRealize @ r,
     TKVarSet[vid, 12]; Normal @ TTensorData @ TRealize @ r},
    {{5., 5., 5., 5.}, {7., 7., 7., 7.}, {12., 12., 12., 12.}},
    TestID -> "symbolic/rebind-same-graph"
]

(* A symbolic-OUTPUT op -- a {S,4}.{4,3} matmul -> {S,3} -- reads back at the
   BOUND length, not the kvar upper bound.  The readback resolves symbolic dims
   (thvm_wl_tensor_read via kvar_extent_runtime), so it returns the valid
   {bound,3} region instead of trying to allocate a NumericArray at the raw
   kvar-packed 2^31-ish extent (which would spike RAM by ~25 GB). *)
VerificationTest[
    TInit[];
    vid = TKVarAlloc[1, 16];
    a   = TSymbolicAxis[TTensorCreate[ConstantArray[1., {16, 4}]], 0, vid];
    w   = TTensorCreate[ConstantArray[1., {4, 3}]];
    r   = a . w;
    TKVarSet[vid, 5];
    Normal @ TTensorData @ TRealize @ r,
    ConstantArray[4., {5, 3}],
    TestID -> "symbolic/readback-symbolic-output-bounded"
]

(* A multi-layer symbolic-SEQUENCE forward -- two matmuls + an elementwise over
   a symbolic seq dim S -- realized at two lengths from ONE graph (no re-lift,
   no maxSeq).  This is the outer-symbolic {S, dim} path that GPT-2's embedding
   + MLP run on: x{S,8}.W1{8,12} -> +self -> .W2{12,8}, each output = 48. *)
VerificationTest[
    TInit[];
    vid = TKVarAlloc[1, 16];
    x   = TSymbolicAxis[TTensorCreate[ConstantArray[1., {16, 8}]], 0, vid];
    h   = x . TTensorCreate[ConstantArray[0.5, {8, 12}]];
    h2  = h + h;
    y   = h2 . TTensorCreate[ConstantArray[0.5, {12, 8}]];
    {TKVarSet[vid, 5]; Normal @ TTensorData @ TRealize @ y,
     TKVarSet[vid, 7]; Normal @ TTensorData @ TRealize @ y},
    {ConstantArray[48., {5, 8}], ConstantArray[48., {7, 8}]},
    TestID -> "symbolic/multi-layer-seq-forward"
]
