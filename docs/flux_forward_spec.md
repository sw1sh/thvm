# FLUX.2-klein-4B Forward-Pass Spec (for thvm)

A precise, implementation-ready specification of the **forward (inference) pass** of
`black-forest-labs/FLUX.2-klein-4B`, written so an engineer can implement it in thvm
(a tinygrad-style tensor VM) **without re-reading the Wolfram source**.

This is a design doc, not code. Every fact is cited to one of:

- **WL** files under `wolfram/neuralnetworks/Models/FLUX/FLUX/Kernel/`:
  `Transformer.wl`, `RoPE.wl`, `Layers.wl`, `VAE.wl`, `TextEncoders.wl`, `Export.wl`,
  and `Generate.wl` (the MLX ground-truth forward). Scripts:
  `scripts/generate_image_q8.wls`, `scripts/export_full_pipeline_q8.wls`.
- **CFG** = the HuggingFace config JSONs (transformer / vae / text_encoder / model_index).
- **DIFF** = the diffusers reference `transformers/transformer_flux2.py` +
  `embeddings.py` (commit `4b843c8`), used as op-order ground truth.

There are **two** WL implementations of the transformer. They agree on architecture but
the `Generate.wl` MLX path (`Qwen3EncodeMLX` / `FluxSamplerMLX` / `VAEDecodeMLX`, all
backed by a fused C++ `MLFluxDiTStep`) is the *production* forward and is the canonical
reference here. The `Export.wl` NetGraph builders (`BuildDoubleBlock`, `BuildSingleBlockFlux2`,
`BuildVelocityNetwork`) are a second, CoreML-export-oriented construction; where they differ
from `Generate.wl`/DIFF it is flagged (see §C, §I). The standalone `Transformer.wl` blocks are
a third, **stale** copy and disagree on one point (concat order, §C) -- do not port from it.

---

## A. Pipeline overview

Three components, chained `string -> Image`
(`Generate.wl:FluxGenerate`, `export_full_pipeline_q8.wls` "Monolithic Pipeline").

```
prompt (String)
  | host-side BPE chat-template tokenizer (Qwen2TokenizerFast)   [CFG model_index]
  v
input_ids {512}  (int, padded/truncated to txtSeqLen=512)
  | Qwen3-4B text encoder (36 decoder layers, GQA)               [§E]
  | extract hidden states after layers 8,17,26 (0-indexed),
  | concat on feature axis -> {512, 3*2560 = 7680}
  v
text_embeddings {512, 7680}
  | context_embedder:  Linear 7680 -> 3072 (no bias)             [Export.wl txt_in / Generate.wl]
  v
txt {512, 3072}                                                   [= txtSeqLen x dim]

latent z {256, 128}  (init ~ Normal; imgSeqLen=256, latent_channels*p*p=128)
  | 4-step Euler flow-matching denoiser; each step calls the     [§G]
  | MMDiT velocity net v(z, txt, sigma) -> {256, 128}            [§C]
  | z <- z + dt * v
  v
denoised latent z {256, 128}
  | BN denorm + unpatchify {256,128} -> {1,32,32,32} (NCHW)      [§F]
  | AutoencoderKLFlux2 decoder                                    [§F]
  v
image {3, 256, 256}  (RGB, rescaled (x+1)/2 clipped to [0,1])
```

**Shapes for the 256x256 bench** (`FluxGenerate` defaults `ImageSize -> {256,256}`):

| quantity | value | derivation |
|---|---|---|
| image size | 256 x 256 | bench default (`Generate.wl` `Options[FluxGenerate]`) |
| VAE downsample | /8 spatial (3 nearest-2x upsamples in decoder), x /2 patch | block_out_channels 4 levels, patch_size [2,2] (CFG vae) |
| latent spatial | 16 x 16 | `{latentW,latentH} = Round[imgSize/32]` -> 256/32 = 8... see note | 
| latent channels | 32 | CFG vae `latent_channels` |
| **patchify** | p=2 | CFG vae `patch_size [2,2]` -> packs 2x2 spatial into channels |
| img tokens | **imgSeqLen = 256** | latentH*latentW = 16*16 (`Generate.wl` `imgSeqLen = latentH*latentW`) |
| img token dim | **128** | latent_channels * p * p = 32*4 = `in_channels` (CFG transformer `in_channels=128`) |
| txt tokens | **txtSeqLen = 512** | `Generate.wl` `txtSeqLen = 512` |

> **Latent-resolution note / flag.** `Generate.wl` computes
> `{latentW,latentH} = Round[imgSize/32]` = `{8,8}`, which gives imgSeqLen 64, but the
> production sampler (`export_full_pipeline_q8.wls`) hardcodes `latentH=latentW=16` ->
> `imgSeqLen=256`, and the unpatchify is hardcoded `{16,16,32,2,2} -> {32,32,32}`
> (latent 16x16, patch 2 -> 32x32 feature map -> x8 decoder -> 256x256). The `/32`
> in `FluxGenerate` is therefore the patched-latent stride (image/32 = patched-latent
> side), and the **canonical bench numbers are imgSeqLen=256, latent 16x16, latent
> feature map 32x32**. Use 256 / 16 / 32. (The `Round[imgSize/32]` line is a latent-side
> count; `latentH*latentW=256` only when imgSize=512, so the production path fixes 16x16.)

The **patchify** (image -> tokens) is the inverse of the unpatchify in §F: a 16x16x(32 channels)
latent with 2x2 patches becomes 256 tokens of 128 features each. In FLUX.2 the *patchify* is
folded into the latent layout fed to `x_embedder`; thvm receives `z {256,128}` directly and only
needs the **unpatchify** at the end (§F).

