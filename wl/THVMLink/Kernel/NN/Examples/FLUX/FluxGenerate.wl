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

FluxGenerate::usage = "FluxGenerate[prompt$] generates an Image from a text prompt with the FLUX.2-klein-4B text-to-image model (Qwen3-4B text encoder -> MMDiT velocity net, 4-step Euler flow-match -> AutoencoderKLFlux2 decoder).\nFluxGenerate[prompt$, spec$] returns the part(s) named by spec$: \"Image\" (the default Image), \"Latent\" (the initial packed-latent noise z$0, a {S$img, 128} NumericArray that can be fed back via \"InitialLatent\" to reproduce the image -- the round-trip handle), \"Embedding\" (the Qwen text embedding, a {512, 7680} NumericArray), All (an Association of all parts), or a list of those keys (an Association of just those parts).\nFluxGenerate[prompt$, n$] generates n$ images of the same prompt, each with a distinct fresh seed, reusing one model session (only the first is a cold start); FluxGenerate[prompt$, n$, spec$] returns a list of n$ spec$ results.\nFluxGenerate[{prompt$1, prompt$2, $$}] (optionally with a trailing spec$) generates one result per prompt as a batch, building the model session ONCE and replaying the captured kernels per prompt (every result after the first is warm).\nThe following options can be given:\n  \"ImageSize\" {256, 256}    output size; a scalar n$ means a square n$ x n$ image, a pair {w$, h$} a rectangle (rounded to the /16 patch grid).\n  \"Steps\" Automatic    number of Euler sampling steps; Automatic uses \"NumSteps\".\n  RandomSeeding Automatic    Automatic gives a fresh random image each call; an integer seed makes the result reproducible (same seed -> byte-identical image).\n  \"InitialLatent\" Automatic    Automatic samples fresh z$0 ~ N(0,1); a {S$img, 128} array (e.g. a \"Latent\" result) is used as z$0 to seed STAGE 2 deterministically instead of the per-image random noise, so a saved latent reproduces its image (a latent round-trip).\n  \"NegativePrompt\" None    FLUX.2-klein is guidance-distilled (4-step, no classifier-free guidance), so a negative prompt is unsupported; giving one issues FluxGenerate::nocfg and is ignored.\n  \"ShowSteps\" False    True decodes each Euler-step latent through the VAE and shows the denoising progression live (in a notebook); the per-step Images + raw latents are then included under the \"Steps\" key of an All / {\"Steps\"} return.\n  ProgressReporting Automatic    Automatic or True shows a live progress panel (a no-op with no front end); False runs silent.\n  \"Device\" \"metal\"    backend to run on (\"metal\" | \"cpu\" | \"cuda\").\n  \"NumSteps\" 4    legacy alias for \"Steps\".\n  \"ModelDir\" Automatic    weight directory (Automatic -> ~/.cache/thvm/flux2-klein-4b).\n  \"Q8\" False    weight-only int8 quantization of the large QKV/MLP projections; default False (bf16).  q8 halves the device weight footprint but Metal has no int8 matmul (it casts to bf16 in-kernel), so it is a memory-fit option, not a speed one (env FLUX_Q8=1 is an equivalent fallback).";

FluxGenerate::nocfg = "FLUX.2-klein-4B is a guidance-distilled 4-step flow-matching model with no classifier-free guidance, so the \"NegativePrompt\" `1` cannot be applied (a negative prompt needs the conditional-vs-unconditional CFG pass klein was distilled to skip).  It is ignored.";

FluxGenerate::badlatent = "The \"InitialLatent\" value has dimensions `1`, but the current \"ImageSize\" needs a {`2`, 128} latent.  Sampling fresh random noise instead.";

FluxGenerate::badspec = "`1` is not a valid return spec; use \"Image\", \"Latent\", \"Embedding\", All, or a list of those keys.  Returning the Image.";

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
    cs = bs;
    n = 0;
    Do[
        If[ ! MemberQ[bs, b],
            AppendTo[bs, b];
            AppendTo[cs, 256 + n];
            n++
        ]
        ,
        {b, 0, 255}
    ];
    AssociationThread[bs -> FromCharacterCode /@ cs]
]

(* load + memoise the tokenizer tables (vocab string->id, merge-pair->rank,
   byte->unicode, special-token ids) from a tokenizer dir. *)

