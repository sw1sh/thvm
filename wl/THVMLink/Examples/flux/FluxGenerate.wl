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
   1-D RMSNorm gains upload as-is (never reach fxLinear / a matmul).

   The weight stays a LAZY TToDevice (materialised on first matmul use): the
   full-forward sampler captures once with no per-block input rebind, so the
   lazy form is fine and keeps host residency to ~one weight at a time.  The
   per-block JIT sampler (fxSampleJitBlocked), which DOES rebind each block's
   weights as TJit inputs, needs them CONTIGUOUS (see fxBlockWeights) -- a lazy
   safetensor-view weight is staged by the tiled matmul into an internal tensor
   whose tid is not the declared input, so input_replace records ZERO rebind
   sites and the replay keeps block-0's weights. *)
fxTransformerLoader[wt_, dev_] := Module[{cache = <||>},
    n |-> Lookup[cache, n, cache[n] = TToDevice[wt[n], dev]]]

(* STREAMING transformer loader: the <10GB-resident lever via per-block weight
   eviction.  Unlike fxTransformerLoader (which TToDevice-caches EVERY weight,
   pinning all 25 blocks' ~7.75GB resident at once), this returns each weight as
   its bare LAZY disk-mmap TTerm -- no device cache, nothing retained.  When a
   weight feeds a Metal matmul, the runtime zero-copy WRAPS its mmap pages as a
   borrowed MTLBuffer (newBufferWithBytesNoCopy; the GPU reads the pages in
   place, no upload) -- so the ACTIVE weight footprint is whatever the current
   block touches, not the whole model.  After a block's matmuls retire, the
   streaming forward calls TDiskDropWeight on that block's weights to release the
   wrap + MADV_DONTNEED the disk pages, so the next block's pages take their
   place.  The few shared weights (embedders, modulation, norm_out/proj_out) are
   touched every step and never dropped; only the per-block weights stream.
   Memoised so a name maps to ONE disk TTerm across steps (re-faulted on use). *)
fxTransformerStreamLoader[wt_] := Module[{cache = <||>},
    n |-> Lookup[cache, n, cache[n] = wt[n]]]

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
   sampler's rebound input -- cast to the working dtype (bf16 on a GPU) so the
   block modulation matmuls (fxModLinear) run bf16(temb) x bf16(weight) on the
   tensor cores and stay on the batched Metal ICB (~0.6 vs ~4 s/step; an f32
   temb makes them MIXED f32xbf16, which falls to a slow CPU scalar expand).
   The {1,3072} output cast is past the {1,256}-sinusoid codegen bug -- the M=8
   pad in fxModLinear gives the modulation a concrete leading axis.  (A capture
   that re-reads such a bf16 weight across blocks used to bind a freed buffer --
   a JIT buffer-lifetime bug, fixed in src/jit/capture.c; it silently corrupted
   a later VAE decode.)  CPU stays f32 (its weights are f32, already matched). *)