---

## B. Architecture constants

Transformer (MMDiT / `Flux2Transformer2DModel`), from **CFG transformer/config.json** and
`Export.wl:$FluxArchitectures["FLUX.2-klein-4B"]`:

| name | value | source |
|---|---|---|
| hidden_size / dim | **3072** | CFG (derived: heads*head_dim); WL `HiddenSize 3072` |
| num_attention_heads | **24** | CFG `num_attention_heads`; WL `NumHeads 24` |
| attention_head_dim | **128** | CFG `attention_head_dim`; WL `HeadDim 128` |
| mlp_ratio | **3.0** | CFG `mlp_ratio`; WL `MLPRatio 3.0` |
| **mlp_dim** (ff hidden) | **9216** | `Round[3072*3.0]` (WL `$MLPDim`) |
| num_double_blocks (`num_layers`) | **5** | CFG `num_layers`; WL `NumDoubleBlocks 5` |
| num_single_blocks (`num_single_layers`) | **20** | CFG `num_single_layers`; WL `NumSingleBlocks 20` |
| in_channels (img token dim) | **128** | CFG `in_channels`; WL `InChannels 128` |
| joint_attention_dim (text ctx dim) | **7680** | CFG `joint_attention_dim`; WL `ContextDim 7680` |
| rope_theta | **2000** | CFG `rope_theta`; WL `Flux2PosEmbed[...,2000]` |
| axes_dims_rope | **{32,32,32,32}** | CFG `axes_dims_rope`; WL `axesDims = {32,32,32,32}` |
| eps (all RMS/LayerNorm) | **1e-6** | CFG `eps`; WL `"Epsilon" -> 1*^-6` everywhere |
| patch_size | 1 | CFG `patch_size` (transformer patch = 1; VAE patch carries the 2x2) |
| guidance_embeds | **false** | CFG; WL `UseGuidance False` -> guidance branch OFF |
| shared modulation | **true** | WL `SharedModulation True` (modulation produced once, reused per block) |
| bias | **none** | WL `NoBias True` -> all transformer Linear layers have **no bias** |
| timestep_guidance_channels | 256 | CFG (sinusoidal timestep embed width before MLP) |

Qwen3-4B text encoder (`Qwen3ForCausalLM`), from **CFG text_encoder/config.json** and
`TextEncoders.wl:$Qwen3Config`:

| name | value | source |
|---|---|---|
| hidden_size | **2560** | CFG `hidden_size`; WL `HiddenSize 2560` |
| num_hidden_layers | **36** | CFG `num_hidden_layers`; WL `NumLayers 36` |
| num_attention_heads (Q) | **32** | CFG; WL `NumHeads 32` |
| num_key_value_heads (GQA) | **8** | CFG `num_key_value_heads`; WL `NumKVHeads 8` (GQA ratio 4) |
| head_dim | **128** | CFG; WL `HeadDim 128` (note Q dim = 32*128 = 4096 != hidden 2560) |
| intermediate_size (SwiGLU) | **9728** | CFG; WL `IntermediateSize 9728` |
| vocab_size | **151936** | CFG; WL `VocabSize 151936` |
| rms_norm_eps | **1e-6** | CFG; WL `1*^-6` |
| rope_theta | **1000000** | CFG `rope_theta`; WL `RoPETheta 1000000` |
| hidden_act | silu (SwiGLU) | CFG |
| attention_bias | false | CFG -> q/k/v/o no bias |
| tie_word_embeddings | true | CFG (irrelevant: encoder, no lm_head used) |
| pad/bos/eos token id | 151643 / 151643 / 151645 | CFG + WL |
| **extract layers** | after **8,17,26** (0-idx) | WL `ExtractLayers {8,17,26}` = Python `hidden_states[9,18,27]` |

VAE (`AutoencoderKLFlux2`), from **CFG vae/config.json** + `Export.wl:BuildVAEDecoder` / `Generate.wl:VAEDecodeMLX`:

| name | value | source |
|---|---|---|
| in/out channels (image) | 3 / 3 | CFG |
| latent_channels | **32** | CFG `latent_channels` |
| block_out_channels | **[128,256,512,512]** | CFG (decoder traverses reversed) |
| layers_per_block | 2 (config) / **3 resnets per decoder up-block** | CFG `layers_per_block`; WL uses 3 resnets/up-block |
| norm_num_groups | **32** | CFG; WL GroupNorm 32 |
| mid_block_add_attention | true | CFG; WL `VAESelfAttention` present |
| patch_size | **[2,2]** | CFG (the unpatchify factor, §F) |
| act_fn | silu | CFG |
| use_post_quant_conv | true | CFG; WL `post_quant_conv` 32->32 1x1 |
| scaling/shift factor | **absent** | CFG (not present; WL uses a **BatchNorm** denorm instead, §F) |

---

## C. MMDiT transformer forward, op-by-op

This is the **velocity network** `v(z, txt, sigma) -> {256,128}`. Canonical reference:
`Generate.wl:FluxSamplerMLX` body + the fused C++ `MLFluxDiTStep`, cross-checked against
DIFF and `Export.wl:BuildVelocityNetwork`.

### C.0 Inputs (per denoising step)

| input | shape | notes |
|---|---|---|
| `z` (noisy latent / img tokens) | {256, 128} | updated each step |
| `txt` (text cond) | {512, 3072} | `context_embedder(text_emb)`, constant across steps |
| `sigma` | scalar | current noise level for this step (§G) |
| img RoPE `cos`,`sin` | {256, 1, 128} | per-token, head-broadcast; built from img pos ids (§D) |
| txt RoPE `cos`,`sin` | {512, 1, 128} | from txt pos ids (§D) |

