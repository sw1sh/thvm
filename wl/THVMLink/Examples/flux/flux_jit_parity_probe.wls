#!/usr/bin/env wl
(* Parity probe: eager qwenEncode/vaeDecoder vs their TJit-captured forms, on
   ONE fixed input, inside a single persistent context (no TReset).  Isolates
   which captured stage (Qwen or VAE) diverges from eager. *)
If[ Environment["THVM_FWD_RECLAIM"] === $Failed, SetEnvironment["THVM_FWD_RECLAIM" -> "1"]];
If[ Environment["THVM_MMAP_NO_WILLNEED"] === $Failed, SetEnvironment["THVM_MMAP_NO_WILLNEED" -> "1"]];
If[ Environment["THVM_MAX_LIVE_BYTES"] === $Failed, SetEnvironment["THVM_MAX_LIVE_BYTES" -> "30000000000"]];
PacletDirectoryLoad["wl/THVMLink"]; Get["WolframInstitute`THVMLink`"];
Get[FileNameJoin[{DirectoryName[$InputFileName], "FluxGenerate.wl"}]];
TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);

modelDir = Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b";
dev = "metal"; stxt = 512;
qwCfg = <|"heads" -> 32, "kv_heads" -> 8, "head_dim" -> 128, "eps" -> 1.*^-6,
          "theta" -> 1000000, "layers" -> 27, "captureLayers" -> {8, 17, 26}|>;
vaeCfg = <|"eps" -> 1.*^-6, "epsBn" -> 1.*^-4, "gridH" -> 16, "gridW" -> 16|>;
tokDir = FileNameJoin[{modelDir, "tokenizer"}];
qwPaths = FileNameJoin[{modelDir, "text_encoder", #}] & /@
    {"model-00001-of-00002.safetensors", "model-00002-of-00002.safetensors"};
vaePath = FileNameJoin[{modelDir, "vae", "diffusion_pytorch_model.safetensors"}];
td = qwTokenizerData[tokDir];

ctx = TContextNew[dev];
TInContext[ctx, Module[{qw, wfq, qct, qcos, qsin, qwJit, in, eEager, eJit,
                        vt, vKeys, wfV, wsubV, vaeJit, lat, latT, vEager, vJit},
  qw = Join[TSafeTensorLoad[qwPaths[[1]]], TSafeTensorLoad[qwPaths[[2]]]];
  wfq = fxQwenLoader[qw, dev];
  (* QWEN: eager vs jit on the SAME prompt *)
  in = qwenInputs[Sequence @@ qwTokenize["a red apple on a table", td, stxt], wfq, qwCfg];
  eEager = Normal @ qwenEncode[Sequence @@ qwTokenize["a red apple on a table", td, stxt], wfq, qwCfg];
  qct = TRoPEHalfSplitTable[stxt, qwCfg["head_dim"], qwCfg["theta"]];
  qcos = TToDevice[qct[[1]], dev]; qsin = TToDevice[qct[[2]], dev];
  qwJit = TJit[Function[{x, addMask}, qwenForward[x, addMask, qcos, qsin, wfq, qwCfg]]];
  eJit = Normal @ qwJit[in["x"], in["addMask"]];
  pr["QWEN eager dims=", Dimensions[eEager], "  jit dims=", Dimensions[eJit]];
  pr["QWEN eager  mean=", Round[Mean[Flatten[eEager]], 0.0001], " absmax=", Round[Max[Abs[Flatten[eEager]]], 0.001]];
  pr["QWEN jit    mean=", Round[Mean[Flatten[eJit]], 0.0001], " absmax=", Round[Max[Abs[Flatten[eJit]]], 0.001]];
  pr["QWEN eager-vs-jit maxabs=", Round[Max[Abs[Flatten[eEager] - Flatten[eJit]]], 0.0001]];

  (* VAE: eager vs jit on a fixed random latent *)
  vt = TSafeTensorLoad[vaePath]; vKeys = Keys[vt];
  wfV = fxVaeLoader[vt, dev]; wsubV = fxVaeSub[wfV, vKeys];
  SeedRandom[42];
  lat = RandomVariate[NormalDistribution[], {256, 128}];
  latT = TRealize @ TToDevice[TTensorCreate @ NumericArray[lat, "Real32"], dev];
  vEager = Normal @ TRealize @ vaeDecoder[latT, wfV, wsubV, vaeCfg];
  vaeJit = TJit[Function[l, TRealize @ vaeDecoder[l, wfV, wsubV, vaeCfg]]];
  vJit = Normal @ vaeJit[TRealize @ TToDevice[TTensorCreate @ NumericArray[lat, "Real32"], dev]];
  pr["VAE eager dims=", Dimensions[vEager], " mean=", Round[Mean[Flatten[vEager]], 0.0001], " max=", Round[Max[Flatten[vEager]], 0.0001]];
  pr["VAE jit   dims=", Dimensions[vJit], " mean=", Round[Mean[Flatten[vJit]], 0.0001], " max=", Round[Max[Flatten[vJit]], 0.0001]];
  pr["VAE eager-vs-jit maxabs=", Round[Max[Abs[Flatten[vEager] - Flatten[vJit]]], 0.0001]];
]];
TContextDestroy[ctx];
pr["DONE"];
