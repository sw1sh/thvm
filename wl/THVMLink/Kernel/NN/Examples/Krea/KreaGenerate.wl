(* KreaGenerate.wl - Krea 2 (12B single-stream DiT) text-to-image generator for
   thvm, on the generic diffusers-style pipeline (Pipeline.wl).

   Registers the Krea-specific components (the FP8 e4m3fn transformer loader +
   the krTransformer velocity forward, and the AutoencoderKLQwenImage VAE loader
   + krVaeDecode forward) into the shared $tisComponents registry, then routes
   KreaGenerate to tisPipeline driven by the Krea spec (read from the Krea2 config
   dir).  The shared scheduler / Qwen tokenizer are already registered in
   Pipeline.wl; the Qwen3 text encoder is registered here (Krea taps stacked
   hidden states, unlike FLUX's three-layer concat).

   Reached via TImageSynthesize[prompt, Method -> "Krea-2"]; mirrors the
   FluxGenerate calling conventions.  The architecture (see $kreaConfig) is the
   krea-ai/krea-2 mmdit.py SingleStreamDiT; the released turbo FP8 weights
   (krea2_turbo_fp8.safetensors) use the mmdit.py parameter names.

   Part of the WolframInstitute`THVMLink`Examples` package; follows wl/GUIDE.md
   style. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

KreaGenerate::usage = "KreaGenerate[prompt$, $$] synthesizes an Image with the Krea 2 12B single-stream DiT (Qwen3-VL text encoder -> single-stream MMDiT, distilled flow-match -> AutoencoderKLQwenImage decoder) on the generic thvm image-synthesis pipeline.  Reached via TImageSynthesize[prompt$, Method -> \"Krea-2\"], with the same calling conventions as FluxGenerate.  Full-weight generation runs on the GPU (the 12 GB FP8 transformer); the CPU path is for the tiny-config forward validation.";

KreaGenerate::nodir = "The Krea 2 model directory `1` has no model_index.json; set \"ModelDir\" to the diffusers config dir (the krea2_cfg folder under Models/Krea2/weights).";

(* Public so the validation test can build/inspect the spec, the FP8 loader, the
   tiny transformer/VAE configs, and the registered components by context. *)
krSpec::usage = "krSpec[modelDir$] returns the Krea 2 pipeline spec (tisModelSpec over the Krea2 config dir).";
krLoadFp8::usage = "krLoadFp8[t, dev] dequantizes an FP8 e4m3fn weight tensor t (or returns a non-fp8 tensor unchanged) to f32 on dev; the thvm engine decodes float8_e4m3fn natively, so this is a cast.";
krImageConfig::usage = "krImageConfig[] / krImageConfig[overrides$] returns the Krea 2 transformer/VAE architecture config used by the forward (overridable for the tiny CPU validation).";

Begin["`Private`"];

(* Krea 2 architecture config (krea-ai/krea-2 mmdit.py SingleStreamDiT +
   AutoencoderKLQwenImage).  DiT: features 6144 = 48 heads * 128 head_dim, 12 KV
   heads (GQA), 28 blocks; text fusion txtDim 2560, 20 heads (no GQA), 12 tapped
   layers; patch 2, 16 latent channels, RoPE theta 1000 over axes (t,h,w) with
   per-axis dims [head_dim - 12*(head_dim/16), 6*(head_dim/16), 6*(head_dim/16)]
   = [32,48,48] (sum 128); timestep dim 256; distilled 8-step.  VAE base_dim 96,
   dim_mult {1,2,4,4}, 2 res blocks, z_dim 16. *)

$kreaConfig = <|
    "features" -> 6144,
    "heads" -> 48,
    "kvHeads" -> 12,
    "headDim" -> 128,
    "layers" -> 28,
    "txtDim" -> 2560,
    "txtHeads" -> 20,
    "txtKvHeads" -> 20,
    "txtLayers" -> 12,
    "multiplier" -> 4,
    "patch" -> 2,
    "channels" -> 16,
    "theta" -> 1000.,
    "tDim" -> 256,
    "axesDims" -> {32, 48, 48},
    "eps" -> 1.*^-5,
    "steps" -> 8
|>;

$kreaVaeConfig = <|
    "eps" -> 1.*^-12,
    "dimMult" -> {1, 2, 4, 4},
    "numResBlocks" -> 2,
    "zDim" -> 16,
    "latentsMean" -> {-0.7571, -0.7089, -0.9113, 0.1075, -0.1745, 0.9653, -0.1517,
        1.5508, 0.4134, -0.0715, 0.5517, -0.3632, -0.1922, -0.9497, 0.2503, -0.2921},
    "latentsStd" -> {2.8184, 1.4541, 2.3275, 2.6558, 1.2196, 1.7708, 2.6052, 2.0743,
        3.2687, 2.1526, 2.8652, 1.5579, 1.6382, 1.1253, 2.8251, 1.916}
|>;

krImageConfig[] := $kreaConfig
krImageConfig[overrides_Association] := Join[$kreaConfig, overrides]

(* === FP8 e4m3fn loader ============================================
   The transformer turbo weights are float8_e4m3fn weight-only on the 2-D
   matrices.  thvm's TSafeTensorLoad maps the safetensors F8_E4M3 dtype to the
   "fp8e4m3" runtime tag, and the engine decodes fp8e4m3 -> f32 natively
   (src/dtype/fp8.c fp8e4m3_to_f32, a port of tinygrad's fp8_to_float: 1 sign /
   4 exp / 3 mantissa, bias 7).  So dequant is a value-preserving TUOpCast to f32
   (or bf16 on a GPU) -- no host decode.  Non-fp8 weights (norms, biases,
   modulation, kept high-precision in the turbo file) pass through with a cast to
   the working dtype. *)

krFp8Q[t_] := TTensorDType[t] === "fp8e4m3"

krLoadFp8[t_, dev_] := With[{wdt = If[dev === "cpu", "f32", "bf16"]},
    TToDevice[TUOpCast[t, wdt], dev]]

(* a name->TTerm transformer loader: dequantize fp8 weights, cast the rest, cache
   per name.  The 1-D scales/biases/modulation in the FP8 file are stored
   high-precision and pass straight through. *)
krTransformerLoader[wt_, dev_] := Module[{cache = <||>},
    n |-> Lookup[cache, n, cache[n] = TRealize @ krLoadFp8[wt[n], dev]]]

(* === position ids ================================================
   The (t,h,w) rotary coordinates for the [text ; image] sequence (diffusers
   Krea2 _prepare position ids): text tokens are all (0,0,0); image token (h,w)
   is (0, h, w).  Returns a {Stxt+Simg, 3} host list. *)
krPositionIds[stxt_, gridH_, gridW_] := Join[
    ConstantArray[{0, 0, 0}, stxt],
    Flatten[Table[{0, h, w}, {h, 0, gridH - 1}, {w, 0, gridW - 1}], 1]
]

(* === component registration ======================================
   Register the Krea transformer + VAE (and the Qwen3 text encoder) into the
   shared $tisComponents registry.  Done at load time so tisPipeline can resolve
   the Krea spec's class names.  The shared scheduler / Qwen2 tokenizer are
   registered in Pipeline.wl. *)

(* transformer: the loader builds the FP8 weight lookup + the config; the forward
   returns a velocity step closure (zPacked, sigma) |-> velocity, closing over
   the text embedding (stacked tapped states {Stxt, L, txtDim}), the position
   ids, the key-padding mask, and the weights. *)
krRegisterTransformer[] := tisRegisterComponent["Krea2Transformer2DModel", <|
    "loader" -> Function[{compSpec, dev},
        <|"wf" -> krTransformerLoader[TSafeTensorLoad[krTfWeightPath[compSpec]], dev],
          "cfg" -> krMergeConfig[compSpec], "dev" -> dev|>],
    "forward" -> Function[{built, textEmb, attMask, run},
        Module[{wf = built["wf"], cfg = built["cfg"], stxt, pos, mask},
            stxt = Dimensions[textEmb][[1]];
            pos = krPositionIds[stxt, run["gridH"], run["gridW"]];
            mask = attMask;
            (* the velocity step closure: unpack {simg, ch*patch^2} latent rows,
               run krTransformer, return the velocity. *)
            Function[{zPacked, sigma}, krTransformer[zPacked, textEmb, sigma, pos, mask, wf, cfg]]
        ]]
|>]

(* VAE: the loader builds the decoder weight lookup + the wsub slicer; the
   forward unpatchifies the denoised packed latent to {z_dim, gH, gW} and runs
   krVaeDecode. *)
krRegisterVae[] := tisRegisterComponent["AutoencoderKLQwenImage", <|
    "loader" -> Function[{compSpec, dev},
        Module[{vt = TSafeTensorLoad[krVaeWeightPath[compSpec]], keys, wf},
            keys = Keys[vt];
            wf = Module[{cache = <||>},
                n |-> Lookup[cache, n, cache[n] = TRealize @ TToDevice[TUOpCast[vt[n], If[dev === "cpu", "f32", "bf16"]], dev]]];
            <|"wf" -> wf, "keys" -> keys, "cfg" -> krMergeVaeConfig[compSpec], "dev" -> dev|>]],
    "forward" -> Function[{built, zFinal, run},
        Module[{wf = built["wf"], keys = built["keys"], cfg = built["cfg"], zSpatial, wsub},
            wsub = krVaeSub[wf, keys];
            zSpatial = krUnpatchify[zFinal, run["gridH"], run["gridW"], cfg["zDim"], krPatch[cfg]];
            krVaeDecode[zSpatial, wf, wsub, cfg]
        ]]
|>]

(* Qwen3-VL text encoder (Qwen3VLModel language tower): run the 36 decoder
   layers, tap the Krea select-layers, and STACK them per token to {Stxt, L,
   txtDim} (diffusers Krea2 text_encoder_select_layers).  The loader builds the
   weight lookup + the encoder config (layer count / dims from the spec's
   text_config, the actDType + weightPrefix for the device), the forward runs
   krQwenEncodeStack (KreaEncoder.wl).  For text-to-image only the language tower
   is used; the mrope collapses to plain rope (no image tokens), so the Qwen3
   half-split rope is exact (see KreaEncoder.wl). *)
krRegisterEncoder[] := (
    tisRegisterComponent["Qwen3VLModel", krQwenEncoderImpl[]];
    tisRegisterComponent["Qwen3Model", krQwenEncoderImpl[]]
)

krQwenEncoderImpl[] := <|
    "loader" -> Function[{compSpec, dev},
        <|"wf" -> krQwenLoader[TSafeTensorLoad[krEncWeightPath[compSpec]], dev],
          "cfg" -> krEncConfig[compSpec, dev]|>],
    "forward" -> Function[{built, ids, attMask},
        krQwenEncodeStack[ids, attMask, built["wf"], built["cfg"]]]
|>

(* a name->TTerm encoder loader: the embed table stays HOST-resident (qwEmbed
   host-gathers its rows), everything else uploads to the device cast to the
   working dtype, cached per name. *)
krQwenLoader[qwt_, dev_] := Module[{cache = <||>, embedKey},
    embedKey = First[Select[Keys[qwt], StringEndsQ[#, "embed_tokens.weight"] &], "embed_tokens.weight"];
    n |-> Lookup[cache, n, cache[n] =
        If[ StringEndsQ[n, "embed_tokens.weight"], qwt[n],
            TToDevice[TUOpCast[qwt[n], If[dev === "cpu", "f32", "bf16"]], dev]]]
]

(* encoder config from the spec's text_config (Qwen3-VL-4B) + the device dtype +
   the language-tower weight prefix.  A tiny-validation override comes via
   compSpec["thvmConfig"]. *)
krEncConfig[compSpec_, dev_] := Module[{tc, base},
    If[ KeyExistsQ[compSpec, "thvmConfig"], Return[compSpec["thvmConfig"]]];
    tc = Lookup[Lookup[compSpec, "config", <||>], "text_config", <||>];
    base = krQwenTextConfig[<|
        "layers" -> Lookup[tc, "num_hidden_layers", 36],
        "heads" -> Lookup[tc, "num_attention_heads", 32],
        "kv_heads" -> Lookup[tc, "num_key_value_heads", 8],
        "head_dim" -> Lookup[tc, "head_dim", 128],
        "hidden" -> Lookup[tc, "hidden_size", 2560],
        "intermediate" -> Lookup[tc, "intermediate_size", 9728],
        "eps" -> Lookup[tc, "rms_norm_eps", 1.*^-6],
        "theta" -> N @ Lookup[Lookup[tc, "rope_parameters", <||>], "rope_theta", 5000000],
        "txtLayers" -> $kreaConfig["txtLayers"],
        "weightPrefix" -> krEncWeightPrefix[compSpec],
        "actDType" -> If[dev === "cpu", "f32", "bf16"]
    |>];
    base
]

(* the language-tower weight prefix inside the text_encoder safetensors: the
   standalone Qwen3VLTextModel stores layers.* / embed_tokens.weight; inside the
   full Qwen3VLModel the tower is under model.language_model.* (the loader probes
   the actual keys, falling back to "").  Overridable via compSpec["weightPrefix"]. *)
krEncWeightPrefix[compSpec_] := Lookup[compSpec, "weightPrefix", ""]

krEncWeightPath[compSpec_] := Lookup[compSpec, "weightPath", FileNameJoin[{compSpec["dir"], "model.safetensors"}]]

(* register all Krea components.  Done lazily (krEnsureRegistered, run once on the
   first spec/pipeline call) NOT at load time: the Examples loader Gets the model
   files path-sorted, so Krea/* may load BEFORE Pipeline.wl defines
   tisRegisterComponent -- a load-time call would then silently no-op.  By the
   time krSpec / KreaGenerate run, every Examples file is loaded. *)
krEnsureRegistered[] := (
    krRegisterTransformer[];
    krRegisterVae[];
    krRegisterEncoder[];
    krEnsureRegistered[] = Null
)

(* === weight-path + config helpers ================================ *)

(* the transformer weight file: the FP8 turbo single file when present, else the
   sharded diffusers safetensors under the config's transformer subfolder. *)
krTfWeightPath[compSpec_] := Lookup[compSpec, "weightPath", FileNameJoin[{compSpec["dir"], "diffusion_pytorch_model.safetensors"}]]
krVaeWeightPath[compSpec_] := Lookup[compSpec, "weightPath", FileNameJoin[{compSpec["dir"], "diffusion_pytorch_model.safetensors"}]]

(* merge the spec's config.json into the architecture config (the config keys
   override $kreaConfig where named; the tiny validation passes a full override). *)
krMergeConfig[compSpec_] := Lookup[compSpec, "thvmConfig", $kreaConfig]
krMergeVaeConfig[compSpec_] := Lookup[compSpec, "thvmConfig", $kreaVaeConfig]
krPatch[cfg_] := Lookup[cfg, "patch", 2]

(* wsub[prefix]: the sub-Association of (key-with-prefix-stripped -> loaded TTerm)
   for every weight under prefix (reused from the FLUX VAE idiom). *)
krVaeSub[wf_, keys_][prefix_] := Association @ Map[
    (StringDrop[#, StringLength[prefix]] -> wf[#]) &,
    Select[keys, StringStartsQ[#, prefix] &]]

(* unpatchify: the denoised packed latent {simg, z_dim*patch^2} -> {z_dim, gH, gW}.
   The transformer's last layer emits patch*patch*channels per image token; the
   diffusers _unpack_latents reshapes {(gH gW), C p p} -> permute -> {C, gH p,
   gW p}.  patch p, z_dim C; latent grid gH=gridH, gW=gridW (the patched side). *)
krUnpatchify[zPacked_, gridH_, gridW_, zDim_, patch_] := TUOpReshape[
    TUOpPermute[
        TUOpReshape[zPacked, {gridH, gridW, zDim, patch, patch}],
        {2, 0, 3, 1, 4}],
    {zDim, gridH patch, gridW patch}]


(* === spec ========================================================= *)

krSpec[modelDir_String] := (krEnsureRegistered[]; tisModelSpec[modelDir])

(* === public entry =================================================
   Build the Krea spec from the config dir and run tisPipeline.  Full-weight
   generation (the 12 GB FP8 transformer + Qwen3-VL + the QwenImage VAE) runs on
   the GPU; this routes the request through the generic pipeline. *)

Options[KreaGenerate] = {
    "ImageSize" -> {1024, 1024},
    "Steps" -> Automatic,
    RandomSeeding -> Automatic,
    "InitialLatent" -> Automatic,
    "NegativePrompt" -> None,
    "ShowSteps" -> False,
    "Device" -> "metal",
    ProgressReporting -> Automatic,
    "ModelDir" -> Automatic
};

krDefaultModelDir[] := FileNameJoin[{$HomeDirectory, ".cache", "thvm", "krea2", "krea2_cfg"}]

KreaGenerate[prompt_, opts : OptionsPattern[]] := Module[{
    modelDir, dev, steps, seed, initLat, imgSize, w, h, gridH, gridW, spec, runOpts
},
    modelDir = OptionValue["ModelDir"];
    If[modelDir === Automatic, modelDir = krDefaultModelDir[]];
    If[ ! FileExistsQ[FileNameJoin[{modelDir, "model_index.json"}]],
        Message[KreaGenerate::nodir, modelDir];
        Return[$Failed]
    ];
    dev = OptionValue["Device"];
    steps = Replace[OptionValue["Steps"], Automatic -> $kreaConfig["steps"]];
    seed = OptionValue[RandomSeeding];
    initLat = OptionValue["InitialLatent"];
    imgSize = OptionValue["ImageSize"];
    {w, h} = If[ ListQ[imgSize], imgSize, {imgSize, imgSize}];
    (* VAE downsamples /8 and the patchify packs 2x2, so the patched-latent side
       is image/16. *)
    gridH = Round[h/16];  gridW = Round[w/16];
    spec = krSpec[modelDir];
    runOpts = <|
        "Device" -> dev, "Steps" -> steps, "Seed" -> seed,
        "InitialLatent" -> initLat, "Grid" -> {gridH, gridW},
        "TextSeqLen" -> 512
    |>;
    tisPipeline[spec, prompt, runOpts]
]

End[];

EndPackage[];