No guidance vector (klein, `guidance_embeds=false`). Position ids (§D):
`img_pos[h*16+w] = {0, h, w, 0}` for h,w in 0..15; `txt_pos[i] = {0,0,0,i}` for i in 0..511.

### C.1 Embeddings + modulation (computed once per step)

1. **img_in**: `imgOut = z @ x_embedder.weight^T`  ->  {256, 3072}.
   (`Generate.wl` `mlLinear[z, $FW["x_embedder.weight"]]`; no bias.)
2. **timestep embed** (sinusoidal, width 256): with `timeFactor=1000`, `maxPeriod=10000`,
   `half=128`, `freqs = exp(-log(10000) * [0..127]/128)`, `args = (sigma*1000)*freqs`,
   `tEmb = concat(cos(args), sin(args))` -> {256}. (`FluxTimestepEmbed`; DIFF `timestep*1000`.)
   - **Important ordering**: thvm must use `concat(cos, sin)` (cos first). (`Layers.wl:TimestepEmbedLayer`, `Generate.wl:FluxTimestepEmbed`.)
3. **time MLP** (`time_guidance_embed.timestep_embedder`): `vec = linear_2(silu(linear_1(tEmb)))`,
   256 -> 3072 -> 3072, no bias -> `vec {3072}`. (Guidance branch absent.)
4. **Shared modulation** (computed once, reused by every block since `SharedModulation=True`):
   each is `silu(vec) @ W^T` (no bias):
   - `single_stream_modulation.linear`: 3072 -> **3*3072=9216** -> split into
     `(single_shift, single_scale, single_gate)` each {3072}.
   - `double_stream_modulation_img.linear`: 3072 -> **6*3072=18432** -> split into
     `(img_shift1, img_scale1, img_gate1, img_shift2, img_scale2, img_gate2)`.
   - `double_stream_modulation_txt.linear`: 3072 -> 18432 -> `(txt_*1, txt_*2)` likewise.
   (`Layers.wl:BuildSingleModLayer/BuildDoubleMod*`; `Generate.wl` packs these weight ids first.)
   - The same modulation vectors are applied in **all 5 double** and **all 20 single** blocks.
     This is the "shared modulation" of FLUX.2-klein (contrast FLUX.1 per-block modulation).

### C.2 Double-stream block (x5), op order

Two streams (img S=256, txt S=512), each with its own QKV/MLP weights and its own
modulation params, but **joint attention** over `concat(txt, img)` tokens. All RMSNorm/
LayerNorm eps=1e-6, all Linear no-bias. Per block `b` (`transformer_blocks.{b}.*`):

Per stream `s in {img, txt}` (weight prefixes: img=`attn.to_{q,k,v}`, `ff.*`;
txt=`attn.add_{q,k}_proj`/`add_v_proj`, `ff_context.*`):

**Attention phase**
1. `n = LayerNorm(x_s)` -- standardize over feature axis (mean/var), eps 1e-6, **no affine**.
   (DIFF `self.norm1`; `Export.wl:BuildDoubleBlock` uses `LayerNorm2D`. Note: this is plain
   LayerNorm, **not** RMSNorm -- distinct from the q/k norm below.)
2. `m = (1 + scale1_s) * n + shift1_s`  (broadcast the {3072} mod vectors over tokens).
3. `q = m @ Wq_s^T`, `k = m @ Wk_s^T`, `v = m @ Wv_s^T`  -> each {S, 3072}.
4. reshape q,k,v to {S, 24, 128} (heads).
5. **q/k RMSNorm per head** over head_dim=128, with learned scale (`attn.norm_q.weight` /
   `norm_k.weight` for img, `norm_added_q/k.weight` for txt), eps 1e-6. v is **not** normed.
   (`RMSNormPerHead`; DIFF applies qk-norm post-reshape pre-attention.)
6. **RoPE** on q,k (interleaved, §D) using stream-specific cos/sin
   (img uses img cos/sin, txt uses txt cos/sin).

**Joint attention** (after both streams have q,k,v):
7. `Q_s = q_s` (the query for stream s, S_s heads), and shared
   `K = concat(k_txt, k_img)` along the token axis, `V = concat(v_txt, v_img)`
   -> K,V have 512+256 = **768 tokens**.  **TEXT FIRST, then image.**
   (DIFF: `key = torch.cat([encoder_key, key], dim=1)` (text=encoder first);
   `Export.wl:BuildDoubleBlock` `cat_k = {"txt_rope_k","img_rope_k"}` text-first.
   **FLAG**: stale `Transformer.wl:FluxDoubleBlock` concats `{img,txt}` img-first -- WRONG,
   do not use.)
8. attention: `softmax(Q_s K^T / sqrt(128)) V`, multi-head (24 heads, head_dim 128), no mask.
   -> {S_s, 3072}.
9. `a_s = attn_out_s @ Wo_s^T`  (img: `attn.to_out.0`, txt: `attn.to_add_out`), no bias.
10. `x_s = x_s + gate1_s * a_s`  (gated residual; broadcast gate {3072}).

**MLP phase** (SwiGLU/GEGLU-style gating, per stream):
11. `n2 = LayerNorm(x_s)` (eps 1e-6, no affine).
12. `m2 = (1 + scale2_s) * n2 + shift2_s`.
13. `h = m2 @ W_in_s^T` -> {S, **2*9216 = 18432**} (`ff.linear_in` / `ff_context.linear_in`).
14. **SiLU gate**: split h into `(a, g)` each {S, 9216}; `h = silu(a) * g` -> {S, 9216}.
    (`SiLUGateSeq`: silu on first half, multiply by second half.)
