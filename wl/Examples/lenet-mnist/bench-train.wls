#!/usr/bin/env wolframscript
(* bench-train.wls -- LeNet single-sample training benchmark.

   Runs the same lazy in-backend LeNet/Adam step as train.wls, but
   separates warmup, optional bounded autotune, and timed steps.

   Run from the repo root:

       wolframscript -f wl/Examples/lenet-mnist/bench-train.wls
       THVM_TILE=1 TRAIN_BENCH_MODE=autotune MAX_TUNE_KERNELS=32 wolframscript -f wl/Examples/lenet-mnist/bench-train.wls

   Environment knobs:
       TRAIN_BENCH_MODE  baseline | autotune | both  (default both)
       N_STEPS           timed training steps        (default 2)
       WARMUP_STEPS      untimed training steps      (default 1)
       MAX_TUNE_KERNELS  default 32; use All for full post-warmup sweep
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
Get["wl/Examples/_lib/Mnist.wl"];

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

envValue[name_String, default_String] := Module[{v = Environment[name]},
    If[StringQ[v] && StringLength[StringTrim[v]] > 0, v, default]]

parseInt[name_String, default_Integer] := Module[{
    raw = StringTrim @ envValue[name, ToString[default]]
},
    If[StringMatchQ[raw, DigitCharacter ..], ToExpression[raw], default]]

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

formatLimit[Infinity] := "All"
formatLimit[n_] := ToString[n]

formatOpt[TOpt[op_String, axis_Integer, arg_Integer]] :=
    op <> "[axis=" <> ToString[axis] <> ", arg=" <> ToString[arg] <> "]"

formatOpts[{}] := "{}"
formatOpts[opts_List] := "{" <> StringRiffle[formatOpt /@ opts, ", "] <> "}"

formatCounts[a_Association] := StringRiffle[
    KeyValueMap[ToString[#1] <> "=" <> ToString[#2] &, a],
    ", "]

kernelIds[] := If[TKernelCount[] > 1, Range[1, TKernelCount[] - 1], {}]

dispatchKindCounts[] := Counts @ Table[
    TKernelDispatchKind[kid],
    {kid, kernelIds[]}]

candidateScan[] := Module[{kids, props, candidates, groups, reps, groupSizes},
    kids = kernelIds[];
    props = Association @ Table[kid -> TKernelProposed[kid], {kid, kids}];
    candidates = Keys @ Select[props, Length[#] > 0 &];
    groups = GatherBy[candidates, TKernelProgramKey];
    reps = Flatten[
        If[TKernelProgramKey[First[#]] === 0, #, {First[#]}] & /@ groups
    ];
    groupSizes = Association @ Flatten @ Map[
        If[TKernelProgramKey[First[#]] === 0,
            (# -> 1) & /@ #,
            {First[#] -> Length[#]}] &,
        groups
    ];
    <|
        "Props" -> props,
        "Kids" -> reps,
        "AllKids" -> candidates,
        "Groups" -> groups,
        "GroupSizes" -> groupSizes
    |>
]

autotuneSelected[maxTune_] := Module[{
    scan, tuneKids, t0, rows, winners, wallMs
},
    scan = candidateScan[];
    tuneKids = takeLimit[scan["Kids"], maxTune];
    t0 = AbsoluteTime[];
    rows = Table[
        Module[{opts = TKernelAutotune[kid], applied},
            applied = First[opts]["Applied"];
            <|"Kid" -> kid, "Applied" -> applied|>],
        {kid, tuneKids}];
    wallMs = (AbsoluteTime[] - t0) * 1000.0;
    winners = Select[rows, #["Applied"] =!= {} &];
    <|
        "candidate_count" -> Length[scan["Kids"]],
        "live_candidate_count" -> Length[scan["AllKids"]],
        "tuned_count" -> Length[tuneKids],
        "winner_count" -> Length[winners],
        "winner_rows" -> winners,
        "wall_ms" -> Round[wallMs, 0.1]
    |>
]

runBench[name_String, doTune_] := Module[{
    w1, b1, w2, b2, w3, b3, w4, b4, params, mState, vState,
    batch, imgArr, label, imgT, tgtT, forwardLoss, trainStep,
    warmupLosses, scan, tune, t0, losses, wallMs, finalLoss, result
},
    SeedRandom[42];
    TInit[];

    w1 = TGlorot[{6,   1, 5, 5}];  b1 = TZeros[{6}];
    w2 = TGlorot[{16,  6, 5, 5}];  b2 = TZeros[{16}];
    w3 = TGlorot[{120, 256}];      b3 = TZeros[{120}];
    w4 = TGlorot[{10,  120}];      b4 = TZeros[{10}];

    params = {w1, b1, w2, b2, w3, b3, w4, b4};
    mState = TZerosLike /@ params;
    vState = TZerosLike /@ params;

    batch = TMnistBatch[1];
    imgArr = Normal[TTensorData @ batch["images"]][[1]];
    label = First @ Normal[TTensorData @ batch["labels"]];
    imgT = TTensorCreate @ NumericArray[imgArr, "Real32"];
    tgtT = TOneHot[label, 10];

    forwardLoss = Function[{x, target},
        Module[{h1, r1, p1, h2, r2, p2, flat, h3, r3, h4, probs},
            h1   = TConv2D[x, w1, b1];
            r1   = TReLU[h1];
            p1   = TMaxPool2d[r1, 2];
            h2   = TConv2D[p1, w2, b2];
            r2   = TReLU[h2];
            p2   = TMaxPool2d[r2, 2];
            flat = ArrayReshape[p2, {1, 256}];
            h3   = TMatVec[w3, flat] + b3;
            r3   = TReLU[h3];
            h4   = TMatVec[w4, ArrayReshape[r3, {1, 120}]] + b4;
            probs = TSoftmax[h4];
            TCrossEntropyLoss[probs, target]
        ]];

    trainStep[step_Integer] := Module[{loss, lossVal},
        loss = forwardLoss[imgT, tgtT];
        lossVal = First @ Normal @ TTensorData @ TRealize @ loss;
        TAdam[loss, params, mState, vState, step];
        lossVal
    ];

    print[""];
    print["=== ", name, " ==="];
    print["fixed MNIST label: ", label];
    print["warmup steps: ", warmupSteps, "  timed steps: ", nSteps];

    warmupLosses = Table[trainStep[k], {k, warmupSteps}];
    If[warmupSteps > 0,
        print["warmup losses: ", Map[Round[#, 0.0001] &, warmupLosses]]];

    scan = candidateScan[];
    print["post-warmup kernels: ", TKernelCount[] - 1,
          "  live candidates: ", Length[scan["AllKids"]],
          "  tune reps: ", Length[scan["Kids"]]];
    If[Length[scan["Kids"]] > 0,
        Do[
            print["  candidate kid ", kid,
                  " shared=", scan["GroupSizes"][kid],
                  " key=", TKernelProgramKey[kid],
                  " proposals=", formatOpts[scan["Props"][kid]]],
            {kid, Take[scan["Kids"], UpTo[5]]}]];

    tune = If[doTune,
        autotuneSelected[maxTune],
        <|"candidate_count" -> Length[scan["Kids"]],
          "live_candidate_count" -> Length[scan["AllKids"]],
          "tuned_count" -> 0,
          "winner_count" -> 0, "winner_rows" -> {}, "wall_ms" -> 0|>];

    If[doTune,
        print["autotune tuned ", tune["tuned_count"], "/",
              tune["candidate_count"], " representative candidates",
              " (live candidates=", tune["live_candidate_count"], ") in ",
              tune["wall_ms"], " ms; winners=", tune["winner_count"]];
        Do[
            print["  winner kid ", row["Kid"],
                  " applied=", formatOpts[row["Applied"]]],
            {row, Take[tune["winner_rows"], UpTo[8]]}]];

    t0 = AbsoluteTime[];
    losses = Table[trainStep[warmupSteps + k], {k, nSteps}];
    wallMs = (AbsoluteTime[] - t0) * 1000.0;
    finalLoss = First @ Normal @ TTensorData @ TRealize @
        forwardLoss[imgT, tgtT];

    result = <|
        "name" -> name,
        "n_steps" -> nSteps,
        "warmup_steps" -> warmupSteps,
        "losses" -> losses,
        "final_loss" -> finalLoss,
        "wall_ms" -> wallMs,
        "ms_per_step" -> If[nSteps > 0, wallMs / nSteps, 0],
        "kernel_count" -> TKernelCount[] - 1,
        "dispatch_kinds" -> dispatchKindCounts[],
        "tune" -> tune
    |>;

    print["timed losses: ", Map[Round[#, 0.0001] &, losses]];
    print["final loss:   ", Round[finalLoss, 0.0001]];
    print["wall_ms:      ", Round[result["wall_ms"], 0.1]];
    print["ms_per_step:  ", Round[result["ms_per_step"], 0.1]];
    print["kernels:      ", result["kernel_count"]];
    print["dispatch:     ", formatCounts[result["dispatch_kinds"]]];
    result
]

nSteps = parseInt["N_STEPS", 2];
warmupSteps = parseInt["WARMUP_STEPS", 1];
maxTune = parseLimit["MAX_TUNE_KERNELS", 32];
benchMode = ToLowerCase @ envValue["TRAIN_BENCH_MODE", "both"];

print["LeNet training benchmark"];
print["  THVM_BACKEND=", envValue["THVM_BACKEND", "default-cpu"],
      "  THVM_TILE=", envValue["THVM_TILE", "off"]];
print["  TRAIN_BENCH_MODE=", benchMode,
      "  N_STEPS=", nSteps,
      "  WARMUP_STEPS=", warmupSteps,
      "  MAX_TUNE_KERNELS=", formatLimit[maxTune]];

results = Switch[
    benchMode,
    "baseline", {runBench["baseline", False]},
    "autotune", {runBench["autotune", True]},
    _, {runBench["baseline", False], runBench["autotune", True]}
];

If[Length[results] == 2,
    base = results[[1]];
    tuned = results[[2]];
    If[tuned["ms_per_step"] > 0,
        print[""];
        print["speedup baseline/autotune: ",
              Round[base["ms_per_step"] / tuned["ms_per_step"], 0.001], "x"]]];

If[AnyTrue[results, !NumericQ[#["final_loss"]] &],
    print["FAIL: benchmark produced non-numeric final loss"];
    Exit[1]];

print["LeNet training benchmark complete."];
