(* FluxGenerate.wl -- the end-to-end FLUX.2-klein-4B text->image generator for
   thvm.  Chains the four FLUX pieces (Get-loaded alongside this file) into one
   `FluxGenerate[prompts, opts]` entry point:

     prompt(s) (String | {String..})
       -> qwTokenize        host byte-level BPE + Qwen chat template -> {512} ids
       -> qwenEncode        Qwen3-4B text encoder -> {512, 7680} text embedding
       -> context_embedder  (inside fxTransformer) -> {512, 3072}
       -> fxSampleEager     4-step Euler flow-match over the velocity net -> z {S_img,128}
       -> vaeDecoder        AutoencoderKLFlux2 decode -> image {3, H, W} in [0,1]

   Run in two memory-isolated stages (the Qwen 4B encoder + the DiT exceed the
   30GB Metal live-buffer ceiling together): STAGE 1 encodes every prompt in a
   throwaway context and reads the embeddings to the host, then destroys it;
   STAGE 2 loads the transformer + VAE ONCE and loops the sampler+decode over the
   batch.  The transformer weights + RoPE are uploaded once and reused for every
   image, and the per-block tile kernels compile once (the cold cost), so the
   second image of a batch is warm -- not another cold start.  The sampler runs
   EAGER (no TJit): a JIT-captured sampler must rebind the {1,3072} timestep
   embedding as an input, which trips a Metal tile-JIT codegen bug.

   Image size threads through everything: gridH = H/16, gridW = W/16 (the VAE
   downsamples /8 and the patchify packs 2x2, so the patched-latent side is
   image/16), S_img = gridH*gridW image tokens, z0 {S_img,128}, the RoPE table
   is built for that grid, fxSigmas uses S_img, and vaeDecoder unpatchifies back
   to {3, 16*gridH, 16*gridW} = {3, H, W}.

   Get-loaded after THVMLink` AND the four flux pieces (this file Gets them).
   Follows wl/GUIDE.md style. *)

Get[FileNameJoin[{DirectoryName[$InputFileName], "FluxForward.wl"}]];
Get[FileNameJoin[{DirectoryName[$InputFileName], "FluxSampler.wl"}]];
Get[FileNameJoin[{DirectoryName[$InputFileName], "FluxVAE.wl"}]];
Get[FileNameJoin[{DirectoryName[$InputFileName], "QwenEncoder.wl"}]];

(* ============================================================
   Host-side byte-level BPE tokenizer (Qwen2/GPT-4 style).
   Deterministic, ~100 lines: GPT-2 byte->unicode map + the Qwen
   pre-tokenization regex + greedy rank-ordered BPE merge per pre-token,
   then the Qwen chat template wrapped around the user prompt.
   ============================================================ *)

(* default tokenizer dir under the FLUX.2-klein cache *)
fxTokenizerDir[] := Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b/tokenizer";

(* GPT-2 byte->unicode: the 188 printable bytes map to themselves, the other 68
   map to U+0100.. so every byte is a single printable codepoint (avoids
   whitespace/control chars inside the merge strings). *)
qwByteToUnicode[] := Module[{bs, cs, n, b},
    bs = Join[Range[33, 126], Range[161, 172], Range[174, 255]];
    cs = bs;  n = 0;
    Do[ If[ !MemberQ[bs, b], AppendTo[bs, b];  AppendTo[cs, 256 + n];  n++], {b, 0, 255}];
    AssociationThread[bs -> FromCharacterCode /@ cs]]

(* load + memoise the tokenizer tables (vocab string->id, merge-pair->rank,
   byte->unicode, special-token ids) from a tokenizer dir. *)
qwTokenizerData[dir_] := qwTokenizerData[dir] = Module[
    {vocab, added, mergeLines, mergePairs, bpeRank, b2u, specials},
    vocab = Import[FileNameJoin[{dir, "vocab.json"}], "RawJSON"];
    added = Import[FileNameJoin[{dir, "added_tokens.json"}], "RawJSON"];
    vocab = Join[vocab, added];
    mergeLines = Select[StringSplit[Import[FileNameJoin[{dir, "merges.txt"}], "Text"], "\n"],
        (# =!= "" && !StringStartsQ[#, "#version"]) &];
    mergePairs = StringSplit[#, " "] & /@ mergeLines;
    (* rank key = "a\nb" (newline can't occur in a byte-mapped piece) *)
    bpeRank = AssociationThread[
        (StringJoin[#[[1]], "\n", #[[2]]] & /@ mergePairs) -> Range[Length[mergePairs]]];
    b2u = qwByteToUnicode[];
    specials = <|"im_start" -> 151644, "im_end" -> 151645,
                 "think" -> 151667, "think_end" -> 151668, "pad" -> 151643|>;
    <|"vocab" -> vocab, "rank" -> bpeRank, "b2u" -> b2u, "special" -> specials|>]

(* the Qwen pre-tokenization regex (GPT-4 split: case-insensitive contractions,
   letter runs, single digits, punctuation runs, whitespace).  WL's PCRE
   supports \p{L}/\p{N}/(?i:..) directly. *)
$qwPretokRegex = "(?i:'s|'t|'re|'ve|'m|'ll|'d)|[^\\r\\n\\p{L}\\p{N}]?\\p{L}+|\\p{N}| ?[^\\s\\p{L}\\p{N}]+[\\r\\n]*|\\s*[\\r\\n]+|\\s+(?!\\S)|\\s+";

(* greedy BPE: repeatedly merge the lowest-rank adjacent pair (leftmost on ties)
   until no mergeable pair remains.  `mapped` is the byte->unicode string. *)
qwBpe[mapped_String, bpeRank_] := Module[{word, n, pairs, ranks, best, bi, a, b, new, i},
    word = Characters[mapped];
    If[ Length[word] < 2, Return[word]];
    While[True,
        n = Length[word];
        pairs = Table[word[[i]] <> "\n" <> word[[i + 1]], {i, 1, n - 1}];
        ranks = Lookup[bpeRank, pairs, Infinity];
        best = Min[ranks];
        If[ best === Infinity, Break[]];
        bi = First @ FirstPosition[ranks, best];
        a = word[[bi]];  b = word[[bi + 1]];
        new = {};  i = 1;
        While[i <= n,
            If[ i < n && word[[i]] === a && word[[i + 1]] === b,
                AppendTo[new, a <> b];  i += 2,
                AppendTo[new, word[[i]]];  i += 1]];
        word = new;
        If[ Length[word] === 1, Break[]]];
    word]

(* encode one plain text fragment (no special tokens) -> list of ids:
   NFC-normalise, pre-tokenise, byte->unicode each pre-token, BPE-merge, lookup. *)
qwEncodeText[text_String, td_] := Module[{nfc, pretoks, vocab, b2u, rank},
    vocab = td["vocab"];  b2u = td["b2u"];  rank = td["rank"];
    nfc = CharacterNormalize[text, "NFC"];
    pretoks = StringCases[nfc, RegularExpression[$qwPretokRegex]];
    Flatten @ Map[
        Function[pt, vocab[#] & /@ qwBpe[StringJoin[b2u /@ ToCharacterCode[pt, "UTF-8"]], rank]],
        pretoks]]

(* wrap a user prompt in the Qwen chat template (enable_thinking=False,
   add_generation_prompt=True), with special tokens inserted as raw ids:
     <|im_start|>user\n{prompt}<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n
   -> the unpadded id list. *)
qwChatEncode[prompt_String, td_] := With[{sp = td["special"]},
    Join[
        {sp["im_start"]}, qwEncodeText["user\n" <> prompt, td], {sp["im_end"]},
        qwEncodeText["\n", td], {sp["im_start"]}, qwEncodeText["assistant\n", td],
        {sp["think"]}, qwEncodeText["\n\n", td], {sp["think_end"]},
        qwEncodeText["\n\n", td]]]

(* tokenize a prompt to {ids, attMask}, right-padded / truncated to seqLen with
   the pad id (151643); attMask is 1 on real tokens, 0 on pad. *)
qwTokenize[prompt_String, td_, seqLen_:512] := Module[{ids, pad, n, mask},
    ids = qwChatEncode[prompt, td];
    pad = td["special"]["pad"];
    n = Length[ids];
    If[ n >= seqLen,
        {Take[ids, seqLen], ConstantArray[1, seqLen]},
        {Join[ids, ConstantArray[pad, seqLen - n]],
         Join[ConstantArray[1, n], ConstantArray[0, seqLen - n]]}]]

(* ============================================================
   Weight loaders (one cold pass each, shared across the batch).
   ============================================================ *)

(* Each loader memoises into a private Module-local Association symbol (so the
   per-name upload happens once) and returns the name->TTerm closure.  Using a
   real symbol -- not a passed-in <||> literal -- so `cache[n] = ...` is a valid
   part-assignment. *)

(* transformer: bf16 contiguous loader.  Upload each weight AS STORED ({out,in})
   straight to the device -- ONE buffer per weight, ~weights-on-disk resident.
   fxLinear matmuls against the Transpose[w] VIEW (folded into the tiled
   tensor-core matmul's address, no separate dispatch or buffer; see fxLinear in
   FluxForward.wl), so there is NO pre-transpose.  The earlier pre-transpose
   loader (realize Transpose[w] on the host, then upload) held the weight TWICE
   -- the host {out,in} W^T plus the device upload (~2x: the flux-generate OOM).
   1-D RMSNorm gains upload as-is (never reach fxLinear / a matmul). *)
fxTransformerLoader[wt_, dev_] := Module[{cache = <||>},
    n |-> Lookup[cache, n, cache[n] = TToDevice[wt[n], dev]]]

(* Qwen text encoder: bf16, contiguous exactly like the transformer.  The
   diffusers q/k/v/o/gate/up/down weights are stored {out,in}; qwLayer reuses
   fxLinear = TMatMul[x {S,in}, Transpose[w]], reading the weight transposed via
   the tiled-transpose matmul -- so they upload AS STORED, no pre-transpose.  The
   embed table stays HOST-resident and un-transposed (qwEmbed host-gathers its
   rows). *)
fxQwenLoader[qwt_, dev_] := Module[{cache = <||>},
    n |-> Lookup[cache, n, cache[n] =
        If[ n === "model.embed_tokens.weight", qwt[n], TToDevice[qwt[n], dev]]]]

(* VAE: bf16 -> f32 (the VAE conv path wants f32), uploaded to the device.
   vaeDecoder asks for the latent-denorm stats as `bn_running_mean`/`bn_running_var`;
   the safetensors store them dotted (`bn.running_mean`/`bn.running_var`), so alias. *)
fxVaeKey[n_] := Switch[n,
    "bn_running_mean", "bn.running_mean", "bn_running_var", "bn.running_var", _, n];
fxVaeLoader[vt_, dev_] := Module[{cache = <||>},
    n |-> Lookup[cache, n, cache[n] =
        TRealize @ TToDevice[TUOpCast[vt[fxVaeKey[n]], "f32"], dev]]]

(* wsub[prefix]: the sub-Association of (key-with-prefix-stripped -> loaded TTerm)
   for every weight name under `prefix`.  `keys` is a List of names, so Select
   (not KeySelect, which is for Associations). *)
fxVaeSub[wf_, keys_][prefix_] := Association @ Map[
    (StringDrop[#, StringLength[prefix]] -> wf[#]) &,
    Select[keys, StringStartsQ[#, prefix] &]]

(* ============================================================
   Timestep embedding: sigma -> {1, dim} (sinusoid -> time MLP).
   sinusoid(256, cos-first) -> linear_1 (256->3072) -> SiLU -> linear_2
   (3072->3072), no bias.  Returned as a {1,3072} TTerm.
   ============================================================ *)
(* The {1,256} sinusoid keeps f32 (NOT cast to bf16): a bf16 cast of a
   leading-unit-axis {1,N} tensor trips a Metal tile-JIT codegen bug (degenerate
   `a0*0` index), and the time MLP is a tiny {1,256}x{256,3072} matmul where the
   f32-act/bf16-weight path costs nothing.  The result temb {1,3072} is the
   sampler's rebound input. *)
fxTembFn[wf_, dev_][sigma_] := With[{
        s = TRealize @ TToDevice[TTensorCreate[{fxTimestepSinusoid[sigma, 256]}], dev]},
    fxLinear[TSiLU @ fxLinear[s, wf["time_guidance_embed.timestep_embedder.linear_1.weight"]],
             wf["time_guidance_embed.timestep_embedder.linear_2.weight"]]]

(* ============================================================
   FluxGenerate[prompts, opts] -- the public entry point.
   ============================================================ *)

Options[FluxGenerate] = {
    "ImageSize" -> {256, 256}, "Seed" -> 0, "Device" -> "metal",
    "NumSteps" -> 4, "ModelDir" -> Automatic, "ReturnImages" -> True};

FluxGenerate[prompt_String, opts : OptionsPattern[]] := First @ FluxGenerate[{prompt}, opts];

FluxGenerate[prompts_List, opts : OptionsPattern[]] := Module[
    {imgSize, seed, dev, nSteps, modelDir, returnImages, w, h, gridH, gridW, simg, stxt,
     tokDir, td, tfPath, qwPaths, vaePath, fxCfg, qwCfg, vaeCfg,
     ctxEnc, encHost, wt, wfT, vt, vKeys, wfV, wsubV,
     ca, rope, rc, rs, tembFn, sigmas, results},

    imgSize = OptionValue["ImageSize"];
    seed = OptionValue["Seed"];  dev = OptionValue["Device"];
    nSteps = OptionValue["NumSteps"];  returnImages = OptionValue["ReturnImages"];
    modelDir = OptionValue["ModelDir"] /. Automatic ->
        Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b";
    {w, h} = imgSize;
    (* patched-latent grid: image/16 (VAE /8 * patch /2).  S_img = gridH*gridW. *)
    gridH = Round[h/16];  gridW = Round[w/16];  simg = gridH gridW;  stxt = 512;

    tokDir = FileNameJoin[{modelDir, "tokenizer"}];
    tfPath = FileNameJoin[{modelDir, "transformer", "diffusion_pytorch_model.safetensors"}];
    qwPaths = FileNameJoin[{modelDir, "text_encoder", #}] & /@
        {"model-00001-of-00002.safetensors", "model-00002-of-00002.safetensors"};
    vaePath = FileNameJoin[{modelDir, "vae", "diffusion_pytorch_model.safetensors"}];

    fxCfg = <|"eps" -> 1.*^-6, "num_double" -> 5, "num_single" -> 20, "heads" -> 24, "head_dim" -> 128|>;
    (* only run through the last captured layer (26): layers 27..35 unused. *)
    qwCfg = <|"heads" -> 32, "kv_heads" -> 8, "head_dim" -> 128, "eps" -> 1.*^-6,
              "theta" -> 1000000, "layers" -> 27, "captureLayers" -> {8, 17, 26}|>;
    vaeCfg = <|"eps" -> 1.*^-6, "epsBn" -> 1.*^-4, "gridH" -> gridH, "gridW" -> gridW|>;

    td = qwTokenizerData[tokDir];
    sigmas = fxSigmas[simg, nSteps];

    (* --- STAGE 1: text-encode every prompt in an ISOLATED context, read each
       embedding back to the HOST, then DESTROY the context.  The two big models
       (Qwen 4B + the DiT) together exceed the 30GB live-buffer ceiling on this
       box, and there is no targeted device free -- only TContextDestroy frees a
       runtime's buffers wholesale.  Carrying the encodings across the boundary as
       host arrays ({stxt,7680} f32, ~15MB each) lets the Qwen weights be fully
       released before the transformer loads, so the two models are NEVER
       co-resident. --- *)
    encHost = TInContext[ctxEnc = TContextNew[dev],
        Module[{qw, wfq, e},
            qw = Join[TSafeTensorLoad[qwPaths[[1]]], TSafeTensorLoad[qwPaths[[2]]]];
            wfq = fxQwenLoader[qw, dev];
            e = (Normal @ qwenEncode[Sequence @@ qwTokenize[#, td, stxt], wfq, qwCfg]) & /@ prompts;
            e]];
    TContextDestroy[ctxEnc];                             (* frees the Qwen buffers *)
    (* clear slot-0's heap: the Qwen encode registered symbolic-dim / fresh-label
       state that otherwise leaks into the transformer's tile-JIT codegen as an
       unresolved kvar (`/*?*/` index -> Metal compile failure).  encHost is pure
       host data, so the reset only drops device/heap state. *)
    TReset[];

    (* --- STAGE 2: load transformer + VAE in a FRESH context, build RoPE, then
       re-upload each host embedding and sample + decode. --- *)
    wt = TSafeTensorLoad[tfPath];
    wfT = fxTransformerLoader[wt, dev];
    vt = TSafeTensorLoad[vaePath];  vKeys = Keys[vt];
    wfV = fxVaeLoader[vt, dev];  wsubV = fxVaeSub[wfV, vKeys];

    (* 4-axis RoPE table {stxt+simg, 128}, text-first; reshaped per block inside
       the transformer.  Built once for this grid.  Activations ride the working
       dtype (bf16 on Metal) so every matmul is bf16(act) x bf16(weight) and stays
       on the Metal simdgroup-matrix gemm: a mixed f32-act x bf16-weight matmul
       falls off the gemm classifier onto a CPU scalar path (a multi-GB expand at
       these shapes).  fxLinear is dtype-agnostic, so this is a load-time choice. *)
    ca = If[ dev === "cpu", TRealize[#] &, (TRealize @ TToDevice[TUOpCast[#, "bf16"], dev]) &];
    rope = fxRopeTable[gridH, gridW, stxt];
    rc = ca @ TTensorCreate[rope["cos"]];
    rs = ca @ TTensorCreate[rope["sin"]];
    tembFn = fxTembFn[wfT, dev];

    (* --- per-prompt sample + decode.  Each prompt re-uploads its host embedding
       as enc0 and runs the eager sampler + VAE over the resident weights + RoPE.
       The FIRST image pays the kernel compile (cold); the rest reuse the compiled
       tile kernels, so the second image of a batch is warm.  Per-image wall time
       is printed so the cold->warm amortization is visible. --- *)
    results = MapIndexed[
        Function[{encArr, idx},
            Module[{enc0, z, zPacked, img, dt},
                {dt, img} = AbsoluteTiming @ Module[{e, zz, zp},
                    e = ca @ TTensorCreate @ NumericArray[encArr, "Real32"];      (* {stxt,7680} *)
                    (* z0 ~ N(0,1) {simg,128}, seeded per item for reproducibility;
                       cast to the working dtype so the x_embedder matmul stays bf16. *)
                    SeedRandom[seed + First[idx]];
                    zz = ca @ TTensorCreate @ NumericArray[
                        RandomVariate[NormalDistribution[], {simg, 128}], "Real32"];
                    zp = fxSampleEager[zz, e, sigmas, tembFn, rc, rs, wfT, fxCfg, ca];
                    (* the transformer runs bf16 -> zp is bf16; the VAE weights are
                       f32, so cast the latent to f32 for the decoder. *)
                    TRealize @ vaeDecoder[TRealize @ TUOpCast[zp, "f32"], wfV, wsubV, vaeCfg]];
                WriteString["stdout", "  [FluxGenerate] image ", First[idx], "/", Length[encHost],
                    "  ", Round[dt, 1], " s", If[ First[idx] === 1, "  (cold: incl. kernel compile)", "  (warm)"], "\n"];
                $Output // Flush;
                If[ returnImages, fxToImage[img], Normal[img]]]],
        encHost];
    results]

(* Eager 4-step Euler flow-match: z <- z + (sigma[k+1]-sigma[k]) * v, with the
   velocity v recomputed each step from a fresh, concrete temb.  This does NOT
   TJit-wrap the velocity net: a TJit-captured sampler must rebind temb as an
   input, and a {1,3072} leading-unit-axis matmul input trips the Metal tile-JIT
   symbolic-stride codegen bug (`/*?*/` index).

   The Euler update (z + dt*v) is done on the HOST and the running latent is
   re-uploaded as a fresh leaf each step: a single transformer forward sits right
   at the 30GB Metal live-buffer ceiling, so the captured graph each step must be
   EXACTLY one bare forward -- adding even the dt*v device op on top tips it over
   (four stacked forwards in one graph would, without TJit's cross-step buffer
   reuse, pin every step's peak at once).  Reading v to the host, updating there,
   and re-uploading {S_img,128} (a few hundred KB) severs the retention chain so
   each step starts from a clean device heap with only the weights/RoPE resident.
   Returns the device latent for the VAE. *)
fxSampleEager[z0_, enc0_, sigmas_, tembFn_, rc_, rs_, wf_, cfg_, ca_] := Module[
    {zh = Normal[z0], k, dt, vh},
    Do[ dt = sigmas[[k + 1]] - sigmas[[k]];
        vh = Normal @ TRealize @ fxTransformer[
            ca @ TTensorCreate @ NumericArray[zh, "Real32"],
            enc0, tembFn[sigmas[[k]]], rc, rs, wf, cfg];
        zh = zh + dt vh;                                 (* host Euler step *)
        TGCCollect[],
        {k, 1, Length[sigmas] - 1}];
    ca @ TTensorCreate @ NumericArray[zh, "Real32"]]

(* {3,H,W} pixels in [0,1] -> an Image (clip for any tiny fp overshoot). *)
fxToImage[t_] := Image[Clip[Normal[t], {0., 1.}], Interleaving -> False, ColorSpace -> "RGB"]