15. `o = h @ W_out_s^T` -> {S, 3072} (`ff.linear_out` / `ff_context.linear_out`).
16. `x_s = x_s + gate2_s * o`.

Output of block: updated `img {256,3072}`, `txt {512,3072}`. Repeat for all 5 blocks
(same shared modulation vectors each time).

### C.3 Bridge double -> single

After the 5 double blocks, concatenate **txt then img** into one stream of 768 tokens:
`x = concat(txt {512,3072}, img {256,3072})` -> {768, 3072}.
(`Export.wl:BuildVelocityNetwork` `join_for_single = {"split_txt","split_img"}` (txt first);
single-block RoPE uses `mergedCos = Join[txtCos, imgCos]` i.e. txt-first
(`export_full_pipeline_q8.wls`).)

### C.4 Single-stream block (x20), op order

One fused stream over all 768 tokens. Weights `single_transformer_blocks.{b}.attn.*`.
Uses the **single** shared modulation `(single_shift, single_scale, single_gate)` (each {3072}),
same for all 20 blocks. Per block:

1. `n = LayerNorm(x)` (eps 1e-6, no affine).
   (**FLAG**: `Export.wl:BuildSingleBlockFlux2` uses `LayerNorm2D` (standardize). DIFF
   `self.norm` for the single block is also LayerNorm. The stale `Transformer.wl:FluxSingleBlock`
   uses `RMSNorm2D` -- mismatch; trust LayerNorm.)
2. `m = (1 + single_scale) * n + single_shift`.
3. **fused projection** `to_qkv_mlp_proj`: `p = m @ W^T` ->
   {768, 3*3072 + 2*9216 = 9216 + 18432 = **27648**}.
   Split: `q,k,v` = first 3*3072 (each {768,3072}); `mlp_in` = remaining {768, 18432}.
   (`Generate.wl` computes `singleMlpHalfDim = (qkvMlpDim - 3*dim)/2 = 9216`.)
4. reshape q,k,v to {768,24,128}; **q/k RMSNorm per head** (eps 1e-6, scales
   `attn.norm_q.weight`/`norm_k.weight`); **RoPE** interleaved on q,k using the merged
   txt-then-img cos/sin (768 tokens).
5. **attention** (parallel to MLP): `softmax(QK^T/sqrt(128)) V`, 24 heads, no mask -> {768,3072}.
6. **MLP** (parallel): split `mlp_in` into `(a,g)` each {768,9216}; `mlp = silu(a)*g` -> {768,9216}.
7. `cat = concat(attn_out {768,3072}, mlp {768,9216})` -> {768, **12288**}.
8. `o = cat @ to_out.weight^T` -> {768, 3072} (`attn.to_out`, no bias).
9. `x = x + single_gate * o`. (gated residual.)

Output {768, 3072}. Repeat 20x.

### C.5 Final layer + unpatchify

1. take the **image** rows back out: `img = x[txt+1 :: ]` i.e. the **last 256** of 768
   (since the bridge put txt first, img is the tail). (`Export.wl` `extract_img =
   PartLayer[{txtSeqLen+1 ;; seqLen}]`.)
2. `n = LayerNorm(img)` (eps 1e-6, no affine).
3. **final modulation** (`norm_out.linear`, **has bias**): `ms = silu(vec) @ W^T + b` ->
   {2*3072=6144}; split into `(scale, shift)` each {3072} (**scale first, then shift** --
   `Export.wl:fullFinalLayer` `get_scale=PartLayer[1]`, `get_shift=PartLayer[2]`).
4. `m = (1 + scale) * n + shift`.
5. `out = m @ proj_out.weight^T` (+ bias if present) -> {256, **128**} (= velocity).
   (`proj_out`: 3072 -> 128.)

The velocity {256,128} is returned to the sampler (§G), which does `z <- z + dt * v`.
After 4 steps, the final `z {256,128}` goes to the VAE (§F).

---

## D. Multi-axis RoPE (the parity-critical part)

FLUX.2 uses **4-axis** rotary embeddings (`axes_dims_rope = {32,32,32,32}`, sum = 128 = head_dim),
`theta = 2000`. Positions are 4-tuples `{T,H,W,L}` (`Generate.wl`/`RoPE.wl:Flux2PosEmbed`):

- **image tokens**: `pos = {0, h, w, 0}` for the 16x16 grid (T=0, H=row, W=col, L=0).
- **text tokens**: `pos = {0, 0, 0, i}` for i in 0..511 (only the 4th axis varies).
  (`Qwen3TextIds`, `Generate.wl:txtPosIds`.)

**Frequency build** (per axis a, dim `d_a = 32`; `RoPE.wl:RoPEFrequencies` + DIFF
`get_1d_rotary_pos_embed(..., repeat_interleave_real=True)`):

```
for i in 0 .. d_a/2 - 1:   theta_i = 1 / 2000^(2 i / d_a)        # 16 freqs per axis
freqs_a = [theta_0, theta_1, ..., theta_15]                      # length 16
# interleave-repeat each freq:  -> length 32
freqs_a_rep = [theta_0, theta_0, theta_1, theta_1, ..., theta_15, theta_15]
angles_a[pos] = pos[a] * freqs_a_rep                             # outer product, {S, 32}
cos_a = cos(angles_a);  sin_a = sin(angles_a)                    # {S, 32}
```

Concatenate the four axes on the feature dim:
`cos = concat(cos_T, cos_H, cos_W, cos_L)` -> {S, 128}; same for sin.
(`Flux2PosEmbed` does `ArrayFlatten[{cosList}]`; DIFF `torch.cat(cos_out, dim=-1)`.)
cos/sin are reshaped to {S, 1, 128} for head broadcast.

