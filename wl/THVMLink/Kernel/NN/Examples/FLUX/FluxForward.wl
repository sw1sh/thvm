(* FluxForward.wl -- a compact FLUX.2-klein-4B forward for thvm.

   The tinygrad `examples/stable_diffusion.py` shape: read the named bf16
   tensors out of the diffusers safetensors and wire ops, looped over the
   block config -- NOT a hand-exported per-block NetGraph.  Get-loaded
   after WolframInstitute`THVMLink` (uses the public TUOp* surface + the NN library).
   Architecture + tensor names: docs/flux_forward_spec.md.

   The reusable NN primitives (RMSNorm, SiLU, SwiGLU, interleaved RoPE,
   head-split attention) now live in WolframInstitute`THVMLink`s NN library; this file keeps
   only the FLUX-specific glue: the BLAS-friendly linear, the affine-free
   block LayerNorm, AdaLN modulation, gated residual, and the double /
   single block + transformer assembly. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

(* Public building blocks of the FLUX forward, so a benchmark or test can load
   the example by context (Get["WolframInstitute`THVMLink`Examples`"]) and drive
   the transformer directly; the remaining block/modulation glue stays private. *)
fxTransformer::usage = "fxTransformer[z, enc, temb, ropeCos, ropeSin, wf, cfg] runs the FLUX.2-klein-4B MMDiT forward (double + single stream blocks) and returns the velocity prediction; wf is a weight lookup function and cfg the {num_double, num_single, heads, head_dim, eps} config.";
fxLinear::usage = "fxLinear[x, w] is the FLUX linear y = x . Transpose[w] for a diffusers weight stored {out, in}, run on the bf16 tensor-core matmul path against the weight view (no resident transpose).";
fxQuantizableQ::usage = "fxQuantizableQ[name, dims] is True for a weight worth q8 weight-only quantization (2-D, both dims >= 2048, excluding modulation/norm_out/timestep_embedder).";
fxQuantizeWeight::usage = "fxQuantizeWeight[w] quantizes a weight to weight-only q8, returning <|\"q\" -> int8 weight, \"s\" -> per-output-channel bf16 scale|>.";

Begin["`Private`"];

(* --- linear: a diffusers weight is stored {out, in}, so y = x . W^T.  The
       weight arg `w` is the weight AS STORED ({out, in}, loaded contiguous on
       the device) and fxLinear matmuls against the Transpose[w] VIEW.  The
       Metal tiled tensor-core matmul stages a transposed B operand directly
       from the weight's strides (render_uop.c rmu_emit_matmul_tc_tiled b_trans
       path; Tests/metal_transposed_matmul.wlt), so the transpose is a FREE view
       folded into the matmul's address computation:
       (1) no separate transpose dispatch in the JIT-captured/replayed velocity
           stream (a view carries no compute), and
       (2) no second resident W^T buffer -- the weight is held ONCE on the
           device (~weights-on-disk).  The earlier pre-transpose kept BOTH the
           {out,in} upload AND the realised {in,out} W^T (~2x the weight bytes:
           the flux-generate buffer OOM).
       bf16-native: w stays bf16, so the matmul is bf16(act) x bf16(W) on the
       simdgroup tensor cores (no bf16->f32 cast).  RMSNorm/LayerNorm gains are
       1-D and never reach fxLinear, so the loader leaves them as-is. --- *)
(* THVM_TC_TRANSPOSE=1 (CUDA speed lever): REALIZE the transpose so the matmul's
   B operand is a contiguous LOAD instead of a Transpose-VIEW.  On CUDA the
   parallel-TC recogniser (uop_recognise_tc_parallel) rejects a matmul whose
   operand is a transpose-view (NOT-TC-TEMPLATE) and falls to the naive
   one-warp-per-16x16 path (~5 TFLOPS on H100); a contiguous bf16 B is recognised
   and fires the tiled WMMA GEMM (Krea velNet cold denoise 94.5s -> 26.6s).  The
   default (no env) keeps the runtime Transpose-view -- Metal's simdgroup TC path
   handles it directly, so this is a CUDA-only lever.  TRADEOFF: the per-call
   TRealize@Transpose materialises a contiguous {in,out} bf16 weight the capture
   retains (~24GB across the velNet), so a served warm gen needs the ~3GB
   memory-shave to fit an 80GB H100 (see [[project_krea2_port]]). *)
