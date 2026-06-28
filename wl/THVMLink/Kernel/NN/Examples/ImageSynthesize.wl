(* ImageSynthesize.wl - TImageSynthesize, the unified text-to-image entry point
   for the thvm example models.  It dispatches on the Method option to a per-model
   generator: "Flux-2" runs the FLUX.2-klein-4B pipeline (FluxGenerate); "Krea-2"
   runs the Krea 2 12B DiT pipeline (KreaGenerate).  Shaped after the built-in
   ImageSynthesize but structured like FluxGenerate for now.  Part of the
   WolframInstitute`THVMLink`Examples` package; follows wl/GUIDE.md style. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

TImageSynthesize::usage = "TImageSynthesize[prompt$] synthesizes an Image from a text prompt with a thvm diffusion model chosen by the Method option.\nTImageSynthesize[prompt$, spec$] returns the named part(s) (\"Image\", \"Latent\", \"Embedding\", All, or a list of those keys); TImageSynthesize[prompt$, n$] makes n$ images; TImageSynthesize[{prompt$1, $$}] runs a batch.  These follow the calling conventions of the selected model's generator.\nMethod chooses the model: \"Flux-2\" (default; FLUX.2-klein-4B, 4-step flow-match) or \"Krea-2\" (Krea 2 12B DiT, 8-step turbo).  Every other option is forwarded to that model's generator (see FluxGenerate, KreaGenerate).";

TImageSynthesize::badmethod = "`1` is not a known Method; use one of `2`.  Falling back to \"Flux-2\".";

Begin["`Private`"];

(* Method -> the model generator symbol, fully context-qualified so the registry
   binds the PUBLIC generator symbols regardless of the order Examples.wl loads the
   model files: an unqualified reference would resolve in this Private context for a
   model whose file has not parsed yet.  Each generator owns its own
   FluxGenerate-style options; TImageSynthesize strips Method and forwards only the
   options the chosen generator declares (FilterRules against its Options).  Adding
   a model is one entry here plus its generator file (auto-loaded by Examples.wl). *)

$imageSynthGenerators = <|
    "Flux-2" -> WolframInstitute`THVMLink`Examples`FluxGenerate,
    "Krea-2" -> WolframInstitute`THVMLink`Examples`KreaGenerate
|>;

Options[TImageSynthesize] = {Method -> "Flux-2"};

TImageSynthesize[args___, opts : OptionsPattern[]] := Block[{method = OptionValue[Method], gen},
    gen = Lookup[$imageSynthGenerators, method, Missing[]];
    If[ MissingQ[gen],
        Message[TImageSynthesize::badmethod, method, Keys[$imageSynthGenerators]];
        gen = $imageSynthGenerators["Flux-2"]
    ];
    gen[args, FilterRules[{opts}, Options[gen]]]
]

End[];

EndPackage[];