**Application to q,k** (per head, **INTERLEAVED**; `use_real_unbind_dim = -1` in DIFF):

```
# x: {S, H, 128}; cos,sin: {S, 1, 128}
x_pairs   = reshape(x, {S, H, 64, 2})              # adjacent pairs (x[2i], x[2i+1])
x_real    = x_pairs[..., 0]                         # even indices
x_imag    = x_pairs[..., 1]                         # odd indices
x_rotated = flatten(stack([-x_imag, x_real], -1))   # -> [-x1, x0, -x3, x2, ...] interleaved
out       = x * cos + x_rotated * sin
```

This is **interleaved** rotation (pairs are adjacent `(x[2i], x[2i+1])`), matched by the
interleave-repeated freqs `[f0,f0,f1,f1,...]`. Confirmed by:
- DIFF `embeddings.apply_rotary_emb` use_real_unbind_dim==-1:
  `x_real,x_imag = x.reshape(...,-1,2).unbind(-1); x_rotated = stack([-x_imag,x_real],-1).flatten(3)`.
- DIFF `get_1d_rotary_pos_embed` `repeat_interleave_real=True`: `freqs.cos().repeat_interleave(2)`.
- `Generate.wl:mlApplyRoPEInterleaved` (reshape `{S,H,D/2,2}`, `concat([-x2, x1])`).
- `RoPE.wl:RotateHalf` (reshape to `{n,2}`, output `[-x2, x1]` per pair).

