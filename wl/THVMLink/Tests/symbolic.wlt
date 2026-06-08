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

(* === Symbolic MULTI-HEAD attention ====================================

   The single-head primitive above runs per head; multi-head needs a SPLIT
   of {S, dim} into n_heads x {S, d_head} and a CONCAT back to {S, dim},
   both over the symbolic sequence S.

   SPLIT route (a): reshape {S,dim} -> {S,n_heads,d_head} (kvar extent `sym`
   for the seq dim) then PERMUTE {1,0,2} -> {n_heads,S,d_head} and SHRINK one
   head -> {S,d_head}.  A kvar PERMUTE survives the addressing: this test
   reshapes a known column pattern (x[i,c]=10 i+c), permutes, and asserts the
   per-head columns land correctly.  (PERMUTE of a kvar axis WORKS.)

   CONCAT: the {seq,dim} headStitch idiom (TUOpPad each head into its column
   slice + sum) is UNUSABLE over a symbolic axis -- TUOpPad of a kvar tensor
   reads through the static hi-strides and returns ALL ZEROS (a kvar PAD
   addressing bug; tested both inner-axis-only and middle-axis pads).  The
   PAD-free concat used below instead: reshape each head {S,d_head} ->
   {S,1,d_head}, EXPAND to {S,n_heads,d_head}, multiply by a SYMBOLIC one-hot
   column selector (a {hi,n_heads,1} host const marked symbolic on axis 0,
   expanded to {S,n_heads,d_head}), sum the heads, reshape -> {S,dim}.  Every
   op is one the single-head eager primitive already exercises over kvar. *)

(* SPLIT route (a): a kvar reshape + PERMUTE + per-head SHRINK preserves the
   per-head columns.  x[i,c] = 10 i + c ; head 0 = cols {0,1}, head 1 = cols
   {2,3}; after permute to {n_heads,S,d_head} head h row i = {10 i + 2 h,
   10 i + 2 h + 1}.  Asserted at S=4. *)
