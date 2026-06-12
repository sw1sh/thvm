(* FluxForward.wl -- a compact FLUX.2-klein-4B forward for thvm.

   The tinygrad `examples/stable_diffusion.py` shape: read the named bf16
   tensors out of the diffusers safetensors and wire ops, looped over the
   block config -- NOT a hand-exported per-block NetGraph.  Get-loaded
   after THVMLink` (uses the public TUOp* surface + the NN library).
   Architecture + tensor names: docs/flux_forward_spec.md.

   The reusable NN primitives (RMSNorm, SiLU, SwiGLU, interleaved RoPE,
   head-split attention) now live in THVMLink`s NN library; this file keeps
   only the FLUX-specific glue: the BLAS-friendly linear, the affine-free
   block LayerNorm, AdaLN modulation, gated residual, and the double /
   single block + transformer assembly. *)

(* --- linear: a diffusers weight is stored {out, in}, so y = x . W^T.
       Materialise W^T as a CONTIGUOUS {in, out} buffer (TRealize[Transpose[w]])
       and matmul against that.  Two reasons:
       (1) the weight is a constant, so the transpose-copy is a one-time cost
           amortised across every step (in a JIT'd sampler it is captured once);
       (2) on Metal a matmul whose B operand is a transposed VIEW reads the
           strided bytes as if row-major and returns garbage (see
           Tests/metal_transposed_matmul.wlt); realizing the transpose to a
           contiguous buffer feeds the matmul a row-major B and is exact.
       This is bf16-native: w may stay bf16 (no bf16->f32 TUOpCast), which
       halves the weight bytes loaded and skips the per-weight cast pass -- the
       f32 cast was only needed for the old cblas transB view path. --- *)
fxLinear[x_, w_] := TMatMul[x, TRealize[Transpose[w]]]

(* --- affine-free LayerNorm over the last axis (FLUX block norms use
       elementwise_affine=False; the modulation supplies scale/shift).  This is
       exactly the library TLayerNorm; aliased for readability in the block
       assembly below. --- *)
fxLayerNorm[x_, eps_] := TLayerNorm[x, eps]

(* --- AdaLN modulation: (1 + scale) * LayerNorm(x) + shift; scale,shift {1,dim}
       broadcast over the sequence axis (the mod vectors are shared per block).
       The {1,dim} mod vectors are EXPAND'd to {S,dim} explicitly: a bare
       {S,dim} * {1,dim} via the Plus/Times numel-cycle aligns only the first
       row and writes denormals into the rest (the TLayerNormAffine caveat). --- *)
fxModulate[x_, shift_, scale_, eps_] := With[{s = Dimensions[x], ln = TLayerNorm[x, eps]},
    With[{scB = TUOpExpand[scale, s], shB = TUOpExpand[shift, s]},
        ln * (scB + 1) + shB]]

(* --- gated residual: x + gate * y; gate {1,dim} EXPAND'd over the seq axis
       (same numel-cycle caveat as fxModulate). --- *)
fxGateAdd[x_, gate_, y_] := With[{s = Dimensions[y]},
    x + TUOpExpand[gate, s] * y]

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
    simg = Dimensions[img0][[1]];  stxt = Dimensions[txt0][[1]];  dim = h*dh;
    (* attention: modulate, project, per-head RMSNorm (v unnormed) *)
    imgN = fxModulate[img0, mods["img_shift_msa"], mods["img_scale_msa"], eps];
    txtN = fxModulate[txt0, mods["txt_shift_msa"], mods["txt_scale_msa"], eps];
    qi = TRMSNorm[ArrayReshape[fxLinear[imgN, W["to_q"]], {simg, h, dh}], W["norm_q"], eps];
    ki = TRMSNorm[ArrayReshape[fxLinear[imgN, W["to_k"]], {simg, h, dh}], W["norm_k"], eps];
    vi = ArrayReshape[fxLinear[imgN, W["to_v"]], {simg, h, dh}];
    qt = TRMSNorm[ArrayReshape[fxLinear[txtN, W["add_q_proj"]], {stxt, h, dh}], W["norm_added_q"], eps];
    kt = TRMSNorm[ArrayReshape[fxLinear[txtN, W["add_k_proj"]], {stxt, h, dh}], W["norm_added_k"], eps];
    vt = ArrayReshape[fxLinear[txtN, W["add_v_proj"]], {stxt, h, dh}];
    (* text-first joint concat on the seq axis (axis 1), then RoPE *)
    Q = Join[qt, qi, 1];  K = Join[kt, ki, 1];  V = Join[vt, vi, 1];
    rc = ArrayReshape[ropeCos, {stxt + simg, 1, dh}];  rs = ArrayReshape[ropeSin, {stxt + simg, 1, dh}];
    Q = TRoPEInterleaved[Q, rc, rs];  K = TRoPEInterleaved[K, rc, rs];
    ctx  = THeadAttention[Q, K, V, scale];               (* {Stxt+Simg, dim} *)
    ctxT = ctx[[1 ;; stxt]];
    ctxI = ctx[[stxt + 1 ;; stxt + simg]];
    img = fxGateAdd[img0, mods["img_gate_msa"], fxLinear[ctxI, W["to_out_0"]]];
    txt = fxGateAdd[txt0, mods["txt_gate_msa"], fxLinear[ctxT, W["to_add_out"]]];
    (* SwiGLU MLP per stream *)
    imgN2 = fxModulate[img, mods["img_shift_mlp"], mods["img_scale_mlp"], eps];
    img = fxGateAdd[img, mods["img_gate_mlp"],
        fxLinear[TSwiGLU[fxLinear[imgN2, W["ff_linear_in"]]], W["ff_linear_out"]]];
    txtN2 = fxModulate[txt, mods["txt_shift_mlp"], mods["txt_scale_mlp"], eps];
    txt = fxGateAdd[txt, mods["txt_gate_mlp"],
        fxLinear[TSwiGLU[fxLinear[txtN2, W["ffc_linear_in"]]], W["ffc_linear_out"]]];
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
    s = Dimensions[x0][[1]];  dim = h*dh;
    xn     = fxModulate[x0, mod["shift"], mod["scale"], eps];
    (* realize the fused projection: qkv + mlp are both shrinks of it, so the
       attention and MLP paths would otherwise each re-lift this big matmul into
       the final fused concat+to_out matmul -- the deepest DAG in the model,
       which overflows the recursive lift walk.  A leaf read cuts both paths. *)
    qkvmlp = TRealize @ fxLinear[xn, W["to_qkv_mlp_proj"]];        (* {S, 3*dim + 2*mlp} *)
    qkvw   = Dimensions[qkvmlp][[2]];
    qkv    = qkvmlp[[All, 1 ;; 3 dim]];                          (* {S, 3*dim} *)
    mlp    = qkvmlp[[All, 3 dim + 1 ;; qkvw]];                   (* {S, 2*mlp} *)
    q = TRMSNorm[ArrayReshape[qkv[[All, 1 ;; dim]],       {s, h, dh}], W["norm_q"], eps];
    k = TRMSNorm[ArrayReshape[qkv[[All, dim + 1 ;; 2 dim]], {s, h, dh}], W["norm_k"], eps];
    v =          ArrayReshape[qkv[[All, 2 dim + 1 ;; 3 dim]], {s, h, dh}];
    rc = ArrayReshape[ropeCos, {s, 1, dh}];  rs = ArrayReshape[ropeSin, {s, 1, dh}];
    q = TRoPEInterleaved[q, rc, rs];  k = TRoPEInterleaved[k, rc, rs];
    attn = THeadAttention[q, k, v, scale];                       (* {S, dim} *)
    mlpG = TSwiGLU[mlp];                                         (* {S, mlp} *)
    (* realize the joined attn|mlp before to_out: it feeds the final matmul's
       contraction operand, so a leaf read keeps that lift shallow. *)
    fxGateAdd[x0, mod["gate"], fxLinear[TRealize @ Join[attn, mlpG, 2], W["to_out"]]]]

