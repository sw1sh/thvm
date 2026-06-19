(* FluxGenerate.wl -- the end-to-end FLUX.2-klein-4B text->image generator for
   thvm.  Chains the four FLUX pieces (Get-loaded alongside this file) into one
   `FluxGenerate[prompts, opts]` entry point:

     prompt(s) (String | {String..})
       -> qwTokenize        host byte-level BPE + Qwen chat template -> {512} ids
       -> qwenEncode        Qwen3-4B text encoder -> {512, 7680} text embedding
       -> context_embedder  (inside fxTransformer) -> {512, 3072}
       -> fxSampleJit       4-step Euler flow-match over the velocity net -> z {S_img,128}
       -> vaeDecoder        AutoencoderKLFlux2 decode -> image {3, H, W} in [0,1]

   Built ONCE as a persistent session ($fxSession): ONE shared context holds the
   Qwen encoder, the DiT, and the VAE (zero-copy disk-mmap weights co-reside well
   under the 30GB ceiling).  A call runs three stages over that session -- STAGE 1
   Qwen-encodes every prompt (each {512,7680} embedding read to the host), STAGE 2
   samples each latent (device-resident), STAGE 3 VAE-decodes -- and a later call
   on a new prompt replays the same captures (no rebuild).  The transformer weights
   + RoPE are uploaded once and reused for every image, and the per-block tile
   kernels compile once (the cold cost), so the
   second image of a batch is warm -- not another cold start.  The sampler is
   device-resident: the velocity net is TJit-captured (fxVelocityJit) and the
   4-step Euler runs as device ops between replays (fxSampleJit), so the running
   latent never leaves the device (no per-step host round-trip).  (The whole-loop
   single-capture variant fxSampleJitFull is the further-faithful target, blocked
   on capture-time memory planning -- see its comment.)

   Image size threads through everything: gridH = H/16, gridW = W/16 (the VAE
   downsamples /8 and the patchify packs 2x2, so the patched-latent side is
   image/16), S_img = gridH*gridW image tokens, z0 {S_img,128}, the RoPE table
   is built for that grid, fxSigmas uses S_img, and vaeDecoder unpatchifies back
   to {3, 16*gridH, 16*gridW} = {3, H, W}.

   Part of the WolframInstitute`THVMLink`Examples` package (the four flux pieces -
   FluxForward / FluxSampler / FluxVAE / QwenEncoder - load alongside it into the
   shared Examples`Private` context).  Follows wl/GUIDE.md style. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

FluxGenerate::usage = "FluxGenerate[prompt$] generates an image from a text prompt with the FLUX.2-klein-4B text-to-image model (Qwen3-4B text encoder -> MMDiT velocity net, 4-step Euler flow-match -> AutoencoderKLFlux2 decoder), returning an Image.\nFluxGenerate[{prompt$1, prompt$2, $$}] generates one image per prompt as a batch, building the model session ONCE and replaying the captured kernels per prompt (the second image is warm, not a second cold start).\nOptions: \"ImageSize\" (default {256, 256}), RandomSeeding (Automatic -> a fresh random image each call; give an integer for a reproducible seed), \"Device\" (\"metal\" | \"cpu\" | \"cuda\"), \"NumSteps\" (4), \"ModelDir\" (Automatic -> ~/.cache/thvm/flux2-klein-4b), \"ReturnImages\" (True; False returns raw {3, H, W} arrays).";

Begin["`Private`"];


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
   lazy form is fine and keeps host residency to ~one weight at a time. *)
(* q8 gate (env FLUX_Q8=1): a 2-D weight with both dims >= 2048 is a per-block
   QKV / out / MLP projection -- the warm-hot bulk AND ~all the resident weight
   bytes.  Quantising those to int8 + per-output-channel scale halves the device
   weight footprint and the cold upload; fxLinear reads int8 on the tensor-core
   tiled path then dequantises (see fxQuantizeWeight / fxLinear in FluxForward.wl).
   Smaller / 1-D tensors (RMSNorm gains, the in/out embedders, modulation
   linears that run at M<=8) stay bf16 -- the quant overhead would not pay off
   and the leading-unit-axis temb path is codegen-sensitive. *)
fxQuant8Q[] := Environment["FLUX_Q8"] === "1";
fxLoadTfWeight[n_, t_, dev_] :=
    If[ fxQuant8Q[] && fxQuantizableQ[n, Dimensions[t]],
        fxQuantizeWeight[TToDevice[t, dev]],
        TToDevice[t, dev]]

fxTransformerLoader[wt_, dev_] := Module[{cache = <||>},
    n |-> Lookup[cache, n, cache[n] = fxLoadTfWeight[n, wt[n], dev]]]

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

   PERSISTENT SESSION.  The first call builds a session ($fxSession,
   keyed by {modelDir,dev,imgSize,nSteps}): ONE long-lived context holds
   the Qwen + transformer + VAE weights (zero-copy mmap wraps, ~0 phys
   footprint, so all three co-reside under the 30GB ceiling), the RoPE
   table, the timestep MLP, and the ONCE-captured velocity JIT.  The
   context is KEPT ALIVE -- never TContextDestroy'd -- so later calls
   skip the ~20s transformer capture and the weight reloads, replaying
   the captured kernels.  The three stages (Qwen encode, transformer
   sample, VAE decode) all run inside the session context.

   No TReset between stages: TReset (thvm_reset) clears the JIT capture
   slots (jit_capture_reset_all) and the kvar registry, which would void
   the persistent velJit.  The old design's TReset-between-stages was
   there to drop Qwen's symbolic-dim/fresh-label state before the
   transformer's tile-JIT codegen; the session avoids that hazard a
   different way -- the transformer velJit is captured FIRST (before any
   Qwen forward runs in the session), so its codegen never sees Qwen's
   kvar state.  Qwen + VAE derive every shape/dtype from kvar-free
   realized weights, so their (later, eager-or-captured) forwards add no
   kvar that could leak into the already-captured transformer.
   ============================================================ *)

Options[FluxGenerate] = {
    "ImageSize" -> {256, 256}, RandomSeeding -> Automatic, "Device" -> "metal",
    "NumSteps" -> 4, "ModelDir" -> Automatic, "ReturnImages" -> True};

(* module-level session cache: key -> <|ctx, wfq, qwCfg, td, stxt,
   wfT, ca, rc, rs, tembFn, velJit, sigmas, simg, fxCfg,
   wfV, wsubV, vaeCfg, dev|>.  One entry per {modelDir,dev,imgSize,nSteps}. *)
$fxSession = <||>;

FluxGenerate[prompt_String, opts : OptionsPattern[]] := First @ FluxGenerate[{prompt}, opts];

(* build (or fetch) the persistent session for these settings. *)
fxSessionGet[dev_, imgSize_, nSteps_, modelDir_] := Module[
    {key = {modelDir, dev, imgSize, nSteps}},
    Lookup[$fxSession, Key[key], $fxSession[key] = fxSessionBuild[dev, imgSize, nSteps, modelDir]]]

(* The three models' weights (~16 GB across Qwen + transformer + VAE
   safetensors) are zero-copy disk-mmap wraps.  The default MADV_WILLNEED
   readahead faults the WHOLE of each file resident at load time, so a session
   peaks at ~19 GB of resident weight pages even though each weight is read once
   to upload.  THVM_MMAP_NO_WILLNEED defers the fault to the per-weight upload
   (which then MADV_DONTNEEDs the pages), keeping the host weight working set to
   roughly one weight at a time -- session peak drops to ~5 GB.  Set here (not
   globally) so the session is memory-bounded by default; a user export wins. *)
fxBoundMemory[] := (
    If[ Environment["THVM_MMAP_NO_WILLNEED"] === $Failed,
        SetEnvironment["THVM_MMAP_NO_WILLNEED" -> "1"]];
    (* Buffer-reuse OFF for the FLUX session: the velocity/Qwen/VAE captures
       recycle output buffers, and on Apple9 (M3) the batched ICB's [cmd
       setBarrier] does NOT reliably order those write-after-write recycles, so
       a recycling capture replayed through the ICB reads stale bytes and every
       prompt collapsed to one image.  With reuse off there is no recycle, so the
       ICB is correct -- and the ICB (no longer declined as a recycling stream)
       replaces the per-op decline, cutting warm ~9s -> ~7s.  A user export of
       THVM_METAL_REUSE_BUFS wins.  (The fully-fast path -- recycle + ICB via an
       arena-slice memory plan so the recycle is Apple-hazard-trackable -- is the
       open follow-up.) *)
    If[ Environment["THVM_METAL_REUSE_BUFS"] === $Failed,
        SetEnvironment["THVM_METAL_REUSE_BUFS" -> "0"]]);

fxSessionBuild[dev_, imgSize_, nSteps_, modelDir_] := Module[
    {w, h, gridH, gridW, simg, stxt, tokDir, td, tfPath, qwPaths, vaePath,
     fxCfg, qwCfg, vaeCfg, sigmas, ctxQ, ctxT, ctxV, qwJit, wfq, velJit, ca,
     rc, rs, tembFn, vaeJit},
    fxBoundMemory[];
    {w, h} = imgSize;
    gridH = Round[h/16];  gridW = Round[w/16];  simg = gridH gridW;  stxt = 512;
    tokDir = FileNameJoin[{modelDir, "tokenizer"}];
    tfPath = FileNameJoin[{modelDir, "transformer", "diffusion_pytorch_model.safetensors"}];
    qwPaths = FileNameJoin[{modelDir, "text_encoder", #}] & /@
        {"model-00001-of-00002.safetensors", "model-00002-of-00002.safetensors"};
    vaePath = FileNameJoin[{modelDir, "vae", "diffusion_pytorch_model.safetensors"}];
    fxCfg = <|"eps" -> 1.*^-6, "num_double" -> 5, "num_single" -> 20, "heads" -> 24, "head_dim" -> 128|>;
    qwCfg = <|"heads" -> 32, "kv_heads" -> 8, "head_dim" -> 128, "eps" -> 1.*^-6,
              "theta" -> 1000000, "layers" -> 27, "captureLayers" -> {8, 17, 26}|>;
    vaeCfg = <|"eps" -> 1.*^-6, "epsBn" -> 1.*^-4, "gridH" -> gridH, "gridW" -> gridW|>;
    td = qwTokenizerData[tokDir];
    sigmas = fxSigmas[simg, nSteps];

    (* ONE shared persistent context for all three models.  An earlier design used
       a separate context per model, but LAM_SHAPE_TABLE (the side table that tells
       materialize a captured lambda's input shape) is keyed by raw heap loc, and
       heap locs are PER-CONTEXT -- each context's heap restarts at the same low
       locs.  So the qwJit and velJit capture lambdas landed at the SAME loc in
       their separate contexts and collided in that global table: whichever
       captured second read the other's Shape, built its view index from the wrong
       dims, and leaked a stale tensor handle into the index expression -- the
       tile-JIT codegen then emitted `/*?*/` ("expected expression") and every
       transformer kernel failed to compile.  A single context gives every capture
       lambda a globally-unique loc, so no collision.  The cross-capture buffer
       aliasing the per-context split was meant to prevent does NOT occur: each TJit
       capture retains its own buffers, so the three replays read/write disjoint
       slots (verified -- the batch produces correct, distinct images).  Weights are
       zero-copy disk-mmap wraps (~0 phys footprint on Apple unified memory), so all
       three models co-reside well under the 30GB ceiling.  Each stage crosses to
       the next as a HOST array. *)

    (* --- load + RoPE + temb + capture velJit (in the shared context). --- *)
    ctxT = TContextNew[dev];
    {velJit, ca, rc, rs, tembFn} = TInContext[ctxT,
        Module[{wt, wfT, rope, rcL, rsL, caL, tembFnL, vj},
            caL = If[ dev === "cpu", TRealize[#] &, (TRealize @ TToDevice[TUOpCast[#, "bf16"], dev]) &];
            wt = TSafeTensorLoad[tfPath];
            wfT = fxTransformerLoader[wt, dev];
            rope = fxRopeTable[gridH, gridW, stxt];
            rcL = caL @ TTensorCreate[rope["cos"]];  rsL = caL @ TTensorCreate[rope["sin"]];
            tembFnL = fxTembFn[wfT, dev];
            vj = fxVelocityJit[rcL, rsL, wfT, fxCfg];
            {vj, caL, rcL, rsL, tembFnL}]];

    (* --- Qwen context: load weights + capture the 27-layer device forward.
       cos/sin (length stxt) are closed over (constant across prompts); the
       per-prompt {stxt,2560} x + {stxt,stxt} addMask are the rebound JIT inputs.
       Every shape is concrete (kvar-free). --- *)
    ctxQ = ctxT;  (* shared context (see above): distinct heap locs, no LAM_SHAPE collision *)
    {qwJit, wfq} = TInContext[ctxQ,
        Module[{qw, wfqL, qct, qcos, qsin, qj},
            qw = Join[TSafeTensorLoad[qwPaths[[1]]], TSafeTensorLoad[qwPaths[[2]]]];
            wfqL = fxQwenLoader[qw, dev];
            qct = TRoPEHalfSplitTable[stxt, qwCfg["head_dim"], qwCfg["theta"]];
            qcos = TToDevice[qct[[1]], dev];  qsin = TToDevice[qct[[2]], dev];
            qj = TJit[Function[{x, addMask}, qwenForward[x, addMask, qcos, qsin, wfqL, qwCfg]]];
            {qj, wfqL}]];

    (* --- VAE context: load weights (f32) + capture the conv decode.  The packed
       latent {simg,128} is the only rebound JIT input.  TRealize the body:
       vaeDecoder ends with a lazy TClip, so a bare capture would hand the replay
       an unrealized UOP (Normal -> Missing). --- *)
    ctxV = ctxT;  (* shared context (see above) *)
    vaeJit = TInContext[ctxV,
        Module[{vt, vKeys, wfV, wsubV},
            vt = TSafeTensorLoad[vaePath];  vKeys = Keys[vt];
            wfV = fxVaeLoader[vt, dev];  wsubV = fxVaeSub[wfV, vKeys];
            TJit[Function[lat, TRealize @ vaeDecoder[lat, wfV, wsubV, vaeCfg]]]]];

    <|"ctxQ" -> ctxQ, "ctxT" -> ctxT, "ctxV" -> ctxV, "dev" -> dev, "ca" -> ca,
      "wfq" -> wfq, "qwCfg" -> qwCfg, "td" -> td, "stxt" -> stxt,
      "rc" -> rc, "rs" -> rs, "tembFn" -> tembFn,
      "velJit" -> velJit, "sigmas" -> sigmas, "simg" -> simg, "fxCfg" -> fxCfg,
      "vaeCfg" -> vaeCfg, "qwJit" -> qwJit, "vaeJit" -> vaeJit|>]

FluxGenerate[prompts_List, opts : OptionsPattern[]] := Module[
    {imgSize, seed, dev, nSteps, modelDir, returnImages, sess, key,
     encDev, latents, results},

    imgSize = OptionValue["ImageSize"];
    seed = OptionValue[RandomSeeding];  dev = OptionValue["Device"];
    nSteps = OptionValue["NumSteps"];  returnImages = OptionValue["ReturnImages"];
    modelDir = OptionValue["ModelDir"] /. Automatic ->
        Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b";

    key = {modelDir, dev, imgSize, nSteps};
    sess = fxSessionGet[dev, imgSize, nSteps, modelDir];

    Module[{ca = sess["ca"], stxt = sess["stxt"], simg = sess["simg"],
            sigmas = sess["sigmas"], n = Length[prompts], t1, t2, t3},
        (* --- STAGE 1: Qwen text-encode the whole batch (in the Qwen context). --- *)
        {t1, encDev} = AbsoluteTiming @ TInContext[sess["ctxQ"],
            fxQwenEncodeBatch[sess, prompts]];                         (* {n,stxt,7680} device *)

        (* --- STAGE 2: transformer sample each prompt's latent (transformer
           context).  velJit is the persistent per-step capture; fxSampleJit keeps
           the running latent z ON DEVICE across the 4 Euler steps (device add, no
           per-step host read of v / re-upload of z) -- one final host read. --- *)
        {t2, latents} = AbsoluteTiming @ TInContext[sess["ctxT"],
            MapIndexed[
                Function[{encArr, idx},
                    Module[{ee, z, i = First[idx], lat},
                        ee = ca @ encArr;   (* {stxt,7680} device, straight from STAGE 1 *)
                        If[ seed === Automatic, SeedRandom[], SeedRandom[seed + i]];
                        z = ca @ TTensorCreate @ NumericArray[
                            RandomVariate[NormalDistribution[], {simg, 128}], "Real32"];
                        lat = Normal @ fxSampleJit[sess["velJit"], z, ee, sigmas, sess["tembFn"], ca];
                        fxDbg["  enc[", i, "] mean=", Round[Mean[Flatten[Normal[encArr]]], 0.0001],
                            "  lat mean=", Round[Mean[Flatten[lat]], 0.0001]];
                        lat]],
                encDev]];
        fxTiming[t1, t2];

        (* --- STAGE 3: VAE decode each latent (VAE context). --- *)
        {t3, results} = AbsoluteTiming @ TInContext[sess["ctxV"],
            Map[
                Function[lat,
                    Module[{img = fxVaeDecodeCached[sess, lat]},
                        If[ returnImages, fxToImage[img], Normal[img]]]],
                latents]];
        fxTiming[t3];
        results]]

(* per-stage timing print, gated by THVM_FLUX_TIMING (off by default). *)
fxTiming[ts__] := If[ Environment["THVM_FLUX_TIMING"] =!= $Failed,
    WriteString["stdout", "    [stage] ", Round[{ts}, 0.01], " s\n"]; $Output // Flush]

(* HoldAll so debug arguments (e.g. Mean[Flatten[Normal[enc]]]) are NOT
   evaluated unless THVM_FLUX_TIMING is set -- otherwise a device->host Normal
   readout would fire on every prompt just to compute a debug mean, defeating
   the keep-tensors-on-device path below. *)
SetAttributes[fxDbg, HoldAll];
fxDbg[a___] := If[ Environment["THVM_FLUX_TIMING"] =!= $Failed,
    WriteString["stdout", a, "\n"]; $Output // Flush]

(* ============================================================
   STAGE-1 (Qwen) and STAGE-3 (VAE) batch helpers for the session.
   ============================================================ *)

(* Qwen encode the whole prompt batch -> list of {stxt, 7680} host arrays.

   The 27-layer device forward is TJit-captured ONCE (cached in the session's
   "qwJit" slot, keyed by seq length stxt): the first prompt pays the cold
   capture, every later prompt (this call AND subsequent calls) replays it,
   rebinding only its host-prepped {stxt,2560} x + {stxt,stxt} addMask.  cos/sin
   are constant for a fixed stxt, so they are closed over by the capture (taken
   from the first prompt's inputs).  This both cuts the eager ~4s/prompt dispatch
   overhead to a batched ICB replay AND bounds the live bytes (a replay reuses
   the captured buffers instead of allocating a fresh forward each call).  Each
   Normal read crosses to the host as a {stxt,7680} f32 array (~15MB). *)
fxQwenEncodeBatch[sess_, prompts_List] := With[
    {wfq = sess["wfq"], qwCfg = sess["qwCfg"], td = sess["td"], stxt = sess["stxt"],
     jit = sess["qwJit"]},
    Function[p,
        Module[{tok, in, out, tTok, tIn, tRep, xr, mr},
            {tTok, tok} = AbsoluteTiming @ qwTokenize[p, td, stxt];
            {tIn, in} = AbsoluteTiming @ qwenInputs[Sequence @@ tok, wfq, qwCfg];
            (* realize the JIT inputs contiguous so input_replace finds rebind sites
               on replay -- a lazy/non-contiguous input gets 0 sites and silently
               keeps the capture-time (first prompt's) bytes (see capture.c). *)
            xr = TRealize[in["x"]];  mr = TRealize[in["addMask"]];
            (* Keep the Qwen encoding ON DEVICE (TRealize, not Normal): STAGE 2
               (shared context) feeds it straight into the velocity JIT, so a
               ~512x7680 device->host->device roundtrip + NumericArray rebuild is
               avoided per prompt. *)
            {tRep, out} = AbsoluteTiming[TRealize @ jit[xr, mr]];
            fxDbg["    qwen x mean=", Round[Mean[Flatten[Normal[xr]]], 0.0001],
                "  tokenize=", Round[tTok, 0.001], " replay=", Round[tRep, 0.001]];
            out]] /@ prompts]

(* VAE decode one host latent {simg,128} -> {3,H,W} device pixels.  The conv
   decode is TJit-captured ONCE (session "vaeJit"); each call replays it,
   rebinding only the uploaded packed latent.  Bounds live bytes + cuts the
   eager ~2s dispatch overhead to a batched ICB replay. *)
fxVaeDecodeCached[sess_, lat_] := With[{dev = sess["dev"]},
    sess["vaeJit"][TRealize @ TToDevice[TTensorCreate @ NumericArray[lat, "Real32"], dev]]]

(* Capture the velocity net ONCE for the whole batch: z, enc, AND temb are all
   rebound TJit inputs (rc/rs/weights are closed over -- shared across images),
   so a 2-image batch pays ONE cold capture and every image after the first is a
   warm replay (rebinding its prompt encoding + noise + per-step temb), NOT a
   re-capture.  Closing enc over the function instead would re-capture per image
   (the closure differs), turning each "warm" image back into a cold start. *)
fxVelocityJit[rc_, rs_, wf_, cfg_] :=
    TJit[Function[{z, e, tb}, fxTransformer[z, e, tb, rc, rs, wf, cfg]]]

(* WHOLE-LOOP faithful sampler: TJit-capture the ENTIRE 4-step Euler in ONE
   graph (tinygrad examples/minrf.py:148 wraps the whole sample loop in one
   @TinyJit), so an image is ONE replay rebinding only the {S_img,128} noise +
   {S_txt,dim} enc.  This is the end-state faithful sampler, but it is BLOCKED on
   B1: thvm's capture allocates each kernel output eagerly during the recording
   materialize (jit_capture_release_retained_except builds a `live` set of EVERY
   op output, capture.c:1432-1466), so 4 stacked forwards pin ~4x peak (~30GB,
   exceeds the live-byte ceiling) BEFORE the finalize packer assigns reuse.  The
   faithful fix is capture-time memory planning (port tinygrad's lazy schedule +
   memory_plan so allocation happens AFTER lifetime planning, with reuse, not
   eagerly per dispatch).  Until B1 lands, Stage 2 uses fxSampleJit below (per-step
   replay + DEVICE Euler), which is feasible and already removes the host round-
   trip.  Kept here as the implemented target so wiring it is a one-line switch. *)
fxSampleJitFull[rc_, rs_, wf_, cfg_, tembs_, dts_, dev_] :=
    TJit[Function[{z0, enc},
        Module[{z = z0, k, ncast},
            ncast = If[ dev === "cpu", # &, TUOpCast[#, "bf16"] &];
            Do[ z = ncast @ TUOpAdd[z,
                    TUOpMul[fxTransformer[z, enc, tembs[[k]], rc, rs, wf, cfg],
                            TUOpConst[N[dts[[k]]]]]],
                {k, 1, Length[tembs]}];
            z]]]

(* Per-step device-resident Euler over a SHARED captured velocity net
   (fxVelocityJit): replay the one-forward capture per step, rebinding z + enc +
   temb, and do the Euler update z = z + (sigma[k+1]-sigma[k]) v as a DEVICE op so
   the running latent NEVER leaves the device (no per-step Normal of v, no
   re-upload of z -- one final host read).  z is re-cast to the working dtype
   each step (ca: bf16 on GPU) so the next replay's matmuls stay on the tensor
   cores / batched ICB.  temb is M=1 ({1,3072}); its modulation matmuls pad to a
   concrete M=8 (fxModLinear) so the symbolic-M kvar never declines the Metal ICB.
   This is the feasible faithful sampler pending the B1 whole-loop capture above. *)
fxSampleJit[vfn_, z0_, enc0_, sigmas_, tembFn_, ca_] := Module[
    {z = TRealize @ TUOpCast[z0, "f32"], k, dt, v},
    Do[ dt = sigmas[[k + 1]] - sigmas[[k]];
        (* velocity from the bf16-input replay (ca: bf16 on GPU / f32 on CPU); the
           Euler accumulates in the f32 z accumulator -- exactly the host path's
           f32 step with a bf16 net input, but z never leaves the device. *)
        v = vfn[ca @ z, enc0, tembFn[sigmas[[k]]]];
        z = TRealize @ TUOpAdd[z, TUOpMul[TUOpCast[v, "f32"], TUOpConst[N[dt]]]],
        {k, 1, Length[sigmas] - 1}];
    z]

(* {3,H,W} pixels in [0,1] -> an Image (clip for any tiny fp overshoot). *)
fxToImage[t_] := Image[Clip[Normal[t], {0., 1.}], Interleaving -> False, ColorSpace -> "RGB"]

End[];

EndPackage[];