VerificationTest[
    Module[{hi = 8, dim = 4, nHeads = 2, dHead = 2, vid, sym, er, x, splitHead},
        TInit[];
        vid = TKVarAlloc[1, hi];
        sym = 2^31 + vid;
        er[t_] := TRealize[t];
        TKVarSet[vid, 4];
        x = TSymbolicAxis[
            TTensorCreate[Table[N[10 i + c], {i, 0, hi - 1}, {c, 0, dim - 1}]],
            0, vid];
        splitHead[t_, h_] := er @ TUOpReshape[
            TUOpShrink[
                TUOpPermute[TUOpReshape[t, {sym, nHeads, dHead}], {1, 0, 2}],
                {{h, h + 1}, {0, sym}, {0, dHead}}],
            {sym, dHead}];
        {Normal @ TTensorData @ TRealize @ splitHead[x, 0],
         Normal @ TTensorData @ TRealize @ splitHead[x, 1]}
    ],
    {Table[N[10 i + c], {i, 0, 3}, {c, 0, 1}],
     Table[N[10 i + 2 + c], {i, 0, 3}, {c, 0, 1}]},
    TestID -> "symbolic/multihead-split-permute",
    SameTest -> (Max @ Abs[Flatten[#1] - Flatten[#2]] < 1.*^-5 &)
]

(* FULL symbolic multi-head causal attention, EAGER, realized at S=4 then the
   SAME construction re-run at S=6.  n_heads=2, d_head=2, dim=4.  Q,K,V are
   fixed {hi,dim} host tensors; the result is asserted against an independent
   host-side multi-head causal-attention oracle at each bound. *)
VerificationTest[
    Module[
        {hi = 8, dim = 4, nHeads = 2, dHead = 2, qHost, kHost, vHost, scale,
         hostMHA, mhaSym},
        qHost = Table[N[0.1 (i + 1) + 0.01 c], {i, 0, hi - 1}, {c, 0, dim - 1}];
        kHost = Table[N[0.1 (i + 1) - 0.01 c], {i, 0, hi - 1}, {c, 0, dim - 1}];
        vHost = Table[N[i + 0.5 c], {i, 0, hi - 1}, {c, 0, dim - 1}];
        scale = 1. / Sqrt[N @ dHead];
        (* host oracle: split columns, per-head causal softmax-attn, concat *)
        hostMHA[S_] := ArrayFlatten[{Table[
            Module[{qh, kh, vh, sc, msk, w},
                qh = qHost[[1 ;; S, h dHead + 1 ;; (h + 1) dHead]];
                kh = kHost[[1 ;; S, h dHead + 1 ;; (h + 1) dHead]];
                vh = vHost[[1 ;; S, h dHead + 1 ;; (h + 1) dHead]];
                sc = (qh . Transpose[kh]) scale +
                    Table[If[ j <= i, 0., -10.^9], {i, 0, S - 1}, {j, 0, S - 1}];
                w = Table[Exp[sc[[r]] - Max[sc[[r]]]], {r, 1, S}];
                w = w / Total[w, {2}];
                w . vh],
            {h, 0, nHeads - 1}]}];
        mhaSym[Sval_] := Module[
            {vid, sym, er, qT, kT, vT, ramp, ri, rj, mask, splitHead, perHead,
             heads, buildSlot},
            TInit[];
            vid = TKVarAlloc[1, hi];
            sym = 2^31 + vid;
            er[t_] := TRealize[t];
            TKVarSet[vid, Sval];
            qT = TSymbolicAxis[TTensorCreate[qHost], 0, vid];
            kT = TSymbolicAxis[TTensorCreate[kHost], 0, vid];
            vT = TSymbolicAxis[TTensorCreate[vHost], 0, vid];
            ramp = TSymbolicAxis[TTensorCreate[N @ Range[0, hi - 1]], 0, vid];
            ri = er @ TUOpExpand[TUOpReshape[ramp, {sym, 1}], {sym, sym}];
            rj = er @ TUOpExpand[TUOpReshape[ramp, {1, sym}], {sym, sym}];
            mask = er @ TUOpMul[
                TUOpCmplt[TUOpAdd[ri, TUOpNeg[rj]], TUOpConst[0.]],
                TUOpConst[-30.]];
            splitHead[t_, h_] := er @ TUOpReshape[
                TUOpShrink[
                    TUOpPermute[TUOpReshape[t, {sym, nHeads, dHead}], {1, 0, 2}],
                    {{h, h + 1}, {0, sym}, {0, dHead}}],
                {sym, dHead}];
            perHead[h_] := Module[
                {qh, kh, vh, qe, ke, scores, masked, m, me, e, sm, se, w, we, ve},
                qh = splitHead[qT, h];
                kh = splitHead[kT, h];
                vh = splitHead[vT, h];
                qe = er @ TUOpExpand[TUOpReshape[qh, {sym, 1, dHead}],
                    {sym, sym, dHead}];
                ke = er @ TUOpExpand[TUOpReshape[kh, {1, sym, dHead}],
                    {sym, sym, dHead}];
                scores = er @ TUOpMul[
                    er @ TUOpReduce[er @ TUOpMul[qe, ke], 2, "SUM"],
                    TUOpConst[scale]];
                masked = er @ TUOpAdd[scores, mask];
                m  = er @ TUOpReduce[masked, 1, "MAX"];
                me = er @ TUOpExpand[TUOpReshape[m, {sym, 1}], {sym, sym}];
                e  = er @ TUOpExp2[er @ TUOpMul[
                    er @ TUOpAdd[masked, TUOpNeg[me]], TUOpConst[N @ Log2[E]]]];
                sm = er @ TUOpReduce[e, 1, "SUM"];
                se = er @ TUOpExpand[TUOpReshape[sm, {sym, 1}], {sym, sym}];
                w  = er @ TUOpMul[e, TUOpRecip[se]];
                we = er @ TUOpExpand[TUOpReshape[w, {sym, sym, 1}],
                    {sym, sym, dHead}];
                ve = er @ TUOpExpand[TUOpReshape[vh, {1, sym, dHead}],
                    {sym, sym, dHead}];
                er @ TUOpReduce[er @ TUOpMul[we, ve], 1, "SUM"]];
            heads = perHead /@ Range[0, nHeads - 1];
            (* PAD-free concat: symbolic one-hot column selector + sum *)
            buildSlot[hh_, h_] := Module[{he, ohS},
                he = er @ TUOpExpand[TUOpReshape[hh, {sym, 1, dHead}],
                    {sym, nHeads, dHead}];
                ohS = TSymbolicAxis[
                    TTensorCreate[
                        Table[N @ Boole[s == h], {hi}, {s, 0, nHeads - 1}, {1}]],
                    0, vid];
                er @ TUOpMul[he,
                    er @ TUOpExpand[ohS, {sym, nHeads, dHead}]]];
            Chop[Normal @ TTensorData @ TRealize @ er @ TUOpReshape[
                er @ Total @ Table[buildSlot[heads[[h + 1]], h],
                    {h, 0, nHeads - 1}],
                {sym, dim}], 1.*^-6]];
        {Max @ Abs[Flatten[mhaSym[4] - hostMHA[4]]],
         Max @ Abs[Flatten[mhaSym[6] - hostMHA[6]]]}
    ],
    {0., 0.},  (* sym output == host oracle at S=4 and S=6 *)
    TestID -> "symbolic/multihead-causal-attention-eager",
    SameTest -> (Max @ Abs[#1 - #2] < 1.*^-4 &)
]

(* BONUS: a full symbolic TRANSFORMER BLOCK, EAGER --
   x -> multi-head causal self-attention -> residual add -> ReLU MLP
   ({S,dim}.{dim,4 dim} -> relu -> .{4 dim,dim}) -> residual add.
   ReLU is x*(0<x) via TUOpCmplt (no binary-max UOP).  Realized at S=4 and
   S=6, asserted against an independent host-side block oracle. *)
VerificationTest[
    Module[
        {hi = 8, dim = 4, nHeads = 2, dHead = 2, dff = 16, xHost, w1Host,
         w2Host, scale, hostMHA, hostBlock, symBlock, relu},
        xHost  = Table[N[0.1 (i + 1) + 0.01 c], {i, 0, hi - 1}, {c, 0, dim - 1}];
        w1Host = Table[N[0.05 (a - b)], {a, 0, dim - 1}, {b, 0, dff - 1}];
        w2Host = Table[N[0.03 (a + b)], {a, 0, dff - 1}, {b, 0, dim - 1}];
        scale = 1. / Sqrt[N @ dHead];
        relu  = Max[#, 0.] &;
        hostMHA[x_, S_] := ArrayFlatten[{Table[
            Module[{qh, sc, w},
                qh = x[[All, h dHead + 1 ;; (h + 1) dHead]];
                sc = (qh . Transpose[qh]) scale +
                    Table[If[ j <= i, 0., -10.^9], {i, 0, S - 1}, {j, 0, S - 1}];
                w = Table[Exp[sc[[r]] - Max[sc[[r]]]], {r, 1, S}];
                w = w / Total[w, {2}];
                w . qh],
            {h, 0, nHeads - 1}]}];
        hostBlock[S_] := Module[{x, r1},
            x  = xHost[[1 ;; S]];
            r1 = x + hostMHA[x, S];
            r1 + Map[relu, r1 . w1Host, {2}] . w2Host];
        symBlock[Sval_] := Module[
            {vid, sym, er, xT, ramp, ri, rj, mask, splitHead, perHead, heads,
             buildSlot, attn, r1, hid},
            TInit[];
            vid = TKVarAlloc[1, hi];
            sym = 2^31 + vid;
            er[t_] := TRealize[t];
            TKVarSet[vid, Sval];
            xT = TSymbolicAxis[TTensorCreate[xHost], 0, vid];
            ramp = TSymbolicAxis[TTensorCreate[N @ Range[0, hi - 1]], 0, vid];
            ri = er @ TUOpExpand[TUOpReshape[ramp, {sym, 1}], {sym, sym}];
            rj = er @ TUOpExpand[TUOpReshape[ramp, {1, sym}], {sym, sym}];
            mask = er @ TUOpMul[
                TUOpCmplt[TUOpAdd[ri, TUOpNeg[rj]], TUOpConst[0.]],
                TUOpConst[-30.]];
            splitHead[t_, h_] := er @ TUOpReshape[
                TUOpShrink[
                    TUOpPermute[TUOpReshape[t, {sym, nHeads, dHead}], {1, 0, 2}],
                    {{h, h + 1}, {0, sym}, {0, dHead}}],
                {sym, dHead}];
            perHead[h_] := Module[
                {qh, qe, ke, scores, masked, m, me, e, sm, se, w, we, ve},
                qh = splitHead[xT, h];     (* Q = K = V = x per head *)
                qe = er @ TUOpExpand[TUOpReshape[qh, {sym, 1, dHead}],
                    {sym, sym, dHead}];
                ke = er @ TUOpExpand[TUOpReshape[qh, {1, sym, dHead}],
                    {sym, sym, dHead}];
                scores = er @ TUOpMul[
                    er @ TUOpReduce[er @ TUOpMul[qe, ke], 2, "SUM"],
                    TUOpConst[scale]];
                masked = er @ TUOpAdd[scores, mask];
                m  = er @ TUOpReduce[masked, 1, "MAX"];
                me = er @ TUOpExpand[TUOpReshape[m, {sym, 1}], {sym, sym}];
                e  = er @ TUOpExp2[er @ TUOpMul[
                    er @ TUOpAdd[masked, TUOpNeg[me]], TUOpConst[N @ Log2[E]]]];
                sm = er @ TUOpReduce[e, 1, "SUM"];
                se = er @ TUOpExpand[TUOpReshape[sm, {sym, 1}], {sym, sym}];
                w  = er @ TUOpMul[e, TUOpRecip[se]];
                we = er @ TUOpExpand[TUOpReshape[w, {sym, sym, 1}],
                    {sym, sym, dHead}];
                ve = er @ TUOpExpand[TUOpReshape[qh, {1, sym, dHead}],
                    {sym, sym, dHead}];
                er @ TUOpReduce[er @ TUOpMul[we, ve], 1, "SUM"]];
            heads = perHead /@ Range[0, nHeads - 1];
            buildSlot[hh_, h_] := Module[{he, ohS},
                he = er @ TUOpExpand[TUOpReshape[hh, {sym, 1, dHead}],
                    {sym, nHeads, dHead}];
                ohS = TSymbolicAxis[
                    TTensorCreate[
                        Table[N @ Boole[s == h], {hi}, {s, 0, nHeads - 1}, {1}]],
                    0, vid];
                er @ TUOpMul[he,
                    er @ TUOpExpand[ohS, {sym, nHeads, dHead}]]];
            attn = er @ TUOpReshape[
                er @ Total @ Table[buildSlot[heads[[h + 1]], h],
                    {h, 0, nHeads - 1}],
                {sym, dim}];
            r1  = er @ TUOpAdd[xT, attn];          (* residual *)
            hid = With[{pre = er[r1 . TTensorCreate[w1Host]]},
                er @ TUOpMul[pre, TUOpCmplt[TUOpConst[0.], pre]]];  (* relu *)
            Chop[Normal @ TTensorData @ TRealize @ er @ TUOpAdd[
                r1, er[hid . TTensorCreate[w2Host]]], 1.*^-6]];  (* residual *)
        {Max @ Abs[Flatten[symBlock[4] - hostBlock[4]]],
         Max @ Abs[Flatten[symBlock[6] - hostBlock[6]]]}
    ],
    {0., 0.},
    TestID -> "symbolic/transformer-block-eager",
    SameTest -> (Max @ Abs[#1 - #2] < 1.*^-4 &)
]

(* === PUBLIC-OP symbolic multi-head attention =========================

   The construction above is now packaged behind the PUBLIC ops:
   TMultiHeadAttention auto-selects the eager symbolic branch when Q's
   leading (sequence) axis is kvar-PACKED (>= 2^31), and TCausalMaskSym
   builds the symbolic {S,S} causal bias.  This test calls only those
   public ops -- TMultiHeadAttention[Q, K, V, nHeads, TCausalMaskSym[vid,
   hi]] on symbolic-seq {S,dim} Q,K,V -- and asserts against an
   independent host-side multi-head causal-attention oracle at S=4 AND
   S=6.  The default per-head scale is 1/Sqrt[dHead]; the oracle uses the
   same.  This proves the surface op (not an inlined graph) now flows a
   symbolic sequence. *)
VerificationTest[
    Module[
        {hi = 8, dim = 4, nHeads = 2, dHead = 2, qHost, kHost, vHost, scale,
         hostMHA, mhaPublic},
        qHost = Table[N[0.1 (i + 1) + 0.01 c], {i, 0, hi - 1}, {c, 0, dim - 1}];
        kHost = Table[N[0.1 (i + 1) - 0.01 c], {i, 0, hi - 1}, {c, 0, dim - 1}];
        vHost = Table[N[i + 0.5 c], {i, 0, hi - 1}, {c, 0, dim - 1}];
        scale = 1. / Sqrt[N @ dHead];
        (* host oracle: split columns, per-head causal softmax-attn, concat *)
        hostMHA[S_] := ArrayFlatten[{Table[
            Module[{qh, kh, vh, sc, w},
                qh = qHost[[1 ;; S, h dHead + 1 ;; (h + 1) dHead]];
                kh = kHost[[1 ;; S, h dHead + 1 ;; (h + 1) dHead]];
                vh = vHost[[1 ;; S, h dHead + 1 ;; (h + 1) dHead]];
                sc = (qh . Transpose[kh]) scale +
                    Table[If[ j <= i, 0., -10.^9], {i, 0, S - 1}, {j, 0, S - 1}];
                w = Table[Exp[sc[[r]] - Max[sc[[r]]]], {r, 1, S}];
                w = w / Total[w, {2}];
                w . vh],
            {h, 0, nHeads - 1}]}];
        mhaPublic[Sval_] := Module[{vid, qT, kT, vT, out},
            TInit[];
            vid = TKVarAlloc[1, hi];
            TKVarSet[vid, Sval];
            qT = TSymbolicAxis[TTensorCreate[qHost], 0, vid];
            kT = TSymbolicAxis[TTensorCreate[kHost], 0, vid];
            vT = TSymbolicAxis[TTensorCreate[vHost], 0, vid];
            out = TMultiHeadAttention[qT, kT, vT, nHeads,
                TCausalMaskSym[vid, hi]];
            Chop[Normal @ TTensorData @ TRealize @ out, 1.*^-6]];
        {Max @ Abs[Flatten[mhaPublic[4] - hostMHA[4]]],
         Max @ Abs[Flatten[mhaPublic[6] - hostMHA[6]]]}
    ],
    {0., 0.},  (* public-op output == host oracle at S=4 and S=6 *)
    TestID -> "symbolic/multihead-causal-attention-public-op",
    SameTest -> (Max @ Abs[#1 - #2] < 1.*^-4 &)
]
