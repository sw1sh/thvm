(* qwen_encoder_parity_chunked.wls -- the Qwen3-4B encoder parity run split into
   layer chunks across separate kernel invocations, for a memory-pressured host.

   The single-process parity (qwen_encoder_parity.wls) needs ~8 GB free RAM: it
   pages in both 8 GB bf16 shards plus the per-layer f32 activations, which a
   loaded host (swap near full) can SIGKILL mid-run.  This runner processes a
   [START..END] layer range in one kernel, checkpoints the hidden state to /tmp,
   and loads ONLY the shard(s) that range touches (shard 1 = embed + layers
   0..20; shard 2 = layers 20..35), so each chunk's resident set stays ~3-5 GB.

   Env args: QW_START, QW_END (inclusive layer range), QW_IN ("embed" or a
   checkpoint path), QW_OUT (output checkpoint).  Captures hs_9/18/27 to
   /tmp/qwen_cap_{8,17,26}.st; the final text_emb is the [hs9|hs18|hs27] concat
   of those three (assemble + compare to /tmp/flux_e2e_ref/inputs.safetensors).

   Three-chunk run (cwd = repo root, CPU/DEV unset):
     QW_START=0  QW_END=8  QW_IN=embed          QW_OUT=/tmp/qwen_h8.st  ...
     QW_START=9  QW_END=17 QW_IN=/tmp/qwen_h8.st QW_OUT=/tmp/qwen_h17.st ...
     QW_START=18 QW_END=26 QW_IN=/tmp/qwen_h17.st QW_OUT=/tmp/qwen_h26.st ...
   prefix each with  THVM_MAX_BUF_BYTES=2000000000 wolframscript -file <this> *)

PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"];
Get["wl/THVMLink/Examples/flux/FluxForward.wl"];
Get["wl/THVMLink/Examples/flux/QwenEncoder.wl"];
TInit[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);
corr[a_, b_] := Correlation[Flatten@N@a, Flatten@N@b];
maxd[a_, b_] := Max@Abs@Flatten[N@a - N@b];

start  = ToExpression@Environment["QW_START"];
end    = ToExpression@Environment["QW_END"];
inSpec = Environment["QW_IN"];
outF   = Environment["QW_OUT"];

ref = TSafeTensorLoad["/tmp/flux_qwen_ref/qwen_hs.safetensors"];
ids = Normal@ref["input_ids"];  am = Normal@ref["attention_mask"];
blob = "/Volumes/T9/hf-cache/hub/models--black-forest-labs--FLUX.2-klein-4B/blobs/";

(* load only the shard(s) this chunk's layer range (+ embed) touches *)
pr["chunk layers ", ToString@start, "..", ToString@end, " loading shards ..."];
needS1 = (inSpec === "embed") || (start <= 20);
needS2 = (end >= 20);
w1 = If[needS1, TSafeTensorLoad[blob <> "8c0506e7f4936fa7e26183a4fd8da4e2bdbc5990ba64ae441f965d51228f36ea"], <||>];
w2 = If[needS2, TSafeTensorLoad[blob <> "82f2bd839378541b0557bfabaf37c7d3d637071fdcb73302dedd7cf61162ce07"], <||>];
wf = Join[w1, w2];
pr["loaded shards: s1=", ToString@needS1, " s2=", ToString@needS2, " keys=", ToString@Length@Keys@wf];

{cos, sin} = qwRopeCosSin[Length@ids, 128, 1000000.];
addMask = qwAddMask[am];
lcfg = <|"heads" -> 32, "kv_heads" -> 8, "head_dim" -> 128, "eps" -> 1.*^-6|>;

xh = If[inSpec === "embed",
    Normal @ qwEmbed[wf["model.embed_tokens.weight"], ids],
    Normal @ TSafeTensorLoad[inSpec]["h"]];
pr["input ready, max ", ToString[Max@Abs@Flatten@xh, InputForm]];

refMap = <|0 -> "hs_1", 1 -> "hs_2", 8 -> "hs_9", 17 -> "hs_18", 26 -> "hs_27"|>;
x = TTensorCreate[xh];
Do[ x = TTensorCreate[Normal @ TRealize @ qwLayer[x, cos, sin, addMask, qwLayerW[wf, i], lcfg]];
    If[KeyExistsQ[refMap, i],
       With[{o = Normal@x, r = Normal@ref[refMap[i]]},
        pr["  ", refMap[i], " (layer ", ToString@i, ")  corr = ", ToString[corr[o, r], InputForm],
           "  max|d| = ", ToString[maxd[o, r], InputForm]]]];
    If[MemberQ[{8, 17, 26}, i],
       TSafeTensorSave[<|"h" -> x|>, "/tmp/qwen_cap_" <> ToString@i <> ".st"]],
    {i, start, end}];

TSafeTensorSave[<|"h" -> x|>, outF];

(* the last chunk assembles + validates the final {512, 7680} text_emb *)
If[end >= 26,
    With[{textEmb = TRealize @ fxConcat[
            {TSafeTensorLoad["/tmp/qwen_cap_8.st"]["h"],
             TSafeTensorLoad["/tmp/qwen_cap_17.st"]["h"],
             TSafeTensorLoad["/tmp/qwen_cap_26.st"]["h"]}, 2],
          e2e = TSafeTensorLoad["/tmp/flux_e2e_ref/inputs.safetensors"]},
        pr["text_emb  corr = ", ToString[corr[Normal@textEmb, Normal@e2e["text_emb"]], InputForm],
           "   max|d| = ", ToString[maxd[Normal@textEmb, Normal@e2e["text_emb"]], InputForm]]]];

pr["chunk done, saved ", outF];