(* ============================================================
   Full transformer forward (loop 5 double + 20 single blocks).
   See docs/flux_forward_spec.md + diffusers Flux2Transformer2DModel.
   ============================================================ *)

(* --- Flux2Modulation: SiLU(temb) -> Linear(modW) -> chunk(3*sets) into a
       list of {1,dim} vectors, in order (shift,scale,gate) per set.  temb is
       {1,dim}; modW is {sets*3*dim, dim} (shared across all blocks). --- *)
fxModChunks[temb_, modW_, sets_] := Module[{d, mod},
    d   = Dimensions[temb][[2]];
    mod = fxLinear[TSiLU[temb], modW];                      (* {1, sets*3*d} *)
    Table[mod[[All, (i - 1) d + 1 ;; i d]], {i, 3 sets}]]

(* --- AdaLayerNormContinuous (norm_out): emb=Linear(SiLU(temb)); the diffusers
       chunk is (scale, shift); out = (1+scale)*LayerNorm(x) + shift. --- *)
fxNormOut[x_, temb_, normW_, eps_] := With[{d = Dimensions[x][[2]], emb = fxLinear[TSiLU[temb], normW]},
    fxModulate[x, emb[[All, d + 1 ;; 2 d]], emb[[All, 1 ;; d]], eps]]

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
    stxt = Dimensions[enc0][[1]];
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
    hidden = TRealize @ Join[enc, hidden, 1];                             (* {S_txt+S_img, dim} *)
    Do[ hidden = TRealize @ fxSingleBlock[hidden, smod, ropeCos, ropeSin, fxSglW[wf, i], cfg], {i, 0, nS - 1}];
    hidden = hidden[[stxt + 1 ;; Dimensions[hidden][[1]]]];               (* drop text *)
    fxLinear[fxNormOut[hidden, temb, wf["norm_out.linear.weight"], eps], wf["proj_out.weight"]]]
