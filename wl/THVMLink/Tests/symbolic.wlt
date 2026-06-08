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

(* REGRESSION GUARD: repeated fresh-kvar TCausalMaskSym {S,S} realizes.
   Allocating a NEW kvar each time and realizing a TCausalMaskSym {S,S} once
   crashed on the ~4th fresh kvar: the hand-coded UPCAST/UNROLL opt admitted a
   symbolic axis (its extent is kvar-PACKED, 0x80000000|id, which `> 1` wrongly
   passed), then uop_dag_apply_split's raw `extent / k` CLEARED the kvar flag
   (0x80000004 / 4 = 0x20000001) -> a ~500M-iter loop -> out-of-bounds store.
   The "every 4th fresh kvar" signature was exact: split bails on `extent % k
   != 0`, so 0x80000001/2/3 escaped; 0x80000004 was the first fresh id divisible
   by k=4.  A SHARED vid (id stays 1) never divided evenly, so it only surfaced
   on the multi-block GPT-2 forward.  FIXED e6e2163c (exclude kvar axes from
   upcast/unroll, per tinygrad codegen/opt/postrange.py).  Run in a SUBPROCESS
   so any regression's SIGABRT does not take down this suite; asserts the child
   exits cleanly. *)
VerificationTest[
    Module[{script, res},
        script = "
PacletDirectoryLoad[\"wl/THVMLink\"]; Get[\"THVMLink`\"];
Do[ Module[{vid, m},
        vid = TKVarAlloc[1, 8]; TKVarSet[vid, 4];
        m   = TCausalMaskSym[vid, 8];
        Normal @ TTensorData @ TRealize @ m], {6}];
WriteString[\"stdout\", \"OK\"];";
        res = RunProcess[{"wolframscript", "-code", script}];
        res["ExitCode"]
    ],
    0,
    TestID -> "symbolic/repeated-fresh-kvar-causal-mask-no-crash"
]

(* === TJit captures + replays the symbolic EAGER forward, rebinding S ========

   The public-op symbolic MHA (the "...-public-op" test above) is built ONCE
   and wrapped in TJit; the first call CAPTURES its eager-realize dispatch
   sequence and each later call REPLAYS the cached dispatches, rebinding the
   sequence length S in place via TKVarSet (the kvar's runtime bound is a
   per-dispatch argument -- cpu_jit_kvar_vals re-reads kvar_runtime() at fire
   time, src/backend/cpu/jit.c).  This is the symbolic generation pattern: lift
   once, then re-dispatch at the running length with no re-lift.

   CAPTURE-AT-HI is load-bearing.  Every intermediate is realized to a
   contiguous leaf sized at the kvar UPPER BOUND `hi` (view_create strides via
   kvar_extent_static, src/view/create.c), but each kernel's store loop only
   writes `a0 < V_S` rows.  Capturing at S < hi leaves rows [S, hi) of the
   leaves un-primed; replaying UPWARD then reads that stale tail.  Capturing at
   S = hi primes every row, so replaying at any S <= hi is correct.  This test
   captures at hi and asserts the replay matches the freshly-rebuilt non-JIT
   symbolic forward at S = hi, hi-2, AND hi-3. *)
VerificationTest[
    Module[
        {hi = 8, dim = 4, nHeads = 2, vid, qH, kH, vH, qI, kI, vI, mask,
         fn, nonJit, jit},
        TInit[];
        qH = Table[N[0.1 (i + 1) + 0.01 c], {i, 0, hi - 1}, {c, 0, dim - 1}];
        kH = Table[N[0.1 (i + 1) - 0.01 c], {i, 0, hi - 1}, {c, 0, dim - 1}];
        vH = Table[N[i + 0.5 c], {i, 0, hi - 1}, {c, 0, dim - 1}];
        (* non-JIT oracle: rebuild the symbolic forward fresh at each S *)
        nonJit[Sv_] := Module[{v, q, k, vt, o},
            TInit[];
            v = TKVarAlloc[1, hi]; TKVarSet[v, Sv];
            q = TSymbolicAxis[TTensorCreate[qH], 0, v];
            k = TSymbolicAxis[TTensorCreate[kH], 0, v];
            vt = TSymbolicAxis[TTensorCreate[vH], 0, v];
            o = TMultiHeadAttention[q, k, vt, nHeads, TCausalMaskSym[v, hi]];
            Chop[Normal @ TTensorData @ TRealize @ o, 1.*^-6]];
        (* JIT path: ONE kvar, ONE closure, captured at S = hi *)
        vid = TKVarAlloc[1, hi];
        qI = TSymbolicAxis[TTensorCreate[qH], 0, vid];
        kI = TSymbolicAxis[TTensorCreate[kH], 0, vid];
        vI = TSymbolicAxis[TTensorCreate[vH], 0, vid];
        mask = TCausalMaskSym[vid, hi];
        fn = TJit[ TRealize @ TMultiHeadAttention[qI, kI, vI, nHeads, mask] & ];
        TKVarSet[vid, hi];
        Chop[Normal @ TTensorData @ fn[], 1.*^-6];   (* CAPTURE at hi *)
        jit[Sv_] := (TKVarSet[vid, Sv]; Chop[Normal @ TTensorData @ fn[], 1.*^-6]);
        {Max @ Abs[Flatten[jit[hi]     - nonJit[hi]]],
         Max @ Abs[Flatten[jit[hi - 2] - nonJit[hi - 2]]],
         Max @ Abs[Flatten[jit[hi - 3] - nonJit[hi - 3]]]}
    ],
    {0., 0., 0.},
    TestID -> "symbolic/jit-capture-replay-rebind-S",
    SameTest -> (Max @ Abs[#1 - #2] < 1.*^-4 &)
]

(* TAppendAt: the WL KV-cache append surface (decode roadmap Lever 2 step 1).
   Write a {1,4} row into a {8,4} cache at a runtime kvar offset 3 then 5
   (rebound), confirming only those rows change -- the WL wrapper over the
   validated C append (tests/test_sym_kvcache_append.c). *)
VerificationTest[
    Module[{posVid, cache},
        TInit[];
        posVid = TKVarAlloc[1, 8];
        cache  = TRealize[TTensorCreate[ConstantArray[0., {8, 4}]]];
        TKVarSet[posVid, 3];
        TAppendAt[cache, TTensorCreate[ConstantArray[1., {1, 4}]], posVid];
        TKVarSet[posVid, 5];
        TAppendAt[cache, TTensorCreate[ConstantArray[2., {1, 4}]], posVid];
        Normal @ TTensorData @ cache
    ],
    ReplacePart[ConstantArray[0., {8, 4}],
        {4 -> ConstantArray[1., 4], 6 -> ConstantArray[2., 4]}],
    TestID -> "symbolic/kvcache-append-wl"
]

(* TDecodeAttend: GPT-2's per-step DECODE attention over a KV cache (decode
   roadmap Lever 2 step 2).  A single new query q1 ({1,dim}) attends a
   symbolic-length cached K/V prefix.  The toy: nCtx=8, dim=4, nHeads=2.
   Pre-fill t "past" rows in the caches, append a NEW {1,dim} kNew/vNew row at
   posVid=t, then decode-attend with q1 over the first t+1 cached rows (incl.
   the appended token).  Asserted against a host-side single-query multi-head
   attention oracle (per head: q.cachedK^T -> softmax -> .cachedV, concat).
   Run at t=3 then t=5 (rebinding posVid + lenVid=t+1) to prove the runtime
   append offset + prefix length are LIVE across positions.

   q1 leading axis is the LITERAL 1, the caches' is the kvar lenVid -- the
   seqQ=1 / seqK=len decode fork that symLeadingQ[q1] (False) does NOT route
   through the symbolic TMultiHeadAttention clause. *)
VerificationTest[
    Module[
        {nCtx = 8, dim = 4, nHeads = 2, dHead = 2, scale, kPast, vPast,
         q1Host, kNewHost, vNewHost, kAll, vAll, hostDecode, runStep},
        scale = 1. / Sqrt[N @ dHead];
        (* distinct known values for every row.  Rows 0..nCtx-2 are candidate
           "past" tokens; the last appended row is kNew/vNew. *)
        kPast[i_]   := Table[N[0.1 (i + 1) - 0.013 c], {c, 0, dim - 1}];
        vPast[i_]   := Table[N[(i + 0.7) + 0.3 c], {c, 0, dim - 1}];
        q1Host      = {Table[N[0.2 - 0.05 c], {c, 0, dim - 1}]};   (* {1,dim} *)
        kNewHost[t_] := {Table[N[0.4 + 0.02 c - 0.01 t], {c, 0, dim - 1}]};
        vNewHost[t_] := {Table[N[9.0 + 0.5 c + 0.1 t], {c, 0, dim - 1}]};
        (* host oracle: single-query multi-head attention over the first t+1
           cached rows (rows 0..t-1 = past, row t = appended new token). *)
        hostDecode[t_] := Module[{kRows, vRows, perHead},
            kRows = Append[Table[kPast[i], {i, 0, t - 1}], kNewHost[t][[1]]];
            vRows = Append[Table[vPast[i], {i, 0, t - 1}], vNewHost[t][[1]]];
            perHead[h_] := Module[{qh, kh, vh, sc, w},
                qh = q1Host[[All, h dHead + 1 ;; (h + 1) dHead]];        (* {1,dHead} *)
                kh = kRows[[All, h dHead + 1 ;; (h + 1) dHead]];        (* {t+1,dHead} *)
                vh = vRows[[All, h dHead + 1 ;; (h + 1) dHead]];        (* {t+1,dHead} *)
                sc = (qh . Transpose[kh]) scale;                       (* {1,t+1} *)
                w  = Map[(Exp[# - Max[#]] / Total[Exp[# - Max[#]]]) &, sc];
                w . vh];                                               (* {1,dHead} *)
            ArrayFlatten[{Table[perHead[h], {h, 0, nHeads - 1}]}]];     (* {1,dim} *)
        (* thvm decode step at position t: a fresh kvar pair, caches pre-filled
           with the t past rows (rows >= t left zero -- only 0..t-1 matter for
           the prefix), then TDecodeAttend appends + attends. *)
        runStep[t_] := Module[
            {posVid, lenVid, kInit, vInit, kCache, vCache, kNewT, vNewT},
            TInit[];
            posVid = TKVarAlloc[1, nCtx];     (* append ROW   = t      *)
            lenVid = TKVarAlloc[1, nCtx];     (* prefix LEN   = t + 1  *)
            (* caches sized {nCtx,dim}; rows 0..t-1 = the past tokens *)
            kInit = Table[If[ i < t, kPast[i], ConstantArray[0., dim]],
                {i, 0, nCtx - 1}];
            vInit = Table[If[ i < t, vPast[i], ConstantArray[0., dim]],
                {i, 0, nCtx - 1}];
            kCache = TRealize[TTensorCreate[N @ kInit]];
            vCache = TRealize[TTensorCreate[N @ vInit]];
            kNewT  = TTensorCreate[N @ kNewHost[t]];
            vNewT  = TTensorCreate[N @ vNewHost[t]];
            TKVarSet[posVid, t];              (* append at row t      *)
            TKVarSet[lenVid, t + 1];          (* attend over t+1 rows *)
            Chop[Normal @ TTensorData @ TRealize @ TDecodeAttend[
                TTensorCreate[N @ q1Host], kCache, vCache, kNewT, vNewT,
                nHeads, posVid, lenVid, scale], 1.*^-6]];
        {Max @ Abs[Flatten[runStep[3] - hostDecode[3]]],
         Max @ Abs[Flatten[runStep[5] - hostDecode[5]]]}
    ],
    {0., 0.},  (* decode output == host oracle at t=3 and t=5 *)
    TestID -> "symbolic/decode-attend-wl",
    SameTest -> (Max @ Abs[#1 - #2] < 1.*^-4 &)
]

(* LEVEL A (decode roadmap Lever 2 step 3): a SINGLE toy transformer block
   processed two ways at a fixed position t over a small prefix, asserting the
   DECODE block output equals the FULL block's LAST row.  This is the
   block-decode ASSEMBLY check -- attention (cache-append + single-query attend)
   + residual + MLP -- without GPT-2.

   Toy block: dim=4, nHeads=2, dHead=2.  Q=K=V=x per head (identity
   projection, as in symbolic/transformer-block-eager); causal multi-head
   self-attention -> residual add -> MLP (x.w1 -> relu -> .w2) -> residual add.
   The new {1,dim} token's k/v are appended into a prefilled cache (rows
   0..t-1 = the past tokens' k/v == the past x rows), the single query attends
   the t+1-row prefix, and the SAME residual + MLP run on the {1,dim} result.

   FULL path: the host-side block oracle over the {t+1,dim} prefix, last row.
   Decode == full's last row at t=3 then t=5 (rebinding posVid + lenVid),
   proving the runtime append offset + prefix length + the assembled block are
   correct across positions. *)
VerificationTest[
    Module[
        {nCtx = 8, dim = 4, nHeads = 2, dHead = 2, dff = 8, scale, xRow, w1, w2,
         relu, hostMHALast, hostBlockLast, decodeBlock},
        scale = 1. / Sqrt[N @ dHead];
        relu  = Max[#, 0.] &;
        (* per-position input rows (the residual stream entering the block) *)
        xRow[i_]   := Table[N[0.15 (i + 1) + 0.02 c - 0.01 i c], {c, 0, dim - 1}];
        w1 = Table[N[0.05 (a - b) + 0.01], {a, 0, dim - 1}, {b, 0, dff - 1}];
        w2 = Table[N[0.03 (a + b) - 0.02], {a, 0, dff - 1}, {b, 0, dim - 1}];
        (* host single-query MHA: the LAST query (row t) attends rows 0..t
           (causal -> all prior + self), per head q.K^T softmax .V, concat *)
        hostMHALast[t_] := Module[{rows, qLast, perHead},
            rows  = Table[xRow[i], {i, 0, t}];                  (* {t+1,dim} *)
            qLast = xRow[t];                                    (* {dim}     *)
            perHead[h_] := Module[{qh, kh, vh, sc, w},
                qh = qLast[[h dHead + 1 ;; (h + 1) dHead]];     (* {dHead}   *)
                kh = rows[[All, h dHead + 1 ;; (h + 1) dHead]]; (* {t+1,dHead} *)
                vh = kh;
                sc = (kh . qh) scale;                           (* {t+1}     *)
                w  = Exp[sc - Max[sc]] / Total[Exp[sc - Max[sc]]];
                w . vh];                                        (* {dHead}   *)
            Flatten[Table[perHead[h], {h, 0, nHeads - 1}]]];    (* {dim}     *)
        (* host FULL block, last row: x_t + attn_t, then + relu MLP *)
        hostBlockLast[t_] := Module[{r1},
            r1 = xRow[t] + hostMHALast[t];
            r1 + (relu /@ (r1 . w1)) . w2];
        (* thvm DECODE block at position t: caches prefilled with rows 0..t-1's
           k/v (== past x rows), TDecodeAttend appends row t's k/v + attends,
           then the SAME residual + MLP over the {1,dim} result. *)
        decodeBlock[t_] := Module[
            {posVid, lenVid, kInit, vInit, kCache, vCache, q1, kNew, vNew, xNew,
             attn, r1, hid, w1T, w2T},
            TInit[];
            posVid = TKVarAlloc[1, nCtx];
            lenVid = TKVarAlloc[1, nCtx];
            (* caches rows 0..t-1 = the past tokens' k/v (== x rows) *)
            kInit = Table[If[ i < t, xRow[i], ConstantArray[0., dim]],
                {i, 0, nCtx - 1}];
            vInit = kInit;
            kCache = TRealize[TTensorCreate[N @ kInit]];
            vCache = TRealize[TTensorCreate[N @ vInit]];
            xNew  = {xRow[t]};                                  (* {1,dim} *)
            q1    = TTensorCreate[N @ xNew];
            kNew  = TTensorCreate[N @ xNew];
            vNew  = TTensorCreate[N @ xNew];
            w1T   = TTensorCreate[N @ w1];
            w2T   = TTensorCreate[N @ w2];
            TKVarSet[posVid, t];
            TKVarSet[lenVid, t + 1];
            attn = TDecodeAttend[q1, kCache, vCache, kNew, vNew, nHeads,
                posVid, lenVid, scale];                         (* {1,dim} *)
            r1   = TRealize @ TUOpAdd[TTensorCreate[N @ xNew], attn];
            hid  = With[{pre = TRealize[r1 . w1T]},
                TRealize @ TUOpMul[pre, TUOpCmplt[TUOpConst[0.], pre]]];
            Chop[Normal @ TTensorData @ TRealize @ TUOpAdd[r1, hid . w2T],
                1.*^-6]];
        {Max @ Abs[Flatten[decodeBlock[3] - {hostBlockLast[3]}]],
         Max @ Abs[Flatten[decodeBlock[5] - {hostBlockLast[5]}]]}
    ],
    {0., 0.},  (* decode block output == full block's last row at t=3 and t=5 *)
    TestID -> "symbolic/decode-block-vs-full-last-row",
    SameTest -> (Max @ Abs[#1 - #2] < 1.*^-4 &)
]

(* Multi-step decode ACCUMULATION (Lever 2 step 4 core): cache persists + grows
   across TDecodeAttend calls.  V_t=t -> out_t = avg(V[0..t]) = t/2. *)
VerificationTest[
    Module[{kC, vC, posVid, lenVid, q},
        TInit[];
        posVid = TKVarAlloc[1, 8]; lenVid = TKVarAlloc[1, 8];
        kC = TRealize[TTensorCreate[ConstantArray[0., {8, 4}]]];
        vC = TRealize[TTensorCreate[ConstantArray[0., {8, 4}]]];
        q  = TTensorCreate[ConstantArray[1., {1, 4}]];
        Table[
            TKVarSet[posVid, t]; TKVarSet[lenVid, t + 1];
            First @ First @ Normal @ TTensorData @ TRealize @ TDecodeAttend[
                q, kC, vC, TTensorCreate[ConstantArray[1., {1, 4}]],
                TTensorCreate[ConstantArray[N[t], {1, 4}]], 2, posVid, lenVid, 1.0],
            {t, 0, 3}]
    ],
    {0., 0.5, 1., 1.5},
    SameTest -> (Max @ Abs[#1 - #2] < 1.*^-4 &),
    TestID -> "symbolic/decode-loop-accumulate"
]

(* STEP 4: the full DECODE LOOP over a toy transformer block, cache built
   INCREMENTALLY from EMPTY (token 0 appends row 0 -- the offset-0 path -- then
   each step reads the accumulated cache + appends its own row), asserting EVERY
   step's output equals the full forward's row at that position.  This is the
   real prefill->decode equivalence: decode step t over a t-row cache == the
   full block's row t, for t = 0..3.  (decode-block-vs-full-last-row PREFILLED
   rows 0..t-1 + appended row t; here the cache is grown only through the decode
   path, so step 0's row-0 append is on the critical path.)  Toy block matches
   symbolic/transformer-block-eager: dim=4, nHeads=2, identity Q=K=V projection,
   causal MHA -> residual -> ReLU MLP -> residual. *)
VerificationTest[
    Module[
        {nCtx = 8, dim = 4, nHeads = 2, dHead = 2, dff = 8, scale, xRow, w1, w2,
         relu, hostBlock, kC, vC, posVid, lenVid, w1T, w2T, decStep},
        scale = 1. / Sqrt[N @ dHead];
        relu  = Max[#, 0.] &;
        xRow[i_] := Table[N[0.15 (i + 1) + 0.02 c - 0.01 i c], {c, 0, dim - 1}];
        w1 = Table[N[0.05 (a - b) + 0.01], {a, 0, dim - 1}, {b, 0, dff - 1}];
        w2 = Table[N[0.03 (a + b) - 0.02], {a, 0, dff - 1}, {b, 0, dim - 1}];
        (* host full-block oracle: row t attends rows 0..t (causal), then the
           same residual + ReLU MLP -- the row the FULL forward produces at t. *)
        hostBlock[t_] := Module[{rows, qLast, perHead, attn, r1},
            rows  = Table[xRow[i], {i, 0, t}];
            qLast = xRow[t];
            perHead[h_] := Module[{qh, kh, sc, w},
                qh = qLast[[h dHead + 1 ;; (h + 1) dHead]];
                kh = rows[[All, h dHead + 1 ;; (h + 1) dHead]];
                sc = (kh . qh) scale;
                w  = Exp[sc - Max[sc]] / Total[Exp[sc - Max[sc]]];
                w . kh];                                         (* V == K *)
            attn = Flatten[Table[perHead[h], {h, 0, nHeads - 1}]];
            r1   = qLast + attn;
            r1 + (relu /@ (r1 . w1)) . w2];
        TInit[];
        posVid = TKVarAlloc[1, nCtx]; lenVid = TKVarAlloc[1, nCtx];
        kC  = TRealize[TTensorCreate[ConstantArray[0., {nCtx, dim}]]];
        vC  = TRealize[TTensorCreate[ConstantArray[0., {nCtx, dim}]]];
        w1T = TTensorCreate[N @ w1]; w2T = TTensorCreate[N @ w2];
        (* one decode step at the CURRENT position t over the growing cache *)
        decStep[t_] := Module[{xNew, attn, r1, hid},
            TKVarSet[posVid, t]; TKVarSet[lenVid, t + 1];
            xNew = {xRow[t]};                                    (* {1,dim} *)
            attn = TDecodeAttend[TTensorCreate[N @ xNew], kC, vC,
                TTensorCreate[N @ xNew], TTensorCreate[N @ xNew],
                nHeads, posVid, lenVid, scale];
            r1   = TRealize @ TUOpAdd[TTensorCreate[N @ xNew], attn];
            hid  = With[{pre = TRealize[r1 . w1T]},
                TRealize @ TUOpMul[pre, TUOpCmplt[TUOpConst[0.], pre]]];
            Chop[Normal @ TTensorData @ TRealize @ TUOpAdd[r1, hid . w2T],
                1.*^-6]];
        Max @ Abs @ Flatten[
            Table[decStep[t], {t, 0, 3}] - Table[{hostBlock[t]}, {t, 0, 3}]]
    ],
    0.,
    SameTest -> (Abs[#1 - #2] < 1.*^-4 &),
    TestID -> "symbolic/decode-loop-vs-full-block"
]

(* STEP 4 session allocation: TDecodeInit[net] sizes the decode state from the
   net STRUCTURE alone (no forward eval) -- one {nCtx, dim} KV cache per
   AttentionLayer the decode fold visits, two position kvars, write position 0.
   A synthetic NetGraph (token embed dim=8/vocab=20, position embed nCtx=5,
   two AttentionLayers) stands in for GPT-2's structure; {count, shape, kvars,
   pos} must match what TDecodeStep consumes.  (End-to-end decode over a REAL
   GPT-2 is the LEVEL B notebook check -- NetModel eval is unavailable here.) *)
VerificationTest[
    Module[{net, st},
        net = Quiet @ NetInitialize @ NetGraph[
            <|"tok" -> EmbeddingLayer[8, 20], "pos" -> EmbeddingLayer[8, 5],
              "a1" -> AttentionLayer[], "a2" -> AttentionLayer[]|>,
            {NetPort["tokIn"] -> "tok", NetPort["posIn"] -> "pos",
             {"tok", "tok", "tok"} -> "a1", {"a1", "a1", "a1"} -> "a2"},
            "tokIn" -> {3, "Integer"}, "posIn" -> {3, "Integer"}];
        st = TDecodeInit[net];
        {Length[st["kCaches"]], Length[st["vCaches"]],
         Dimensions[Normal @ TTensorData @ First @ st["kCaches"]],
         st["posVid"] =!= st["lenVid"], st["pos"]}
    ],
    {2, 2, {5, 8}, True, 0},
    TestID -> "symbolic/decode-init-allocates-caches"
]

(* STEP 5: the decode loop under TJit -- capture ONCE, replay per step rebinding
   the kvars (posVid/lenVid) AND the new token's projected k/v.  kNew/vNew are
   COMPUTED (matmul dispatch outputs), as in the real decode (W_k/W_v
   projections), so they recompute from the rebound per-step inputs on replay;
   the cache-append offset re-resolves from kvar_runtime at fire (the JIT
   offset-rebind) and the eager softmax's host gather re-runs off the live cache
   (the JIT_OP_GATHER replay).  Captured at the loop's MAX prefix (lenVid=4),
   then replayed at lenVid=1..4 -> out_t = avg(V[0..t]) = t/2 = {0,.5,1,1.5}.
   Guards the full decode-under-TJit chain: append offset rebind + cache-read
   regather + kvar loop-bound rebind. *)
VerificationTest[
    Module[{posVid, lenVid, kC, vC, idn, ones, fn},
        TInit[];
        posVid = TKVarAlloc[1, 8]; lenVid = TKVarAlloc[1, 8];
        kC  = TRealize[TTensorCreate[ConstantArray[0., {8, 4}]]];
        vC  = TRealize[TTensorCreate[ConstantArray[0., {8, 4}]]];
        idn = TTensorCreate[N @ IdentityMatrix[4]];
        ones = TTensorCreate[ConstantArray[1., {1, 4}]];
        fn = TJit[{q, kin, vin} |-> Module[{kn, vn},
            kn = TRealize[kin . idn]; vn = TRealize[vin . idn];
            TRealize @ TDecodeAttend[q, kC, vC, kn, vn, 2, posVid, lenVid, 1.0]]];
        TKVarSet[posVid, 3]; TKVarSet[lenVid, 4];     (* capture at the max prefix *)
        fn[ones, ones, TTensorCreate[ConstantArray[0., {1, 4}]]];
        Table[
            TKVarSet[posVid, t]; TKVarSet[lenVid, t + 1];
            First @ First @ Normal @ TTensorData @ fn[ones, ones,
                TTensorCreate[ConstantArray[N[t], {1, 4}]]],
            {t, 0, 3}]
    ],
    {0., 0.5, 1., 1.5},
    SameTest -> (Max @ Abs[#1 - #2] < 1.*^-4 &),
    TestID -> "symbolic/decode-loop-jit-replay"
]
