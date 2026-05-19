#!/usr/bin/env wolframscript
(* bench-train.wls -- beautiful_mnist steady training-loop benchmark.

   Measures the repeated TJit replay path for the tinygrad
   beautiful_mnist architecture:

       Conv 1->32 5, ReLU, Conv 32->32 5, ReLU, BatchNorm32, MaxPool
       Conv 32->64 3, ReLU, Conv 64->64 3, ReLU, BatchNorm64, MaxPool
       Flatten, Linear 576->10, sparse categorical cross entropy, Adam

   The first capture/autotune cost is deliberately outside the timed
   metric.  Run from the repo root:

       THVM_BACKEND=metal THVM_TILE=1 wolframscript -f wl/Examples/beautiful-mnist/bench-train.wls
       BENCH_MODE=forward BS=512 THVM_BACKEND=metal THVM_TILE=1 wolframscript -f wl/Examples/beautiful-mnist/bench-train.wls
       BS=512 WARMUP_STEPS=2 N_STEPS=5 THVM_BACKEND=metal THVM_TILE=1 THVM_AUTOTUNE=1 wolframscript -f wl/Examples/beautiful-mnist/bench-train.wls

   BENCH_MODE:
       train    forward + TGradMany + Adam assignments (default)
       forward  forward loss only, useful for separating kernel quality
       grad-w5  one realized gradient for the final linear weight
       grad-N   one realized gradient for params[[N]]
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

envValue[name_String, default_String] := Module[{v = Environment[name]},
    If[StringQ[v] && StringLength[StringTrim[v]] > 0, v, default]]

parseInt[name_String, default_Integer] := Module[{
    raw = StringTrim @ envValue[name, ToString[default]]
},
    If[StringMatchQ[raw, DigitCharacter ..], ToExpression[raw], default]]

parseIntList[name_String] := Module[{
    raw = StringTrim @ envValue[name, ""],
    parts
},
    If[raw === "", Return[{}]];
    parts = StringTrim /@ StringSplit[raw, {",", ";", " "}];
    ToExpression /@ Select[parts, StringMatchQ[#, DigitCharacter ..] &]
]

formatCounts[a_Association] := StringRiffle[
    KeyValueMap[ToString[#1] <> "=" <> ToString[#2] &, a],
    ", "]

topCount = parseInt["PROFILE_TOP", 12];

kernelIds[] := If[TKernelCount[] > 1, Range[1, TKernelCount[] - 1], {}]

dispatchKindCounts[] := Counts @ Table[
    TKernelDispatchKind[kid],
    {kid, kernelIds[]}]

inputShapes[kid_Integer] := Quiet[
    TTensorShape /@ (THVMLink`Private`tenTermFromTid /@ TKernelInputs[kid])]

formatMaybeAssoc[a_Association] := If[Length[a] == 0, "{}", formatCounts[a]]

formatKernelProgram[kid_Integer] /; kid <= 0 := "{}"
formatKernelProgram[kid_Integer] := Module[{info = TKernelInfo[kid]},
    ToString[Lookup[info, "program", {}], InputForm]]

formatKernelLoweredCounts[kid_Integer] /; kid <= 0 :=
    <|"scalar" -> Missing["NotLowered"], "tile" -> Missing["NotLowered"]|>
formatKernelLoweredCounts[kid_Integer] := Module[{tu},
    tu = TKernelTileUops[kid];
    <|"scalar" -> Missing["Removed"],
      "tile"   -> If[Head[tu] === Missing, tu, Counts[tu[[All, "op"]]]]|>
]

formatKernelBrief[kid_Integer] /; kid <= 0 := "kid=0"
formatKernelBrief[kid_Integer] := Module[{info, lowered, program, opCounts, proposed},
    info = TKernelInfo[kid];
    program = Lookup[info, "program", {}];
    opCounts = Counts[Lookup[program, "opcode", {}]];
    lowered = formatKernelLoweredCounts[kid];
    proposed = Quiet @ Check[TKernelProposed[kid], $Failed];
    StringRiffle[{
        "kid=" <> ToString[kid],
        "kind=" <> TKernelDispatchKind[kid],
        "inputs=" <> ToString[TKernelInputs[kid], InputForm],
        "out=" <> ToString[Lookup[info, "output_numel", 0]],
        "ops=" <> formatMaybeAssoc[opCounts],
        "scalar=" <> ToString[lowered["scalar"], InputForm],
        "tile=" <> ToString[lowered["tile"], InputForm],
        "proposed=" <> ToString[proposed, InputForm]
    }, " "]
]

kernelAxesInfo[kid_Integer] := Quiet @ Check[
    First @ TKernelOpts[kid],
    <|"Applied" -> {}, "AxisTypes" -> {}, "FullShape" -> {}|>]

kernelOutputTid[kid_Integer] := Quiet @ Check[TKernelTable[][[kid, 2]], 0]

kernelConsumerKids[kid_Integer] := Module[{tid = kernelOutputTid[kid]},
    If[tid <= 0, Return[{}]];
    Select[kernelIds[], MemberQ[TKernelInputs[#], tid] &]
]

formatRun[row_Association] := StringRiffle[{
    "start=" <> ToString[row["Start"]],
    "end=" <> ToString[row["End"]],
    "count=" <> ToString[row["Count"]],
    "kinds=" <> formatMaybeAssoc[row["DispatchKindCounts"]],
    "scalar=" <> ToString[row["ScalarLowered"]],
    "tile=" <> ToString[row["TileLowered"]],
    "total_out=" <> ToString[row["TotalOutputNumel"]],
    "top_keys=" <> formatMaybeAssoc[row["TopProgramKeys"]],
    "out_numel=" <> formatMaybeAssoc[row["TopOutputNumels"]],
    "op_counts=" <> formatMaybeAssoc[row["TopOpCounts"]]
}, " "]

formatGraphRun[row_Association] := StringRiffle[{
    "start=" <> ToString[row["Start"]],
    "end=" <> ToString[row["End"]],
    "consumed=" <> ToString[row["Consumed"]],
    "encoded=" <> ToString[row["EncodedDispatches"]],
    "tile=" <> ToString[row["TileDispatches"]],
    "jit=" <> ToString[row["JitDispatches"]],
    "alias=" <> ToString[row["AliasDispatches"]],
    "skipped=" <> ToString[row["SkippedDispatches"]],
    "blocker=" <> ToString[row["Blocker"]],
    "blocker_kid=" <> ToString[row["BlockerKid"]],
    "blocker_key=" <> ToString[row["BlockerProgramKey"]],
    "blocker_out=" <> ToString[row["BlockerOutputNumel"]],
    "blocker_ops=" <> ToString[row["BlockerOpCount"]],
    "total_out=" <> ToString[row["TotalOutputNumel"]],
    "top_keys=" <> formatMaybeAssoc[row["TopProgramKeys"]],
    "out_numel=" <> formatMaybeAssoc[row["TopOutputNumels"]],
    "op_counts=" <> formatMaybeAssoc[row["TopOpCounts"]]
}, " "]

formatMetalMemory[p_Association] := StringRiffle[{
    "live=" <> ToString[p["LiveBytes"]],
    "retained=" <> ToString[p["RetainedBytes"]],
    "deferred=" <> ToString[p["DeferredBytes"]],
    "deferred_count=" <> ToString[p["DeferredCount"]],
    "freelist_bytes=" <> ToString[p["FreelistBytes"]],
    "peak_live=" <> ToString[p["PeakLiveBytes"]],
    "peak_retained=" <> ToString[p["PeakRetainedBytes"]],
    "peak_deferred=" <> ToString[p["PeakDeferredBytes"]],
    "buffers=" <> ToString[p["BufferCount"]] <> "/" <>
        ToString[p["LiveBuffers"]] <> "/" <>
        ToString[p["RetainedBuffers"]]
}, " "]

randTensor[shape_List, scale_ : 0.05] :=
    TTensorCreate @ NumericArray[
        RandomVariate[NormalDistribution[0., scale], shape],
        "Real32"]

zerosTensor[shape_List] := TTensorCreate @ NumericArray[
    ConstantArray[0., shape],
    "Real32"]

onesTensor[shape_List] := TTensorCreate @ NumericArray[
    ConstantArray[1., shape],
    "Real32"]

makeSyntheticBatch[bs_Integer] := Module[{imgs, labels},
    imgs = RandomReal[{0.0, 1.0}, {bs, 1, 28, 28}];
    labels = Mod[Range[0, bs - 1], 10];
    {TTensorCreate @ NumericArray[imgs, "Real32"],
     TTensorCreate @ NumericArray[labels, "Integer32"],
     labels}
]

SeedRandom[42];
TInit[];

bs = parseInt["BS", 512];
warmupSteps = parseInt["WARMUP_STEPS", 1];
nSteps = parseInt["N_STEPS", 3];
doPostAutotune = envValue["POST_AUTOTUNE", "0"] === "1";
postAutotuneKids = parseIntList["POST_AUTOTUNE_KIDS"];
postAutotuneTop = parseInt["POST_AUTOTUNE_TOP", 0];
postAutotuneMetric = envValue["POST_AUTOTUNE_METRIC", "ReduceFlops"];
benchMode = ToLowerCase @ envValue["BENCH_MODE", "train"];
showProfile = envValue["SHOW_PROFILE", "0"] === "1";
showProgramProfile = envValue["SHOW_PROGRAM_PROFILE", "0"] === "1" || showProfile;
showMemoryProfile = envValue["SHOW_MEMORY_PROFILE", "1"] === "1";
showFusionGaps = envValue["SHOW_FUSION_GAPS", "0"] === "1" || showProgramProfile;
showHotCounters = envValue["SHOW_HOT_COUNTERS", "0"] === "1";
dumpTopProgram = envValue["DUMP_TOP_PROGRAM", "0"] === "1";
dumpTopLowered = envValue["DUMP_TOP_LOWERED", "0"] === "1";
dumpJitCapture = envValue["DUMP_JIT_CAPTURE", "0"] === "1";
dumpMemoryPlan = envValue["DUMP_MEMORY_PLAN", "0"] === "1";
dumpMemoryIR = envValue["DUMP_MEMORY_IR", "0"] === "1";
dumpGroupConsumers = envValue["DUMP_GROUP_CONSUMERS", "0"] === "1";

print["beautiful_mnist training benchmark"];
print["  THVM_BACKEND=", envValue["THVM_BACKEND", "default-cpu"],
      "  THVM_TILE=", envValue["THVM_TILE", "off"],
      "  THVM_AUTOTUNE=", envValue["THVM_AUTOTUNE", "off"],
      "  THVM_METAL_GRAPH_REPLAY=", envValue["THVM_METAL_GRAPH_REPLAY", "default-on"],
      "  THVM_METAL_GRAPH_MAX_DISPATCHES=", envValue["THVM_METAL_GRAPH_MAX_DISPATCHES", "256"],
      "  THVM_METAL_DEFER_BYTES=", envValue["THVM_METAL_DEFER_BYTES", "1073741824"],
      "  THVM_METAL_FREELIST_BYTES=", envValue["THVM_METAL_FREELIST_BYTES", "1073741824"]];
print["  BS=", bs,
      "  WARMUP_STEPS=", warmupSteps,
      "  N_STEPS=", nSteps,
      "  BENCH_MODE=", benchMode,
      "  SHOW_PROFILE=", If[showProfile, "1", "0"],
      "  SHOW_PROGRAM_PROFILE=", If[showProgramProfile, "1", "0"],
      "  SHOW_MEMORY_PROFILE=", If[showMemoryProfile, "1", "0"],
      "  SHOW_FUSION_GAPS=", If[showFusionGaps, "1", "0"],
      "  SHOW_HOT_COUNTERS=", If[showHotCounters, "1", "0"],
      "  DUMP_TOP_PROGRAM=", If[dumpTopProgram, "1", "0"],
      "  DUMP_TOP_LOWERED=", If[dumpTopLowered, "1", "0"],
      "  DUMP_JIT_CAPTURE=", If[dumpJitCapture, "1", "0"],
      "  DUMP_MEMORY_PLAN=", If[dumpMemoryPlan, "1", "0"],
      "  DUMP_MEMORY_IR=", If[dumpMemoryIR, "1", "0"],
      "  DUMP_GROUP_CONSUMERS=", If[dumpGroupConsumers, "1", "0"],
      "  PROFILE_TOP=", topCount,
      "  POST_AUTOTUNE=", If[doPostAutotune, "1", "0"],
      "  POST_AUTOTUNE_TOP=", postAutotuneTop,
      "  POST_AUTOTUNE_METRIC=", postAutotuneMetric,
      "  POST_AUTOTUNE_KIDS=", ToString[postAutotuneKids, InputForm]];

W1 = randTensor[{32, 1, 5, 5}];   b1 = zerosTensor[{32}];
W2 = randTensor[{32, 32, 5, 5}];  b2 = zerosTensor[{32}];
g1 = onesTensor[{32}];            bn1 = zerosTensor[{32}];
W3 = randTensor[{64, 32, 3, 3}];  b3 = zerosTensor[{64}];
W4 = randTensor[{64, 64, 3, 3}];  b4 = zerosTensor[{64}];
g2 = onesTensor[{64}];            bn2 = zerosTensor[{64}];
W5 = randTensor[{576, 10}];       b5 = zerosTensor[{10}];

params = {W1, b1, W2, b2, g1, bn1, W3, b3, W4, b4, g2, bn2, W5, b5};
mState = TZerosLike /@ params;
vState = TZerosLike /@ params;

{xSlot, tgtSlot, labels} = makeSyntheticBatch[bs];
print["params=", Total[Times @@ TTensorShape[#] & /@ params],
      "  fixed synthetic labels=", Take[labels, UpTo[12]]];
If[showMemoryProfile,
    print["metal_memory init: ", formatMetalMemory @ TMetalMemoryProfile[]]];

forwardLoss[x_TTerm, target_TTerm] := Module[{
    h1, r1, h2, r2, n1, p1, h3, r3, h4, r4, n2, p2, flat, logits
},
    h1 = TConv2D[x, W1, b1];
    r1 = TReLU[h1];
    h2 = TConv2D[r1, W2, b2];
    r2 = TReLU[h2];
    n1 = TBatchNormTrain[r2, g1, bn1];
    p1 = TMaxPool2d[n1, 2];
    h3 = TConv2D[p1, W3, b3];
    r3 = TReLU[h3];
    h4 = TConv2D[r3, W4, b4];
    r4 = TReLU[h4];
    n2 = TBatchNormTrain[r4, g2, bn2];
    p2 = TMaxPool2d[n2, 2];
    flat = TUOpReshape[p2, {bs, 576}];
    logits = TLinear[flat, W5, b5];
    TSparseCategoricalCrossEntropy[logits, target]
]

gradIndexFromMode[mode_String] := Module[{s},
    s = StringCases[mode, "grad-" ~~ n : DigitCharacter .. :> n];
    If[s === {}, 0, ToExpression[First[s]]]]

trainStep = Switch[benchMode,
    "forward",
        Function[{stepIdx}, TRealize @ forwardLoss[xSlot, tgtSlot]; Null],
    "grad-w5",
        Function[{stepIdx}, Module[{loss = TMaterialize @ forwardLoss[xSlot, tgtSlot]},
            TRealize @ TGrad[loss, W5]; Null]],
    _ /; StringStartsQ[benchMode, "grad-"] && gradIndexFromMode[benchMode] > 0,
        Function[{stepIdx}, Module[{
            loss = TMaterialize @ forwardLoss[xSlot, tgtSlot],
            idx = gradIndexFromMode[benchMode]
        },
            TRealize @ TGrad[loss, params[[idx]]]; Null]],
    _,
        Function[{stepIdx}, Module[{loss = forwardLoss[xSlot, tgtSlot]},
            TAdam[loss, params, mState, vState, stepIdx]; Null]]
];

jittedStep = TJit[trainStep];

runStep[step_Integer, label_String] := Module[{t0, dt, kBefore, kAfter},
    kBefore = TKernelCount[];
    t0 = AbsoluteTime[];
    jittedStep[step];
    dt = (AbsoluteTime[] - t0) * 1000.0;
    kAfter = TKernelCount[];
    print[label, " step ", step,
          ": wall=", NumberForm[dt, {8, 1}], "ms",
          " kernels(new)=", kAfter - kBefore,
          " jit-ops=", TJitOpCount[jittedStep],
          " heap=", THeapPos[]];
    dt
]

warmupTimes = Table[runStep[i, "warmup"], {i, warmupSteps}];

If[doPostAutotune,
    Module[{t0 = AbsoluteTime[], tuned},
        tuned = TKernelAutotuneUnique[];
        print["post-autotune kernels=", Length[tuned],
              " wall=", NumberForm[(AbsoluteTime[] - t0) * 1000.0, {8, 1}], "ms"]]]

If[postAutotuneTop > 0,
    Module[{t0 = AbsoluteTime[], tuned, profile},
        profile = TProfileAll[];
        tuned = TKernelAutotuneTop[profile, postAutotuneTop, postAutotuneMetric];
        print["post-autotune-top count=", Length[tuned],
              " metric=", postAutotuneMetric,
              " wall=", NumberForm[(AbsoluteTime[] - t0) * 1000.0, {8, 1}], "ms",
              " result=", ToString[tuned, InputForm]]]]

If[postAutotuneKids =!= {},
    Module[{t0 = AbsoluteTime[], tuned},
        tuned = Association @ Table[
            kid -> TKernelAutotune[kid],
            {kid, Select[postAutotuneKids, 1 <= # < TKernelCount[] &]}];
        print["post-autotune-kids count=", Length[tuned],
              " wall=", NumberForm[(AbsoluteTime[] - t0) * 1000.0, {8, 1}], "ms",
              " result=", ToString[tuned, InputForm]]]]

profileBeforeTimed = If[showProfile || showProgramProfile,
    TProfileAll[],
    <||>];
memoryBeforeTimed = If[showMemoryProfile, TMetalMemoryProfile[], <||>];
gpuTimeBeforeTimed = TMetalGpuTime[];
If[showHotCounters, THotCountersReset[]];
timedTimes = Table[runStep[warmupSteps + i, "timed"], {i, nSteps}];
gpuTimeAfterTimed = TMetalGpuTime[];
hotCountersTimed = If[showHotCounters, THotCounters[], <||>];
profileAfterTimed = If[showProfile || showProgramProfile,
    TProfileAll[],
    <||>];
memoryAfterTimed = If[showMemoryProfile, TMetalMemoryProfile[], <||>];
profileTimedDelta = If[showProfile || showProgramProfile,
    TProfileDelta[profileBeforeTimed, profileAfterTimed],
    <||>];

print[""];
print["timed_ms=", NumberForm[timedTimes, {8, 1}]];
print["steady_ms_per_step=", NumberForm[Mean[timedTimes], {8, 1}]];
Module[{dUs = gpuTimeAfterTimed["TotalUs"] - gpuTimeBeforeTimed["TotalUs"],
        dFlush = gpuTimeAfterTimed["FlushCount"] - gpuTimeBeforeTimed["FlushCount"]},
    print["gpu_us_per_step=", NumberForm[N[dUs / Max[nSteps, 1]], {10, 1}],
          "  gpu_ms_per_step=", NumberForm[N[dUs / 1000. / Max[nSteps, 1]], {8, 3}],
          "  gpu_flushes=", dFlush,
          "  gpu_us_total=", dUs,
          "  (NB: 0 unless THVM_JIT_REPLAY_NOSKIP=1 -- forward mode discards the loss so replay is liveness-skipped)"]];
print["kernels=", TKernelCount[] - 1,
      " dispatch=", formatCounts[dispatchKindCounts[]]];
If[showMemoryProfile,
    print["metal_memory before_timed: ", formatMetalMemory[memoryBeforeTimed]];
    print["metal_memory after_timed:  ", formatMetalMemory[memoryAfterTimed]]];
If[showHotCounters,
    print["hot_counters timed: ", formatCounts[hotCountersTimed]]];
If[dumpJitCapture,
    Module[{summary = TJitCaptureSummary[jittedStep]},
        print["jit_capture_summary: op_count=", summary["OpCount"],
              " kinds=", formatMaybeAssoc[summary["KindCounts"]],
              " dispatch_kinds=", formatMaybeAssoc[summary["DispatchKindCounts"]],
              " replay_live_dispatches=", summary["ReplayLiveDispatches"],
              " replay_skipped=", summary["ReplaySkipped"],
              " replay_packed_dispatches=", summary["ReplayPackedDispatches"],
              " run_count=", summary["RunCount"],
              " dispatch_runs=", summary["DispatchRuns"],
              " graph_run_count=", summary["GraphRunCount"],
              " graph_encoded_dispatches=", summary["GraphEncodedDispatches"],
              " graph_tile_dispatches=", summary["GraphTileDispatches"],
              " graph_jit_dispatches=", summary["GraphJitDispatches"],
              " graph_alias_dispatches=", summary["GraphAliasDispatches"],
              " graph_skipped_dispatches=", summary["GraphSkippedDispatches"],
              " graph_blockers=", formatMaybeAssoc[summary["GraphBlockers"]],
              " top_keys=", formatMaybeAssoc[summary["TopProgramKeys"]],
              " top_out_numel=", formatMaybeAssoc[summary["TopOutputNumels"]]];
        print["jit_capture_top_runs:"];
        Do[print["  ", formatRun[row]], {row, summary["TopRuns"]}];
        print["jit_capture_top_graph_runs:"];
        Do[print["  ", formatGraphRun[row]], {row, summary["TopGraphRuns"]}]]];
If[dumpMemoryPlan,
    Module[{plan = First @ TMemoryPlan[], bufs, metalLive, top},
        bufs = plan["Bufs"];
        metalLive = Select[bufs, #["backend"] === 2 && #["refcount"] > 0 &];
        top = Take[ReverseSortBy[metalLive, #["nbytes"] &], UpTo[topCount]];
        print["memory_plan peak_bytes=", plan["Peak"]["peak_bytes"],
              " peak_depth=", plan["Peak"]["peak_depth"],
              " total_bytes=", plan["Peak"]["total_bytes"],
              " metal_live_bufs=", Length[metalLive]];
        print["top live metal buffers by bytes:"];
        Do[
            Module[{
                kid = row["producer_kid"],
                originKid = Lookup[row, "origin_producer_kid", row["producer_kid"]],
                originSourceTid = Lookup[row, "origin_source_tid", 0],
                aliasChain = Lookup[row, "producer_alias_chain", {}],
                info,
                ops
            },
                info = If[kid > 0, TKernelInfo[kid], <||>];
                ops = If[KeyExistsQ[info, "program"],
                    Counts[info["program"][[All, "opcode"]]],
                    <||>];
                print["  buf=", row["id"],
                      " bytes=", row["nbytes"],
                      " refs=", row["refcount"],
                      " producer=", kid,
                      " origin=", originKid,
                      " origin_src_tid=", originSourceTid,
                      " prod_out=", Lookup[info, "output_numel", 0],
                      " prod_ops=", Lookup[info, "n_ops", 0],
                      " prod_op_hist=", formatMaybeAssoc[ops],
                      " consumers=", Length[row["consumer_kids"]],
                      " span=", row["alive_span"],
                      " dtype=", row["dtype"],
                      " aliases=", ToString[Take[row["alias_tids"], UpTo[4]], InputForm]];
                If[dumpMemoryIR,
                    If[aliasChain =!= {},
                        print["    alias_chain=", ToString[aliasChain, InputForm]]];
                    print["    producer_ir=", formatKernelBrief[kid]];
                    print["    producer_program=", formatKernelProgram[kid]];
                    If[originKid =!= kid,
                        print["    origin_ir=", formatKernelBrief[originKid]];
                        print["    origin_program=", formatKernelProgram[originKid]]];
                    print["    consumer_kids=", ToString[row["consumer_kids"], InputForm]];
                    Do[
                        print["    consumer_ir=", formatKernelBrief[ck]],
                        {ck, Take[row["consumer_kids"], UpTo[topCount]]}]]
            ],
            {row, top}]]];
If[showProgramProfile,
    Module[{groups = Take[TProfileProgramGroups[profileTimedDelta], UpTo[topCount]]},
        print["top timed program shapes by total dispatch time:"];
        Do[
            print["  key=", row["Key"],
                  " rep_kid=", row["RepKid"],
                  " kids=", row["KidCount"],
                  " count=", row["DispatchCount"],
                  " total_us=", row["TotalUs"],
                  " avg_us=", NumberForm[row["AvgUs"], {8, 1}],
                  " flops=", row["Flops"],
                  " kinds=", formatMaybeAssoc[row["DispatchKinds"]],
                  " n_inputs=", row["InputCount"],
                  " n_ops=", row["OpCount"],
                  " out_numel=", row["OutputNumel"],
                  " total_out=", row["TotalOutputNumel"],
                  " applied=", ToString[kernelAxesInfo[row["RepKid"]]["Applied"], InputForm],
                  " axes=", ToString[kernelAxesInfo[row["RepKid"]]["AxisTypes"], InputForm],
                  " ops=", formatMaybeAssoc[row["Ops"]],
                  " inputs=", inputShapes[row["RepKid"]]];
            If[dumpTopProgram,
                print["    program=", TKernelInfo[row["RepKid"]]["program"]]];
            If[dumpTopLowered,
                Module[{kid = row["RepKid"], tu},
                    tu = TKernelTileUops[kid];
                    print["    proposed=", TKernelProposed[kid]];
                    print["    tile=", If[Head[tu] === Missing,
                        tu,
                        Counts[tu[[All, "op"]]]]];
                    print["    tile_plan=", TKernelTilePlan[kid]]]],
            {row, groups}]];
    Module[{groups = Take[
        ReverseSortBy[TProfileProgramGroups[profileTimedDelta], Lookup[#, "Flops", 0] &],
        UpTo[topCount]]},
        print["top timed program shapes by static flop estimate:"];
        Do[
            print["  key=", row["Key"],
                  " rep_kid=", row["RepKid"],
                  " kids=", row["KidCount"],
                  " count=", row["DispatchCount"],
                  " flops=", row["Flops"],
                  " out_numel=", row["OutputNumel"],
                  " total_out=", row["TotalOutputNumel"],
                  " kinds=", formatMaybeAssoc[row["DispatchKinds"]],
                  " n_inputs=", row["InputCount"],
                  " n_ops=", row["OpCount"],
                  " applied=", ToString[kernelAxesInfo[row["RepKid"]]["Applied"], InputForm],
                  " axes=", ToString[kernelAxesInfo[row["RepKid"]]["AxisTypes"], InputForm],
                  " ops=", formatMaybeAssoc[row["Ops"]]],
            {row, groups}]];
    Module[{groups = Take[
        ReverseSortBy[TProfileProgramGroups[profileTimedDelta], Lookup[#, "DispatchCount", 0] &],
        UpTo[topCount]]},
        print["top timed program shapes by dispatch count:"];
        Do[
            print["  key=", row["Key"],
                  " rep_kid=", row["RepKid"],
                  " kids=", row["KidCount"],
                  " count=", row["DispatchCount"],
                  " total_us=", row["TotalUs"],
                  " avg_us=", NumberForm[row["AvgUs"], {8, 1}],
                  " kinds=", formatMaybeAssoc[row["DispatchKinds"]],
                  " n_inputs=", row["InputCount"],
                  " n_ops=", row["OpCount"],
                  " out_numel=", row["OutputNumel"],
                  " total_out=", row["TotalOutputNumel"],
                  " applied=", ToString[kernelAxesInfo[row["RepKid"]]["Applied"], InputForm],
                  " axes=", ToString[kernelAxesInfo[row["RepKid"]]["AxisTypes"], InputForm],
                  " ops=", formatMaybeAssoc[row["Ops"]]];
            If[dumpGroupConsumers,
                Module[{consumers = kernelConsumerKids[row["RepKid"]]},
                    print["    consumers=", ToString[consumers, InputForm]];
                    Do[
                        print["    consumer_ir=", formatKernelBrief[ck]],
                        {ck, Take[consumers, UpTo[topCount]]}]]],
            {row, groups}]]];
If[showFusionGaps,
    Module[{gaps = Take[TProfileFusionGaps[profileTimedDelta], UpTo[topCount]]},
        print["top timed fusion gaps:"];
        Do[
            print["  status=", row["FusionStatus"],
                  " key=", row["Key"],
                  " rep_kid=", row["RepKid"],
                  " total_us=", row["TotalUs"],
                  " count=", row["DispatchCount"],
                  " kinds=", formatMaybeAssoc[row["DispatchKinds"]],
                  " scalar=", row["ScalarLowered"],
                  " tile=", row["TileLowered"],
                  " proposals=", row["ProposalCount"],
                  " n_inputs=", row["InputCount"],
                  " n_ops=", row["OpCount"],
                  " out_numel=", row["OutputNumel"],
                  " ops=", formatMaybeAssoc[row["Ops"]]],
            {row, gaps}]]];
If[showProfile,
    Module[{rows},
        rows = ReverseSortBy[
            Values[profileTimedDelta],
            #["TotalUs"] &];
        print["top timed kernels by total dispatch time:"];
        Do[
            print["  kid=", row["Kid"],
                  " kind=", row["DispatchKind"],
                  " count=", row["DispatchCount"],
                  " total_us=", row["TotalUs"],
                  " avg_us=", NumberForm[row["AvgUs"], {8, 1}],
                  " gflops=", NumberForm[row["GFlopsPerSec"], {8, 1}],
                  " inputs=", inputShapes[row["Kid"]],
                  " props=", TKernelProposed[row["Kid"]]],
            {row, Take[rows, UpTo[12]]}]]];
(* Per-kernel TRUE GPU time -- only meaningful when THVM_METAL_PROFILE_PEROP=1
   (each kernel then dispatches in its own command buffer).  Sorted by
   per-step GPU us; nSteps fires per kernel during the timed loop, so the
   per-step number is GpuUs/GpuSamples * (samples/step) ~= GpuUs/nSteps when
   each kernel fires once per step. *)
If[showProfile,
    Module[{perop = TMetalPerOpProfile[], rows},
        If[Length[perop] > 0,
            rows = ReverseSortBy[
                KeyValueMap[Append[#2, "Kid" -> #1] &, perop],
                #["GpuUs"] &];
            print["per-kernel TRUE GPU time (THVM_METAL_PROFILE_PEROP):"];
            Do[
                print["  kid=", row["Kid"],
                      " kind=", row["DispatchKind"],
                      " gpu_us_per_step=", NumberForm[
                          N[row["GpuUs"] / Max[nSteps, 1]], {10, 1}],
                      " gpu_us_total=", row["GpuUs"],
                      " samples=", row["GpuSamples"],
                      " gflops=", NumberForm[row["GFlopsPerSec"], {8, 1}],
                      " out_numel=", Lookup[Quiet @ TKernelInfo[row["Kid"]], "output_numel", 0],
                      " axes=", ToString[kernelAxesInfo[row["Kid"]]["AxisTypes"], InputForm],
                      " inputs=", inputShapes[row["Kid"]]],
                {row, rows}],
            print["per-kernel TRUE GPU time: (none -- set THVM_METAL_PROFILE_PEROP=1)"]]]];
print["total_buf_bytes=", TTotalBufBytes[], " heap=", THeapPos[]];
print["done."];
