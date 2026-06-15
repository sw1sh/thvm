#!/usr/bin/env wl
(* qwJit rebind probe: capture qwJit on prompt A, replay on prompt B, compare to
   EAGER B.  Confirms the 27-layer Qwen replay rebinds x + addMask (not frozen to
   A).  Also times the replay + reports TJit graph-run summary (ICB eligibility). *)
If[ Environment["THVM_FWD_RECLAIM"] === $Failed, SetEnvironment["THVM_FWD_RECLAIM" -> "1"]];
If[ Environment["THVM_MMAP_NO_WILLNEED"] === $Failed, SetEnvironment["THVM_MMAP_NO_WILLNEED" -> "1"]];
If[ Environment["THVM_MAX_LIVE_BYTES"] === $Failed, SetEnvironment["THVM_MAX_LIVE_BYTES" -> "30000000000"]];
PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"];
Get[FileNameJoin[{DirectoryName[$InputFileName], "FluxGenerate.wl"}]];
TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);
modelDir = Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b";
dev = "metal"; stxt = 512;
qwCfg = <|"heads" -> 32, "kv_heads" -> 8, "head_dim" -> 128, "eps" -> 1.*^-6, "theta" -> 1000000, "layers" -> 27, "captureLayers" -> {8, 17, 26}|>;
tokDir = FileNameJoin[{modelDir, "tokenizer"}];
qwPaths = FileNameJoin[{modelDir, "text_encoder", #}] & /@ {"model-00001-of-00002.safetensors", "model-00002-of-00002.safetensors"};
td = qwTokenizerData[tokDir];
ctx = TContextNew[dev];
TInContext[ctx, Module[{qw, wfq, qct, qcos, qsin, qwJit, inA, inB, capA, eagA, repB, eagB, dC, dR},
  qw = Join[TSafeTensorLoad[qwPaths[[1]]], TSafeTensorLoad[qwPaths[[2]]]]; wfq = fxQwenLoader[qw, dev];
  qct = TRoPEHalfSplitTable[stxt, qwCfg["head_dim"], qwCfg["theta"]]; qcos = TToDevice[qct[[1]], dev]; qsin = TToDevice[qct[[2]], dev];
  qwJit = TJit[Function[{x, addMask}, qwenForward[x, addMask, qcos, qsin, wfq, qwCfg]]];
  inA = qwenInputs[Sequence @@ qwTokenize["a red apple on a table", td, stxt], wfq, qwCfg];
  inB = qwenInputs[Sequence @@ qwTokenize["a blue bird on a branch", td, stxt], wfq, qwCfg];
  {dC, capA} = AbsoluteTiming[Normal @ qwJit[inA["x"], inA["addMask"]]];   (* capture A *)
  eagA = Normal @ qwenEncode[Sequence @@ qwTokenize["a red apple on a table", td, stxt], wfq, qwCfg];
  {dR, repB} = AbsoluteTiming[Normal @ qwJit[inB["x"], inB["addMask"]]];   (* replay B *)
  eagB = Normal @ qwenEncode[Sequence @@ qwTokenize["a blue bird on a branch", td, stxt], wfq, qwCfg];
  pr["capture A ", Round[dC,0.01], "s  mean=", Round[Mean[Flatten[capA]],0.0001], "  vs eagerA maxabs=", Round[Max[Abs[Flatten[capA]-Flatten[eagA]]],0.001]];
  pr["replay  B ", Round[dR,0.01], "s  mean=", Round[Mean[Flatten[repB]],0.0001], "  vs eagerB maxabs=", Round[Max[Abs[Flatten[repB]-Flatten[eagB]]],0.001]];
  pr["A vs B differ maxabs=", Round[Max[Abs[Flatten[capA]-Flatten[repB]]],0.001]];
  pr["graph summary: ", TJitCaptureSummary[qwJit]];
]];
TContextDestroy[ctx];
pr["DONE"];