fxLinear[x_, w_TTerm] := TRealize[TMatMul[TRealize[x],
    If[Environment["THVM_TC_TRANSPOSE"] === "1", TRealize @ Transpose[w], Transpose[w]]]]

(* --- fxLinearFused: a matmul whose output is NOT separately realized, so its
       SINGLE-consumer elementwise epilogue (a gated residual `x + gate*mm`)
       fuses onto the matmul's tiled tensor-core store (THVM_FUSE_MATMUL_EPILOGUE
       + the split-M/N TC collapse keep it on the TILED simdgroup path).  Use
       ONLY where the matmul has exactly one consumer that is the gate AND the
       gate output is the realized block boundary (the MLP down-projection ->
       gated residual); the multi-consumer qkv projections must keep fxLinear's
       outer TRealize or the shared joint-attention chain re-lifts. --- *)
fxLinearFused[x_, w_TTerm] := TMatMul[TRealize[x], Transpose[w]]

(* q8: a quantised projection arrives as its int8 Association, not a bare TTerm.
   The int8 matmul must be a realised STORE(REDUCE) for the tensor-core recogniser
   (see fxLinear), so a fully-fused form is impossible; reuse the q8 fxLinear,
   which realises the matmul and leaves the dequant multiply to fuse into the
   consumer (the same single-consumer benefit fxLinearFused gives the bf16 path). *)
fxLinearFused[x_, q_Association] := fxLinear[x, q]

(* --- q8 weight-only quantization.  A diffusers linear weight {out, in} is
       stored int8 with a per-output-channel (per-row) fp scale:
         scale[o] = max_i |w[o,i]| / 127        (1-D {out})
         wI8[o,i] = round(w[o,i] / scale[o])     (int8, in [-127, 127])
       max|w| per row is recovered without an abs/min op as
       sqrt(reduce_max(w^2)) (REDUCE_MAX is the only max kind; squaring makes
       the reduce sign-blind), mirroring the TRMSNorm reduce-then-broadcast
       idiom.  Quantising on-device keeps host residency to ~0 (the int8 result
       is half the bytes; the bf16 source frees after the realize).  fxLinear
       dispatches on the Association: the matmul reads int8 (Cast->bf16 folded
       in-kernel, render_uop.c rmu_emit_matmul_tc_tiled b_is_int8), staying on
       the simdgroup tensor-core tiled path, then a separate per-output-channel
       scale multiply dequantises.  Metal has no int8 MMA, so this is a
       cold-load / memory optimisation (~half the weight bytes), not a warm
       compute win -- the cast->bf16 runs the same bf16 MMA rate. --- *)
(* A weight is q8-quantisable iff it is a 2-D matrix both of whose dims are
   >= 2048 (the per-block QKV/out/MLP projections -- the warm-hot bulk + ~all the
   resident bytes) AND it is consumed by fxLinear.  The temb-fed linears
   (modulation, norm_out, timestep_embedder) go through fxModLinear, which uses
   the weight directly (Transpose / Dimensions) -- an int8 Association would break
   it -- and their input temb carries a symbolic-M kvar (no resolvable dequant row
   count); they are tiny M=1 matmuls with no q8 payoff, so leave them bf16. *)
fxQuantizableQ[name_String, dims_List] := Length[dims] === 2 && Min[dims] >= 2048 && ! StringContainsQ[name, "modulation" | "norm_out" | "timestep_embedder"];