> **DO NOT** implement the half-split convention (`x[:64]`, `x[64:]`, rotate as
> `concat(-x2, x1)`) for the **FLUX.2** transformer -- that is `use_real_unbind_dim=-2`
> and gives wrong results. (The Qwen3 encoder DOES use the half-split convention, §E --
> they are different. This is the #1 cross-port pitfall.)

---

## E. Qwen3-4B text encoder forward

Canonical: `Generate.wl:Qwen3EncodeMLX`. 36 decoder layers, GQA, **causal + padding mask**,
extract after layers 8/17/26, concat -> {512, 7680}. The tokenizer is **host-side**, out of
scope for the thvm forward (see §H bottom).

**Setup (once):**
- `x = embedding_lookup(model.embed_tokens.weight, input_ids)` -> {512, 2560}.
- attention mask {512,512}: `mask[i,j] = -inf if (j>i) or input_ids[j]==pad(151643) else 0`
  (causal upper-triangular + padding columns). (`Generate.wl` `attnMaskData`.)
- RoPE cos/sin for Qwen3 (theta=**1000000**, head_dim 128, **half-split** convention):
  `freqSeq[i] = 1/1000000^(2i/128)` for i in 0..63; `freqs = outer(positions, freqSeq)` {512,64};
  `cos = [cos(freqs) | cos(freqs)]`, `sin = [sin(freqs) | sin(freqs)]` -> {512,128}
  (**Join/concat halves, NOT interleave**; `TextEncoders.wl:Qwen3RoPELayer`, `Generate.wl`).

**Per layer i (0..35)** (`model.layers.{i}.*`), pre-norm transformer:
1. `ln1 = RMSNorm(x, input_layernorm.weight)` (eps 1e-6, scale only).
2. `q = ln1 @ q_proj^T` {512, 32*128=4096}; `k,v = ln1 @ {k,v}_proj^T` {512, 8*128=1024} (no bias).
3. reshape: q {512,32,128}, k,v {512,8,128}.
4. **q/k RMSNorm per head** over head_dim (scales `self_attn.q_norm.weight`/`k_norm.weight`,
   eps 1e-6). (Qwen3 has qk-norm like FLUX.2.)
5. **RoPE** (half-split, §above) on q and k.
6. **GQA expand**: repeat each of the 8 kv-heads 4x -> k,v {512,32,128}
   (`broadcast_to {S,8,4,128} -> reshape {S,32,128}`).
7. attention: SDPA `softmax(QK^T/sqrt(128) + mask) V`, 32 heads -> {512,4096}.
8. `attnOut = (attn reshaped {512,4096}) @ o_proj^T`; `h = x + attnOut`.
9. `ln2 = RMSNorm(h, post_attention_layernorm.weight)`.
10. **SwiGLU MLP**: `gate = ln2 @ gate_proj^T`, `up = ln2 @ up_proj^T` (each {512,9728});
    `mlp = silu(gate) * up`; `down = mlp @ down_proj^T` {512,2560}; `x = h + down`.
11. if i in {8,17,26}: append `x` to `extracted`.

**Output**: `concat(extracted[8], extracted[17], extracted[26])` on feature axis ->
**{512, 3*2560 = 7680}**. (`Generate.wl` `MLOp["concatenate"][extracted, -1]`.)
This {512,7680} is the text embedding; `context_embedder` (Linear 7680->3072, no bias) maps it
to `txt {512,3072}` for the DiT (§A, §C.0).

> **Layer-index subtlety** (`TextEncoders.wl` comment): Python `hidden_states[9,18,27]` are the
> states *after* layers 8/17/26 (0-indexed) because `hidden_states[0]` is the embedding. thvm
> should extract **after running layer 8, 17, 26** (0-indexed loop), exactly as above.

**Chat-template tokenization boundary (host-side, NOT in thvm forward).** The prompt is wrapped:
`<|im_start|>user\n{prompt}<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n` then BPE'd,
padded/truncated to 512, pad id 151643. Special ids: im_start 151644, user 872, "\n" 198,
im_end 151645, assistant 77091, think 151667, "\n\n" 271, /think 151668.
(`TextEncoders.wl:Qwen3ChatTokenize`, `Qwen3Tokenizer.wl`.) thvm consumes the resulting
`input_ids {512}` only.

---

## F. VAE decoder forward (`AutoencoderKLFlux2` decoder)

Canonical: `Generate.wl:VAEDecodeMLX` (+ `Export.wl:BuildVAEDecoder`). Input is the denoised
latent `z {256,128}`; output is `image {3,256,256}`. Data flow is **NCHW** throughout
(conv ops transpose to NHWC internally on MLX; thvm can keep NCHW or its native conv layout).

**Pre-decode (denorm + unpatchify):**
1. **BatchNorm denorm**: `z = z * sqrt(bn.running_var + 1e-4) + bn.running_mean` (the VAE here
   normalizes the latent with a BN instead of the usual scaling/shift scalars -- CFG has no
   scaling_factor; `Generate.wl` uses `bn.running_var/mean`, `export_full_pipeline_q8.wls` uses
   eps 1e-5). -> {256,128}.
2. **unpatchify** {256,128} -> {1,32,32,32} NCHW:
   `reshape {16,16,32,2,2} -> transpose to {32,16,2,16,2} (axes (2,0,3,1,4)) -> reshape {1,32,32,32}`.
   (latent 16x16, 32 channels, 2x2 patch -> 32x32 spatial.)
3. **post_quant_conv**: 1x1 conv 32->32 (`post_quant_conv.weight/bias`, pad 0).
4. **conv_in**: 3x3 conv 32->512 pad 1 (`decoder.conv_in`).

**Mid block** (channels 512, `mid_block_add_attention=true`):
5. ResBlock(512,512) `decoder.mid_block.resnets.0`.
6. **Self-attention** `decoder.mid_block.attentions.0`: GroupNorm(32) -> 1x1 q,k,v ->
   reshape to {HW, C}=({1024},512) -> `softmax(QK^T/sqrt(512)) V` -> 1x1 out_proj -> + residual.
7. ResBlock(512,512) `decoder.mid_block.resnets.1`.

**Up blocks** (decoder traverses high->low channels; 3 resnets each; nearest-2x upsample +
3x3 conv between levels; first resnet of a level that changes channels has a 1x1 conv_shortcut):
8. up_blocks.0: ResBlock(512,512) x3, then **upsample x2** + conv (`upsamplers.0.conv`). -> 64x64.
9. up_blocks.1: ResBlock(512,512) x3, **upsample x2** + conv. -> 128x128.
10. up_blocks.2: ResBlock(512,**256**)[shortcut], ResBlock(256,256) x2, **upsample x2** + conv. -> 256x256.
11. up_blocks.3: ResBlock(256,**128**)[shortcut], ResBlock(128,128) x2, **NO upsample**. -> 256x256.

**ResBlock(inCh,outCh)** (`Generate.wl:mlResBlock`):
`h = GroupNorm(32, norm1) ; h = silu(h) ; h = conv3x3(conv1) ; h = GroupNorm(32, norm2) ;
 h = silu(h) ; h = conv3x3(conv2) ; out = h + (conv1x1(conv_shortcut, x) if inCh!=outCh else x)`.
All GroupNorm eps 1e-6.

**Output head:**
12. GroupNorm(32) `decoder.conv_norm_out` -> silu -> 3x3 conv 128->3 `decoder.conv_out` -> {3,256,256}.
13. `image = clip((out + 1)/2, 0, 1)`, CHW->HWC, RGB. (`Generate.wl` tail / `DecodeLatent`.)

> Note the WL `VAE.wl` uses `BatchNormalizationLayer[]` placeholders where the real net uses
> **GroupNorm(32)** -- trust `Generate.wl`/`Export.wl:VAEGroupNorm` (GroupNorm, 32 groups).
> The conv weight layout in safetensors is PyTorch OIHW `{out,in,kh,kw}`; `Generate.wl`
> transposes to OHWI for MLX -- thvm should use whatever layout its conv expects.

---

## G. Sampler: 4-step Euler flow-matching

Canonical: `Generate.wl:FluxSamplerMLX` (and identical schedule in `export_full_pipeline_q8.wls`).
`FlowMatchEulerDiscreteScheduler` (CFG model_index), **time-shifted** (mflux/diffusers style).

**Schedule (numSteps = 4, imgSeqLen = 256):**
```
a1 = 8.73809524e-5 ; b1 = 1.89833333
a2 = 0.00016927    ; b2 = 0.45666666
m200 = a2*256 + b2 ;  m10 = a1*256 + b1
a    = (m200 - m10)/190 ; b = m200 - 200*a
mu   = a*4 + b                                  # resolution/step-dependent time shift
sigmasLin = Subdivide(1.0, 1/4, 3)              # = [1.0, 0.75, 0.5, 0.25]  (numSteps points)
sigmas[k] = exp(mu) / (exp(mu) + (1/sigmasLin[k] - 1))   # k=0..3  (time-shifted)
sigmasFull = [sigmas[0], sigmas[1], sigmas[2], sigmas[3], 0.0]
dt[k]      = sigmasFull[k+1] - sigmasFull[k]    # 4 negative steps
```
`sigmasLin = [1.0, 0.75, 0.5, 0.5? ]` -> `Subdivide[1.0, 1/4, 3]` yields the 4 values
`[1.0, 0.75, 0.5, 0.25]`. The `exp(mu)/(exp(mu)+(1/s-1))` shift maps these to the actual sigmas;
sigmas are not round numbers (mu depends on the 256-token resolution). The final target is 0.

**Loop (4 iterations):**
```
z = normal({256, 128})                          # initial noise
for k in 0..3:
    sigma     = sigmas[k]
    sigmaNext = sigmas[k+1] if k<3 else 0.0
    dt        = sigmaNext - sigma                # < 0
    v         = MMDiT_velocity(z, txt, sigma)    # §C, full transformer
    z         = z + dt * v                       # Euler update
return z                                          # denoised latent -> VAE
```
(`Generate.wl` loop; `dt = sigmaNext - sigma`; `z = z + dt*velocity`.)

> A simpler linear schedule `sigma[i] = 1 - i/n` appears in `Export.wl:BuildSamplerLoop`
> (a CoreML placeholder) -- ignore it; the production schedule is the time-shifted one above.

---

## H. Tensor-name map (HF safetensors -> forward use)

Transformer weights live in `transformer/diffusion_pytorch_model.safetensors`
(single shard; **no .index.json**). Names confirmed by `Export.wl:BuildDoubleBlock`/
`BuildSingleBlockFlux2`/`BuildVelocityNetwork` and `Generate.wl` weight-id packing,
which read these exact keys. `{i}` = block index.

**Top-level (global):**

| tensor name | shape | feeds (forward §) |
|---|---|---|
| `x_embedder.weight` | {3072, 128} | img_in Linear z->hidden (C.1.1) |
| `context_embedder.weight` | {3072, 7680} | text proj 7680->3072 (A, E) |
| `time_guidance_embed.timestep_embedder.linear_1.weight` | {3072, 256} | time MLP in (C.1.3) |
| `time_guidance_embed.timestep_embedder.linear_2.weight` | {3072, 3072} | time MLP out (C.1.3) |
| `single_stream_modulation.linear.weight` | {9216, 3072} | single mod -> shift/scale/gate (C.1.4) |
| `double_stream_modulation_img.linear.weight` | {18432, 3072} | img mod (6 vectors) (C.1.4) |
| `double_stream_modulation_txt.linear.weight` | {18432, 3072} | txt mod (6 vectors) (C.1.4) |
| `norm_out.linear.weight` (+ `.bias`) | {6144, 3072} | final mod scale/shift (C.5.3) |
| `proj_out.weight` (+ `.bias`) | {128, 3072} | final hidden->latent (C.5.5) |

**Double block `transformer_blocks.{i}.` (i = 0..4):**

| tensor name | feeds |
|---|---|
| `attn.to_q.weight` / `attn.to_k.weight` / `attn.to_v.weight` | img q/k/v proj (C.2.3) |
| `attn.norm_q.weight` / `attn.norm_k.weight` | img per-head qk RMSNorm (C.2.5) |
| `attn.to_out.0.weight` | img attn output proj (C.2.9) |
| `attn.add_q_proj.weight` / `attn.add_k_proj.weight` / `attn.add_v_proj.weight` | txt q/k/v proj |
| `attn.norm_added_q.weight` / `attn.norm_added_k.weight` | txt per-head qk RMSNorm |
| `attn.to_add_out.weight` | txt attn output proj |
| `ff.linear_in.weight` / `ff.linear_out.weight` | img MLP in (->18432) / out (C.2.13-15) |
| `ff_context.linear_in.weight` / `ff_context.linear_out.weight` | txt MLP in/out |

(No per-block modulation tensors -- modulation is the shared `*_modulation.linear` above.
No bias on any double-block weight.)

**Single block `single_transformer_blocks.{i}.` (i = 0..19):**

| tensor name | shape | feeds |
|---|---|---|
| `attn.to_qkv_mlp_proj.weight` | {27648, 3072} | fused q,k,v (3*3072) + mlp_in (2*9216) (C.4.3) |
| `attn.norm_q.weight` / `attn.norm_k.weight` | {128} | per-head qk RMSNorm (C.4.4) |
| `attn.to_out.weight` | {3072, 12288} | fused (attn 3072 + mlp 9216) -> 3072 (C.4.8) |

**Qwen3 text encoder** `text_encoder/*.safetensors`, keys `model.layers.{i}.` (i=0..35):
`input_layernorm.weight`, `post_attention_layernorm.weight`,
`self_attn.{q,k,v,o}_proj.weight`, `self_attn.{q,k}_norm.weight`,
`mlp.{gate,up,down}_proj.weight`; plus `model.embed_tokens.weight` {151936,2560}.
(`TextEncoders.wl:LoadQwen3LayerWeights`, `Generate.wl:Qwen3EncodeMLX`.)

**VAE** `vae/diffusion_pytorch_model.safetensors`: `bn.running_mean`/`bn.running_var`
(latent denorm), `post_quant_conv.{weight,bias}`, `decoder.conv_in.*`,
`decoder.mid_block.resnets.{0,1}.{norm1,conv1,norm2,conv2}.*`,
`decoder.mid_block.attentions.0.{group_norm,to_q,to_k,to_v,to_out.0}.*`,
`decoder.up_blocks.{0..3}.resnets.{0,1,2}.{norm1,conv1,norm2,conv2[,conv_shortcut]}.*`,
`decoder.up_blocks.{0,1,2}.upsamplers.0.conv.*`,
`decoder.conv_norm_out.*`, `decoder.conv_out.*`. (`Generate.wl:VAEDecodeMLX`.)

> Naming note: native FLUX-style names (`img_in`, `txt_in`, `single_blocks.{i}.linear1`,
> `final_layer.linear`) appear in `Export.wl:$WeightMap` as **aliases**, but the real
> safetensors uses the diffusers names above (`x_embedder`, `context_embedder`,
> `single_transformer_blocks.{i}.attn.to_qkv_mlp_proj`, `proj_out`, ...). thvm reads the
> diffusers names.

---

## I. What thvm should loop vs. hand-write

**Loop / block factory (the big win).** The 25 transformer blocks are exactly **two templates**
parameterized only by `dim=3072, heads=24, head_dim=128, mlp_dim=9216`:

- **double block** template x5 (`transformer_blocks.{i}`), §C.2.
- **single block** template x20 (`single_transformer_blocks.{i}`), §C.4.

The WL `Export.wl` hand-builds these with ~200 lines of NetGraph each and then *manually
chains* them (`BuildMergedDoubleBlocks`, `BuildMergedSingleBlocks` build the connection lists
by hand) -- in thvm this collapses to a single `for i in range(5)` / `for i in range(20)`
over a parameter dict keyed by `i`. Similarly the **36 Qwen3 layers** (§E) are one template
x36 (`Generate.wl:Qwen3EncodeMLX` already does this as a clean Python-style loop -- port that
shape, not the `Export.wl` per-layer NetGraph copy-paste). The **shared modulation** is
computed once and broadcast to every block -- do not recompute per block.

The VAE up-blocks are a near-loop: 4 levels x (3 resnets + maybe upsample), differing only in
`(inCh, outCh, hasUpsample)`. A small table `[(512,512,T),(512,512,T),(512,256,T),(256,128,F)]`
+ a resnet helper covers it.

**Genuinely per-component (hand-write once, not in the block loop):**

- **Multi-axis RoPE** (§D): the 4-axis interleaved freq build + position-id construction
  (`{0,h,w,0}` img, `{0,0,0,i}` txt). cos/sin are computed once and reused by all blocks.
  This is the single highest-risk parity item (interleaved vs half-split -- and the Qwen3
  encoder uses the *other* convention, §E).
- **Timestep sinusoidal embed + time MLP + the three modulation projections** (§C.1) -- once per step.
- **Joint-attention concat order** (text-first, §C.2.7 / bridge §C.3) -- a fixed wiring choice,
  but a known footgun (the stale `Transformer.wl` got it backwards).
- **Final layer** (LayerNorm + final modulation w/ bias + proj_out + unpatchify) (§C.5).
- **VAE decoder** (§F): conv/groupnorm/resnet/attention/upsample -- structurally unlike the DiT,
  plus the BN-denorm + unpatchify pre-step.
- **Sampler schedule** (§G): the time-shifted sigma formula (resolution-dependent `mu`).
- **Tokenizer** (§E bottom): host-side BPE + chat template; thvm only consumes `input_ids`.

**Copy-paste in the WL impl that a thvm loop eliminates:** `Export.wl` lines ~590-921 (the two
block builders are each fully written out), and ~1003-1263 (`BuildMergedSingleBlocksA/B`,
`BuildMergedSingleBlocks`, `BuildMergedDoubleBlocks` -- three near-identical hand-rolled chain
constructors that exist only to satisfy CoreML's ~15-layer graph limit; thvm has no such limit
and needs exactly one loop each).

---

### Cross-impl disagreements flagged (collected)

1. **Double/single-block norm**: `Generate.wl` + `Export.wl` + DIFF use **LayerNorm**
   (standardize) for the pre-attention/pre-MLP norm; the **stale** `Transformer.wl` uses
   **RMSNorm**. Trust LayerNorm (eps 1e-6, no affine). The **q/k** norm is separately RMSNorm
   per-head in all impls.
2. **Joint-attention K/V concat order**: text-first (`concat(txt, img)`) per DIFF, `Export.wl`,
   `Generate.wl`. Stale `Transformer.wl:FluxDoubleBlock` is img-first -- wrong.
3. **Latent size**: production is **256 img tokens / 16x16 latent / 32x32 feature map** for
   256x256 output; `FluxGenerate`'s `Round[imgSize/32]` is a latent-side stride, not imgSeqLen.
4. **VAE latent denorm**: a **BatchNorm** (`bn.running_mean/var`), not a scalar
   scaling/shift_factor (CFG has none). eps differs between scripts (1e-4 vs 1e-5); use the
   value shipped in the production sampler unless parity testing says otherwise.
5. **Sampler schedule**: time-shifted FlowMatchEuler (§G), not the linear `1 - i/n` CoreML stub.

---

## Summary

- FLUX.2-klein-4B = **Qwen3-4B text encoder** (36 GQA layers, extract@8/17/26 -> {512,7680})
  -> **context_embedder -> {512,3072}**; **MMDiT** (5 double + 20 single blocks, dim 3072 /
  24 heads / 128 head_dim / mlp 9216, shared modulation, 4-axis RoPE theta=2000) producing a
  velocity over a **{256,128}** latent; a **4-step time-shifted Euler** flow-matching loop; then
  **AutoencoderKLFlux2** decode (BN-denorm + 2x2 unpatchify + conv/resnet/mid-attn/3x upsample)
  to a 256x256 RGB image.
- The **RoPE is interleaved** (`use_real_unbind_dim=-1`, freqs `repeat_interleave(2)`); the
  **Qwen3 RoPE is half-split** -- mixing them up is the top parity bug. All norms eps=1e-6;
  all transformer Linears are bias-free except `norm_out.linear`/`proj_out`.
- The 25 DiT blocks + 36 Qwen3 layers are **3 looped templates**; only RoPE, modulation,
  the final layer, the VAE, and the scheduler are genuinely per-component. The WL source
  hand-unrolls all blocks (CoreML constraint) -- thvm replaces that with three `for` loops.
- All facts cited to WL `Generate.wl`/`Export.wl`/`RoPE.wl`/`TextEncoders.wl`/`VAE.wl` +
  HF configs + diffusers `transformer_flux2.py`; disagreements between the three WL
  implementations are flagged inline (norm type, concat order, latent size, denorm, schedule).

File: `/Users/swish/src/thvm/docs/flux_forward_spec.md`
