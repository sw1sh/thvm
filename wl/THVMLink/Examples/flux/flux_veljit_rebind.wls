#!/usr/bin/env wl
(* velJit rebind probe: capture fxVelocityJit on one (z,e,temb), then replay on a
   DIFFERENT (z,e,temb), and confirm the output CHANGES (inputs rebind).  Mirrors
   the session path: a separate warmup capture, then real replays.  Times each
   replay to confirm it actually recomputes (not a cached constant). *)
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
fxCfg = <|"eps" -> 1.*^-6, "num_double" -> 5, "num_single" -> 20, "heads" -> 24, "head_dim" -> 128|>;
ctx = TContextNew[dev];
TInContext[ctx, Module[{wt, wfT, ca, rope, rc, rs, tembFn, velJit, mk, e1, z1, t1, e2, z2, t2,
                        o0, dwarm, o1, d1, o2, d2, o3, d3},
  wt = TSafeTensorLoad[tfPath]; wfT = fxTransformerLoader[wt, dev];
  ca = (TRealize @ TToDevice[TUOpCast[#, "bf16"], dev]) &;
  rope = fxRopeTable[16, 16, stxt];
  rc = ca @ TTensorCreate[rope["cos"]]; rs = ca @ TTensorCreate[rope["sin"]];
  tembFn = fxTembFn[wfT, dev];
  velJit = fxVelocityJit[rc, rs, wfT, fxCfg];
  mk[s_, d__] := (SeedRandom[s]; ca @ TTensorCreate @ NumericArray[RandomReal[{-0.05, 0.05}, {d}], "Real32"]);
  (* warmup capture on random A *)
  {dwarm, o0} = AbsoluteTiming[Normal @ velJit[mk[1, simg, 128], mk[2, stxt, 7680], tembFn[0.9]]];
  pr["warmup(capture) ", Round[dwarm, 0.01], " s  mean=", Round[Mean[Flatten[o0]], 0.0001]];
  (* replay on SAME A *)
  {d1, o1} = AbsoluteTiming[Normal @ velJit[mk[1, simg, 128], mk[2, stxt, 7680], tembFn[0.9]]];
  pr["replay(same A)  ", Round[d1, 0.01], " s  mean=", Round[Mean[Flatten[o1]], 0.0001], "  vs-warm maxabs=", Round[Max[Abs[Flatten[o1]-Flatten[o0]]],0.0001]];
  (* replay on DIFFERENT B -- output MUST change *)
  {d2, o2} = AbsoluteTiming[Normal @ velJit[mk[7, simg, 128], mk[8, stxt, 7680], tembFn[0.3]]];
  pr["replay(diff B)  ", Round[d2, 0.01], " s  mean=", Round[Mean[Flatten[o2]], 0.0001], "  vs-A maxabs=", Round[Max[Abs[Flatten[o2]-Flatten[o0]]],0.0001]];
  (* direct (non-jit) forward on B for ground truth *)
  {d3, o3} = AbsoluteTiming[Normal @ TRealize @ fxTransformer[mk[7,simg,128], mk[8,stxt,7680], tembFn[0.3], rc, rs, wfT, fxCfg]];
  pr["direct B        ", Round[d3, 0.01], " s  mean=", Round[Mean[Flatten[o3]], 0.0001], "  vs-replayB maxabs=", Round[Max[Abs[Flatten[o2]-Flatten[o3]]],0.0001]];
]];
TContextDestroy[ctx];
pr["DONE"];