fxQuantizeWeight[w_TTerm] := With[{shape = Dimensions[w]},
    Module[{sumShape, maxsq, rms, scale, inv, wq},
        sumShape = ReplacePart[shape, Length[shape] -> 1];       (* {out, 1} *)
        maxsq = TUOpReshape[TUOpReduce[w * w, Length[shape] - 1, "MAX"], sumShape];
        rms   = Sqrt[maxsq + 1.*^-12];                           (* max|w| per row *)
        scale = rms * (1. / 127.);                                 (* {out, 1} *)
        inv   = TUOpRecip[scale];                                (* exact 1/scale *)
        wq    = TUOpCast[w * TUOpExpand[inv, shape], "i8"];      (* truncate to int8 *)
        <|"q" -> TRealize[wq], "s" -> TRealize[TUOpCast[TUOpReshape[scale, {First[shape]}], "bf16"]]|>
    ]
]

(* q8 linear: int8 matmul on the TC tiled path (the Cast->bf16 is folded into
   the kernel B-staging), realised, then dequantised by the per-output-channel
   scale.  The matmul MUST be its own realised STORE(REDUCE) for the TC
   recogniser to fire (a fused trailing scale-multiply would make the store
   value a MUL, declining the tensor-core path); the scale is therefore a
   separate broadcast multiply.  Scale is EXPAND'd to the output shape (not a
   bare {1,out} numel-cycle) per the fxModulate/fxGateAdd caveat. *)
fxLinear[x_, q_Association] := Module[{xr, mm, outShape},
    (* Realise x FIRST: x may be an unreduced symbolic Plus (a gated residual / a
       modulation `ln*(sc+1)+sh`), whose Dimensions is {2} (arg count), not its
       tensor shape.  TRealize collapses it to a concrete TTerm -- the bf16 path
       does this implicitly inside the matmul; q8 needs the realised handle for the
       output-shape query too. *)
    xr = TRealize[x];
    mm = TRealize @ TMatMul[xr, Transpose[TUOpCast[q["q"], "bf16"]]];
    (* Output {S, outd}: seq rows from the realised INPUT (a concrete-M block
       activation -- the Dimensions[xr] fxModulate relies on), out-dim from the
       kvar-free realised int8 weight.  Dimensions[mm] is $Failed under capture
       (the realised-matmul term carries no shape).  q8 is gated to linears fed a
       concrete-M activation; the temb-fed ones (symbolic-M) are excluded upstream. *)
    outShape = ReplacePart[Dimensions[xr], -1 -> First[Dimensions[q["q"]]]];
    (* scale-multiply left UNREALIZED so it fuses into the consumer kernel. *)
    TUOpMul[mm, TUOpExpand[TUOpReshape[q["s"], {1, Last[outShape]}], outShape]]
]

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
        ln * (scB + 1) + shB
    ]
]

(* --- gated residual: x + gate * y; gate {1,dim} EXPAND'd over the seq axis
       (same numel-cycle caveat as fxModulate). --- *)
fxGateAdd[x_, gate_, y_] := With[{s = Dimensions[y]}, x + TUOpExpand[gate, s] * y]

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
fxDoubleBlock[img0_, txt0_, mods_, ropeCos_, ropeSin_, W_, cfg_] := Module[{h, dh, eps, scale, simg, stxt, dim, imgN, txtN, qi, ki, vi, qt, kt, vt, Q, K, V, rc, rs, ctx, ctxT, ctxI, img, txt, imgN2, txtN2},
    h = cfg["heads"];  dh = cfg["head_dim"];  eps = cfg["eps"];  scale = 1 / Sqrt[N[dh]];
    simg = Dimensions[img0][[1]];  stxt = Dimensions[txt0][[1]];  dim = h * dh;
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
    img = fxGateAdd[img, mods["img_gate_mlp"], fxLinearFused[TSwiGLU[fxLinear[imgN2, W["ff_linear_in"]]], W["ff_linear_out"]]];
    txtN2 = fxModulate[txt, mods["txt_shift_mlp"], mods["txt_scale_mlp"], eps];
    txt = fxGateAdd[txt, mods["txt_gate_mlp"], fxLinearFused[TSwiGLU[fxLinear[txtN2, W["ffc_linear_in"]]], W["ffc_linear_out"]]];
    {img, txt}
]

