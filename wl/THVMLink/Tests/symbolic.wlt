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

(* A full causal-masked attention block over a SYMBOLIC sequence S, computed
   EAGER -- each reduce / binary-op / broadcast EXPAND realized to a contiguous
   leaf (`er`) before the next op consumes it.  Eager sidesteps the fused-
   broadcast addressing of a doubly-symbolic {S,S} graph: the C reference
   (tests/test_sym_attn_block.c / test_sym_attn_causal.c) proves the SAME
   construction is correct on the engine; this bridges it to the WL surface --
   the on-ramp to a symbolic GPT-2 with no fixed maxSeq.

   Q=K=ones{S,d}, V[j,k]=j ; scores = einsum ik,jk->ij -> + causal mask ->
   softmax over the key axis -> einsum ij,jk->ik.  The causal weights are
   1/(i+1) for j<=i and 0 for j>i; the output out[i,0] = causal-weighted avg
   of V[0..i] = (1/(i+1)) sum_{j<=i} j = i/2.

   The {S,1}/{1,S} ramp reshapes pass the kvar-PACKED extent (2^31 + vid)
   directly as the shape dim so the symbolic axis survives the movement op
   (the same value the C reference writes into view.shape).  TTensorData then
   reads the bound {S,..} region back through the buffer's static (hi) strides.

   Realized at S=4, then the WHOLE eager construction re-run at S=6 from the
   SAME hi-sized leaves -- one set of symbolic kernels runs at either bound. *)
VerificationTest[
    Module[{hi = 8, d = 3, vid, sym, er, ramp, ones, ri, rj, mask, qe, ke,
            scores, masked, m, me, e, sm, se, w, weights, vmat, we, ve, outT, out},
        TInit[];
        vid = TKVarAlloc[1, hi];
        sym = 2^31 + vid;                              (* kvar-packed extent *)
        er[t_] := TRealize[t];                         (* realize -> contiguous leaf *)
        TKVarSet[vid, 4];
        ramp = TSymbolicAxis[TTensorCreate[N @ Range[0, hi - 1]], 0, vid];
        ones = TSymbolicAxis[TTensorCreate[ConstantArray[1., {hi, d}]], 0, vid];
        (* causal mask {S,S}: ri[i,j]=i, rj[i,j]=j, mask=(i-j<0)?-30:0 *)
        ri = er @ TUOpExpand[TUOpReshape[ramp, {sym, 1}], {sym, sym}];
        rj = er @ TUOpExpand[TUOpReshape[ramp, {1, sym}], {sym, sym}];
        mask = er @ TUOpMul[
            TUOpCmplt[TUOpAdd[ri, TUOpNeg[rj]], TUOpConst[0.]], TUOpConst[-30.]];
        (* scores = einsum ik,jk->ij (reduce over d = axis 2) *)
        qe = er @ TUOpExpand[TUOpReshape[ones, {sym, 1, d}], {sym, sym, d}];
        ke = er @ TUOpExpand[TUOpReshape[ones, {1, sym, d}], {sym, sym, d}];
        scores = er @ TUOpReduce[er @ TUOpMul[qe, ke], 2, "SUM"];
        masked = er @ TUOpAdd[scores, mask];
        (* softmax over the key axis (axis 1); exp via the EXP2 + log2(e) chain *)
        m  = er @ TUOpReduce[masked, 1, "MAX"];
        me = er @ TUOpExpand[TUOpReshape[m, {sym, 1}], {sym, sym}];
        e  = er @ TUOpExp2[er @ TUOpMul[
            er @ TUOpAdd[masked, TUOpNeg[me]], TUOpConst[N @ Log2[E]]]];
        sm = er @ TUOpReduce[e, 1, "SUM"];
        se = er @ TUOpExpand[TUOpReshape[sm, {sym, 1}], {sym, sym}];
        w  = er @ TUOpMul[e, TUOpRecip[se]];           (* {S,S} causal weights *)
        weights = Chop[Normal @ TTensorData @ TRealize @ w, 1.*^-6];
        (* out = einsum ij,jk->ik (reduce over j = axis 1), V[j,k]=j *)
        vmat = er @ TUOpExpand[TUOpReshape[ramp, {sym, 1}], {sym, d}];
        we = er @ TUOpExpand[TUOpReshape[w, {sym, sym, 1}], {sym, sym, d}];
        ve = er @ TUOpExpand[TUOpReshape[vmat, {1, sym, d}], {sym, sym, d}];
        outT = er @ TUOpReduce[er @ TUOpMul[we, ve], 1, "SUM"];
        out  = Chop[Normal @ TTensorData @ TRealize @ outT, 1.*^-6];
        {weights, out[[All, 1]]}
    ],
    {(* causal softmax weights: row i is 1/(i+1) for j<=i, 0 for j>i *)
     {{1., 0, 0, 0},
      {1/2., 1/2., 0, 0},
      {1/3., 1/3., 1/3., 0},
      {1/4., 1/4., 1/4., 1/4.}},
     (* out[i,0] = causal-weighted avg of V[0..i] = i/2 *)
     {0, 1/2., 1., 3/2.}},
    TestID -> "symbolic/causal-attention-eager",
    SameTest -> (Max @ Abs[Flatten[#1] - Flatten[#2]] < 1.*^-5 &)
]

(* The SAME eager causal-attention construction re-run at a SECOND bound S=6:
   one set of hi-sized symbolic leaves, two sequence lengths.  Rows 4 and 5
   (absent at S=4) now carry their causal weights 1/5 and 1/6 -- the symbolic
   kernels iterate the bound length with no re-lift of the constructors. *)
VerificationTest[
    Module[{hi = 8, vid, sym, er, ramp, ri, rj, mask, m, me, e, sm, se, w},
        TInit[];
        vid = TKVarAlloc[1, hi];
        sym = 2^31 + vid;
        er[t_] := TRealize[t];
        TKVarSet[vid, 6];
        ramp = TSymbolicAxis[TTensorCreate[N @ Range[0, hi - 1]], 0, vid];
        ri = er @ TUOpExpand[TUOpReshape[ramp, {sym, 1}], {sym, sym}];
        rj = er @ TUOpExpand[TUOpReshape[ramp, {1, sym}], {sym, sym}];
        mask = er @ TUOpMul[
            TUOpCmplt[TUOpAdd[ri, TUOpNeg[rj]], TUOpConst[0.]], TUOpConst[-30.]];
        m  = er @ TUOpReduce[mask, 1, "MAX"];
        me = er @ TUOpExpand[TUOpReshape[m, {sym, 1}], {sym, sym}];
        e  = er @ TUOpExp2[er @ TUOpMul[
            er @ TUOpAdd[mask, TUOpNeg[me]], TUOpConst[N @ Log2[E]]]];
        sm = er @ TUOpReduce[e, 1, "SUM"];
        se = er @ TUOpExpand[TUOpReshape[sm, {sym, 1}], {sym, sym}];
        w  = er @ TUOpMul[e, TUOpRecip[se]];
        Chop[Normal @ TTensorData @ TRealize @ w, 1.*^-6]
    ],
    Table[If[ j <= i, 1./(i + 1), 0], {i, 0, 5}, {j, 0, 5}],
    TestID -> "symbolic/causal-attention-eager-rebind-S6",
    SameTest -> (Max @ Abs[Flatten[#1] - Flatten[#2]] < 1.*^-5 &)
]
