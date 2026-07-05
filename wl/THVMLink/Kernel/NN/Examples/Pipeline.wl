(* Pipeline.wl - a generic, diffusers-style image-synthesis pipeline for thvm.

   A diffusers model is its config directory: a model_index.json names the
   pipeline's components (scheduler / text_encoder / tokenizer / transformer /
   vae), each as a (library, class-name) pair, and each component has its own
   subfolder with a config.json and weights.  This file reads that directory into
   a spec (tisModelSpec), maps each diffusers class name to a thvm component
   implementation via a registry ($tisComponents), and runs the shared
   flow-match generation flow over those components (tisPipeline): tokenize ->
   text-encode -> Euler denoise over the transformer -> VAE decode.

   FLUX.2-klein and Krea 2 differ ONLY in the transformer / VAE / encoder
   components, which are pluggable through the registry; the orchestration here
   is model-agnostic.  FLUX keeps its own bespoke FluxGenerate path untouched;
   this generic pipeline currently drives Krea 2 (KreaGenerate -> tisPipeline).

   Loaded into the shared WolframInstitute`THVMLink`Examples` package alongside
   the FLUX and Krea component files; reuses their helpers (fxLinear, the Qwen
   encoder, the flow-match sigma schedule).  Follows wl/GUIDE.md style. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

(* Public surface so a test can load the package by context and drive the spec
   loader / registry / orchestration directly on tiny CPU configs. *)
tisModelSpec::usage = "tisModelSpec[modelDir$] reads a diffusers model_index.json and the per-component config.json files under modelDir$ into a spec Association: \"components\" (component role -> <|\"class\", \"library\", \"dir\", \"config\"|>) plus \"modelDir\".";
tisComponents::usage = "tisComponents[] returns the component registry: a diffusers class name -> <|\"loader\", \"forward\"|> thvm implementation.  tisComponent[class$] looks one up.";
tisPipeline::usage = "tisPipeline[spec$, prompt$, opts$] runs the generic flow-match image-synthesis flow (tokenize -> text-encode -> Euler denoise over the transformer -> VAE decode) over the components named by spec$, returning the requested part(s).";
tisRegisterComponent::usage = "tisRegisterComponent[class$, impl$] registers a thvm component implementation impl$ (<|\"loader\", \"forward\"|>) under the diffusers class name class$ in the shared $tisComponents registry.  Public (not just Private) so a model file can register its components regardless of the Examples load order.";

Begin["`Private`"];

(* The on-device activation dtype the host-built inputs (latent, RoPE, time
   embedding, mask) cast to on a GPU; the generator (e.g. KreaGenerate's "Q8"
   option) sets it per run.  "bf16" (default) gives fast bf16 x bf16 matmuls;
   "f32" is full precision.  The cpu path always stays f32 regardless. *)
$tisActDtype = "bf16";

(* ============================================================
   tisModelSpec -- read a diffusers config directory into a spec.
   ============================================================ *)

(* a component entry in model_index.json is [library, class_name]; the leading
   underscore keys (_class_name, _diffusers_version) are pipeline metadata, not
   components.  Each non-meta key is a role (scheduler / text_encoder /
   tokenizer / transformer / vae) whose subfolder holds the component config. *)

tisComponentRoleQ[role_String] := ! StringStartsQ[role, "_"]

(* read one component's config.json (if its subfolder has one); the tokenizer /
   scheduler may carry their config under a different filename, so Missing is
   tolerated and the registry loader falls back to its own defaults. *)

tisReadConfig[dir_String] := Module[{f = FileNameJoin[{dir, "config.json"}]},
    If[ FileExistsQ[f], Import[f, "RawJSON"], Missing["NoConfig"]]
]

tisModelSpec[modelDir_String] := Module[{idxFile, idx, roles, comps},
    idxFile = FileNameJoin[{modelDir, "model_index.json"}];
    If[ ! FileExistsQ[idxFile],
        Message[tisPipeline::nospec, modelDir];
        Return[$Failed]
    ];
    idx = Import[idxFile, "RawJSON"];
    roles = Select[Keys[idx], tisComponentRoleQ];
    comps = Association @ Map[
        Function[role,
            With[{pair = idx[role], sub = FileNameJoin[{modelDir, role}]},
                role -> <|
                    "library" -> First[pair],
                    "class" -> Last[pair],
                    "dir" -> sub,
                    "config" -> tisReadConfig[sub]
                |>
            ]
        ],
        roles
    ];
    <|"modelDir" -> modelDir, "components" -> comps, "index" -> idx|>
]

(* ============================================================
   $tisComponents -- the class-name -> thvm implementation registry.

   A component implementation is <|"loader" -> (compSpec, dev) |-> built,
   "forward" -> (built, args) |-> result|>.  Registering a model is one entry
   per new class; the shared classes (FlowMatchEulerDiscreteScheduler, the Qwen3
   encoder, the Qwen2 tokenizer) are registered here once and reused.  The Krea
   transformer / VAE register themselves from Krea/KreaGenerate.wl via
   tisRegisterComponent so the heavy forward code stays in the model's file.
   ============================================================ *)

(* init guarded: a model file may load (and register its components) BEFORE this
   file in the recursive Examples Get (path-sorted), so do NOT clobber an
   already-populated registry. *)
If[ ! AssociationQ[$tisComponents], $tisComponents = <||>];

tisRegisterComponent[class_String, impl_Association] := (
    If[ ! AssociationQ[$tisComponents], $tisComponents = <||>];
    $tisComponents[class] = impl)

tisComponent[class_String] := Lookup[$tisComponents, class, Missing["NoComponent", class]]

tisComponents[] := $tisComponents

(* --- shared scheduler: FlowMatchEulerDiscreteScheduler.  Reuses the FLUX
   resolution-shifted sigma schedule (fxSigmas).  The Krea distilled-turbo path
   uses a fixed mu shift (is_distilled), handled by the model's "Steps" /
   schedule override; the generic scheduler exposes the sigma list. --- *)

tisRegisterComponent["FlowMatchEulerDiscreteScheduler", <|
    "loader" -> Function[{compSpec, dev}, compSpec],
    "sigmas" -> Function[{built, seq, nSteps}, fxSigmas[seq, nSteps]]
|>];

(* --- shared tokenizer: Qwen2Tokenizer.  Reuses the FLUX byte-level BPE +
   Qwen chat template (qwTokenizerData / qwTokenize).  The loader returns the
   tokenizer tables; the forward tokenizes one prompt to {ids, attMask}. --- *)

tisRegisterComponent["Qwen2Tokenizer", <|
    "loader" -> Function[{compSpec, dev}, qwTokenizerData[compSpec["dir"]]],
    "forward" -> Function[{built, prompt, seqLen}, qwTokenize[prompt, built, seqLen]]
|>];

(* ============================================================
   tisPipeline -- the generic orchestration.

   Structured like FluxGenerate's three stages but driven by the spec's
   components: each component's loader builds it, then the shared flow runs
   tokenize (tokenizer) -> encode (text_encoder) -> Euler denoise (transformer +
   scheduler) -> decode (vae).  The Krea transformer / VAE supply their own
   loader + forward through the registry, so this code never names a model.

   The transformer "forward" gets a step closure: (zPacked, sigma) |-> velocity,
   over which the Euler loop runs z <- z + (sigmaNext - sigma) * v.  The vae
   "forward" maps the final packed latent to the RGB image.
   ============================================================ *)

tisPipeline::nospec = "No model_index.json found under `1`; cannot build a pipeline spec.";
tisPipeline::nocomp = "The diffusers class `1` (role `2`) has no thvm component registered; the generic pipeline cannot run this model yet.";

(* resolve a role's built component, erroring if its class is unregistered. *)

(* Built components are cached for the session (keyed modelDir + role + device),
   like FLUX's model session: a repeat call reuses the loaded device-resident
   weights instead of re-reading + re-pinning them (a cold transformer load alone
   is ~24 GB and tens of seconds). *)
$tisBuilt = <||>;

tisBuildComponent[spec_, role_String, dev_] := Module[{comp, class, impl, key},
    comp = spec["components"][role];
    If[ MissingQ[comp], Return[Missing["NoRole", role]]];
    class = comp["class"];
    impl = tisComponent[class];
    If[ MissingQ[impl],
        Message[tisPipeline::nocomp, class, role];
        Return[$Failed]
    ];
    key = {spec["modelDir"], role, dev};
    Lookup[$tisBuilt, Key[key],
        $tisBuilt[key] = <|"impl" -> impl, "comp" -> comp, "built" -> impl["loader"][comp, dev]|>
    ]
]

(* the shared flow.  opts carries the run knobs (seq lengths, steps, image grid,
   device, seed, initial latent, return spec); the model's transformer / vae
   forwards close over their built weights + the pipeline config.  Returns the
   denoised latent + image (the caller's KreaGenerate assembles the return
   parts), keeping this orchestration model-agnostic. *)

tisPipeline::sigmas = "The sigma schedule `1` is not a numeric vector; check the \"Steps\" / \"Sigmas\" options.";

tisPipeline[spec_Association, prompt_, opts_Association] := tisWithProgress[
    Lookup[opts, "ProgressReporting", Automatic] =!= False,
    tisPipelineCore[spec, prompt, opts]
]

tisPipelineCore[spec_Association, prompt_, opts_Association] := Module[{
    dev, seqLen, nSteps, gridH, gridW, simg, seed, initLat,
    tok, enc, tf, vae, ids, attMask, textEmb, sigmas, stepFn, z0, zFinal, image
},
    dev = Lookup[opts, "Device", "cpu"];
    seqLen = Lookup[opts, "TextSeqLen", 512];
    nSteps = Lookup[opts, "Steps", 8];
    {gridH, gridW} = Lookup[opts, "Grid", {16, 16}];
    simg = gridH gridW;
    seed = Lookup[opts, "Seed", Automatic];
    initLat = Lookup[opts, "InitialLatent", Automatic];
    (* --- build components from the spec --- *)
    tisReport["Loading model components...", 0.];
    tok = tisBuildComponent[spec, "tokenizer", dev];
    enc = tisBuildComponent[spec, "text_encoder", dev];
    tf = tisBuildComponent[spec, "transformer", dev];
    vae = tisBuildComponent[spec, "vae", dev];
    If[ AnyTrue[{tok, enc, tf, vae}, # === $Failed &], Return[$Failed]];
    (* --- STAGE 1: tokenize --- *)
    tisReport["Tokenizing the prompt...", 0.02];
    {ids, attMask} = tok["impl"]["forward"][tok["built"], prompt, seqLen];
    (* --- STAGE 2: text-encode (stacked tapped hidden states for Krea) --- *)
    tisReport["Encoding the prompt...", 0.05];
    textEmb = enc["impl"]["forward"][enc["built"], ids, attMask];
    (* --- STAGE 3: flow-match Euler denoise over the transformer velocity net.
       The transformer forward yields a step closure (z, sigma) |-> velocity
       closing over the text embedding, the RoPE/position ids, and weights.
       Guard the schedule before any device work: a non-numeric sigma vector
       (a bad "Steps" / "Sigmas" option) would otherwise cascade into opaque
       Part / TUOpConst failures deep in the denoise loop. --- *)
    sigmas = Lookup[opts, "Sigmas", fxSigmas[simg, nSteps]];
    If[ ! VectorQ[sigmas, NumericQ],
        Message[tisPipeline::sigmas, Short[sigmas]];
        Return[$Failed]
    ];
    tisReport["Building the velocity net...", 0.25];
    stepFn = tf["impl"]["forward"][tf["built"], textEmb, attMask, <|
        "gridH" -> gridH, "gridW" -> gridW, "simg" -> simg, "device" -> dev
    |>];
    z0 = tisInitialLatent[initLat, simg, tf["comp"]["config"], seed, dev];
    zFinal = tisEulerSample[stepFn, z0, sigmas];
    (* --- STAGE 4: VAE decode (TRealize so the lazy decoder graph materializes to a
       concrete TTerm the caller can Normal-read). --- *)
    tisReport["Decoding the latent...", 0.92];
    image = TRealize @ vae["impl"]["forward"][vae["built"], zFinal, <|
        "gridH" -> gridH, "gridW" -> gridW, "device" -> dev
    |>];
    <|"latent" -> z0, "denoised" -> zFinal, "image" -> image, "embedding" -> textEmb|>
]

(* Run body under a live progress panel that re-reads the mutated text +
   progress (RuleDelayed, so each panel refresh sees the current values); the
   Block-scoped tisReport[text, frac] is what the pipeline stages call to
   advance it.  Reporting off -> just the body.  Same mechanism as FLUX's
   fxWithProgress (FluxGenerate.wl); Progress`EvaluateWithProgress is a
   near-no-op with no front end, so headless (wl / wolframscript) runs never
   error. *)