(* --- SINGLE-stream block (parallel ViT-22B): the QKV projections are fused
       with the FF input projection (to_qkv_mlp_proj {3*dim + 2*mlp, dim}) and
       the attention output projection is fused with the FF output projection
       (to_out {dim, dim + mlp}).  x0 {S, dim} is the ALREADY [txt; img]-concat
       sequence; mod is one shift/scale/gate; one gated residual.  Op-order per
       diffusers Flux2ParallelSelfAttnProcessor. --- *)
fxSingleBlock[x0_, mod_, ropeCos_, ropeSin_, W_, cfg_] := Module[{h, dh, eps, scale, s, dim, qkvw, xn, qkvmlp, qkv, mlp, q, k, v, rc, rs, attn, mlpG},
    h = cfg["heads"];  dh = cfg["head_dim"];  eps = cfg["eps"];  scale = 1 / Sqrt[N[dh]];
    s = Dimensions[x0][[1]];  dim = h * dh;
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
    fxGateAdd[x0, mod["gate"], fxLinearFused[TRealize @ Join[attn, mlpG, 2], W["to_out"]]]
]

(* ============================================================
   Full transformer forward (loop 5 double + 20 single blocks).
   See docs/flux_forward_spec.md + diffusers Flux2Transformer2DModel.
   ============================================================ *)

(* --- temb-modulation matmul.  temb is {1,dim} (one timestep), so SiLU(temb).W^T
       is an M=1 matmul.  As a JIT-captured sampler input, an M=1 matmul makes the
       leading axis SYMBOLIC (kvar), which declines the Metal ICB for the WHOLE
       captured velocity stream -> per-op dispatch (~5x slower replay, profiled).
       Pad to a CONCRETE M=8 (row 0 = the real timestep, rows 1-7 zero), matmul,
       take row 0 -- the matmul carries no kvar, so the ICB batches the big
       projections.  The 8x M is a tiny matmul (modW is small vs the projections),
       and rows 1-7 (zeros) cost nothing downstream. --- *)
(* M=1 temb modulation: pad {1,dim} to a CONCRETE M=8 (Join 7 zero rows) so the
   symbolic-M kvar never declines the Metal ICB, matmul a transposed weight, slice
   row 0.  The pad's column count (dim) and dtype come from the WEIGHT w
   ({out,dim}, a realized contiguous tensor with a fixed concrete shape), NOT from
   v: v is the captured temb whose LEADING axis is a symbolic-M kvar, so
   Dimensions[v]/TTensorDType[v] return $Failed/Missing in a JIT capture (the kvar
   is unresolved at graph-build time) -- which poisons the whole modulation.  w is
   kvar-free, so TLastDim[w]/TDType[w] are always concrete. *)
fxModLinear[v_, w_] := TMatMul[Join[v, TZeros[{7, Last[Dimensions[w]]}], 1], Transpose[w]][[1 ;; 1]]

(* --- Flux2Modulation: SiLU(temb) -> Linear(modW) -> chunk(3*sets) into a
       list of {1,dim} vectors, in order (shift,scale,gate) per set.  temb is
       {1,dim}; modW is {sets*3*dim, dim} (shared across all blocks). --- *)
fxModChunks[temb_, modW_, sets_] := Module[{d, mod},
    d   = Last[Dimensions[modW]];                          (* dim: from the kvar-free
        weight, NOT Dimensions[temb] (the captured temb has a symbolic-M kvar, so
        its shape query returns $Failed at graph-build time). *)
    mod = fxModLinear[TSiLU[temb], modW];                   (* {1, sets*3*d} *)
    Table[mod[[All, (i - 1) d + 1 ;; i d]], {i, 3 sets}]
]

(* --- AdaLayerNormContinuous (norm_out): emb=Linear(SiLU(temb)); the diffusers
       chunk is (scale, shift); out = (1+scale)*LayerNorm(x) + shift. --- *)
fxNormOut[x_, temb_, normW_, eps_] := With[{d = Last[Dimensions[normW]], emb = fxModLinear[TSiLU[temb], normW]},
    (* d (dim) from the kvar-free normW {2*dim,dim}, NOT Dimensions[x] (x can carry a
       symbolic-seq kvar in a JIT capture -> $Failed shape query). *)
    fxModulate[x, emb[[All, d + 1 ;; 2 d]], emb[[All, 1 ;; d]], eps]
]