fxTembFn[wf_, dev_][sigma_] := With[{
        s = TRealize @ TToDevice[TTensorCreate[{fxTimestepSinusoid[sigma, 256]}], dev]},
    With[{temb = fxLinear[TSiLU @ fxLinear[s, wf["time_guidance_embed.timestep_embedder.linear_1.weight"]],
                          wf["time_guidance_embed.timestep_embedder.linear_2.weight"]]},
        If[ dev === "cpu", temb, TRealize @ TUOpCast[temb, "bf16"]]]]

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
     ctxEnc, encHost, ctxT, ctxV, latents, sigmas, results},

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
            e
        ]
    ];
    TContextDestroy[ctxEnc];                             (* frees the Qwen buffers *)
    (* clear slot-0's heap: the Qwen encode registered symbolic-dim / fresh-label
       state that otherwise leaks into the transformer's tile-JIT codegen as an
       unresolved kvar (`/*?*/` index -> Metal compile failure).  encHost is pure
       host data, so the reset only drops device/heap state. *)
    TReset[];

    (* --- STAGE 2: load transformer + VAE in a FRESH context, build RoPE, then
       re-upload each host embedding and sample + decode. --- *)
    (* --- STAGE 2: transformer sampling in its OWN context.  The transformer
       weights + the (large) JIT velocity capture are freed by TContextDestroy
       before the VAE runs, so the e2e peak is max(stage), NOT transformer+VAE
       co-resident.  Each prompt's latent crosses the boundary as a HOST array
       ({S_img,128} f32, a few hundred KB). --- *)
    latents = TInContext[ctxT = TContextNew[dev],
        Module[{wt, wfT, ca, rope, rc, rs, tembFn, velJit},
            wt = TSafeTensorLoad[tfPath];
            (* STREAMING loader (no cache): each weight uploads fresh on access and
               is freed once its block's matmuls retire (the realize-boundary
               reclaim, THVM_FWD_RECLAIM), so only the ACTIVE block's weights stay
               resident -- the per-block sampler holds ~one block (~600MB) instead
               of all 7.75GB.  Paired with THVM_MMAP_NO_WILLNEED so the safetensors
               file isn't paged fully resident.  Validated: 5 double blocks at
               3.8GB RSS (cached loader was 8.8GB). *)
            wfT = fxTransformerLoader[wt, dev];
            (* bf16 working dtype: every matmul is bf16(act) x bf16(weight) on the
               Metal simdgroup-matrix gemm (a mixed f32xbf16 matmul falls to a CPU
               scalar expand).  fxLinear is dtype-agnostic, so this is a driver choice. *)
            ca = If[ dev === "cpu", TRealize[#] &, (TRealize @ TToDevice[TUOpCast[#, "bf16"], dev]) &];
            rope = fxRopeTable[gridH, gridW, stxt];
            rc = ca @ TTensorCreate[rope["cos"]];  rs = ca @ TTensorCreate[rope["sin"]];
            tembFn = fxTembFn[wfT, dev];
            (* capture the velocity net ONCE, shared across the whole batch (enc is a
               rebound input, not closed over) -- image 1 pays the cold capture, every
               later image is a warm replay. *)
            velJit = fxVelocityJit[rc, rs, wfT, fxCfg];
            MapIndexed[
                Function[{encArr, idx},
                    Module[{e, zz, zp, dt},
                        {dt, zp} = AbsoluteTiming @ Module[{ee, z},
                            ee = ca @ TTensorCreate @ NumericArray[encArr, "Real32"];   (* {stxt,7680} *)
                            SeedRandom[seed + First[idx]];
                            z = ca @ TTensorCreate @ NumericArray[
                                RandomVariate[NormalDistribution[], {simg, 128}], "Real32"];
                            Normal @ fxSampleJit[velJit, z, ee, sigmas, tembFn, ca]];  (* HOST latent *)
                        WriteString["stdout", "  [FluxGenerate] sample ", First[idx], "/", Length[encHost],
                            "  ", Round[dt, 1], " s", If[ First[idx] === 1, "  (cold: incl. capture)", "  (warm)"], "\n"];
                        $Output // Flush;
                        zp]],
                encHost]
        ]
    ];
    TContextDestroy[ctxT];  TReset[];

    (* --- STAGE 3: VAE decode in a FRESH context (transformer freed). --- *)
    results = TInContext[ctxV = TContextNew[dev],
        Module[{vt, vKeys, wfV, wsubV},
            vt = TSafeTensorLoad[vaePath];  vKeys = Keys[vt];
            wfV = fxVaeLoader[vt, dev];  wsubV = fxVaeSub[wfV, vKeys];
            Map[
                Function[lat,
                    Module[{img},
                        img = TRealize @ vaeDecoder[
                            TRealize @ TToDevice[TTensorCreate @ NumericArray[lat, "Real32"], dev],
                            wfV, wsubV, vaeCfg];
                        If[ returnImages, fxToImage[img], Normal[img]]]],
                latents]]];
    TContextDestroy[ctxV];
    results
]

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

(* STREAMING 4-step Euler: the per-block-eviction sampler.  Same eager Euler
   loop + host re-upload as fxSampleEager, but the velocity net is the STREAMING
   forward fxTransformerStreamed -- each block's weights are zero-copy wrapped to
   Metal on use then evicted (TDiskDropWeight) once the block retires, so only
   the ACTIVE block's weights are resident (~one block, not all 25).  Pair with
   wf = fxTransformerStreamLoader (no device cache) so nothing pins the weights.
   Numerically identical to fxSampleEager (per-block eviction is byte-exact).
   Returns the device latent for the VAE. *)
fxSampleStreamed[z0_, enc0_, sigmas_, tembFn_, rc_, rs_, wf_, cfg_, ca_] := Module[
    {zh = Normal[z0], k, dt, vh},
    Do[ dt = sigmas[[k + 1]] - sigmas[[k]];
        vh = Normal @ TRealize @ fxTransformerStreamed[
            ca @ TTensorCreate @ NumericArray[zh, "Real32"],
            enc0, tembFn[sigmas[[k]]], rc, rs, wf, cfg];
        zh = zh + dt vh;                                 (* host Euler step *)
        TGCCollect[],
        {k, 1, Length[sigmas] - 1}];
    ca @ TTensorCreate @ NumericArray[zh, "Real32"]]

(* Capture the velocity net ONCE for the whole batch: z, enc, AND temb are all
   rebound TJit inputs (rc/rs/weights are closed over -- shared across images),
   so a 2-image batch pays ONE cold capture and every image after the first is a
   warm replay (rebinding its prompt encoding + noise + per-step temb), NOT a
   re-capture.  Closing enc over the function instead would re-capture per image
   (the closure differs), turning each "warm" image back into a cold start. *)
