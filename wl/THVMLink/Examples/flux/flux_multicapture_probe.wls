#!/usr/bin/env wl
(* Multi-capture interaction probe: capture velJit, THEN capture qwJit, THEN
   capture vaeJit -- in the SAME context, the session order -- then replay velJit
   with a DIFFERENT input.  If velJit replay no longer rebinds (output frozen to
   warmup OR near-instant), a later capture corrupted velJit's slot. *)
If[ Environment["THVM_FWD_RECLAIM"] === $Failed, SetEnvironment["THVM_FWD_RECLAIM" -> "1"]];
If[ Environment["THVM_MMAP_NO_WILLNEED"] === $Failed, SetEnvironment["THVM_MMAP_NO_WILLNEED" -> "1"]];
If[ Environment["THVM_MAX_LIVE_BYTES"] === $Failed, SetEnvironment["THVM_MAX_LIVE_BYTES" -> "30000000000"]];
PacletDirectoryLoad["wl/THVMLink"]; Get["WolframInstitute`THVMLink`"];
Get[FileNameJoin[{DirectoryName[$InputFileName], "FluxGenerate.wl"}]];
TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);
modelDir = Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b";
dev = "metal"; stxt = 512; simg = 256;
tfPath = FileNameJoin[{modelDir, "transformer", "diffusion_pytorch_model.safetensors"}];
qwPaths = FileNameJoin[{modelDir, "text_encoder", #}] & /@ {"model-00001-of-00002.safetensors", "model-00002-of-00002.safetensors"};
vaePath = FileNameJoin[{modelDir, "vae", "diffusion_pytorch_model.safetensors"}];
fxCfg = <|"eps" -> 1.*^-6, "num_double" -> 5, "num_single" -> 20, "heads" -> 24, "head_dim" -> 128|>;
qwCfg = <|"heads" -> 32, "kv_heads" -> 8, "head_dim" -> 128, "eps" -> 1.*^-6, "theta" -> 1000000, "layers" -> 27, "captureLayers" -> {8, 17, 26}|>;
vaeCfg = <|"eps" -> 1.*^-6, "epsBn" -> 1.*^-4, "gridH" -> 16, "gridW" -> 16|>;
ctx = TContextNew[dev];
TInContext[ctx, Module[{wt, wfT, ca, rope, rc, rs, tembFn, velJit, mk, o0, o1, d1,
                        qw, wfq, qct, qcos, qsin, qwJit, vt, vKeys, wfV, wsubV, vaeJit, mkf},
  wt = TSafeTensorLoad[tfPath]; wfT = fxTransformerLoader[wt, dev];
  ca = (TRealize @ TToDevice[TUOpCast[#, "bf16"], dev]) &;
  rope = fxRopeTable[16, 16, stxt];
  rc = ca @ TTensorCreate[rope["cos"]]; rs = ca @ TTensorCreate[rope["sin"]];
  tembFn = fxTembFn[wfT, dev];
  velJit = fxVelocityJit[rc, rs, wfT, fxCfg];
  mk[s_, d__] := (SeedRandom[s]; ca @ TTensorCreate @ NumericArray[RandomReal[{-0.05, 0.05}, {d}], "Real32"]);
  mkf[s_, d__] := (SeedRandom[s]; TRealize @ TToDevice[TTensorCreate @ NumericArray[RandomReal[{-0.05, 0.05}, {d}], "Real32"], dev]);
  o0 = Normal @ velJit[mk[1, simg, 128], mk[2, stxt, 7680], tembFn[0.9]];
  pr["velJit captured  mean=", Round[Mean[Flatten[o0]], 0.0001]];
  (* now capture qwJit *)
  qw = Join[TSafeTensorLoad[qwPaths[[1]]], TSafeTensorLoad[qwPaths[[2]]]]; wfq = fxQwenLoader[qw, dev];
  qct = TRoPEHalfSplitTable[stxt, qwCfg["head_dim"], qwCfg["theta"]]; qcos = TToDevice[qct[[1]], dev]; qsin = TToDevice[qct[[2]], dev];
  qwJit = TJit[Function[{x, addMask}, qwenForward[x, addMask, qcos, qsin, wfq, qwCfg]]];
  qwJit[mk[3, stxt, 2560], mk[4, stxt, stxt]];
  pr["qwJit captured"];
  (* capture vaeJit *)
  vt = TSafeTensorLoad[vaePath]; vKeys = Keys[vt]; wfV = fxVaeLoader[vt, dev]; wsubV = fxVaeSub[wfV, vKeys];
  vaeJit = TJit[Function[l, TRealize @ vaeDecoder[l, wfV, wsubV, vaeCfg]]];
  vaeJit[mkf[5, simg, 128]];
  pr["vaeJit captured"];
  (* NOW replay velJit with DIFFERENT input -- must change + take ~0.6s *)
  {d1, o1} = AbsoluteTiming[Normal @ velJit[mk[7, simg, 128], mk[8, stxt, 7680], tembFn[0.3]]];
  pr["velJit replay(diff) ", Round[d1, 0.01], " s  mean=", Round[Mean[Flatten[o1]], 0.0001], "  vs-cap maxabs=", Round[Max[Abs[Flatten[o1]-Flatten[o0]]],0.0001]];
]];
TContextDestroy[ctx];
pr["DONE"];
