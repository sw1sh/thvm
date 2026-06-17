(* ::Package:: *)
(* NN/Attention.wl - attention building blocks: scaled dot-product
   (whole-tensor and head-split), multi-head with the KV-cache decode fork,
   causal / padding additive masks, rotary embeddings (interleaved and
   half-split conventions), and GQA head expansion. *)

BeginPackage["WolframInstitute`THVMLink`"];

GeneralUtilities`SetUsage[TAttention, "TAttention[Q$, K$, V$] computes scaled dot-product attention softmax(Q$ @ Transpose[K$] / sqrt(d_k)) @ V$. Q$ is {seq_q, d_k}, K$ is {seq_k, d_k}, V$ is {seq_k, d_v}, giving {seq_q, d_v}. Pure assembly of TMatMul + TSoftmax + TMatMul, the two matmuls dispatching through cblas_sgemm."];
GeneralUtilities`SetUsage[THeadAttention, "THeadAttention[q$, k$, v$, scale$] computes scaled dot-product attention over ALREADY head-split tensors q$, k$, v$ of shape {S, H, D} (heads ride a batched matmul over the leading H axis), returning {S, H*D}. The model applies per-head RMSNorm / RoPE / GQA-expansion before calling this (so TMultiHeadAttention's own head split cannot be reused). scores are q$ @ Transpose[k$] * scale$ softmaxed over keys.
THeadAttention[q$, k$, v$, scale$, addMask$] adds the additive bias addMask$ ({S, S_k}, e.g. a causal+padding mask) to the scores before softmax."];
GeneralUtilities`SetUsage[TMultiHeadAttention, "TMultiHeadAttention[Q$, K$, V$, nHeads$, mask$] computes multi-head scaled dot-product attention. Q$ is {seq_q, dim}; K$, V$ are {seq_k, dim} (read independently, so seq_k may differ for cross-attention or a single-query step over a KV cache); dim = nHeads$ * d_head; mask$ is a {seq_q, seq_k} additive bias (use TCausalMask) or None. Each projection is split to {nHeads$, seq, d_head}, scaled-dot per head, concatenated back to {seq_q, dim}.
TMultiHeadAttention[Q$, K$, V$, nHeads$, mask$, scale$] uses an explicit scale$ in place of the default 1/Sqrt[d_head] (pass 1.0 when Q$ is already pre-scaled, as GPT-2's NetGraph does)."];
GeneralUtilities`SetUsage[TDecodeAttend, "TDecodeAttend[q1$, kCache$, vCache$, kNew$, vNew$, nHeads$, posVid$, lenVid$, scale$] is GPT-2 per-step decode attention over a KV cache. It appends kNew$/vNew$ into kCache$/vCache$ at row posVid$, marks the {len, dim} prefix symbolic on lenVid$, and attends the single query q1$ ({1, dim}) over the cached prefix multi-head, returning {1, dim}. See the public symbol page."];
GeneralUtilities`SetUsage[TCausalMask, "TCausalMask[seq$] returns a {seq$, seq$} TTerm whose entries are 0 at (i, j) with j <= i and -1e9 at j > i. Add it to attention scores before softmax to zero out future-token attention."];
GeneralUtilities`SetUsage[TCausalMaskSym, "TCausalMaskSym[vid$, hi$] returns the symbolic-sequence causal mask: a {S, S} TTerm (S the kvar dimension vid$, upper bound hi$) with 0 at (i, j) for j <= i and a large negative bias at j > i. Built eagerly from a symbolic ramp so a symbolic-seq TMultiHeadAttention can add it to scores before softmax. Use in place of TCausalMask[seq] when the sequence axis is a kvar, passing the result as TMultiHeadAttention's mask argument."];
GeneralUtilities`SetUsage[TPaddingCausalMask, "TPaddingCausalMask[attentionMask$] returns the {S, S} additive causal+padding bias for a host attention mask attentionMask$ (a length-S list, 1 for a real token / 0 for padding): 0 where token i may attend to j (j <= i causal AND attentionMask$[j] real), a large negative bias elsewhere.
TPaddingCausalMask[attentionMask$, neg$] uses neg$ as the masked-out bias (default -1e9)."];
GeneralUtilities`SetUsage[TRoPEInterleaved, "TRoPEInterleaved[x$, cos$, sin$] applies the interleaved rotary embedding (the FLUX.2 DiT convention, use_real_unbind_dim=-1) to x$ {S, H, D}: adjacent pairs (x$[2i], x$[2i+1]) rotate to (-x$[2i+1], x$[2i]); cos$, sin$ are {S, 1, D} broadcast over heads. The Qwen3 text encoder uses TRoPEHalfSplit instead -- the two conventions are NOT interchangeable."];
GeneralUtilities`SetUsage[TRoPEHalfSplit, "TRoPEHalfSplit[x$, cos$, sin$] applies the half-split rotary embedding (the Qwen3 / GPT-NeoX rotate_half convention) to x$ {S, H, D}: rotate_half(x$) = concat[-x$[D/2:], x$[:D/2]], y = x$ cos$ + rotate_half(x$) sin$; cos$, sin$ are {S, 1, D} broadcast over heads. The FLUX DiT uses TRoPEInterleaved instead."];
GeneralUtilities`SetUsage[TRoPEHalfSplitTable, "TRoPEHalfSplitTable[seq$, headDim$, theta$] builds the half-split (NEOX) rotary cos/sin tables for the Qwen3 convention: inv_freq[i] = theta$^(-2i/headDim$), freqs = outer(pos, inv_freq), emb = concat[freqs, freqs]. Returns {cos, sin} each {seq$, 1, headDim$} so they broadcast over the head axis, ready for TRoPEHalfSplit."];
GeneralUtilities`SetUsage[TRepeatKV, "TRepeatKV[x$, rep$] is the grouped-query-attention head expansion: a {S, Hkv, D} tensor whose Hkv kv heads each serve rep$ query heads, broadcast to {S, Hkv*rep$, D} in HF repeat_interleave order (kv head j serves query heads rep$*j .. rep$*j+rep$-1). Insert a unit axis, expand it to rep$, fold back so the repeats land contiguously per kv head."];

Begin["`Private`"];

(* === whole-tensor scaled-dot attention =================================== *)

(* Idiomatic: `q . Transpose[k]` lowers via the Dot + Transpose
   UpValues; `SoftmaxLayer[2][...]` lowers via the Layer-call
   UpValue installed in Tensor.wl; `/ Sqrt[N @ dk]` and `... . v`
   ride the Times / Power / Dot UpValues.  Reads as the textbook
   formula. *)
TAttention[q_TTerm, k_TTerm, v_TTerm] := With[{dk = tUopShape[k][[2]]},
    SoftmaxLayer[2][(q . Transpose[k]) / Sqrt[N @ dk]] . v]

(* === batched matmul over a head / batch axis ============================= *)

(* {s, dim} -> {s, nHeads, dHead} -> {nHeads, s, dHead}: heads become a leading
   BATCH axis.  Transpose with a 1-indexed perm via the WL UpValue. *)
mhaHeadView[t_, s_, nHeads_, dHead_] := Transpose[ArrayReshape[t, {s, nHeads, dHead}], {2, 1, 3}]

(* Batched matmul A:{B,M,K} . Bm:{B,K,N} -> {B,M,N} as MUL + REDUCE over K.
   thvm has no batched-Dot primitive (TMatMul is rank-2), so this is TMatMul's
   own RESHAPE+EXPAND+MUL+REDUCE pattern with a leading batch axis prepended;
   ONE reduce kernel for all B batches. *)
mhaBmm[a_, bm_, bb_, m_, kk_, nn_] := TUOpReduce[
    TUOpMul[
        TUOpExpand[TUOpReshape[a,  {bb, m, kk, 1}], {bb, m, kk, nn}],
        TUOpExpand[TUOpReshape[bm, {bb, 1, kk, nn}], {bb, m, kk, nn}]],
    2, "SUM"]

(* M-major batched matmul: A:{B,M,K} . Bm:{B,K,N} -> {M,B,N} (batch axis in the
   MIDDLE, not leading).  Used for the attention @V so the context lands as
   {seqQ, nHeads, dHead}: the merge to {seqQ, dim} is then a CONTIGUOUS reshape,
   not a {nHeads,seqQ,dHead}->transpose->{seqQ,dim} non-mergeable reshape (the
   latter lowers to an IDIV/IMOD view that the downstream output-projection
   matmul reads -- a non-affine operand cblas/simdgroup both reject, forcing a
   scalar reduce: 12x140ms on the GPT-2 CPU forward).  A is read transposed
   (affine strided), so the bmm itself still routes through the batched-gemm
   BLAS classifier. *)
mhaBmmM[a_, bm_, bb_, m_, kk_, nn_] := TUOpReduce[
    TUOpMul[
        TUOpExpand[TUOpReshape[Transpose[a, {2, 1, 3}], {m, bb, kk, 1}], {m, bb, kk, nn}],
        TUOpExpand[TUOpReshape[bm, {1, bb, kk, nn}], {m, bb, kk, nn}]],
    2, "SUM"]

(* === head-split scaled-dot attention ===================================== *)

(* THeadAttention[q, k, v, scale, addMask]: scaled-dot attention over
   ALREADY head-split q/k/v {S, H, D} (the model applied per-head RMSNorm /
   RoPE / GQA-expansion first, so TMultiHeadAttention's own head split can't
   be reused).  Heads ride the leading batch axis: ONE batched reduce per
   matmul.  addMask is an additive {S, S_k} bias broadcast over heads, or
   None for unmasked. *)
THeadAttention[q_TTerm, k_TTerm, v_TTerm, scale_ ? NumericQ] :=
    THeadAttention[q, k, v, scale, None]
THeadAttention[q_TTerm, k_TTerm, v_TTerm, scale_ ? NumericQ, addMask_] := Module[
    {sq, h, dh, sk, qh, kh, vh, scores, scoresM, attn, out},
    {sq, h, dh} = tUopShape[q];
    sk = tUopShape[k][[1]];
    qh = Transpose[q, {2, 1, 3}];  kh = Transpose[k, {2, 1, 3}];  vh = Transpose[v, {2, 1, 3}];
    scores  = mhaBmm[qh, Transpose[kh, {1, 3, 2}], h, sq, dh, sk] * scale;
    scoresM = If[ addMask === None, scores,
        scores + TUOpExpand[ArrayReshape[addMask, {1, sq, sk}], {h, sq, sk}]];
    attn = TSoftmax[scoresM, 2];
    out  = mhaBmm[attn, vh, h, sq, sk, dh];
    ArrayReshape[Transpose[out, {2, 1, 3}], {sq, h * dh}]]

(* === rotary embeddings =================================================== *)

(* TRoPEInterleaved[x, cos, sin]: the FLUX.2 DiT convention
   (use_real_unbind_dim=-1).  x {S,H,D}; cos,sin {S,1,D}; adjacent pairs
   (x[2i], x[2i+1]) rotate to (-x[2i+1], x[2i]).  Distinct from the Qwen3
   half-split TRoPEHalfSplit, not interchangeable. *)
TRoPEInterleaved[x_TTerm, cos_TTerm, sin_TTerm] := Module[{s, h, d, xr, xe, xo, xrot},
    {s, h, d} = tUopShape[x];
    xr   = TUOpReshape[x, {s, h, d/2, 2}];
    xe   = TUOpReshape[xr[[All, All, All, 1]], {s, h, d/2}];   (* even lanes *)
    xo   = TUOpReshape[xr[[All, All, All, 2]], {s, h, d/2}];   (* odd lanes *)
    xrot = TUOpReshape[
        Join[TUOpReshape[-xo, {s, h, d/2, 1}], TUOpReshape[xe, {s, h, d/2, 1}], 4],
        {s, h, d}];
    x * TUOpExpand[cos, {s, h, d}] + xrot * TUOpExpand[sin, {s, h, d}]]

(* TRoPEHalfSplit[x, cos, sin]: the Qwen3 / GPT-NeoX rotate_half convention.
   x {S,H,D}; cos,sin {S,1,D}; rotate_half(x) = concat[-x[D/2:], x[:D/2]];
   y = x cos + rotate_half(x) sin.  Distinct from the interleaved
   TRoPEInterleaved. *)
TRoPEHalfSplit[x_TTerm, cos_TTerm, sin_TTerm] := Module[{s, h, d, half, lo, hi, rot},
    {s, h, d} = tUopShape[x];
    half = d/2;
    lo  = x[[All, All, 1 ;; half]];
    hi  = x[[All, All, half + 1 ;; d]];
    rot = Join[-hi, lo, 3];
    x * TUOpExpand[cos, {s, h, d}] + rot * TUOpExpand[sin, {s, h, d}]]

(* TRoPEHalfSplitTable[seq, headDim, theta]: host-side cos/sin for the
   half-split convention: inv_freq[i] = theta^(-2i/headDim);
   freqs = outer(pos, inv_freq); emb = concat[freqs, freqs].  Returns {cos, sin}
   each {seq, 1, headDim} so they broadcast over the head axis. *)
TRoPEHalfSplitTable[seq_Integer, headDim_Integer, theta_ ? NumericQ] := Module[
    {half, invFreq, emb},
    half = headDim/2;
    invFreq = Table[N[theta]^(-2 i/headDim), {i, 0, half - 1}];
    emb = Table[Join[#, #] &[p invFreq], {p, 0, seq - 1}];
    {
        ArrayReshape[TTensorCreate[N[Cos[emb]]], {seq, 1, headDim}],
        ArrayReshape[TTensorCreate[N[Sin[emb]]], {seq, 1, headDim}]
    }
]

(* === grouped-query head expansion ======================================== *)

(* TRepeatKV[x, rep]: GQA head expansion of a {S, Hkv, D} tensor whose Hkv kv
   heads each serve `rep` query heads, broadcast to {S, Hkv*rep, D} in HF
   repeat_interleave order (kv head j serves query heads rep*j .. rep*j+rep-1).
   Insert a unit axis, EXPAND it to rep, fold back: the repeats land
   contiguously per kv head. *)
TRepeatKV[x_TTerm, rep_Integer] := Module[{s, hkv, d},
    {s, hkv, d} = tUopShape[x];
    ArrayReshape[
        TUOpExpand[ArrayReshape[x, {s, hkv, 1, d}], {s, hkv, rep, d}],
        {s, hkv*rep, d}]
]

(* === causal / padding masks ============================================== *)

TCausalMask[seq_Integer] := TTensorCreate @ NumericArray[
    Table[ If[ j <= i, 0.0, -1.0*^9], {i, seq}, {j, seq}],
    "Real32"]

(* TPaddingCausalMask[attentionMask, neg]: the {S, S} additive bias for a
   host attention mask (1 real / 0 pad): 0 where token i may attend to j
   (j <= i causal AND attentionMask[j] real), a large negative elsewhere. *)
TPaddingCausalMask[attentionMask_List, neg_ : -1.*^9] := With[{s = Length[attentionMask]},
    TTensorCreate[N @ Table[
        If[ j <= i && attentionMask[[j]] == 1, 0., neg], {i, s}, {j, s}]]]

(* A kvar-PACKED shape dim is 2^31 + vid: the leading axis of a
   symbolic-sequence tensor reads back >= 2^31 from tUopShape.  This is
   the discriminator the symbolic-seq attention branch keys off. *)
$kvarPackBase = 2^31
symDimQ[d_]   := IntegerQ[d] && d >= $kvarPackBase
symVid[d_]    := d - $kvarPackBase            (* recover vid from a packed dim *)

(* True when a TTerm's leading axis is a kvar-packed (symbolic) dim.
   Safe on a $Failed shape -- symLeadingQ short-circuits to False rather
   than indexing into $Failed -- so it can guard the symbolic dispatch. *)
symLeadingQ[t_TTerm] := With[{s = tUopShape[t]},
    ListQ[s] && Length[s] >= 1 && symDimQ[First[s]]]

(* The kvar-PACKED extent for symbolic dim vid (KVAR_FLAG | vid in C): pass it
   as a SHRINK bound / reshape dim so the movement op carries the symbolic
   axis (view_apply_shrink decodes a packed bound via kvar_extent_runtime). *)
TKVarPack[vid_Integer] := $kvarPackBase + vid

(* TCausalMaskSym[vid, hi] -- the symbolic-sequence causal mask, built
   EAGER from a symbolic ramp (the validated construction in
   symbolic.wlt's "symbolic/causal-attention-eager").  Each reduce /
   broadcast-EXPAND is realized to a contiguous leaf before the next op
   consumes it, so the doubly-symbolic {S,S} fused-broadcast addressing
   never arises.  mask[i,j] = 0 for j <= i, -30 for j > i. *)
TCausalMaskSym[vid_Integer, hi_Integer] := Module[
    {sym, er, ramp, ri, rj},
    sym  = $kvarPackBase + vid;
    er[t_] := TRealize[t];
    ramp = TSymbolicAxis[TTensorCreate[N @ Range[0, hi - 1]], 0, vid];
    ri   = er @ TUOpExpand[TUOpReshape[ramp, {sym, 1}], {sym, sym}];
    rj   = er @ TUOpExpand[TUOpReshape[ramp, {1, sym}], {sym, sym}];
    er @ TUOpMul[
        TUOpCmplt[TUOpAdd[ri, TUOpNeg[rj]], TUOpConst[0.]], TUOpConst[-30.]]
]

(* === KV-cache decode attention =========================================== *)

(* TAppendAt[cache, src, posVid] -- the in-place KV-cache APPEND: write `src`
   (a {1, ...} row) into `cache` ({nCtx, ...}) at the runtime row offset bound
   to kvar `posVid`, leaving other rows untouched.  An ASSIGN whose dst is a
   SHRUNK view of the cache at the kvar offset (tinygrad's
   `cache[..., start_pos:start_pos+1, ...] = src`); the C mechanism is the
   validated tests/test_sym_kvcache_append.c path (view_apply_shrink decodes
   the kvar begin, the offset-aware buf write lands at row t).  `cache`'s
   buffer is mutated in place, so callers holding it observe the write;
   returns `cache`. *)
TAppendAt[cache_TTerm, src_TTerm, posVid_Integer] := Module[
    {p = TKVarPack[posVid], tailRanges},
    tailRanges = (d |-> {0, d}) /@ Rest[tUopShape[cache]];
    TRealize[TAssign[TUOpShrink[cache, Join[{{p, p + 1}}, tailRanges]], src]];
    cache]

(* TDecodeAttend -- GPT-2's per-step DECODE attention over a KV cache (tinygrad
   gpt2.py with T=1, start_pos driving the append + prefix length).  A single new
   query q1 ({1, dim}) attends a symbolic-length cached K/V prefix.  Two kvars:
   `posVid` (the append ROW, runtime = pos) and `lenVid` (the cached PREFIX
   LENGTH, runtime = pos + 1).  This is JUST `TMultiHeadAttention` at seqQ == 1
   (literal) over a symbolic seqK == len, with no causal mask (a single new query
   attends every cached key, all causally prior -- tinygrad's T=1 mask=None):
     1. APPEND kNew/vNew ({1, dim}) into the {nCtx, dim} caches at row posVid.
     2. MARK the {len, dim} cached prefix symbolic on lenVid.
     3. Attend q1 over the symbolic prefix via the unified TMultiHeadAttention.
   The per-head output's row axis is the literal 1 (the symbolic key axis is
   reduced away by the softmax), so headStitch's PAD is over a literal axis and
   no per-head realize is needed. *)
TDecodeAttend[q1_TTerm, kCache_TTerm, vCache_TTerm, kNew_TTerm, vNew_TTerm,
              nHeads_Integer, posVid_Integer, lenVid_Integer,
              scale_ ? NumericQ] := (
    TAppendAt[kCache, kNew, posVid];
    TAppendAt[vCache, vNew, posVid];
    TMultiHeadAttention[q1, TSymbolicAxis[kCache, 0, lenVid],
        TSymbolicAxis[vCache, 0, lenVid], nHeads, None, scale])

(* === multi-head attention (full projection split) ======================== *)

TMultiHeadAttention[q_TTerm, k_TTerm, v_TTerm,
                    nHeads_Integer, mask_ : None] := With[{
    dHead = tUopShape[q][[2]] / nHeads
},
    TMultiHeadAttention[q, k, v, nHeads, mask, 1 / Sqrt[N @ dHead]]
]

(* Multi-head attention, one clause for every sequence regime.  Q,K,V are
   {seq, dim}; K/V carry their own length, so seqK may differ from seqQ (cross-
   attention, and the single-query decode step over a KV cache).  `mask` is an
   additive {seqQ, seqK} bias (TCausalMask / TCausalMaskSym) or None.

   The seq axes may be LITERAL or kvar-PACKED (symbolic length S) -- ONE fully
   lazy body either way, no eager realize, no special-casing.  `ArrayReshape` /
   `Transpose` / `TUOpShrink` / `Dot` / `TSoftmax` / `headStitch` index a
   symbolic axis correctly by relying on two engine behaviors: view_apply_shrink
   sizes numel at the kvar hi bound (so the per-head split addresses correctly)
   and the BLAS GEMM dispatch writes the QK^T scores at the output's hi row
   stride, not the runtime N. *)
TMultiHeadAttention[q_TTerm, k_TTerm, v_TTerm,
                    nHeads_Integer, mask_, scale_ ? NumericQ] := With[{
    shapeQ = tUopShape[q],
    shapeK = tUopShape[k]
},
    Module[{seqQ, seqK, dim, dHead},
        seqQ  = shapeQ[[1]];
        seqK  = shapeK[[1]];      (* K / V carry their own length: seqK may
                                     differ from seqQ for cross-attention and
                                     for a single-query step over a KV cache. *)
        dim   = shapeQ[[2]];
        dHead = dim / nHeads;
        If[ symLeadingQ[q] || symLeadingQ[k],
            (* SYMBOLIC seq (the KV-cache decode step): per-head loop.  The kvar
               axis flows through a per-head TUOpShrink; the batched rank-4
               EXPAND/REDUCE over a symbolic axis is not yet validated, so the
               literal fast path below is gated off here. *)
            Module[{sliceHead, perHead},
                sliceHead[hView_, h_, s_] := ArrayReshape[
                    TUOpShrink[hView, {{h, h + 1}, {0, s}, {0, dHead}}], {s, dHead}];
                perHead[h_] := With[{
                    qS = sliceHead[mhaHeadView[q, seqQ, nHeads, dHead], h, seqQ],
                    kS = sliceHead[mhaHeadView[k, seqK, nHeads, dHead], h, seqK],
                    vS = sliceHead[mhaHeadView[v, seqK, nHeads, dHead], h, seqK]
                },
                    Module[{scores, scoresM},
                        scores  = (qS . Transpose[kS]) * scale;
                        scoresM = If[ mask === None, scores, scores + mask];
                        TSoftmax[scoresM, 1] . vS]];
                headStitch[perHead /@ Range[0, nHeads - 1], nHeads, dHead]],
            (* LITERAL seq: ONE batched scaled-dot over the head axis (~8 kernels)
               instead of nHeads per-head chains + a PAD/Total stitch (~8*nHeads).
               Heads are the leading batch axis; token-identical to the per-head
               path (verified err ~3e-8). *)
            Module[{qh, kh, vh, scores, scoresM, ctx},
                qh = mhaHeadView[q, seqQ, nHeads, dHead];   (* {nHeads, seqQ, dHead} *)
                kh = mhaHeadView[k, seqK, nHeads, dHead];
                vh = mhaHeadView[v, seqK, nHeads, dHead];
                scores = mhaBmm[qh, Transpose[kh, {1, 3, 2}], nHeads, seqQ, dHead, seqK] * scale;
                scoresM = If[ mask === None, scores,
                    scores + TUOpExpand[TUOpReshape[mask, {1, seqQ, seqK}], {nHeads, seqQ, seqK}]];
                ctx = mhaBmmM[TSoftmax[scoresM, 2], vh, nHeads, seqQ, seqK, dHead]; (* {seqQ, nHeads, dHead} *)
                ArrayReshape[ctx, {seqQ, dim}]]]
    ]
]

(* Stitch a list of nHeads {seq_q, dHead} TTerms into {seq_q, dim=
   nHeads*dHead} via per-head PAD into the corresponding column
   slice + sum.  No STACK op in thvm.  The row (seq_q) axis is left
   untouched, so the stitch needs only nHeads and dHead. *)
headStitch[heads_List, nHeads_, dHead_] := Total @ Table[
    TUOpPad[heads[[h + 1]],
        {{0, 0}, {h * dHead, (nHeads - h - 1) * dHead}}],
    {h, 0, nHeads - 1}]

End[];

EndPackage[];