(* assemble a double-block mods Association from the 6 shared img/txt chunks *)
fxDoubleMods[dImg_, dTxt_] := <|
    "img_shift_msa" -> dImg[[1]],
    "img_scale_msa" -> dImg[[2]],
    "img_gate_msa" -> dImg[[3]],
    "img_shift_mlp" -> dImg[[4]],
    "img_scale_mlp" -> dImg[[5]],
    "img_gate_mlp" -> dImg[[6]],
    "txt_shift_msa" -> dTxt[[1]],
    "txt_scale_msa" -> dTxt[[2]],
    "txt_gate_msa" -> dTxt[[3]],
    "txt_shift_mlp" -> dTxt[[4]],
    "txt_scale_mlp" -> dTxt[[5]],
    "txt_gate_mlp" -> dTxt[[6]]
|>

(* per-block weight Associations from a name->TTerm loader wf (diffusers names) *)
fxDblW[wf_, i_] := With[{p = "transformer_blocks." <> ToString[i] <> "."},
    <|
        "to_q" -> wf[p <> "attn.to_q.weight"],
        "to_k" -> wf[p <> "attn.to_k.weight"],
        "to_v" -> wf[p <> "attn.to_v.weight"],
        "add_q_proj" -> wf[p <> "attn.add_q_proj.weight"],
        "add_k_proj" -> wf[p <> "attn.add_k_proj.weight"],
        "add_v_proj" -> wf[p <> "attn.add_v_proj.weight"],
        "norm_q" -> wf[p <> "attn.norm_q.weight"],
        "norm_k" -> wf[p <> "attn.norm_k.weight"],
        "norm_added_q" -> wf[p <> "attn.norm_added_q.weight"],
        "norm_added_k" -> wf[p <> "attn.norm_added_k.weight"],
        "to_out_0" -> wf[p <> "attn.to_out.0.weight"],
        "to_add_out" -> wf[p <> "attn.to_add_out.weight"],
        "ff_linear_in" -> wf[p <> "ff.linear_in.weight"],
        "ff_linear_out" -> wf[p <> "ff.linear_out.weight"],
        "ffc_linear_in" -> wf[p <> "ff_context.linear_in.weight"],
        "ffc_linear_out" -> wf[p <> "ff_context.linear_out.weight"]
    |>
]

fxSglW[wf_, i_] := With[{p = "single_transformer_blocks." <> ToString[i] <> "."},
    <|
        "to_qkv_mlp_proj" -> wf[p <> "attn.to_qkv_mlp_proj.weight"],
        "to_out" -> wf[p <> "attn.to_out.weight"],
        "norm_q" -> wf[p <> "attn.norm_q.weight"],
        "norm_k" -> wf[p <> "attn.norm_k.weight"]
    |>
]

(* --- the full transformer.  hidden0 {S_img, in_ch}; enc0 {S_txt, joint_dim};
       temb {1, dim}; ropeCos/Sin {S_txt+S_img, head_dim} ([txt;img] order);
       wf a name->TTerm loader.  Eager block-by-block realize bounds memory
       (each fxLinear materialises its f32 weight cast).  Returns {S_img, out_ch}. --- *)
fxTransformer[hidden0_, enc0_, temb_, ropeCos_, ropeSin_, wf_, cfg_] := Module[{eps, nD, nS, stxt, mods, smod, hidden, enc, ss},
    eps = cfg["eps"];  nD = cfg["num_double"];  nS = cfg["num_single"];
    stxt = Dimensions[enc0][[1]];
    mods = fxDoubleMods[fxModChunks[temb, wf["double_stream_modulation_img.linear.weight"], 2], fxModChunks[temb, wf["double_stream_modulation_txt.linear.weight"], 2]];
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
    fxLinear[fxNormOut[hidden, temb, wf["norm_out.linear.weight"], eps], wf["proj_out.weight"]]
]

End[];

EndPackage[];
