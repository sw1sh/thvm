(* qwen_encoder_parity.wls -- incremental parity for the Qwen3-4B text encoder
   against a transformers ground truth.  Validates each stage (embedding,
   after-layer-0, after-layer-1, after-layer-8/17/26, final text_emb) by
   correlation + max|d| vs the reference safetensors dumps.

   Needs ~8 GB free host RAM (bf16 weights + f32 activations); on a
   memory-pressured host run the chunked variant (qwen_chunk) instead.

   Run:  wolframscript -file <this>  (cwd = repo root; CPU/DEV unset) *)

PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"];
Get["wl/THVMLink/Examples/flux/FluxForward.wl"];
Get["wl/THVMLink/Examples/flux/QwenEncoder.wl"];
TInit[];

pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);

corr[a_, b_] := With[{x = Flatten@N@a, y = Flatten@N@b},
    Correlation[x, y]];
maxd[a_, b_] := Max@Abs@Flatten[N@a - N@b];
report[name_, o_, r_] := pr[
    StringPadRight[name, 12], "  corr = ", ToString[corr[o, r], InputForm],
    "   max|d| = ", ToString[maxd[o, r], InputForm],
    "   ref max|.| = ", ToString[Max@Abs@Flatten@N@r, InputForm]];

(* --- reference dumps --- *)
ref  = TSafeTensorLoad["/tmp/flux_qwen_ref/qwen_hs.safetensors"];
e2e  = TSafeTensorLoad["/tmp/flux_e2e_ref/inputs.safetensors"];
ids  = Normal@ref["input_ids"];
am   = Normal@ref["attention_mask"];
pr["seq len ", ToString@Length@ids, "   real tokens ", ToString@Total@am];

(* --- weights: both bf16 shards, merged --- *)
blob = "/Volumes/T9/hf-cache/hub/models--black-forest-labs--FLUX.2-klein-4B/blobs/";
pr["loading shard 1 ..."];
w1 = TSafeTensorLoad[blob <> "8c0506e7f4936fa7e26183a4fd8da4e2bdbc5990ba64ae441f965d51228f36ea"];
pr["loading shard 2 ..."];
w2 = TSafeTensorLoad[blob <> "82f2bd839378541b0557bfabaf37c7d3d637071fdcb73302dedd7cf61162ce07"];
wf = Join[w1, w2];
pr["merged weight keys ", ToString@Length@Keys@wf];

cfg = <|"hidden" -> 2560, "heads" -> 32, "kv_heads" -> 8, "head_dim" -> 128,
        "eps" -> 1.*^-6, "theta" -> 1000000., "layers" -> 27,
        "captureLayers" -> {8, 17, 26}|>;

(* --- shared host-side rope + mask --- *)
{cos, sin} = qwRopeCosSin[Length@ids, cfg["head_dim"], cfg["theta"]];
addMask = qwAddMask[am];
lcfg = <|"heads" -> cfg["heads"], "kv_heads" -> cfg["kv_heads"],
         "head_dim" -> cfg["head_dim"], "eps" -> cfg["eps"]|>;

(* one layer: realize, then re-enter as a fresh host leaf so the prior layer's
   f32 weight casts free and the next layer recycles them (memory-bounded).
   qwLayerWHost streams the bf16 weights per layer (host f32, transient) so the
   8 GB of mmap weights never accumulate resident across 27 layers. *)
step[xx_, i_] := TTensorCreate[Normal @ TRealize @ qwLayer[xx, cos, sin, addMask, qwLayerWHost[wf, i], lcfg]];

(* === stage 1: embedding (hs_0) === *)
x = qwEmbed[wf["model.embed_tokens.weight"], ids];
report["hs_0", Normal@x, Normal@ref["hs_0"]];

(* === stages 2..: layers 0..26, capturing hs_1 / hs_2 / hs_9 / hs_18 / hs_27 === *)
cap = <||>;
Do[ x = step[x, i];
    Which[i == 0,  report["hs_1",  Normal@x, Normal@ref["hs_1"]],
          i == 1,  report["hs_2",  Normal@x, Normal@ref["hs_2"]],
          i == 8,  cap[9]  = x;  report["hs_9",  Normal@x, Normal@ref["hs_9"]],
          i == 17, cap[18] = x;  report["hs_18", Normal@x, Normal@ref["hs_18"]],
          i == 26, cap[27] = x;  report["hs_27", Normal@x, Normal@ref["hs_27"]]];
    pr["  [layer ", ToString@i, " done, thvm buf ", ToString[Round[TTotalBufBytes[]/1.*^6]], " MB]"],
    {i, 0, 26}];

(* === final: text_emb {512, 7680} = [hs9 | hs18 | hs27] per token === *)
textEmb = TRealize @ fxConcat[{cap[9], cap[18], cap[27]}, 2];
report["text_emb", Normal@textEmb, Normal@e2e["text_emb"]];

pr["DONE"];
