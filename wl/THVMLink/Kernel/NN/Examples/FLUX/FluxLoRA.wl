(* FluxLoRA.wl -- LoRA (Low-Rank Adaptation) loading for FluxGenerate.

   A LoRA stores, per target Linear weight W {out,in}, two low-rank factors
   down/A {rank,in} and up/B {out,rank} (plus an optional per-module alpha).  The
   effective merged weight is

       W' = W + (scale * alpha/rank) * (B . A)        (B.A is {out,in}, == W's shape)

   This file MERGES that delta into the weight at load time (no architecture
   change): fxLoadTfWeight (FluxGenerate.wl) asks fxLoRAMerge whether a LoRA
   targets the weight name it is loading, and if so adds the summed delta as an
   on-device TTerm op BEFORE the device upload / q8 quantization, so the rest of
   the capture/replay pipeline runs unchanged on W'.

   The crux is the name mapping.  Civitai / HuggingFace FLUX LoRAs use one of:
     - diffusers/PEFT:  transformer.<module>.lora_A.weight / .lora_B.weight (+ .alpha)
     - kohya:           lora_unet_<module>.lora_down.weight / .lora_up.weight (+ .alpha)
                        (the module path has dots replaced by underscores)
   fxLoRANormalizeName strips the lora_A/B|down/up|alpha suffix to get the module
   key, undoes the kohya dot->underscore mangling, and maps the diffusers module
   path to the thvm FLUX.2-klein weight name.  A module that does not resolve to a
   known FLUX weight is COLLECTED (not silently dropped) and reported via
   FluxGenerate::loraskip -- many real LoRAs touch only a subset, and a FLUX.1
   LoRA's single-block / FF / norm modules simply do not exist in FLUX.2-klein's
   weight set (its single blocks fuse qkv+mlp into to_qkv_mlp_proj, and it has 5
   double + 20 single blocks vs FLUX.1's 19 + 38).

   Part of the WolframInstitute`THVMLink`Examples` package; loaded alongside the
   other FLUX pieces into Examples`Private`.  Follows wl/GUIDE.md style. *)

BeginPackage["WolframInstitute`THVMLink`Examples`", {"WolframInstitute`THVMLink`"}];

Begin["`Private`"];

(* ============================================================
   Resolve the "LoRA" option to a canonical list of {path, scale}.
   ============================================================ *)

(* fxLoRAResolve[spec] normalizes the user's "LoRA" option to a List of
   {absolutePath, scale} pairs (scale a real, default 1.0).  Accepts:
     None                                  -> {}            (no LoRA)
     "path.safetensors"                    -> {{path, 1.0}}
     {"a.safetensors", "b.safetensors"}    -> {{a,1.0},{b,1.0}}
     {"a.safetensors" -> 0.8, "b" -> 0.5}  -> {{a,0.8},{b,0.5}}
     a single  "path" -> scale  rule       -> {{path, scale}}
   Paths are made absolute (ExpandFileName) so the session key is stable
   regardless of the caller's working directory. *)

fxLoRAResolve[None] := {};

fxLoRAResolve[Automatic] := {};

fxLoRAResolve[path_String] := {{ExpandFileName[path], 1.0}};

fxLoRAResolve[Rule[path_String, scale_]] := {{ExpandFileName[path], N[scale]}};

fxLoRAResolve[specs_List] := Flatten[fxLoRAResolve /@ specs, 1];

fxLoRAResolve[other_] := (Message[FluxGenerate::lorabadspec, other]; {});

(* a stable key for the session cache: the resolved {path,scale} list (paths
   already absolute).  Hash it so two different LoRA sets -> different sessions
   and a no-LoRA call (empty list) collapses to a fixed sentinel that never
   collides with any real LoRA. *)

fxLoRAKey[resolved_List] := If[resolved === {}, None, Hash[resolved, "SHA256"]];

(* ============================================================
   Name mapping: LoRA tensor name -> thvm FLUX.2-klein weight name.
   ============================================================ *)

(* A LoRA tensor is one of  <module>.lora_A.weight | .lora_B.weight | .alpha
   (diffusers/PEFT) or  <module>.lora_down.weight | .lora_up.weight | .alpha
   (kohya).  fxLoRASplit returns {moduleKey, role} where role is "down"|"up"|
   "alpha", or $Failed if the name is not a recognised LoRA tensor. *)

fxLoRASplit[name_String] := Which[
    StringEndsQ[name, ".lora_A.weight"],    {StringDrop[name, -StringLength[".lora_A.weight"]],    "down"},
    StringEndsQ[name, ".lora_B.weight"],    {StringDrop[name, -StringLength[".lora_B.weight"]],    "up"},
    StringEndsQ[name, ".lora_down.weight"], {StringDrop[name, -StringLength[".lora_down.weight"]], "down"},
    StringEndsQ[name, ".lora_up.weight"],   {StringDrop[name, -StringLength[".lora_up.weight"]],   "up"},
    StringEndsQ[name, ".lora_A"],           {StringDrop[name, -StringLength[".lora_A"]],           "down"},
    StringEndsQ[name, ".lora_B"],           {StringDrop[name, -StringLength[".lora_B"]],           "up"},
    (* XLabs-AI FLUX LoRAs use a bare ".down.weight"/".up.weight" suffix off a
       "...processor.<proj>_lora<n>" module (a third convention, FLUX.1-only) --
       recognise them so such a file is REPORTED via loraskip, not silently no-op'd. *)
    StringEndsQ[name, ".down.weight"],      {StringDrop[name, -StringLength[".down.weight"]],      "down"},
    StringEndsQ[name, ".up.weight"],        {StringDrop[name, -StringLength[".up.weight"]],        "up"},
    StringEndsQ[name, ".alpha"],            {StringDrop[name, -StringLength[".alpha"]],            "alpha"},
    True,                                   $Failed
];

(* kohya stores the module path with dots replaced by underscores and a
   "lora_unet_" / "lora_te_" prefix.  Undo that to the dotted diffusers module
   path so the single mapping table below handles both conventions.  The
   ambiguity (an underscore that was a real underscore vs a dot) is resolved by
   only converting the known structural separators FLUX module paths use --
   matching the diffusers module names we map against. *)

fxLoRADekohya[mod_String] := Module[{m = mod},
    m = StringReplace[m, StartOfString ~~ ("lora_unet_" | "lora_transformer_" | "lora_te_" | "lora_te1_" | "lora_te2_") -> ""];
    (* kohya replaces every "." with "_"; rebuild the dotted path.  The diffusers
       FLUX module paths contain only these segment shapes, so reinsert dots at
       the structural boundaries (block index, the .attn., the projection names,
       the numeric sub-indices like to_out.0 / ff.net.0). *)
    m = StringReplace[m, {
        "single_transformer_blocks_" -> "single_transformer_blocks.",
        "transformer_blocks_" -> "transformer_blocks.",
        "_attn_" -> ".attn.",
        "_norm1_context_" -> ".norm1_context.",
        "_norm1_" -> ".norm1.",
        "_norm_" -> ".norm.",
        "_ff_context_net_" -> ".ff_context.net.",
        "_ff_net_" -> ".ff.net.",
        "_proj_mlp_" -> ".proj_mlp.",
        "_proj_out_" -> ".proj_out.",
        "_linear_" -> ".linear."
    }];
    (* a trailing "_<n>" sub-index (to_out_0, net_0, net_2) -> ".<n>" *)
    m = StringReplace[m, ("_" ~~ d : DigitCharacter ~~ EndOfString) :> "." <> d];
    m
];

(* Strip a leading "transformer." (PEFT prefixes the diffusers model name) and
   any "diffusion_model." / "model." wrappers so the mapping table sees the bare
   diffusers module path. *)

fxLoRAStripPrefix[mod_String] := StringReplace[
    mod, StartOfString ~~ ("transformer." | "diffusion_model." | "model.diffusion_model." | "model.") -> ""
];

(* The mapping: a bare FLUX module path -> a LIST of targets, each a
   {thvmWeightName, sliceSpec} pair.  sliceSpec is All (the up-factor B feeds the
   target whole, a 1:1 / direct map) or one of "third1"|"third2"|"third3" (the
   target gets one equal third of B's output rows -- a FUSED qkv LoRA split across
   klein's separate q/k/v weights).  An empty list {} means the module has no
   FLUX.2-klein counterpart (collected + reported, never silently dropped).

   TWO source conventions both produce the SAME klein weight names:

   (1) DIFFUSERS (PEFT FLUX.1 LoRAs, "transformer."-prefixed): klein's
       double-block attn names already MATCH diffusers (separate to_q/to_k/to_v,
       add_*_proj, to_out.0, to_add_out), so those map 1:1.  The diffusers FF
       (ff.net.0.proj / ff.net.2) and per-block norm (norm1.linear) and ALL
       single-block per-projection modules have no klein target (klein's FF is
       ff.linear_in/out, its norm is a shared modulation linear, and its single
       blocks fuse qkv+mlp into to_qkv_mlp_proj) -> {}.

   (2) BFL / ComfyUI (klein-NATIVE LoRAs, "diffusion_model."-prefixed, the
       ai-toolkit muppet LoRA): a different module vocabulary that maps ONTO the
       same klein diffusers weights, including the fused-qkv -> separate q/k/v
       SPLIT (img_attn.qkv -> {to_q,to_k,to_v} thirds; txt_attn.qkv ->
       {add_q,add_k,add_v} thirds) and the FUSED single-block linear1/linear2 ->
       to_qkv_mlp_proj/to_out direct maps. *)

fxLoRAModuleMap[mod_String] := Module[{m = fxLoRAStripPrefix[mod], dm, db, sb, rest},
    (* --- BFL / ComfyUI double blocks: double_blocks.N.<sub> --- *)
    dm = StringCases[m, StartOfString ~~ "double_blocks." ~~ i : DigitCharacter .. ~~ "." ~~ r__ ~~ EndOfString :> {i, r}];
    If[ dm =!= {},
        With[{i = dm[[1, 1]], r = dm[[1, 2]], p = "transformer_blocks." <> dm[[1, 1]] <> "."},
            Return @ Switch[r,
                "img_attn.qkv", {{p <> "attn.to_q.weight", "third1"}, {p <> "attn.to_k.weight", "third2"}, {p <> "attn.to_v.weight", "third3"}},
                "txt_attn.qkv", {{p <> "attn.add_q_proj.weight", "third1"}, {p <> "attn.add_k_proj.weight", "third2"}, {p <> "attn.add_v_proj.weight", "third3"}},
                "img_attn.proj", {{p <> "attn.to_out.0.weight", All}},
                "txt_attn.proj", {{p <> "attn.to_add_out.weight", All}},
                "img_mlp.0",     {{p <> "ff.linear_in.weight", All}},
                "img_mlp.2",     {{p <> "ff.linear_out.weight", All}},
                "txt_mlp.0",     {{p <> "ff_context.linear_in.weight", All}},
                "txt_mlp.2",     {{p <> "ff_context.linear_out.weight", All}},
                _,               {}
            ]
        ]
    ];
    (* --- BFL / ComfyUI single blocks: single_blocks.N.linear1|linear2 (DIRECT,
           fused<->fused, same shape) --- *)
    sb = StringCases[m, StartOfString ~~ "single_blocks." ~~ i : DigitCharacter .. ~~ "." ~~ r__ ~~ EndOfString :> {i, r}];
    If[ sb =!= {},
        With[{p = "single_transformer_blocks." <> sb[[1, 1]] <> "."},
            Return @ Switch[sb[[1, 2]],
                "linear1", {{p <> "attn.to_qkv_mlp_proj.weight", All}},
                "linear2", {{p <> "attn.to_out.weight", All}},
                _,         {}
            ]
        ]
    ];
    (* --- DIFFUSERS double-block attention projections (these map 1:1) --- *)
    db = StringCases[m, StartOfString ~~ "transformer_blocks." ~~ i : DigitCharacter .. ~~ "." ~~ r__ ~~ EndOfString :> {i, r}];
    If[ db =!= {},
        With[{p = "transformer_blocks." <> db[[1, 1]] <> "."},
            Return @ Switch[db[[1, 2]],
                "attn.to_q",       {{p <> "attn.to_q.weight", All}},
                "attn.to_k",       {{p <> "attn.to_k.weight", All}},
                "attn.to_v",       {{p <> "attn.to_v.weight", All}},
                "attn.add_q_proj", {{p <> "attn.add_q_proj.weight", All}},
                "attn.add_k_proj", {{p <> "attn.add_k_proj.weight", All}},
                "attn.add_v_proj", {{p <> "attn.add_v_proj.weight", All}},
                "attn.to_out.0",   {{p <> "attn.to_out.0.weight", All}},
                "attn.to_add_out", {{p <> "attn.to_add_out.weight", All}},
                _,                 {}
            ]
        ]
    ];
    {}
];

(* fxLoRANormalizeName: a LoRA tensor name -> {targets, role, mod} where targets
   is the (possibly empty) list of {thvmWeightName, sliceSpec} from fxLoRAModuleMap.
   Splits off the role, de-kohya's the module path, then maps it.  The caller pairs
   down/up/alpha by thvmWeightName and slices per target. *)

fxLoRANormalizeName[name_String] := Module[{split, mod, role, targets},
    split = fxLoRASplit[name];
    If[split === $Failed, Return[$Failed]];
    {mod, role} = split;
    mod = fxLoRADekohya[mod];
    targets = fxLoRAModuleMap[mod];
    {targets, role, mod}
];

(* ============================================================
   Load one LoRA safetensors -> per-FLUX-weight delta factors.
   ============================================================ *)

(* fxLoRALoadOne[path, scale] reads the safetensors, normalizes every tensor's
   name, and returns

       <| "deltas"   -> <| thvmWeightName -> {scale, alpha, rank, down, up, sliceSpec} ... |>,
          "unmatched" -> { dottedModulePath ... } |>

   pairing the down (A) and up (B) factors per module, then EXPANDING each module
   to its target list (a fused-qkv module fans out to 3 thvm weights, each with
   the same down/up but a different sliceSpec).  alpha defaults to rank (so
   alpha/rank = 1) when the LoRA stores no .alpha.  A recognised LoRA tensor whose
   module maps to NO klein target is collected in "unmatched" (deduplicated).
   down/up are kept as lazy mmap TTerms; the rank is read from down's leading
   dim. *)

fxLoRALoadOne[path_String, scale_] := Module[
    {tensors, byMod, modTargets, deltas, unmatched, names}
    ,
    If[ ! FileExistsQ[path],
        Message[FluxGenerate::loramissing, path];
        Return[<|"deltas" -> <||>, "unmatched" -> {}|>]
    ];
    tensors = TSafeTensorLoad[path];
    names = Keys[tensors];
    (* group tensor TTerms by their dotted module key; record the module's target
       list once (the same for its down/up/alpha tensors). *)
    byMod = <||>;
    modTargets = <||>;
    unmatched = {};
    Do[
        Module[{norm = fxLoRANormalizeName[nm]},
            If[ norm =!= $Failed,
                With[{targets = norm[[1]], role = norm[[2]], mod = norm[[3]]},
                    If[ targets === {},
                        (* a recognised LoRA tensor whose module has no klein target *)
                        AppendTo[unmatched, mod]
                        ,
                        byMod[mod] = Append[Lookup[byMod, mod, <||>], role -> tensors[nm]];
                        modTargets[mod] = targets
                    ]
                ]
            ]
        ]
        ,
        {nm, names}
    ];
    (* build the delta factor tuple per (thvm weight, slice) for every module that
       has BOTH down and up; one module fans out to all its targets. *)
    deltas = <||>;
    Do[
        With[{mod = k, parts = byMod[k], targets = modTargets[k]},
            If[ KeyExistsQ[parts, "down"] && KeyExistsQ[parts, "up"],
                Module[{down = parts["down"], up = parts["up"], rank, alpha, tup},
                    rank = First[Dimensions[down]];                     (* down is {rank,in} *)
                    alpha = If[ KeyExistsQ[parts, "alpha"],
                        (* .alpha is a 0-D / 1-element scalar tensor *)
                        First @ Flatten @ Normal @ TRealize @ parts["alpha"]
                        ,
                        rank                                            (* alpha/rank = 1 *)
                    ];
                    Do[
                        With[{thvmName = tgt[[1]], sliceSpec = tgt[[2]]},
                            tup = {N[scale], N[alpha], rank, down, up, sliceSpec};
                            deltas[thvmName] = Append[Lookup[deltas, thvmName, {}], tup]
                        ]
                        ,
                        {tgt, targets}
                    ]
                ]
            ]
        ]
        ,
        {k, Keys[byMod]}
    ];
    <|"deltas" -> deltas, "unmatched" -> DeleteDuplicates[unmatched]|>
];

(* fxLoRALoad[resolved] loads every {path,scale} in the resolved list and MERGES
   their per-thvm-weight delta factor lists into one
   <| thvmWeightName -> { {scale,alpha,rank,down,up,sliceSpec} ... } |>
   (several LoRAs -- or several modules of one LoRA, e.g. a fused qkv slicing into
   the same weight -- can target the same weight; their deltas SUM).  Each file's
   per-weight value is already a list of tuples, so Join (not Append) so the
   merged value stays a flat list.  Emits ONE FluxGenerate::loraskip listing the
   union of unmatched modules across all files (if any).  Returns <||> for an
   empty list (no LoRA). *)

fxLoRALoad[resolved_List] := Module[{loaded, merged, allUnmatched, nMatched},
    If[resolved === {}, Return[<||>]];
    loaded = fxLoRALoadOne[#[[1]], #[[2]]]& /@ resolved;
    merged = <||>;
    Do[
        Do[
            merged[k] = Join[Lookup[merged, k, {}], one["deltas"][k]]
            ,
            {k, Keys[one["deltas"]]}
        ]
        ,
        {one, loaded}
    ];
    allUnmatched = DeleteDuplicates @ Flatten[#["unmatched"]& /@ loaded];
    nMatched = Length[merged];
    If[ allUnmatched =!= {},
        Message[FluxGenerate::loraskip, Length[allUnmatched], nMatched, fxLoRASkipSummary[allUnmatched]]
    ];
    If[ nMatched === 0 && resolved =!= {},
        Message[FluxGenerate::loranone]
    ];
    merged
];

(* a compact summary of unmatched modules for the message: dedup-collapse the
   per-block numeric index so "transformer_blocks.{0..18}.attn.to_q" reads as one
   line, and cap the list length. *)

fxLoRASkipSummary[unmatched_List] := Module[{collapsed},
    collapsed = DeleteDuplicates[
        StringReplace[#, ("blocks." ~~ DigitCharacter ..) -> "blocks.N"]& /@ unmatched
    ];
    StringRiffle[Take[collapsed, UpTo[12]], ", "] <>
        If[Length[collapsed] > 12, ", ...", ""]
];

(* ============================================================
   Merge: weight name + base TTerm -> base + summed LoRA delta.
   ============================================================ *)

(* The output-row block a sliceSpec selects from an up-factor B with `outFull`
   rows: All -> the whole {1, outFull}; "third1|2|3" -> the corresponding equal
   third (a FUSED qkv B split across klein's separate q/k/v weights).  Returns
   {rowStart, rowCount} (1-indexed start), or $Failed if a third does not divide
   outFull evenly (the fused-vs-separate assumption broke). *)

fxLoRASliceRows[All, outFull_] := {1, outFull};

fxLoRASliceRows[spec_String, outFull_] := If[ Mod[outFull, 3] =!= 0,
    $Failed
    ,
    With[{third = outFull/3, k = Switch[spec, "third1", 0, "third2", 1, "third3", 2, _, $Failed]},
        If[k === $Failed, $Failed, {k*third + 1, third}]
    ]
];

(* the {out,in} dimensions a factor's delta WILL have once sliced: {rowCount, in}
   where rowCount is the sliceSpec's row block of up (= up's full out for All) and
   in is down's last dim.  Used by the shape guard before building the delta. *)

fxLoRAFactorDims[f_List] := Module[{up = f[[5]], down = f[[4]], slice},
    slice = fxLoRASliceRows[f[[6]], First[Dimensions[up]]];
    If[slice === $Failed, $Failed, {slice[[2]], Last[Dimensions[down]]}]
];

(* fxLoRADelta[factors, dev] computes the summed low-rank delta
   Sum_l scale_l*(alpha_l/rank_l) * (B_slice_l . A_l)  as a {out,in} TTerm.  up
   (B) is {outFull,rank}, sliced to {out,rank} per the factor's sliceSpec; down
   (A) is {rank,in}; so B_slice.A is {out,in}.

   The factors are uploaded to the device as bf16 FIRST, then matmul'd: a matmul
   directly on the lazy f16/bf16 disk-mmap factors falls to a scalar host path
   (~8 s per weight on Metal vs ~0.2 s on the bf16 tensor cores -- a 30x cliff),
   so realize each factor onto the device in the compute dtype before the matmul.
   On a GPU that is bf16 (the tensor-core path); on CPU it is f32.  A fused-qkv
   sliceSpec slices B's output ROWS (Part on axis 1) to the target's third before
   the matmul, so the delta has the separate q/k/v weight's out-dim. *)

fxLoRADelta[factors_List, dev_] := Module[{dt = If[dev === "cpu", "f32", "bf16"], terms},
    terms = Function[f,
        Module[{scale = f[[1]], alpha = f[[2]], rank = f[[3]], down = f[[4]], up = f[[5]], slice, upS, upD, downD, coeff},
            coeff = N[scale * alpha / rank];
            slice = fxLoRASliceRows[f[[6]], First[Dimensions[up]]];
            upS   = If[f[[6]] === All, up, up[[slice[[1]] ;; slice[[1]] + slice[[2]] - 1]]];
            upD   = TRealize @ TToDevice[TUOpCast[upS, dt], dev];     (* {out,rank} *)
            downD = TRealize @ TToDevice[TUOpCast[down, dt], dev];    (* {rank,in} *)
            TUOpMul[TMatMul[upD, downD], TUOpConst[coeff]]            (* {out,in} *)
        ]
    ] /@ factors;
    Fold[TUOpAdd, First[terms], Rest[terms]]
];

(* fxLoRAMerge[loraDeltas, name, t, dev] -- the merge hook fxLoadTfWeight calls.
   If the LoRA set targets weight `name`, return the realized device tensor
   t + delta (in t's dtype) so the downstream device-upload / q8 path sees a
   concrete merged weight W'; otherwise return t unchanged.  The base weight t is
   the lazy mmap bf16 disk tensor; it is uploaded to the device (matching the
   delta), added in f32 for accuracy, and cast back to t's dtype so the merged
   weight is byte-compatible with the no-LoRA loader's bf16 upload. *)

fxLoRAMerge[loraDeltas_, name_String, t_, dev_] := If[
    loraDeltas === <||> || ! KeyExistsQ[loraDeltas, name]
    ,
    t
    ,
    Module[{factors, dims = Dimensions[t], dt = TTensorDType[TRealize[t]], delta, baseD},
        (* Guard a name that maps but whose (possibly sliced) low-rank delta shape
           {out,in} does not match the base weight (a FLUX.1-vs-FLUX.2 dim mismatch
           under a shared module name, or a fused-qkv third that does not divide):
           drop that factor with a message rather than building a malformed add. *)
        factors = Select[
            loraDeltas[name],
            Function[f, fxLoRAFactorDims[f] === dims]
        ];
        If[ factors === {},
            Message[FluxGenerate::lorashape, name];
            Return[t, Module]
        ];
        delta = fxLoRADelta[factors, dev];                                   (* device {out,in} *)
        baseD = TRealize @ TToDevice[TUOpCast[t, If[dev === "cpu", "f32", "bf16"]], dev];
        (* add in f32 (delta is small relative to the weight), cast back to the
           weight's stored dtype so the loader's TToDevice path is unchanged. *)
        TRealize @ TUOpCast[TUOpAdd[TUOpCast[baseD, "f32"], TUOpCast[delta, "f32"]], dt]
    ]
];

End[];

EndPackage[];