fxVelocityJit[rc_, rs_, wf_, cfg_] :=
    TJit[Function[{z, e, tb}, fxTransformer[z, e, tb, rc, rs, wf, cfg]]]

(* JIT 4-step Euler over a SHARED captured velocity net (fxVelocityJit).  Replay
   per step, rebinding z (re-uploaded from the host Euler state) + enc0 + temb.
   TJit's cross-step buffer reuse keeps the resident set at ONE forward's working
   set.  temb is M=1 ({1,3072}); its modulation matmuls pad to a CONCRETE M=8
   (fxModLinear) so the symbolic-M kvar never declines the Metal ICB and the
   batched replay stays ~0.6 s/step.  Host Euler update + z re-upload each step
   (a few hundred KB) -- same as the eager path. *)
fxSampleJit[vfn_, z0_, enc0_, sigmas_, tembFn_, ca_] := Module[
    {zh = Normal[z0], k, dt, vh},
    Do[ dt = sigmas[[k + 1]] - sigmas[[k]];
        vh = Normal @ vfn[ca @ TTensorCreate @ NumericArray[zh, "Real32"], enc0, tembFn[sigmas[[k]]]];
        zh = zh + dt vh,
        {k, 1, Length[sigmas] - 1}];
    ca @ TTensorCreate @ NumericArray[zh, "Real32"]]

(* Per-block JIT 4-step Euler: the <10GB-peak sampler.  Identical numerics
   to fxSampleJit, but instead of TJit-capturing the FULL 25-block forward
   (which pins every block's activations -- ~8GB -- during the first
   capture, the 16GB peak), capture ONE double + ONE single block kernel
   set and REPLAY each of the 5 double / 20 single blocks by rebinding that
   block's weights + the running {img,txt}/x.  The replayed activations are
   reused across all 25 block-replays, so the transient is ~weights (7.75GB)
   + one block's working set (~0.5GB) instead of weights + all 25 blocks.

   The two block closures (dblFn/sglFn) are captured ONCE (the cold cost) and
   shared across all 4 steps.  Each block's weights + this step's mod chunks +
   the RoPE tables are rebound as EXPLICIT positional TJit inputs (no TSet, no
   fixed slots): TJit collects every TTerm arg as an input_replace site, so the
   replay rebinds them by position.  The per-step mod chunks + temb are computed
   once per step (outside the block loop) and feed elementwise broadcasts (no
   M=1 matmul inside a block), so no kvar / ICB-decline issue.  Host Euler
   update + z re-upload each step (a few hundred KB), same as fxSampleJit. *)
fxSampleJitBlocked[z0_, enc0_, sigmas_, tembFn_, rc_, rs_, wf_, cfg_, ca_] := Module[
    {zh = Normal[z0], k, dt, vh, encDev, dblFn, sglFn, stepMods},
    encDev = TRealize @ fxLinear[enc0, wf["context_embedder.weight"]];   (* {S_txt,dim}, const across steps *)
    (* per-step shared modulation chunks, each a DISTINCT realized buffer (via
       fxRealizeChunks) so its bytes are stable when passed as a replay input. *)
    stepMods[temb_] := Module[{mods, ss, smod},
        mods = fxDoubleMods[
            fxRealizeChunks @ fxModChunks[temb, wf["double_stream_modulation_img.linear.weight"], 2],
            fxRealizeChunks @ fxModChunks[temb, wf["double_stream_modulation_txt.linear.weight"], 2]];
        ss   = fxRealizeChunks @ fxModChunks[temb, wf["single_stream_modulation.linear.weight"], 1];
        smod = <|"shift" -> ss[[1]], "scale" -> ss[[2]], "gate" -> ss[[3]]|>;
        {mods, smod}];
    (* capture the two block closures ONCE; reused across all sampler steps. *)
    dblFn = TJit[fxDblBlockBody[cfg]];
    sglFn = TJit[fxSglBlockBody[cfg]];
    Do[ dt = sigmas[[k + 1]] - sigmas[[k]];
        With[{temb = tembFn[sigmas[[k]]]},
            Module[{ms = stepMods[temb], zdev},
                zdev = ca @ TTensorCreate @ NumericArray[zh, "Real32"];
                vh = Normal @ fxTransformerBlocked[
                    zdev, encDev, ms[[1]], ms[[2]], rc, rs, wf, cfg, dblFn, sglFn, temb, ca]]];
        zh = zh + dt vh,
        {k, 1, Length[sigmas] - 1}];
    ca @ TTensorCreate @ NumericArray[zh, "Real32"]]

(* {3,H,W} pixels in [0,1] -> an Image (clip for any tiny fp overshoot). *)
fxToImage[t_] := Image[Clip[Normal[t], {0., 1.}], Interleaving -> False, ColorSpace -> "RGB"]
