#!/usr/bin/env wolframscript
(* autotune.wls -- bounded LeNet kernel-autotune walkthrough.

   Builds one random-initialized LeNet forward pass, finds live
   kernels that have C-side TKernelProposed candidates, benchmarks a
   bounded subset with TKernelAutotune, then reruns the same forward
   pass to verify the cached winners still produce the same softmax.

   Run from the repo root:

       wolframscript -f wl/Examples/lenet-mnist/autotune.wls
       THVM_TILE=1 MAX_TUNE_KERNELS=All wolframscript -f wl/Examples/lenet-mnist/autotune.wls
       DEV=metal THVM_TILE=1 THVM_KGC=0 wolframscript -f wl/Examples/lenet-mnist/autotune.wls
       DEV=metal THVM_TILE=1 AUTOTUNE=1 N_STEPS=1 wolframscript -f wl/Examples/lenet-mnist/train.wls

   Environment knobs:
       MAX_TUNE_KERNELS   default 16; use All for the full LeNet sweep
       MAX_VARIANT_ROWS   default 3; how many candidate kernels to print
       THVM_KGC=0         keep post-realize program arrays for Metal inspection
       AUTOTUNE=1    tune at kernel fire time instead of post-hoc
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
Get["wl/Examples/_lib/Mnist.wl"];

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

envValue[name_String, default_String] := Module[{v = Environment[name]},
    If[StringQ[v] && StringLength[StringTrim[v]] > 0, v, default]]

parseLimit[name_String, default_Integer] := Module[{
    raw = StringTrim @ envValue[name, ToString[default]]
},
    Which[
        raw === "All" || raw === "all", Infinity,
        StringMatchQ[raw, DigitCharacter ..], ToExpression[raw],
        True, default
    ]]

takeLimit[list_List, Infinity] := list
takeLimit[list_List, n_Integer] := Take[list, UpTo[n]]

formatOpt[None] := "baseline"
formatOpt[TOpt[op_String, axis_Integer, arg_Integer]] :=
    op <> "[axis=" <> ToString[axis] <> ", arg=" <> ToString[arg] <> "]"

formatOpts[{}] := "{}"
formatOpts[opts_List] := "{" <> StringRiffle[formatOpt /@ opts, ", "] <> "}"

formatCounts[a_Association] := StringRiffle[
    KeyValueMap[ToString[#1] <> "=" <> ToString[#2] &, a],
    ", "]

kindCounts[] := Counts @ Table[
    TKernelDispatchKind[kid],
    {kid, 1, TKernelCount[] - 1}]

probVector[t_TTerm] := Flatten @ N @ Normal @ TTensorData @ t

printVariants[kid_Integer] := Module[{vars, rows},
    vars = TKernelVariants[kid];
    rows = First /@ vars;
    print["  kid ", kid, " variants:"];
    Do[
        print["    ", formatOpt[row["Opt"]], "  ",
              Round[N @ row["WallUs"], 0.001], " us"],
        {row, rows}]
]

SeedRandom[42];
maxTune        = parseLimit["MAX_TUNE_KERNELS", 16];
maxVariantRows = parseLimit["MAX_VARIANT_ROWS", 3];

TInit[];
print["LeNet autotune example"];
print["  DEV=", envValue["DEV", "default-cpu"],
      "  THVM_TILE=", envValue["THVM_TILE", "off"],
      "  AUTOTUNE=", envValue["AUTOTUNE", "off"],
      "  THVM_KGC=", envValue["THVM_KGC", "on"]];
print["  MAX_TUNE_KERNELS=", If[maxTune === Infinity, "All", maxTune],
      "  MAX_VARIANT_ROWS=", If[maxVariantRows === Infinity, "All", maxVariantRows]];

net   = NetInitialize @ Quiet[NetModel["LeNet"], Import::nnincmpb];
batch = TMnistBatch[1];
img   = Normal[TTensorData @ batch["images"]][[1]];
label = First @ Normal[TTensorData @ batch["labels"]];
x     = TTensorCreate @ NumericArray[img, "Real32"];

forward[] := TRealize @ TFromNet[net, x]

print["materializing baseline LeNet forward for label ", label, "..."];
probsBefore = probVector @ forward[];
softmaxBefore = Total[probsBefore];
print["  baseline softmax sum: ", Round[softmaxBefore, 0.000001]];
print["  live kernels: ", TKernelCount[] - 1];
print["  dispatch kinds: ", formatCounts @ kindCounts[]];

kids = Range[1, TKernelCount[] - 1];
proposedByKid = Association @ Table[kid -> TKernelProposed[kid], {kid, kids}];
candidateKids = Keys @ Select[proposedByKid, Length[#] > 0 &];
candidateGroups = GatherBy[candidateKids, TKernelProgramKey];
representativeKids = Flatten[
    If[TKernelProgramKey[First[#]] === 0, #, {First[#]}] & /@ candidateGroups
];
groupSizes = Association @ Flatten @ Map[
    If[TKernelProgramKey[First[#]] === 0,
        (# -> 1) & /@ #,
        {First[#] -> Length[#]}] &,
    candidateGroups
];
tuneKids = takeLimit[representativeKids, maxTune];
variantKids = takeLimit[tuneKids, maxVariantRows];

print["  kernels with proposals: ", Length[candidateKids],
      "  tune reps: ", Length[representativeKids]];
If[Length[representativeKids] > 0,
    Do[
        print["    kid ", kid,
              " shared=", groupSizes[kid],
              " key=", TKernelProgramKey[kid],
              " kind=", TKernelDispatchKind[kid],
              " numel=", TKernelInfo[kid]["output_numel"],
              " proposals=", formatOpts[proposedByKid[kid]]],
        {kid, Take[representativeKids, UpTo[8]]}]]

If[Length[variantKids] > 0,
    print["benchmarking variants for ", Length[variantKids], " sample kernels..."];
    Do[printVariants[kid], {kid, variantKids}]];

If[Length[tuneKids] === 0,
    print["no kernels had proposer candidates; nothing to tune."],
    print["autotuning ", Length[tuneKids], " representative candidate kernels..."];
    tunedRows = Table[
        Module[{opts = TKernelAutotune[kid], applied},
            applied = First[opts]["Applied"];
            <|"Kid" -> kid, "Applied" -> applied|>],
        {kid, tuneKids}];
    winners = Select[tunedRows, #["Applied"] =!= {} &];
    print["  kernels with applied winners: ", Length[winners]];
    Do[
        print["    kid ", row["Kid"], " applied=", formatOpts[row["Applied"]]],
        {row, Take[winners, UpTo[12]]}]
];

print["rerunning LeNet forward after tuning cache updates..."];
probsAfter = probVector @ forward[];
softmaxAfter = Total[probsAfter];
maxAbsDiff = Max[Abs[probsAfter - probsBefore]];

print["  tuned softmax sum:   ", Round[softmaxAfter, 0.000001]];
print["  max probability diff: ", Round[maxAbsDiff, 0.000001]];
print["  live kernels now: ", TKernelCount[] - 1];
print["  dispatch kinds now: ", formatCounts @ kindCounts[]];

If[Abs[softmaxAfter - 1.0] > 0.001 || maxAbsDiff > 0.001,
    print["FAIL: autotuned LeNet forward changed probabilities too much"];
    Exit[1]];

print["LeNet autotune PASSED."];