SetAttributes[tisWithProgress, HoldRest];

tisWithProgress[reportQ_, body_] := If[ TrueQ[reportQ],
    Module[{progress = 0., text = "Loading model components..."},
        Block[{
            tisReport = Function[{t, p},
                text = t;
                progress = p
            ]
        },
            Progress`EvaluateWithProgress[body, <|"Text" :> text, "Progress" :> progress, "RemainingTime" -> Automatic|>]
        ]
    ]
    ,
    Block[{tisReport = (Null &)},
        body
    ]
]

(* default: a no-op, so the pipeline stages work when evaluated outside
   tisWithProgress (e.g. a future internal caller). *)
tisReport = (Null &);

(* fresh packed-latent noise z0 ~ N(0,1), or a supplied initial latent.  The
   packed latent is {simg, channels*patch^2}; the transformer config gives the
   channel + patch counts.  Kept host-side then cast to the device. *)

tisInitialLatent[initLat_, simg_, cfg_, seed_, dev_] := Module[{ch, patch, packDim, z},
    ch = Lookup[cfg, "channels", 16];
    patch = Lookup[cfg, "patch", 2];
    packDim = ch patch^2;
    z = If[ MatchQ[initLat, _List | _NumericArray],
        initLat,
        (
            If[seed === Automatic, SeedRandom[], SeedRandom[seed]];
            RandomVariate[NormalDistribution[], {simg, packDim}]
        )
    ];
    (* Upload the latent in the activation dtype (bf16 on GPU): a f32 latent makes
       the patch-embed and every downstream matmul a slow MIXED f32(act) x bf16(W)
       and doubles the captured-intermediate footprint under JIT. *)
    TToDevice[TUOpCast[TTensorCreate[NumericArray[z, "Real32"]], If[dev === "cpu", "f32", $tisActDtype]], dev]
]

(* Euler flow-match loop: z <- z + (sigmaNext - sigma) * velocity, over the
   sigma schedule (descending to 0).  The transformer velocity is either a plain
   closure (z, sigma) -> v (non-JIT), or a <|velNet, condFn|> pair: velNet
   (z, cond$$) -> v is TJit-captured ONCE and REPLAYED each step rebinding z + the
   per-step conditioning condFn[sigma].  The JIT form bounds the tensor-descriptor
   count to a SINGLE forward and amortizes the dispatch (the non-JIT loop
   re-dispatches the whole net every step -- slow, and it exhausts the descriptor
   pool on a 12B DiT).  dt is applied outside the captured graph; each step is
   realized so the running latent stays a concrete TTerm. *)

tisEulerSample[stepFn_Function, z0_, sigmas_] := Module[{z = z0, n = Length[sigmas] - 1, k, dt, v},
    Do[ tisReport["Denoising (step " <> IntegerString[k] <> "/" <> IntegerString[n] <> ")...", 0.35 + 0.55 (k - 1)/n];
        dt = sigmas[[k + 1]] - sigmas[[k]];
        v = stepFn[z, sigmas[[k]]];
        z = TRealize @ TUOpAdd[z, TUOpMul[v, TUOpConst[N[dt]]]],
        {k, 1, n}
    ];
    z
]

tisEulerSample[tfOut_Association, z0_, sigmas_] := Module[{z = z0, vn, cf = tfOut["condFn"], n = Length[sigmas] - 1, k, dt, v},
    (* Default: call the velocity net directly each step (per-block-realized).
       Under the two-phase JIT (THVM_JIT_TWO_PHASE=1) TJit captures the lazy
       whole-velNet schedule once (recording does not pin; the finalize planner
       packs the record) and replays it per step, rebinding (z, t, tvec). *)
    vn = If[Environment["THVM_JIT_TWO_PHASE"] === "1", TJit[tfOut["velNet"]], tfOut["velNet"]];
    Do[ tisReport["Denoising (step " <> IntegerString[k] <> "/" <> IntegerString[n] <> ")...", 0.35 + 0.55 (k - 1)/n];
        dt = sigmas[[k + 1]] - sigmas[[k]];
        v = vn[z, Sequence @@ cf[sigmas[[k]]]];
        z = TRealize @ TUOpAdd[z, TUOpMul[v, TUOpConst[N[dt]]]],
        {k, 1, n}
    ];
    z
]

End[];

EndPackage[];