qwTokenizerData[dir_] := qwTokenizerData[dir] = Module[{vocab, added, mergeLines, mergePairs, bpeRank, b2u, specials},
    vocab = Import[FileNameJoin[{dir, "vocab.json"}], "RawJSON"];
    added = Import[FileNameJoin[{dir, "added_tokens.json"}], "RawJSON"];
    vocab = Join[vocab, added];
    mergeLines = Select[
        StringSplit[Import[FileNameJoin[{dir, "merges.txt"}], "Text"], "\n"],
        (# =!= "" && ! StringStartsQ[#, "#version"])&
    ];
    mergePairs = StringSplit[#, " "]& /@ mergeLines;
    (* rank key = "a\nb" (newline can't occur in a byte-mapped piece) *)
    bpeRank = AssociationThread[(StringJoin[#[[1]], "\n", #[[2]]]& /@ mergePairs) -> Range[Length[mergePairs]]];
    b2u = qwByteToUnicode[];
    specials = <|
        "im_start" -> 151644, "im_end" -> 151645, "think" -> 151667, "think_end" -> 151668, "pad" -> 151643
    |>;
    <|"vocab" -> vocab, "rank" -> bpeRank, "b2u" -> b2u, "special" -> specials|>
]

(* the Qwen pre-tokenization regex (GPT-4 split: case-insensitive contractions,
   letter runs, single digits, punctuation runs, whitespace).  WL's PCRE
   supports \p{L}/\p{N}/(?i:..) directly. *)

$qwPretokRegex = "(?i:'s|'t|'re|'ve|'m|'ll|'d)|[^\\r\\n\\p{L}\\p{N}]?\\p{L}+|\\p{N}| ?[^\\s\\p{L}\\p{N}]+[\\r\\n]*|\\s*[\\r\\n]+|\\s+(?!\\S)|\\s+";

(* greedy BPE: repeatedly merge the lowest-rank adjacent pair (leftmost on ties)
   until no mergeable pair remains.  `mapped` is the byte->unicode string. *)

qwBpe[mapped_String, bpeRank_] := Module[{word, n, pairs, ranks, best, bi, a, b, new, i},
    word = Characters[mapped];
    If[Length[word] < 2, Return[word]];
    While[
        True
        ,
        n = Length[word];
        pairs = Table[word[[i]] <> "\n" <> word[[i + 1]], {i, 1, n - 1}];
        ranks = Lookup[bpeRank, pairs, Infinity];
        best = Min[ranks];
        If[best === Infinity, Break[]];
        bi = First @ FirstPosition[ranks, best];
        a = word[[bi]];
        b = word[[bi + 1]];
        new = {};
        i = 1;
        While[
            i <= n
            ,
            If[ i < n && word[[i]] === a && word[[i + 1]] === b,
                AppendTo[new, a <> b];
                i += 2
                ,
                AppendTo[new, word[[i]]];
                i += 1
            ]
        ];
        word = new;
        If[Length[word] === 1, Break[]]
    ];
    word
]

(* encode one plain text fragment (no special tokens) -> list of ids:
   NFC-normalise, pre-tokenise, byte->unicode each pre-token, BPE-merge, lookup. *)

qwEncodeText[text_String, td_] := Module[{nfc, pretoks, vocab, b2u, rank},
    vocab = td["vocab"];
    b2u = td["b2u"];
    rank = td["rank"];
    nfc = CharacterNormalize[text, "NFC"];
    pretoks = StringCases[nfc, RegularExpression[$qwPretokRegex]];
    Flatten @
        Map[
            Function[pt,
                vocab[#]& /@ qwBpe[StringJoin[b2u /@ ToCharacterCode[pt, "UTF-8"]], rank]
            ]
            ,
            pretoks
        ]
]

(* wrap a user prompt in the Qwen chat template (enable_thinking=False,
   add_generation_prompt=True), with special tokens inserted as raw ids:
     <|im_start|>user\n{prompt}<|im_end|>\n<|im_start|>assistant\n<think>\n\n</think>\n\n
   -> the unpadded id list. *)

qwChatEncode[prompt_String, td_] := With[{sp = td["special"]},
    Join[
        {sp["im_start"]},
        qwEncodeText["user\n" <> prompt, td],
        {sp["im_end"]},
        qwEncodeText["\n", td],
        {sp["im_start"]},
        qwEncodeText["assistant\n", td],
        {sp["think"]},
        qwEncodeText["\n\n", td],
        {sp["think_end"]},
        qwEncodeText["\n\n", td]
    ]
]

(* tokenize a prompt to {ids, attMask}, right-padded / truncated to seqLen with
   the pad id (151643); attMask is 1 on real tokens, 0 on pad. *)

qwTokenize[prompt_String, td_, seqLen_ : 512] := Module[{ids, pad, n, mask},
    ids = qwChatEncode[prompt, td];
    pad = td["special"]["pad"];
    n = Length[ids];
    If[ n >= seqLen,
        {Take[ids, seqLen], ConstantArray[1, seqLen]}
        ,
        {
            Join[ids, ConstantArray[pad, seqLen - n]], Join[ConstantArray[1, n], ConstantArray[0, seqLen - n]]
        }
    ]
]

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

(* q8 gate (the "Q8" option, or env FLUX_Q8=1 as a fallback): a 2-D weight with both dims >= 2048 is a per-block
   QKV / out / MLP projection -- the warm-hot bulk AND ~all the resident weight
   bytes.  Quantising those to int8 + per-output-channel scale halves the device
   weight footprint and the cold upload; fxLinear reads int8 on the tensor-core
   tiled path then dequantises (see fxQuantizeWeight / fxLinear in FluxForward.wl).
   Smaller / 1-D tensors (RMSNorm gains, the in/out embedders, modulation
   linears that run at M<=8) stay bf16 -- the quant overhead would not pay off
   and the leading-unit-axis temb path is codegen-sensitive. *)

(* Resolve the q8 request to a single boolean: the "Q8" option wins, env FLUX_Q8=1
   is a fallback so existing env-driven scripts keep working.  Resolved ONCE at the
   entry handler and threaded as a plain boolean (the session key + the loader read
   it directly), so an env-driven and an option-driven q8 share one session key and
   neither collides with a bf16 session. *)

fxQuant8Q[q8_] := TrueQ[q8] || Environment["FLUX_Q8"] === "1";

fxLoadTfWeight[n_, t_, dev_, q8_] := If[ q8 && fxQuantizableQ[n, Dimensions[t]],
    fxQuantizeWeight[TToDevice[t, dev]]
    ,
    TToDevice[t, dev]
]

fxTransformerLoader[wt_, dev_, q8_] := Module[{cache = <||>},
    n |-> Lookup[cache, n, cache[n] = fxLoadTfWeight[n, wt[n], dev, q8]]
]

(* Qwen text encoder: bf16, contiguous exactly like the transformer.  The
   diffusers q/k/v/o/gate/up/down weights are stored {out,in}; qwLayer reuses
   fxLinear = TMatMul[x {S,in}, Transpose[w]], reading the weight transposed via
   the tiled-transpose matmul -- so they upload AS STORED, no pre-transpose.  The
   embed table stays HOST-resident and un-transposed (qwEmbed host-gathers its
   rows). *)

fxQwenLoader[qwt_, dev_] := Module[{cache = <||>},
    n
    |->
    Lookup[cache, n, cache[n] = If[n === "model.embed_tokens.weight", qwt[n], TToDevice[qwt[n], dev]]]
]

(* VAE weights uploaded to the device.  On a GPU keep them bf16 (their stored
   safetensors dtype): vaeConv routes through the im2col GEMM (TConv2DIm2ColPoolGemm),
   so each conv is a bf16(act) x bf16(W) tensor-core matmul accumulating in f32 --
   half the weight + activation bytes feeding the TC GEMM.  The latent feeding the
   first conv is uploaded bf16 too so the conv is bf16 x bf16, not the slow MIXED
   f32 x bf16.  The CPU path stays f32 (its C JIT promotes bf16 to f32 but needs
   host-boundary widening; f32 weights are already matched).  vaeDecoder asks for
   the latent-denorm stats as `bn_running_mean`/`bn_running_var`; the safetensors
   store them dotted (`bn.running_mean`/`bn.running_var`), so alias. *)

fxVaeKey[n_] := Switch[ n,
    "bn_running_mean",
        "bn.running_mean"
    ,
    "bn_running_var",
        "bn.running_var"
    ,
    _,
        n
];

fxVaeLoader[vt_, dev_] := Module[{cache = <||>, wdt = If[dev === "cpu", "f32", "bf16"]},
    n |-> Lookup[cache, n, cache[n] = TRealize @ TToDevice[TUOpCast[vt[fxVaeKey[n]], wdt], dev]]
]

(* wsub[prefix]: the sub-Association of (key-with-prefix-stripped -> loaded TTerm)
   for every weight name under `prefix`.  `keys` is a List of names, so Select
   (not KeySelect, which is for Associations). *)

fxVaeSub[wf_, keys_][prefix_] := Association @ Map[(StringDrop[#, StringLength[prefix]] -> wf[#])&, Select[keys, StringStartsQ[#, prefix]&]]

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

fxTembFn[wf_, dev_][sigma_] := With[{s = TRealize @ TToDevice[TTensorCreate[{fxTimestepSinusoid[sigma, 256]}], dev]},
    With[{
        temb = fxLinear[
            TSiLU @ fxLinear[s, wf["time_guidance_embed.timestep_embedder.linear_1.weight"]],
            wf["time_guidance_embed.timestep_embedder.linear_2.weight"]
        ]
    },
        If[dev === "cpu", temb, TRealize @ TUOpCast[temb, "bf16"]]
    ]
]

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
    "ImageSize" -> {256, 256},
    "Steps" -> Automatic,
    RandomSeeding -> Automatic,
    "InitialLatent" -> Automatic,
    "NegativePrompt" -> None,
    "ShowSteps" -> False,
    "Device" -> "metal",
    "NumSteps" -> 4,
    "ModelDir" -> Automatic,
    ProgressReporting -> Automatic,
    "Q8" -> False
};

(* module-level session cache: key -> <|ctx, wfq, qwCfg, td, stxt,
   wfT, ca, rc, rs, tembFn, velJit, sigmas, simg, fxCfg,
   wfV, wsubV, vaeCfg, dev|>.  One entry per {modelDir,dev,imgSize,nSteps,q8}. *)

$fxSession = <||>;

(* module-level weight base cache: key -> the size-INDEPENDENT load.  ONE entry per
   {modelDir,dev,q8} holds the shared context, the cast helper, and all
   transformer/Qwen/VAE weights + the (fixed-length) Qwen forward capture.  Every
   ImageSize/Steps session reuses this base, so the ~16 GB of weights load ONCE. *)

$fxWeightBase = <||>;

(* A return spec is a String key ("Image" | "Latent" | "Embedding"), the symbol
   All, or a List of String keys -- type-distinguishable from the Integer count, so
   `spec` can be the last positional argument (idiomatic WL, like Import[file,fmt]).
   $fxSpecKeys are the per-image parts the pipeline can surface. *)

$fxSpecKeys = {"Image", "Latent", "Embedding", "Steps"};

fxSpecQ[All] := True;

fxSpecQ[k_String] := MemberQ[$fxSpecKeys, k];

fxSpecQ[ks_List] := ks =!= {} && AllTrue[ks, fxSpecQ];

fxSpecQ[_] := False;

(* --- single prompt: First @ the one-element batch (so a {prompt} carries the
   spec through one code path). --- *)

FluxGenerate[prompt_String, opts : OptionsPattern[]] := First @ FluxGenerate[{prompt}, "Image", opts];

FluxGenerate[prompt_String, spec_ ? fxSpecQ, opts : OptionsPattern[]] := First @ FluxGenerate[{prompt}, spec, opts];

(* n images of the SAME prompt: one entry per copy, so each gets a distinct fresh
   seed (the per-image SeedRandom in STAGE 2), reusing the one session -- only the
   first image pays the cold build, the rest replay warm.  An optional trailing
   spec selects what each copy returns. *)

FluxGenerate[prompt_String, n_Integer ? Positive, opts : OptionsPattern[]] := FluxGenerate[ConstantArray[prompt, n], "Image", opts];

FluxGenerate[prompt_String, n_Integer ? Positive, spec_ ? fxSpecQ, opts : OptionsPattern[]] := FluxGenerate[ConstantArray[prompt, n], spec, opts];

(* batch with no explicit spec defaults to "Image". *)

FluxGenerate[prompts_List, opts : OptionsPattern[]] := FluxGenerate[prompts, "Image", opts];

(* build (or fetch) the persistent session for these settings. *)

fxSessionGet[dev_, imgSize_, nSteps_, modelDir_, q8_] := Module[{key = {modelDir, dev, imgSize, nSteps, q8}},
    Lookup[
        $fxSession
        ,
        Key[key]
        ,
        $fxSession[key] = Module[{tBuild, s},
            {tBuild, s} = AbsoluteTiming[fxSessionBuild[dev, imgSize, nSteps, modelDir, q8]];
            fxTiming["session-build", tBuild];
            s
        ]
    ]
]

(* The three models' weights (~16 GB across Qwen + transformer + VAE
   safetensors) are zero-copy disk-mmap wraps.  The default MADV_WILLNEED
   readahead faults the WHOLE of each file resident at load time, so a session
   peaks at ~19 GB of resident weight pages even though each weight is read once
   to upload.  THVM_MMAP_NO_WILLNEED defers the fault to the per-weight upload
   (which then MADV_DONTNEEDs the pages), keeping the host weight working set to
   roughly one weight at a time -- session peak drops to ~5 GB.  Set here (not
   globally) so the session is memory-bounded by default; a user export wins. *)

(* Export a THVM_* env var to a default ONLY if the caller has not already set it,
   so a bare FluxGenerate works out of the box while every knob stays overridable.
   Names the variable once (the bare If[Environment[..] === $Failed, SetEnvironment]
   spelled it twice). *)

fxEnvDefault[name_String, value_String] := If[Environment[name] === $Failed, SetEnvironment[name -> value]];

fxBoundMemory[] := (
    fxEnvDefault["THVM_MMAP_NO_WILLNEED", "1"];
(* A bare FluxGenerate call should just work without the caller exporting any
   THVM_* knob.  THVM_FWD_RECLAIM defaults OFF here: it is a realize-boundary GC
   (built for training) that frees the stranded cross-realize weight DiskMaps and
   MADV_DONTNEEDs their mmap pages, so every fresh-process cold re-faults ~16GB of
   weights from disk: the profiled ~8s of cold "materialize".  Off, the
   disk-mmap weights stay RAM-resident like mflux (read once, stay in RAM): cold
   drops ~22s -> ~15s, activations are still freed by the per-realize watermark
   rollback, and the ~19GB resident weight set fits a 48GB box for inference.  A
   memory-constrained caller exports THVM_FWD_RECLAIM=1 to bound the working set
   at a slower cold; the live-bytes ceiling below stays a ~60%-RAM safety belt. *)
    fxEnvDefault["THVM_FWD_RECLAIM", "0"];
    fxEnvDefault["THVM_MAX_LIVE_BYTES", ToString[Round[0.6 $SystemMemory]]];
(* Zero-copy mmap weight wraps (THVM_ZEROCOPY=1): each weight aliases its
   safetensors page-cache pages rather than being uploaded to a pinned device
   buffer.  This is the memory-SAFE default and the load-bearing reason a bare
   FluxGenerate cannot crash the kernel: a borrowed wrap holds no device-owned
   bytes, so metal_record_memory_peak skips it (src/backend/metal/_.m) and the
   ~16GB of FLUX weights never count toward the THVM_MAX_LIVE_BYTES ceiling --
   no matter how many sessions co-reside (a second ImageSize) or how loaded the
   host kernel already is.  Live device buffers (THVM_ZEROCOPY=0) upload + pin
   each weight, which DOES count: a single FLUX session then sits near 0.6*RAM
   and any second resident weight set tips it over, firing the ceiling backstop
   that kills the kernel.  The wrap's one cost is that the OS may evict the
   page-cache under pressure and re-fault it at the DiT stage (a slower cold) --
   but a slower cold beats a dead kernel.  A user who is not memory-bound and
   wants the pinned-resident cold can export THVM_ZEROCOPY=0. *)
    fxEnvDefault["THVM_ZEROCOPY", "1"];
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
    fxEnvDefault["THVM_METAL_REUSE_BUFS", "0"]
);

(* Evaluate body with BEAM=2 (autotune on), then restore the prior BEAM.  Used to
   enable the VAE conv autotune for ONLY the eager pretune + the STAGE-3 VAE
   capture, leaving the velocity/Qwen captures BEAM-off (those regress under BEAM).
   A runtime SetEnvironment reaches the C getenv autotune gate (verified); replays
   never re-autotune, so this only matters at the gen-1 lazy captures. *)

SetAttributes[fxBeamScope, HoldFirst];

fxBeamScope[body_] := Module[{beamSave = Environment["BEAM"], r},
    SetEnvironment["BEAM" -> "2"];
    r = body;
    SetEnvironment["BEAM" -> If[beamSave === $Failed, None, beamSave]];
    r
]

(* fetch (or build once) the size-INDEPENDENT weight base for these
   {modelDir,dev,q8}.  Every ImageSize/Steps session reuses it, so the ~16 GB
   of transformer/Qwen/VAE weights load into ONE device context exactly once. *)

fxWeightBase[dev_, modelDir_, q8_] := Module[{key = {modelDir, dev, q8}},
    Lookup[
        $fxWeightBase
        ,
        Key[key]
        ,
        $fxWeightBase[key] = fxWeightBaseBuild[dev, modelDir, q8]
    ]
]

(* LAYER 1 -- the size-INDEPENDENT load, built ONCE per {modelDir,dev,q8}: the
   ONE shared context, the cast helper, all three models' weights, the temb
   closure, and the (fixed text-length) Qwen forward capture.  The image RoPE
   table, the velocity/VAE captures, and the sigma schedule depend on ImageSize
   and live in fxSessionBuild (LAYER 2). *)

fxWeightBaseBuild[dev_, modelDir_, q8_] := Module[{
    stxt,
    tokDir,
    td,
    tfPath,
    qwPaths,
    vaePath,
    fxCfg,
    qwCfg,
    ctxT,
    caL,
    wfT,
    tembFn,
    wfq,
    qwJit,
    wfV,
    wsubV
},
    fxBoundMemory[];
    stxt = 512;
    tokDir = FileNameJoin[{modelDir, "tokenizer"}];
    tfPath = FileNameJoin[{modelDir, "transformer", "diffusion_pytorch_model.safetensors"}];
    qwPaths = FileNameJoin[{modelDir, "text_encoder", #}]& /@ {"model-00001-of-00002.safetensors", "model-00002-of-00002.safetensors"};
    vaePath = FileNameJoin[{modelDir, "vae", "diffusion_pytorch_model.safetensors"}];
(* Warm the OS page cache for all three safetensors NOW, on background
   threads, so the ~6 s of SSD read overlaps the JIT capture below instead
   of stalling serially inside the first forward's zero-copy fault-in (each
   weight is faulted resident at its first matmul; with the page already
   cached that fault runs at memory speed).  Non-blocking -- returns at once;
   the reads race ahead of the loaders.  ORDER MATCHES ACCESS ORDER: Qwen
   runs FIRST (STAGE 1 text-encode) and the transformer SECOND (STAGE 2
   velocity sample), so prefetch Qwen first -- it is used immediately, with
   the smallest eviction window, while the transformer (the 7.4 GB bulk)
   warms during the ~5 s Qwen encode and is fresh when STAGE 2 faults it.
   Prefetching the transformer first (the old order) left it idle through the
   whole build + Qwen stage, so ~5 GB of it evicted under cache pressure and
   the STAGE-2 fault-in re-read it serially.  VAE (small) last. *)
    TDiskPrefetchAsync[Join[qwPaths, {tfPath, vaePath}]];
    fxCfg = <|"eps" -> 1.*^-6, "num_double" -> 5, "num_single" -> 20, "heads" -> 24, "head_dim" -> 128|>;
    qwCfg = <|
        "heads" -> 32,
        "kv_heads" -> 8,
        "head_dim" -> 128,
        "eps" -> 1.*^-6,
        "theta" -> 1000000,
        "layers" -> 27,
        "captureLayers" -> {8, 17, 26}
    |>;
    td = qwTokenizerData[tokDir];
(* ONE shared persistent context for all three models, reused across EVERY
   ImageSize/Steps session.  An earlier design used a separate context per
   model, but LAM_SHAPE_TABLE (the side table that tells materialize a captured
   lambda's input shape) is keyed by raw heap loc, and heap locs are
   PER-CONTEXT -- each context's heap restarts at the same low locs.  So the
   qwJit and velJit capture lambdas landed at the SAME loc in their separate
   contexts and collided in that global table: whichever captured second read
   the other's Shape, built its view index from the wrong dims, and leaked a
   stale tensor handle into the index expression -- the tile-JIT codegen then
   emitted `/*?*/` ("expected expression") and every transformer kernel failed
   to compile.  A single context gives every capture lambda a globally-unique
   loc, so no collision.  Because this same context is shared across sessions
   too, a second ImageSize never builds a second context with colliding locs
   (and never re-loads the 16 GB of weights) -- the size-specific velJit/vaeJit
   captures of every session land at fresh locs in the one heap.  The
   cross-capture buffer aliasing the per-context split was meant to prevent does
   NOT occur: each TJit capture retains its own buffers, so the replays
   read/write disjoint slots.  Weights are zero-copy disk-mmap wraps (~0 phys
   footprint on Apple unified memory), so all three models co-reside well under
   the 30GB ceiling.  Each stage crosses to the next as a HOST array.

   WARM-PROMPT-COLLAPSE (FIXED): previously only the FIRST prompt in a session
   decoded correctly -- every WARM prompt (a later prompt in a batch OR a second
   call on the cached session) came out as the FIRST prompt's image (a warm "a
   blue bird" after "a red apple" decoded an apple).  The cause was NOT the
   velocity: it produces distinct latents per prompt (verified -- distinct
   per-prompt latent means and a 0.99 image divergence once decoded eagerly).  It
   was the VAE decode's TJit REPLAY baking its latent input -- the warm replay
   re-decoded the FIRST prompt's latent for every image.

   PRIMARY CAUSE (FIXED): the VAE's first conv lowers its im2col over the latent
   as a strided VIEW chain that needs a CONTIGUOUS base, so the scheduler
   uploaded the latent into a contiguous device buffer.  But TJit realized the
   input arg into a DIFFERENT buffer (TRealize is non-memoizing, and the upload
   cache was only primed inside the capture scope) -- so the captured conv read
   the fn-body buffer while input_replace declared the separately-realized one,
   and the rebind found 0 sites.  FIXED by realizing the JIT input arg INSIDE
   the capture scope (Jit.wl jitCaptureOnce) so materialize_copy pins + caches
   the upload by copy-loc; the conv body's re-realize of the same latent now
   HITS that cache and reuses the declared buffer, so input_replace rebinds the
   warm latent.  Verified on the real VAE weights: denorm/unpatch + mid block +
   up blocks 0-1 replay byte-exactly (replay_err 0).

   SECONDARY CAUSE (FIXED): up_block 2 (512ch, 32x32) partially baked because its
   first ResBlock's conv_shortcut unfolds the JIT input directly via a 1x1 im2col,
   recorded as a replayable JIT_OP_GATHER.  capture.c gated that gather's replay to
   DT_FP32/DT_INT32 only, so the bf16 Metal VAE skipped it and the contiguous
   unfold kept the capture-step input -- the shortcut path baked the first prompt
   (cat/dog divergence 0.33 vs eager 0.91 at 64x64).  Fixed by gating the gather
   replay on itemsize (the gather is a pure strided byte permutation, dtype-
   agnostic).  The VAE is now TJit'd -- each prompt's distinct latent is read AND
   the eager decode's ~1.65 GB/gen LiveBytes leak is gone.  The
   flux/warm-prompt-rebinds-text-encoding test in Tests/flux_jit_replay.wlt covers
   the JIT path. *)
    ctxT = TContextNew[dev];
    (* --- load transformer weights + temb closure (in the shared context).  The
       cast helper caL is built here and reused by every size-specific session. --- *)
    {wfT, caL, tembFn} = TInContext[
        ctxT
        ,
        Module[{wt, wfTL, caLL, tembFnL, tLoadTf},
            caLL = If[dev === "cpu", TRealize[#]&, (TRealize @ TToDevice[TUOpCast[#, "bf16"], dev])&];
            {tLoadTf, wt} = AbsoluteTiming[TSafeTensorLoad[tfPath]];
(* COLD-START OVERLAP: background-fault the transformer's disk-mmap pages
   resident NOW, while it is idle.  The transformer is not touched until STAGE
   2 (velocity sample); STAGE 1 (Qwen encode) runs first.  TDiskPrefetchAsync
   above only warms the OS page cache; the per-process minor fault that maps
   those ~3 GB of pages into this address space still happens serially at the
   first STAGE-2 matmul (the zero-copy wrap's touch loop).  Kicking it off here
   on a detached thread overlaps that fault with the rest of session-build plus
   the Qwen text-encode, so STAGE 2 finds the transformer already resident.
   FLUX_NO_WARM_TF=1 disables it for an A/B baseline. *)
            If[ Environment["FLUX_NO_WARM_TF"] =!= "1", TDiskWarmAsync[Values[wt]]];
            wfTL = fxTransformerLoader[wt, dev, q8];
            tembFnL = fxTembFn[wfTL, dev];
            fxTiming["tf-load", tLoadTf];
            {wfTL, caLL, tembFnL}
        ]
    ];
(* --- Qwen context: load weights + capture the 27-layer device forward.
   cos/sin (length stxt) are closed over (constant across prompts); the
   per-prompt {stxt,2560} x + {stxt,stxt} addMask are the rebound JIT inputs.
   Every shape is concrete (kvar-free).  Text length stxt is fixed, so this
   capture is ImageSize-independent and belongs in the weight base. --- *)
    {qwJit, wfq} = TInContext[
        ctxT
        ,
        Module[{qw, wfqL, qct, qcos, qsin, qj, tLoadQw, tCapQw},
            {tLoadQw, qw} = AbsoluteTiming[Join[TSafeTensorLoad[qwPaths[[1]]], TSafeTensorLoad[qwPaths[[2]]]]];
            wfqL = fxQwenLoader[qw, dev];
            qct = TRoPEHalfSplitTable[stxt, qwCfg["head_dim"], qwCfg["theta"]];
            qcos = TToDevice[qct[[1]], dev];
            qsin = TToDevice[qct[[2]], dev];
            {tCapQw, qj} = AbsoluteTiming[
                TJit[
                    Function[{x, addMask},
                        qwenForward[x, addMask, qcos, qsin, wfqL, qwCfg]
                    ]
                ]
            ];
            fxTiming["qw-load", tLoadQw, "qw-capture", tCapQw];
            {qj, wfqL}
        ]
    ];
(* --- VAE context: load weights (f32) + build the conv-decode sub-weights.  The
   size-specific conv-decode capture lives in fxSessionBuild (it closes over the
   ImageSize grid via vaeCfg). --- *)
    {wfV, wsubV} = TInContext[
        ctxT
        ,
        Module[{vt, vKeys, wfVL, wsubVL},
            vt = TSafeTensorLoad[vaePath];
            vKeys = Keys[vt];
            wfVL = fxVaeLoader[vt, dev];
            wsubVL = fxVaeSub[wfVL, vKeys];
            {wfVL, wsubVL}
        ]
    ];
    <|
        "ctxT" -> ctxT,
        "caL" -> caL,
        "wfT" -> wfT,
        "tembFn" -> tembFn,
        "wfq" -> wfq,
        "qwJit" -> qwJit,
        "wfV" -> wfV,
        "wsubV" -> wsubV,
        "td" -> td,
        "fxCfg" -> fxCfg,
        "qwCfg" -> qwCfg,
        "stxt" -> stxt
    |>
]

(* LAYER 2 -- the size-SPECIFIC build, one per {modelDir,dev,imgSize,nSteps,q8}.
   Fetches the shared weight base and adds ONLY the ImageSize-dependent pieces:
   the image RoPE table, the velocity + VAE conv-decode captures, and the sigma
   schedule.  All captures land in the base's shared context. *)

fxSessionBuild[dev_, imgSize_, nSteps_, modelDir_, q8_] := Module[{
    base,
    w,
    h,
    gridH,
    gridW,
    simg,
    ctxT,
    caL,
    vaeCfg,
    sigmas,
    rope,
    rc,
    rs,
    velJit,
    vaeJit
},
    base = fxWeightBase[dev, modelDir, q8];
    ctxT = base["ctxT"];
    caL = base["caL"];
    {w, h} = imgSize;
    gridH = Round[h / 16];
    gridW = Round[w / 16];
    simg = gridH gridW;
    vaeCfg = <|"eps" -> 1.*^-6, "epsBn" -> 1.*^-4, "gridH" -> gridH, "gridW" -> gridW|>;
    sigmas = fxSigmas[simg, nSteps];
    (* --- image RoPE + velocity capture (in the shared context). --- *)
    {rc, rs, velJit} = TInContext[
        ctxT
        ,
        Module[{ropeT, rcL, rsL, vj, tCapTf},
            ropeT = fxRopeTable[gridH, gridW, base["stxt"]];
            rcL = caL @ TTensorCreate[ropeT["cos"]];
            rsL = caL @ TTensorCreate[ropeT["sin"]];
            {tCapTf, vj} = AbsoluteTiming[fxVelocityJit[rcL, rsL, base["wfT"], base["fxCfg"]]];
            fxTiming["tf-capture", tCapTf];
            {rcL, rsL, vj}
        ]
    ];
(* --- VAE conv decode: capture the conv decode.  The packed latent {simg,128}
   is the only rebound JIT input.  TRealize the body: vaeDecoder ends with a
   lazy TClip, so a bare capture would hand the replay an unrealized UOP
   (Normal -> Missing). --- *)
    vaeJit = TInContext[
        ctxT
        ,
        Module[{},
(* EAGER PRETUNE the VAE conv decode (THVM_VAE_PRETUNE).  The VAE capture
   can't BEAM-search -- a bench dispatch mid-capture corrupts the
   recorded forward (autotune.c skips the search under jit_is_capturing);
   the cache-load path it DOES run only APPLIES a winner found earlier.
   So decode one representative latent EAGERLY under BEAM: that benches +
   disk-caches each conv's tile-opt winner, keyed by the conv's
   backend/dtype/shape/axis structure (capture-INDEPENDENT).  The VAE
   TJit captures LAZILY on its first decode call (gen 1, STAGE 3), where
   fxBeamScope re-enables BEAM so the bench-free cache-load applies those
   winners; the capture bakes the tuned axes into the recorded kernels,
   so every replay reuses them (capture.c dispatches the recorded ke
   directly, no re-autotune).  Verified: 63/63 eager-stored VAE keys HIT
   at capture; VAE warm 0.46s -> 0.08s.  The warm latent is RANDOM (a
   zero latent yields a degenerate eager decode that NaNs the first
   replay) and matches the replay's exact dtype path (bf16 GPU, f32 CPU).

   KNOWN LIMITATION: the eager decode shares this context (ctxV == ctxT
   == ctxQ; a separate context hits the LAM_SHAPE_TABLE loc-collision
   documented above), so its bench dispatches pollute the later
   velocity/Qwen capture state -- regressing dit ~2.0s -> ~6s and (with
   stage-3-only BEAM) corrupting velocity latents.  Closing that needs a
   tinygrad-style post-capture autotune pass (compile_linear/beam_search
   on isolated test buffers), so THVM_VAE_PRETUNE is OFF by default. *)
            If[ Environment["THVM_VAE_PRETUNE"] =!= $Failed,
                fxBeamScope @ Module[{warmLat},
                    warmLat = TRealize @ If[ dev === "cpu",
                        TToDevice[TTensorCreate @ NumericArray[RandomVariate[NormalDistribution[], {simg, 128}], "Real32"], dev]
                        ,
                        TUOpCast[
                            TToDevice[TTensorCreate @ NumericArray[RandomVariate[NormalDistribution[], {simg, 128}], "Real32"], dev],
                            "bf16"
                        ]
                    ];
                    TRealize @ vaeDecoder[warmLat, base["wfV"], base["wsubV"], vaeCfg]
                ]
            ];
(* TJit decode (the captured conv decode replays per prompt).  The packed
   latent {simg,128} is the only rebound JIT input; every warm prompt re-reads
   its own distinct latent, so the eager VAE's ~1.65 GB/gen LiveBytes leak is
   gone (the capture's buffers are pinned once and rewritten in place).

   Two replay bugs are fixed for this to be correct:

   PRIMARY (df8653ec): the conv im2col staged the latent into a SECOND device
   buffer the declared input never named, so input_replace found 0 rebind sites
   and every warm replay re-read the capture-time latent.  Fixed in Jit.wl by
   realizing the JIT input inside the capture scope so materialize_copy caches
   the upload by copy-loc and the conv body reuses the declared buffer.

   SECONDARY (capture.c JIT_OP_GATHER replay dtype guard): a conv_shortcut whose
   1x1 im2col unfolds the JIT input directly records a replayable JIT_OP_GATHER
   off that input.  Its replay was gated to DT_FP32/DT_INT32 only, so the bf16
   Metal VAE path skipped the gather and left the contiguous unfold holding the
   capture-step bytes -- up_block 2's shortcut baked the first prompt's input
   (warm decode only partially responded to the latent; cat/dog divergence 0.33
   vs eager 0.91 at 64x64).  Fixed by gating the gather replay on itemsize
   (1/2/4/8 bytes) since it is a pure strided byte permutation, not an
   arithmetic op.  The flux/warm-prompt-rebinds-text-encoding test in
   Tests/flux_jit_replay.wlt covers the JIT path. *)
            TJit @ Function[lat,
                TRealize @ vaeDecoder[lat, base["wfV"], base["wsubV"], vaeCfg]
            ]
        ]
    ];
    <|
        "ctxQ" -> base["ctxT"],
        "ctxT" -> base["ctxT"],
        "ctxV" -> base["ctxT"],
        "dev" -> dev,
        "ca" -> base["caL"],
        "wfq" -> base["wfq"],
        "qwCfg" -> base["qwCfg"],
        "td" -> base["td"],
        "stxt" -> base["stxt"],
        "rc" -> rc,
        "rs" -> rs,
        "tembFn" -> base["tembFn"],
        "velJit" -> velJit,
        "sigmas" -> sigmas,
        "simg" -> simg,
        "fxCfg" -> base["fxCfg"],
        "vaeCfg" -> vaeCfg,
        "qwJit" -> base["qwJit"],
        "vaeJit" -> vaeJit
    |>
]

(* an unrecognised spec (a String / List that fxSpecQ rejects, or anything not the
   count) issues FluxGenerate::badspec and falls back to "Image".  Placed AFTER the
   valid-spec overloads so it only fires on a genuinely bad spec. *)

FluxGenerate[prompt_String, spec_, opts : OptionsPattern[]] := (
    Message[FluxGenerate::badspec, spec];
    FluxGenerate[prompt, "Image", opts]
);

FluxGenerate[prompts_List, spec_ ? fxSpecQ, opts : OptionsPattern[]] := Module[
    {imgSize, seed, dev, nSteps, modelDir, initLat, negPrompt, showSteps, reportQ, q8}
    ,
(* a bare ImageSize -> n means a square n x n image (Set::shape if {n,n} is not
   formed); a pair {w, h} passes through.  Keep this normalize FIRST so the
   scalar reaches the session key + grid math as a pair. *)
    imgSize = Replace[OptionValue["ImageSize"], n_ ? NumberQ :> {Round[n], Round[n]}];
    seed = OptionValue[RandomSeeding];
    dev = OptionValue["Device"];
(* "Steps" is the idiomatic step-count control; "NumSteps" is the legacy alias.
   "Steps" wins when given an integer, else fall back to "NumSteps". *)
    nSteps = Replace[OptionValue["Steps"], Except[_Integer] :> OptionValue["NumSteps"]];
    initLat = OptionValue["InitialLatent"];
    negPrompt = OptionValue["NegativePrompt"];
    showSteps = TrueQ[OptionValue["ShowSteps"]];
    modelDir = OptionValue["ModelDir"] /. Automatic -> Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b";
(* ProgressReporting -> True | Automatic shows the progress panel (Automatic and
   True both report -- the panel itself is a near-no-op with no front end, e.g.
   in wolframscript, so it never errors headless); False runs silent. *)
    reportQ = OptionValue[ProgressReporting] =!= False;
    q8 = fxQuant8Q[TrueQ[OptionValue["Q8"]]];
(* FLUX.2-klein is guidance-distilled (no CFG), so a negative prompt cannot be
   applied -- warn loudly (don't silently no-op) and proceed without it. *)
    If[negPrompt =!= None, Message[FluxGenerate::nocfg, negPrompt]];
    fxWithProgress[reportQ, fxGenerateBody[prompts, spec, imgSize, seed, initLat, showSteps, dev, nSteps, modelDir, q8]]
]

(* Run `body` under a live progress panel that re-reads the mutated `text` +
   `progress` (RuleDelayed so each panel refresh sees the current values).  When
   reporting is off, just evaluate the body (no panel).  fxReport (a Block-scoped
   stage updater) is bound to mutate the two panel vars; fxGenerateBody calls it
   to advance the stage text + fraction.  The session build is held indeterminate
   (Progress -> Indeterminate) because its cold weight load can't be sub-divided
   from here; the per-image stages drive a concrete 0.45 -> 1.0 fraction. *)

SetAttributes[fxWithProgress, HoldRest];

fxWithProgress[reportQ_, body_] := If[ TrueQ[reportQ],
    Module[{progress = 0., text = "Loading FLUX.2-klein-4B..."},
        Block[{
            fxReport = Function[{t, p},
                text = t;
                progress = p
            ]
        },
            Progress`EvaluateWithProgress[body, <|"Text" :> text, "Progress" :> progress, "RemainingTime" -> Automatic|>]
        ]
    ]
    ,
    Block[{fxReport = (Null&)},
        body
    ]
]

(* default: fxReport is a no-op (so fxGenerateBody works even if called directly,
   outside fxWithProgress -- e.g. a future internal caller). *)

fxReport[_, _] := Null;

(* the set of per-image parts to materialise for a return spec.  All -> every part
   the pipeline can surface (+ "Steps" only when ShowSteps is on); a key/list maps
   to itself.  Used to skip stages a spec doesn't need (e.g. "Embedding" alone runs
   no sampler or decode; "Latent" alone runs no VAE). *)

fxNeeded[All, showSteps_] := If[showSteps, $fxSpecKeys, {"Image", "Latent", "Embedding"}];

fxNeeded[k_String, _] := {k};

fxNeeded[ks_List, _] := ks;

(* assemble one image's result from its computed parts (an Association keyed by the
   part name) per the return spec: a single key -> that bare part; All / a list ->
   an Association of the requested parts (All also carries "Steps" when present). *)

fxAssemble[parts_, All] := parts;(* every computed part *)

fxAssemble[parts_, k_String] := parts[k];

fxAssemble[parts_, ks_List] := KeyTake[parts, ks];

(* fraction layout (after the cold build, which is held indeterminate):
   encode 0.45 -> 0.55, sample 0.55 -> 0.90 (scoped per image, each image's N
   Euler steps subdivide its slice), decode 0.90 -> 1.0 (scoped per image).
   `spec` selects which parts to return; `initLat` (Automatic | a {simg,128} array)
   seeds STAGE 2; `showSteps` adds the per-step decoded progression. *)

fxGenerateBody[prompts_, spec_, imgSize_, seed_, initLat_, showSteps_, dev_, nSteps_, modelDir_, q8_] := Module[
    {
        sess,
        encHost,
        z0hosts,
        latents,
        stepLatsAll,
        embeds,
        results,
        n = Length[prompts],
        need,
        wantImg,
        wantLat,
        wantEmb,
        wantSteps,
        z0fixed
    }
    ,
    (* cold-build (or fetch) the session -- indeterminate while the weights load. *)
    fxReport["Loading FLUX.2-klein-4B...", Indeterminate];
    sess = fxSessionGet[dev, imgSize, nSteps, modelDir, q8];
    Module[{ca = sess["ca"], simg = sess["simg"], sigmas = sess["sigmas"], t1, t2, t3},
        need = fxNeeded[spec, showSteps];
        wantImg = MemberQ[need, "Image"];
        wantLat = MemberQ[need, "Latent"];
        wantEmb = MemberQ[need, "Embedding"];
(* per-step collect+decode runs whenever ShowSteps is on -- it feeds BOTH the
   live progression panel and the "Steps" return key. *)
        wantSteps = showSteps;
        z0fixed = fxValidateInitLat[initLat, simg];
(* --- STAGE 1: Qwen text-encode the whole batch (in the Qwen context).
   fxQwenEncodeBatch returns one {stxt,7680} HOST array per prompt (a distinct
   snapshot -- the captured qwJit output buffer is recycled across replays, so
   the encodings must be copied off-device before the next prompt overwrites
   it).  An "Embedding" return is then just those same host arrays. --- *)
        fxReport[If[n > 1, "Encoding " <> ToString[n] <> " prompts...", "Encoding prompt..."], 0.45];
        {t1, encHost} = AbsoluteTiming @ TInContext[sess["ctxQ"], fxQwenEncodeBatch[sess, prompts]]; (* {n,stxt,7680} host *)
        embeds = If[wantEmb, encHost, ConstantArray[Null, n]];
(* --- STAGE 2: transformer sample each prompt's latent (transformer
   context).  velJit is the persistent per-step capture; fxSampleJit keeps
   the running latent z ON DEVICE across the 4 Euler steps (device add, no
   per-step host read of v / re-upload of z) -- one final host read.  Each
   image owns the [0.55, 0.90] slice scaled by its index; its per-step
   callback advances inside that slice (fxStageFrac).  A fixed "InitialLatent"
   (z0fixed) replaces the per-image random noise (so a saved latent
   reproduces its image); Automatic samples fresh per-image seeded noise. --- *)
        If[ ! wantImg && ! wantLat && ! wantSteps,
            (* embedding-only: skip the sampler entirely. *)
            z0hosts = ConstantArray[Null, n];
            latents = ConstantArray[Null, n];
            stepLatsAll = ConstantArray[Null, n]
            ,
(* else run STAGE 2.  z0host is the {simg,128} INITIAL noise the sampler
   starts from -- the round-trip handle returned as "Latent" (fed back via
   "InitialLatent" it reproduces the image deterministically, exactly as a
   saved seed would).  finalLat (the denoised latent) feeds the VAE decode. *)
            fxReport[fxImgText["Generating", 1, n] <> "...", 0.55];
            {t2, {z0hosts, latents, stepLatsAll}} = AbsoluteTiming @ TInContext[
                sess["ctxT"]
                ,
                Transpose @ MapIndexed[
                    Function[{encArr, idx},
                        Module[
                            {
                                ee
                                ,
                                z0host
                                ,
                                z
                                ,
                                i = First[idx]
                                ,
                                samp
                                ,
                                lat
                                ,
                                steps
                                ,
                                stepCb = (
                                    fxReport[
                                        fxImgText["Generating", i, n] <> " (step " <> ToString[#1] <> " of " <> ToString[#2] <> ")...",
                                        fxStageFrac[0.55, 0.90, i, n, #1, #2]
                                    ]&
                                )
                            }
                            ,
(* upload this prompt's host encoding to the device for the
   velocity replay (ca: cast + TToDevice, bf16 on a GPU so the
   context_embedder matmul stays on the tensor cores). *)
                            ee = ca @ TTensorCreate @ NumericArray[encArr, "Real32"];
                            z0host = If[ z0fixed =!= None,
                                z0fixed
                                ,
                                (
                                    If[seed === Automatic, SeedRandom[], SeedRandom[seed + i]];
                                    RandomVariate[NormalDistribution[], {simg, 128}]
                                )
                            ];
                            z = ca @ TTensorCreate @ NumericArray[z0host, "Real32"];
                            samp = fxSampleJit[sess["velJit"], z, ee, sigmas, sess["tembFn"], ca, stepCb, wantSteps];
                            If[wantSteps, {lat, steps} = {Normal[samp[[1]]], samp[[2]]}, {lat, steps} = {Normal[samp], Null}];
                            fxDbg[
                                "  enc[",
                                i,
                                "] mean=",
                                Round[Mean[Flatten[encArr]], 0.0001],
                                "  lat mean=",
                                Round[Mean[Flatten[lat]], 0.0001]
                            ];
                            {z0host, lat, steps}
                        ]
                    ]
                    ,
                    encHost
                ]
            ];
            fxTiming[t1, t2]
        ];
(* --- STAGE 3: VAE decode each latent (VAE context).  The final latent gives
   the image; under ShowSteps the per-step latents are ALSO decoded (one VAE
   pass each, ~0.5s) so the All / {"Steps"} return carries the denoising
   progression as a list of Images.  Under THVM_VAE_PRETUNE, BEAM is on for
   this stage ONLY (fxBeamScope) so the gen-1 lazy VAE capture applies the
   eager-pretuned conv winners; replays here don't re-autotune. --- *)
        results = If[ ! wantImg && ! wantSteps,
            (* no decode needed -- assemble from the embedding / z0 latent alone. *)
            MapThread[fxAssemble[fxImageParts[#1, #2, #3, Null, Null], spec]&, {Range[n], embeds, z0hosts}]
            ,
(* else run STAGE 3.  The decode runs on the FINAL (denoised) latent; the
   "Latent" return part is the INITIAL z0 (the round-trip handle). *)
            fxReport[fxImgText["Decoding", 1, n] <> "...", 0.90];
            {t3, results} = AbsoluteTiming @ TInContext[
                sess["ctxV"]
                ,
                With[{
                    decodeAll = MapIndexed[
                        Function[{finalLat, idx},
                            Module[{i = First[idx], img, stepImgs},
                                fxReport[fxImgText["Decoding", i, n] <> "...", fxStageFrac[0.90, 1.0, i, n, 1, 1]];
                                img = If[wantImg, fxToImage @ fxVaeDecodeCached[sess, finalLat], Null];
                                (* decode each step latent to an Image (the progression). *)
                                stepImgs = If[ wantSteps && stepLatsAll[[i]] =!= Null,
                                    fxToImage @ fxVaeDecodeCached[sess, #]& /@ stepLatsAll[[i]]
                                    ,
                                    Null
                                ];
                                (* live-show the progression filmstrip in a notebook. *)
                                If[showSteps && stepImgs =!= Null, fxShowProgression[stepImgs, i, n]];
                                fxAssemble[
                                    fxImageParts[i, embeds[[i]], z0hosts[[i]], img, If[stepImgs =!= Null, {stepImgs, stepLatsAll[[i]]}, Null]],
                                    spec
                                ]
                            ]
                        ]
                        ,
                        latents
                    ]&
                },
                    If[Environment["THVM_VAE_PRETUNE"] =!= $Failed, fxBeamScope @ decodeAll[], decodeAll[]]
                ]
            ];
            fxTiming[t3];
            results
        ];
        fxReport["Done.", 1.0];
        results
    ]
]

(* assemble the per-part Association for one image: which parts are present depends
   on what the spec needed (a missing part is Null and dropped, so an All / list
   return only carries the parts that were actually computed).  "Latent" is the
   INITIAL z0 noise (the round-trip handle: fed back via "InitialLatent" it
   reproduces the image).  "Steps" packs the per-step decoded Images + the running
   (denoised) per-step latents as <|"Images"->.., "Latents"->..|>. *)

fxImageParts[i_, emb_, z0_, img_, steps_] := DeleteCases[
    <|
        "Image" -> img
        ,
        "Latent" -> If[z0 === Null, Null, NumericArray[z0, "Real32"]]
        ,
        "Embedding" -> If[emb === Null, Null, NumericArray[emb, "Real32"]]
        ,
        "Steps" ->
            If[ steps === Null,
                Null
                ,
                <|"Images" -> steps[[1]], "Latents" -> (NumericArray[#, "Real32"]& /@ steps[[2]])|>
            ]
    |>
    ,
    Null
]

(* validate a user "InitialLatent": Automatic -> None (sample fresh noise); a
   {simg,128} array (NumericArray or nested list) -> that array as z0.  A
   shape mismatch warns (FluxGenerate::badlatent) and falls back to fresh noise. *)

fxValidateInitLat[Automatic, _] := None;

fxValidateInitLat[lat_, simg_] := Module[{arr = Normal[lat], dims},
    dims = Dimensions[arr];
    If[ dims === {simg, 128},
        arr
        ,
        Message[FluxGenerate::badlatent, dims, simg];
        None
    ]
]

(* live denoising progression in a notebook: a labelled filmstrip of the per-step
   decoded Images, printed temporarily so it updates in place as the stages run.
   A no-op with no front end (PrintTemporary returns Null headless). *)

fxShowProgression[stepImgs_, i_, n_] := If[ TrueQ @ $Notebooks,
    PrintTemporary @ Labeled[Row[Thumbnail[#, 96]& /@ stepImgs, Spacer[4]], fxImgText["denoising", i, n], Top]
]

(* "Generating image 3 of 5" for a batch, plain "Generating image" for a single. *)

fxImgText[verb_, i_, n_] := If[n > 1, verb <> " image " <> ToString[i] <> " of " <> ToString[n], verb <> " image"];

(* fraction within [lo, hi] for image i of n at sub-step k of nk: each image owns
   an equal 1/n slice of [lo, hi], and its sub-steps subdivide that slice. *)

fxStageFrac[lo_, hi_, i_, n_, k_, nk_] := lo + (hi - lo) * ((i - 1) + k / nk) / n;

(* per-stage timing print, gated by THVM_FLUX_TIMING (off by default).  Accepts
   either bare numbers ({t1, t2}) or label/number pairs ("tf-load", t, ...). *)

fxTiming[ts__] := If[ Environment["THVM_FLUX_TIMING"] =!= $Failed,
    Print[
        "    [stage] ",
        StringRiffle[(If[NumberQ[#], ToString[Round[#, 0.01]], ToString[#]]&) /@ {ts}, " "],
        " s"
    ]
]

(* HoldAll so debug arguments (e.g. Mean[Flatten[Normal[enc]]]) are NOT
   evaluated unless THVM_FLUX_TIMING is set -- otherwise a device->host Normal
   readout would fire on every prompt just to compute a debug mean, defeating
   the keep-tensors-on-device path below. *)

SetAttributes[fxDbg, HoldAll];

fxDbg[a___] := If[Environment["THVM_FLUX_TIMING"] =!= $Failed, Print[a]]

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

fxQwenEncodeBatch[sess_, prompts_List] := With[{
    wfq = sess["wfq"], qwCfg = sess["qwCfg"], td = sess["td"], stxt = sess["stxt"], jit = sess["qwJit"]
},
    Function[p,
            Module[{tok, in, out, tTok, tIn, tRep, xr, mr},
                {tTok, tok} = AbsoluteTiming @ qwTokenize[p, td, stxt];
                {tIn, in} = AbsoluteTiming @ qwenInputs[Sequence @@ tok, wfq, qwCfg];
(* realize the JIT inputs contiguous so input_replace finds rebind sites
   on replay -- a lazy/non-contiguous input gets 0 sites and silently
   keeps the capture-time (first prompt's) bytes (see capture.c). *)
                xr = TRealize[in["x"]];
                mr = TRealize[in["addMask"]];
(* Read each encoding to a DISTINCT host array (Normal).  The qwJit replay
   writes its {stxt,7680} output into the SAME captured output buffer every
   call, so the per-prompt device tensors all ALIAS that one buffer -- left
   on-device, every prompt in a batch would read the LAST replay's bytes
   (the batch prompt-collapse: {apple,bird} decoded two birds).  Copying to
   the host snapshots each prompt's encoding before the next replay
   overwrites the buffer; STAGE 2 re-uploads per prompt.  (For n=1 there is
   one replay, so this is just the single host read either way.) *)
                {tRep, out} = AbsoluteTiming[Normal @ TRealize @ jit[xr, mr]];
                fxDbg[
                    "    qwen x mean=",
                    Round[Mean[Flatten[Normal[xr]]], 0.0001],
                    "  tokenize=",
                    Round[tTok, 0.001],
                    " replay=",
                    Round[tRep, 0.001]
                ];
                out
            ]
        ] /@ prompts
]

(* VAE decode one host latent {simg,128} -> {3,H,W} device pixels.  The conv
   decode is TJit-captured ONCE (session "vaeJit"); each call replays it,
   rebinding only the uploaded packed latent.  Bounds live bytes + cuts the
   eager ~2s dispatch overhead to a batched ICB replay. *)

fxVaeDecodeCached[sess_, lat_] := With[{dev = sess["dev"]},
    With[
        {up = TToDevice[TTensorCreate @ NumericArray[lat, "Real32"], dev]}
        ,
(* Cast the latent to the VAE weight dtype (bf16 on a GPU) so every conv
   is bf16(act) x bf16(W) -- a MIXED f32-act x bf16-weight conv falls to a
   slow scalar path.  CPU weights are f32, so leave the f32 latent. *)
        sess["vaeJit"][TRealize @ If[dev === "cpu", up, TUOpCast[up, "bf16"]]]
    ]
]

(* Capture the velocity net ONCE for the whole batch: z, enc, AND temb are all
   rebound TJit inputs (rc/rs/weights are closed over -- shared across images),
   so a 2-image batch pays ONE cold capture and every image after the first is a
   warm replay (rebinding its prompt encoding + noise + per-step temb), NOT a
   re-capture.  Closing enc over the function instead would re-capture per image
   (the closure differs), turning each "warm" image back into a cold start. *)

fxVelocityJit[rc_, rs_, wf_, cfg_] := TJit[
    Function[{z, e, tb},
        fxTransformer[z, e, tb, rc, rs, wf, cfg]
    ]
]

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

fxSampleJitFull[rc_, rs_, wf_, cfg_, tembs_, dts_, dev_] := TJit[
    Function[{z0, enc},
        Module[{z = z0, k, ncast},
            ncast = If[dev === "cpu", #&, TUOpCast[#, "bf16"]&];
            Do[
                z = ncast @ TUOpAdd[z, TUOpMul[fxTransformer[z, enc, tembs[[k]], rc, rs, wf, cfg], TUOpConst[N[dts[[k]]]]]]
                ,
                {k, 1, Length[tembs]}
            ];
            z
        ]
    ]
]

(* Per-step device-resident Euler over a SHARED captured velocity net
   (fxVelocityJit): replay the one-forward capture per step, rebinding z + enc +
   temb, and do the Euler update z = z + (sigma[k+1]-sigma[k]) v as a DEVICE op so
   the running latent NEVER leaves the device (no per-step Normal of v, no
   re-upload of z -- one final host read).  z is re-cast to the working dtype
   each step (ca: bf16 on GPU) so the next replay's matmuls stay on the tensor
   cores / batched ICB.  temb is M=1 ({1,3072}); its modulation matmuls pad to a
   concrete M=8 (fxModLinear) so the symbolic-M kvar never declines the Metal ICB.
   This is the feasible faithful sampler pending the B1 whole-loop capture above.
   `stepCb` (optional, default no-op) is called as stepCb[k, nSteps] after each of
   the nSteps Euler updates -- the progress hook for FluxGenerate's panel.

   `collectSteps` (default False) drives the per-step latent collector for
   "ShowSteps" / the All-return "Steps" key: when True, the host {simg,128} latent
   AFTER each Euler update is captured (one extra Normal per step) and the result
   is {finalLat, {stepLat1, .., stepLatN}} instead of the bare device tensor.  Off
   by default, so the no-extra-host-read fast path is unchanged -- the per-step
   reads are only paid when the caller asks to see the denoising progression. *)

fxSampleJit[vfn_, z0_, enc0_, sigmas_, tembFn_, ca_, stepCb_ : (Null&), collectSteps_ : False] := Module[
    {z = TRealize @ TUOpCast[z0, "f32"], nSteps = Length[sigmas] - 1, k, dt, v, ts, stepLats = {}}
    ,
    Do[
        dt = sigmas[[k + 1]] - sigmas[[k]];
(* velocity from the bf16-input replay (ca: bf16 on GPU / f32 on CPU); the
   Euler accumulates in the f32 z accumulator -- exactly the host path's
   f32 step with a bf16 net input, but z never leaves the device. *)
        {ts, v} = AbsoluteTiming[vfn[ca @ z, enc0, tembFn[sigmas[[k]]]]];
        fxDbg["    vel step ", k, " = ", Round[ts, 0.001], " s"];
        z = TRealize @ TUOpAdd[z, TUOpMul[TUOpCast[v, "f32"], TUOpConst[N[dt]]]];
        If[collectSteps, AppendTo[stepLats, Normal[z]]];
        stepCb[k, nSteps]
        ,
        {k, 1, nSteps}
    ];
    If[collectSteps, {z, stepLats}, z]
]

(* {3,H,W} pixels in [0,1] -> an Image (clip for any tiny fp overshoot). *)

fxToImage[t_] := Image[Clip[Normal[t], {0., 1.}], Interleaving -> False, ColorSpace -> "RGB"]

End[];

EndPackage[];