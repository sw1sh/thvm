(* FluxForward.wl -- a compact FLUX.2-klein-4B forward for thvm.

   The tinygrad `examples/stable_diffusion.py` shape: read the named bf16
   tensors out of the diffusers safetensors and wire ops, looped over the
   block config -- NOT a hand-exported per-block NetGraph.  Get-loaded
   after THVMLink` (uses the public TUOp* surface + one alias to the lazy
   shape).  Architecture + tensor names: docs/flux_forward_spec.md.

   This file holds the verified primitive helpers; the double/single block
   factory + the full transformer forward build on top of them. *)

(* The lazy (graph-time) shape; public TUOp* builders below need it but it
   lives in THVMLink`Private`. *)
fxShape = THVMLink`Private`tUopShape;

(* --- linear: a diffusers weight is stored {out, in}, so y = x . W^T.  The
       weight reaches us as a lazy bf16->f32 TUOpCast, which cblas can't read;
       left un-realized, TMatMul fuses cast+transpose into a scalar
       EXPAND-MUL-REDUCE with a multi-GB intermediate (the FLUX ff matmuls take
       minutes that way).  Realize the CAST to a contiguous {out, in} f32
       buffer; Transpose is then a pure view that cblas_sgemm consumes as transB
       -- no transpose shuffle (~40% faster than realizing the transposed
       layout).  (Weights are constants -- a full forward should cache the f32
       cast once; per-call realize is fine for a single block.) --- *)
fxLinear[x_, w_] := TMatMul[x, Transpose[TRealize[w]]]

(* --- RMSNorm over the last axis (FLUX per-head q/k norm, weight {D}):
       y = x * rsqrt(mean(x^2) + eps) * weight --- *)
fxRMSNorm[x_, weight_, eps_] := Module[{s, nd, d, ms, inv, invB, wB},
    s = fxShape[x];  nd = Length[s];  d = Last[s];
    ms   = TUOpReduce[TUOpMul[x, x], nd - 1, "SUM"];          (* drops last -> rank nd-1 *)
    inv  = TUOpRecip[TUOpSqrt[TUOpAdd[TUOpMul[ms, TUOpConst[N[1./d]]], TUOpConst[N[eps]]]]];
    invB = TUOpExpand[TUOpReshape[inv, Append[Most[s], 1]], s];
    wB   = TUOpExpand[TUOpReshape[weight, Join[ConstantArray[1, nd - 1], {d}]], s];
    TUOpMul[TUOpMul[x, invB], wB]]

(* --- affine-free LayerNorm over the last axis (FLUX block norms use
       elementwise_affine=False; the modulation supplies scale/shift):
       y = (x - mean) * rsqrt(var + eps) --- *)
fxLayerNorm[x_, eps_] := Module[{s, nd, d, mu, muB, xc, var, inv, invB},
    s = fxShape[x];  nd = Length[s];  d = Last[s];
    mu   = TUOpMul[TUOpReduce[x, nd - 1, "SUM"], TUOpConst[N[1./d]]];
    muB  = TUOpExpand[TUOpReshape[mu, Append[Most[s], 1]], s];
    xc   = TUOpAdd[x, TUOpNeg[muB]];
    var  = TUOpMul[TUOpReduce[TUOpMul[xc, xc], nd - 1, "SUM"], TUOpConst[N[1./d]]];
    inv  = TUOpRecip[TUOpSqrt[TUOpAdd[var, TUOpConst[N[eps]]]]];
    invB = TUOpExpand[TUOpReshape[inv, Append[Most[s], 1]], s];
    TUOpMul[xc, invB]]

(* --- concat a list of equal-rank tensors along a 1-indexed axis (thvm has
       no CAT op; place each in its slice via TUOpPad + sum -- the headStitch
       idiom).  Used for joint attention K/V, the RoPE interleave, and QKV
       splits. --- *)
fxConcat[xs_List, axis_] := Module[{rank, widths, offsets, total},
    rank    = Length @ fxShape[First[xs]];
    widths  = fxShape[#][[axis]] & /@ xs;
    offsets = Prepend[Accumulate[Most[widths]], 0];
    total   = Total[widths];
    Fold[TUOpAdd, MapThread[{t, off, w} |-> TUOpPad[t,
        Table[If[a === axis, {off, total - off - w}, {0, 0}], {a, rank}]],
        {xs, offsets, widths}]]]

(* --- interleaved rotary embedding (the FLUX.2 DiT convention,
       use_real_unbind_dim=-1): x{S,H,D}; cos,sin{S,1,D}; pairs (x[2i],x[2i+1])
       rotate to (-x[2i+1], x[2i]).  Validated to 8.5e-8 vs the oracle.
       The Qwen3 text encoder uses the DIFFERENT half-split convention -- do
       not reuse this there. --- *)
fxRoPE[x_, cos_, sin_] := Module[{s, h, d, xr, xe, xo, xrot},
    {s, h, d} = fxShape[x];
    xr   = TUOpReshape[x, {s, h, d/2, 2}];
    xe   = TUOpReshape[TUOpShrink[xr, {{0, s}, {0, h}, {0, d/2}, {0, 1}}], {s, h, d/2}];
    xo   = TUOpReshape[TUOpShrink[xr, {{0, s}, {0, h}, {0, d/2}, {1, 2}}], {s, h, d/2}];
    xrot = TUOpReshape[fxConcat[{TUOpReshape[TUOpNeg[xo], {s, h, d/2, 1}],
                                 TUOpReshape[xe,          {s, h, d/2, 1}]}, 4], {s, h, d}];
    TUOpAdd[TUOpMul[x, TUOpExpand[cos, {s, h, d}]], TUOpMul[xrot, TUOpExpand[sin, {s, h, d}]]]]

(* --- exp / sigmoid / SiLU via explicit ops (the SwiGLU MLP gate) --- *)
fxExp[x_]     := TUOpExp2[TUOpMul[x, TUOpConst[N[Log2[E]]]]]
fxSigmoid[x_] := TUOpRecip[TUOpAdd[fxExp[TUOpNeg[x]], TUOpConst[1.]]]
fxSiLU[x_]    := TUOpMul[x, fxSigmoid[x]]

(* --- batched matmul A{B,M,K} . Bm{B,K,N} -> {B,M,N} (the mhaBmm pattern:
       RESHAPE+EXPAND+MUL+REDUCE over K, one reduce kernel for all B). --- *)
fxBmm[a_, bm_, bb_, m_, kk_, nn_] := TUOpReduce[
    TUOpMul[TUOpExpand[TUOpReshape[a,  {bb, m, kk, 1}], {bb, m, kk, nn}],
            TUOpExpand[TUOpReshape[bm, {bb, 1, kk, nn}], {bb, m, kk, nn}]], 2, "SUM"]

(* --- scaled-dot attention over ALREADY-head-split + RoPE'd q,k,v {S,H,Dh}
       (FLUX applies per-head RMSNorm + RoPE before the dot, so the all-in-one
       TMultiHeadAttention can't be used).  Heads -> leading batch axis. --- *)
fxAttention[q_, k_, v_, scale_] := Module[{sq, h, dh, sk, qh, kh, vh, scores, attn, out},
    {sq, h, dh} = fxShape[q];  sk = fxShape[k][[1]];
    qh = Transpose[q, {2, 1, 3}];  kh = Transpose[k, {2, 1, 3}];  vh = Transpose[v, {2, 1, 3}];
    scores = TUOpMul[fxBmm[qh, Transpose[kh, {1, 3, 2}], h, sq, dh, sk], TUOpConst[N[scale]]];
    attn   = TSoftmax[scores, 2];
    out    = fxBmm[attn, vh, h, sq, sk, dh];
    TUOpReshape[Transpose[out, {2, 1, 3}], {sq, h*dh}]]

(* --- AdaLN modulation: (1 + scale) * LayerNorm(x) + shift; scale,shift {dim}
       broadcast over the sequence axis (the mod vectors are shared per block). --- *)
fxModulate[x_, shift_, scale_, eps_] := Module[{s, d, ln, scB, shB},
    s = fxShape[x];  d = Last[s];
    ln  = fxLayerNorm[x, eps];
    scB = TUOpExpand[TUOpReshape[scale, {1, d}], s];
    shB = TUOpExpand[TUOpReshape[shift, {1, d}], s];
    TUOpAdd[TUOpMul[ln, TUOpAdd[scB, TUOpConst[1.]]], shB]]

(* --- gated residual: x + gate * y; gate {dim} broadcast over the seq axis. --- *)
fxGateAdd[x_, gate_, y_] := With[{s = fxShape[y]},
    TUOpAdd[x, TUOpMul[TUOpExpand[TUOpReshape[gate, {1, Last[s]}], s], y]]]

(* --- SwiGLU activation only: {S, 2*mlp} -> chunk -> SiLU(x1)*x2 -> {S, mlp}
       (Flux2SwiGLU; the single block fuses linear_in into to_qkv_mlp_proj and
       linear_out into to_out, so it needs the bare activation). --- *)
fxSwiGLUact[h_] := Module[{s, m2, m, x1, x2},
    {s, m2} = fxShape[h];  m = m2/2;
    x1 = TUOpShrink[h, {{0, s}, {0, m}}];
    x2 = TUOpShrink[h, {{0, s}, {m, m2}}];
    TUOpMul[fxSiLU[x1], x2]]

(* --- SwiGLU MLP with its own linear_in {2*mlp, dim} + linear_out (double block). --- *)
fxSwiGLU[x_, wIn_, wOut_] := fxLinear[fxSwiGLUact[fxLinear[x, wIn]], wOut]

(* --- DOUBLE-stream block (MMDiT).  img0/txt0 {Simg/Stxt, dim}; mods is the
       per-block modulation vectors (post-SiLU-Linear) keyed
       {img,txt}_{shift,scale,gate}_{msa,mlp}; ropeCos/Sin {Stxt+Simg, dim} in
       [txt; img] row order; W the block weights (diffusers names); cfg has
       heads/head_dim/eps.  Op-order per the diffusers Flux2TransformerBlock:
       modulate -> separate img/txt QKV -> per-head RMSNorm(q,k) -> text-first
       joint concat -> RoPE after concat -> attention -> out-proj -> gated
       residual -> SwiGLU MLP -> gated residual.

       Returns {img, txt}.  The two streams share the joint-attention sub-DAG
       (concat Q/K/V -> RoPE -> attention -> softmax), so realize them TOGETHER
       -- TRealize[{img, txt}] -- not per root.  A per-root realize re-lifts the
       shared chain into a duplicate kernel set (94 vs 58 kernels/block; same
       output). --- *)
fxDoubleBlock[img0_, txt0_, mods_, ropeCos_, ropeSin_, W_, cfg_] := Module[
    {h, dh, eps, scale, simg, stxt, dim, imgN, txtN, qi, ki, vi, qt, kt, vt,
     Q, K, V, rc, rs, ctx, ctxT, ctxI, img, txt, imgN2, txtN2},
    h = cfg["heads"];  dh = cfg["head_dim"];  eps = cfg["eps"];  scale = 1/Sqrt[N[dh]];
    simg = fxShape[img0][[1]];  stxt = fxShape[txt0][[1]];  dim = h*dh;
    (* attention: modulate, project, per-head RMSNorm (v unnormed) *)
    imgN = fxModulate[img0, mods["img_shift_msa"], mods["img_scale_msa"], eps];
    txtN = fxModulate[txt0, mods["txt_shift_msa"], mods["txt_scale_msa"], eps];
    qi = fxRMSNorm[TUOpReshape[fxLinear[imgN, W["to_q"]], {simg, h, dh}], W["norm_q"], eps];
    ki = fxRMSNorm[TUOpReshape[fxLinear[imgN, W["to_k"]], {simg, h, dh}], W["norm_k"], eps];
    vi = TUOpReshape[fxLinear[imgN, W["to_v"]], {simg, h, dh}];
    qt = fxRMSNorm[TUOpReshape[fxLinear[txtN, W["add_q_proj"]], {stxt, h, dh}], W["norm_added_q"], eps];
    kt = fxRMSNorm[TUOpReshape[fxLinear[txtN, W["add_k_proj"]], {stxt, h, dh}], W["norm_added_k"], eps];
    vt = TUOpReshape[fxLinear[txtN, W["add_v_proj"]], {stxt, h, dh}];
    (* text-first joint concat on the seq axis (axis 1), then RoPE *)
    Q = fxConcat[{qt, qi}, 1];  K = fxConcat[{kt, ki}, 1];  V = fxConcat[{vt, vi}, 1];
    rc = TUOpReshape[ropeCos, {stxt + simg, 1, dh}];  rs = TUOpReshape[ropeSin, {stxt + simg, 1, dh}];
    Q = fxRoPE[Q, rc, rs];  K = fxRoPE[K, rc, rs];
    ctx  = fxAttention[Q, K, V, scale];                  (* {Stxt+Simg, dim} *)
    ctxT = TUOpShrink[ctx, {{0, stxt}, {0, dim}}];
    ctxI = TUOpShrink[ctx, {{stxt, stxt + simg}, {0, dim}}];
    img = fxGateAdd[img0, mods["img_gate_msa"], fxLinear[ctxI, W["to_out_0"]]];
    txt = fxGateAdd[txt0, mods["txt_gate_msa"], fxLinear[ctxT, W["to_add_out"]]];
    (* SwiGLU MLP per stream *)
    imgN2 = fxModulate[img, mods["img_shift_mlp"], mods["img_scale_mlp"], eps];
    img = fxGateAdd[img, mods["img_gate_mlp"], fxSwiGLU[imgN2, W["ff_linear_in"], W["ff_linear_out"]]];
    txtN2 = fxModulate[txt, mods["txt_shift_mlp"], mods["txt_scale_mlp"], eps];
    txt = fxGateAdd[txt, mods["txt_gate_mlp"], fxSwiGLU[txtN2, W["ffc_linear_in"], W["ffc_linear_out"]]];
    {img, txt}]

(* --- SINGLE-stream block (parallel ViT-22B): the QKV projections are fused
       with the FF input projection (to_qkv_mlp_proj {3*dim + 2*mlp, dim}) and
       the attention output projection is fused with the FF output projection
       (to_out {dim, dim + mlp}).  x0 {S, dim} is the ALREADY [txt; img]-concat
       sequence; mod is one shift/scale/gate; one gated residual.  Op-order per
       diffusers Flux2ParallelSelfAttnProcessor. --- *)
fxSingleBlock[x0_, mod_, ropeCos_, ropeSin_, W_, cfg_] := Module[
    {h, dh, eps, scale, s, dim, qkvw, xn, qkvmlp, qkv, mlp, q, k, v, rc, rs, attn, mlpG},
    h = cfg["heads"];  dh = cfg["head_dim"];  eps = cfg["eps"];  scale = 1/Sqrt[N[dh]];
    s = fxShape[x0][[1]];  dim = h*dh;
    xn     = fxModulate[x0, mod["shift"], mod["scale"], eps];
    (* realize the fused projection: qkv + mlp are both shrinks of it, so the
       attention and MLP paths would otherwise each re-lift this big matmul into
       the final fused concat+to_out matmul -- the deepest DAG in the model,
       which overflows the recursive lift walk.  A leaf read cuts both paths. *)
    qkvmlp = TRealize @ fxLinear[xn, W["to_qkv_mlp_proj"]];        (* {S, 3*dim + 2*mlp} *)
    qkvw   = fxShape[qkvmlp][[2]];
    qkv    = TUOpShrink[qkvmlp, {{0, s}, {0, 3 dim}}];            (* {S, 3*dim} *)
    mlp    = TUOpShrink[qkvmlp, {{0, s}, {3 dim, qkvw}}];         (* {S, 2*mlp} *)
    q = fxRMSNorm[TUOpReshape[TUOpShrink[qkv, {{0, s}, {0, dim}}],      {s, h, dh}], W["norm_q"], eps];
    k = fxRMSNorm[TUOpReshape[TUOpShrink[qkv, {{0, s}, {dim, 2 dim}}],  {s, h, dh}], W["norm_k"], eps];
    v =          TUOpReshape[TUOpShrink[qkv, {{0, s}, {2 dim, 3 dim}}], {s, h, dh}];
    rc = TUOpReshape[ropeCos, {s, 1, dh}];  rs = TUOpReshape[ropeSin, {s, 1, dh}];
    q = fxRoPE[q, rc, rs];  k = fxRoPE[k, rc, rs];
    attn = fxAttention[q, k, v, scale];                          (* {S, dim} *)
    mlpG = fxSwiGLUact[mlp];                                     (* {S, mlp} *)
    (* realize the joined attn|mlp before to_out: it feeds the final matmul's
       contraction operand, so a leaf read keeps that lift shallow. *)
    fxGateAdd[x0, mod["gate"], fxLinear[TRealize @ fxConcat[{attn, mlpG}, 2], W["to_out"]]]]

(* ============================================================
   Full transformer forward (loop 5 double + 20 single blocks).
   See docs/flux_forward_spec.md + diffusers Flux2Transformer2DModel.
   ============================================================ *)

(* --- Flux2Modulation: SiLU(temb) -> Linear(modW) -> chunk(3*sets) into a
       list of {1,dim} vectors, in order (shift,scale,gate) per set.  temb is
       {1,dim}; modW is {sets*3*dim, dim} (shared across all blocks). --- *)
fxModChunks[temb_, modW_, sets_] := Module[{d, mod},
    d   = fxShape[temb][[2]];
    mod = fxLinear[fxSiLU[temb], modW];                     (* {1, sets*3*d} *)
    Table[TUOpShrink[mod, {{0, 1}, {(i - 1) d, i d}}], {i, 3 sets}]]

(* --- AdaLayerNormContinuous (norm_out): emb=Linear(SiLU(temb)); the diffusers
       chunk is (scale, shift); out = (1+scale)*LayerNorm(x) + shift. --- *)
fxNormOut[x_, temb_, normW_, eps_] := With[{d = fxShape[x][[2]], emb = fxLinear[fxSiLU[temb], normW]},
    fxModulate[x, TUOpShrink[emb, {{0, 1}, {d, 2 d}}], TUOpShrink[emb, {{0, 1}, {0, d}}], eps]]

(* assemble a double-block mods Association from the 6 shared img/txt chunks *)
fxDoubleMods[dImg_, dTxt_] := <|
    "img_shift_msa" -> dImg[[1]], "img_scale_msa" -> dImg[[2]], "img_gate_msa" -> dImg[[3]],
    "img_shift_mlp" -> dImg[[4]], "img_scale_mlp" -> dImg[[5]], "img_gate_mlp" -> dImg[[6]],
    "txt_shift_msa" -> dTxt[[1]], "txt_scale_msa" -> dTxt[[2]], "txt_gate_msa" -> dTxt[[3]],
    "txt_shift_mlp" -> dTxt[[4]], "txt_scale_mlp" -> dTxt[[5]], "txt_gate_mlp" -> dTxt[[6]]|>

(* per-block weight Associations from a name->TTerm loader wf (diffusers names) *)
fxDblW[wf_, i_] := With[{p = "transformer_blocks." <> ToString[i] <> "."}, <|
    "to_q" -> wf[p <> "attn.to_q.weight"], "to_k" -> wf[p <> "attn.to_k.weight"], "to_v" -> wf[p <> "attn.to_v.weight"],
    "add_q_proj" -> wf[p <> "attn.add_q_proj.weight"], "add_k_proj" -> wf[p <> "attn.add_k_proj.weight"], "add_v_proj" -> wf[p <> "attn.add_v_proj.weight"],
    "norm_q" -> wf[p <> "attn.norm_q.weight"], "norm_k" -> wf[p <> "attn.norm_k.weight"],
    "norm_added_q" -> wf[p <> "attn.norm_added_q.weight"], "norm_added_k" -> wf[p <> "attn.norm_added_k.weight"],
    "to_out_0" -> wf[p <> "attn.to_out.0.weight"], "to_add_out" -> wf[p <> "attn.to_add_out.weight"],
    "ff_linear_in" -> wf[p <> "ff.linear_in.weight"], "ff_linear_out" -> wf[p <> "ff.linear_out.weight"],
    "ffc_linear_in" -> wf[p <> "ff_context.linear_in.weight"], "ffc_linear_out" -> wf[p <> "ff_context.linear_out.weight"]|>]

fxSglW[wf_, i_] := With[{p = "single_transformer_blocks." <> ToString[i] <> "."}, <|
    "to_qkv_mlp_proj" -> wf[p <> "attn.to_qkv_mlp_proj.weight"], "to_out" -> wf[p <> "attn.to_out.weight"],
    "norm_q" -> wf[p <> "attn.norm_q.weight"], "norm_k" -> wf[p <> "attn.norm_k.weight"]|>]

(* --- the full transformer.  hidden0 {S_img, in_ch}; enc0 {S_txt, joint_dim};
       temb {1, dim}; ropeCos/Sin {S_txt+S_img, head_dim} ([txt;img] order);
       wf a name->TTerm loader.  Eager block-by-block realize bounds memory
       (each fxLinear materialises its f32 weight cast).  Returns {S_img, out_ch}. --- *)
fxTransformer[hidden0_, enc0_, temb_, ropeCos_, ropeSin_, wf_, cfg_] := Module[
    {eps, nD, nS, stxt, mods, smod, hidden, enc, ss},
    eps = cfg["eps"];  nD = cfg["num_double"];  nS = cfg["num_single"];
    stxt = fxShape[enc0][[1]];
    mods = fxDoubleMods[fxModChunks[temb, wf["double_stream_modulation_img.linear.weight"], 2],
                        fxModChunks[temb, wf["double_stream_modulation_txt.linear.weight"], 2]];
    ss   = fxModChunks[temb, wf["single_stream_modulation.linear.weight"], 1];
    smod = <|"shift" -> ss[[1]], "scale" -> ss[[2]], "gate" -> ss[[3]]|>;
    hidden = TRealize @ fxLinear[hidden0, wf["x_embedder.weight"]];      (* {S_img, dim} *)
    enc    = TRealize @ fxLinear[enc0,    wf["context_embedder.weight"]]; (* {S_txt, dim} *)
    (* fxDoubleBlock returns {img, txt} -> hidden(img) is [[1]], enc(txt) is [[2]].
       Realize BOTH in one multi-root TRealize: the img/txt outputs share the
       joint-attention sub-DAG (concat Q/K/V -> RoPE -> attention -> softmax),
       and a per-root TRealize re-lifts that whole shared chain into a second
       identical kernel set (94 kernels/block).  One bundled pass dedups it to
       58 -- byte-identical output, ~38% fewer dispatches. *)
    Do[ With[{r = TRealize @ fxDoubleBlock[hidden, enc, mods, ropeCos, ropeSin, fxDblW[wf, i], cfg]},
            hidden = r[[1]];  enc = r[[2]]], {i, 0, nD - 1}];
    hidden = TRealize @ fxConcat[{enc, hidden}, 1];                       (* {S_txt+S_img, dim} *)
    Do[ hidden = TRealize @ fxSingleBlock[hidden, smod, ropeCos, ropeSin, fxSglW[wf, i], cfg], {i, 0, nS - 1}];
    hidden = TUOpShrink[hidden, {{stxt, fxShape[hidden][[1]]}, {0, fxShape[hidden][[2]]}}];  (* drop text *)
    fxLinear[fxNormOut[hidden, temb, wf["norm_out.linear.weight"], eps], wf["proj_out.weight"]]]
