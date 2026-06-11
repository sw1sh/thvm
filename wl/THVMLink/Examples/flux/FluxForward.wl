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

(* --- linear: a diffusers weight is stored {out, in}, so y = x . W^T --- *)
fxLinear[x_, w_] := TMatMul[x, Transpose[w]]

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
       residual -> SwiGLU MLP -> gated residual. --- *)
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
    qkvmlp = fxLinear[xn, W["to_qkv_mlp_proj"]];                   (* {S, 3*dim + 2*mlp} *)
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
    fxGateAdd[x0, mod["gate"], fxLinear[fxConcat[{attn, mlpG}, 2], W["to_out"]]]]
