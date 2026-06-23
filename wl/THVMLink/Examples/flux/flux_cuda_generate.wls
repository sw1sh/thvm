#!/usr/bin/env wl
(* flux_cuda_generate.wls -- end-to-end FLUX.2-klein-4B text->image generation
   that times a COLD run (session build: weight load + JIT capture + sampler +
   VAE) and a WARM run (the cached session replays), exports both, and prints the
   provision/cold/warm breakdown.  Meant to run ON a GPU pod from the installed
   paclet's Examples/flux dir; the device is DEV (cuda|metal|cpu).

   FluxGenerate caches a per-{modelDir,dev,imgSize,nSteps} session ($fxSession),
   so the 1st call pays the cold start and the 2nd is warm -- this script makes
   that split explicit and exports cold_<i>.png + warm_<i>.png so you can confirm
   the two runs are pixel-identical (same seed) while only the timing differs.

   Env:  PROMPT (single) | BATCH (n, cycles {apple,bird}) ; IMGSIZE (px, def 512);
         SEED (def 0); DEV (def cuda); OUTDIR (def ~/.cache/thvm/flux_out).
   CUDA needs CUDA_PATH + LD_LIBRARY_PATH pointing at the WolframGPUSDK (so nvrtc
   finds cuda_bf16.h) and a generous THVM_MAX_LIVE_BYTES on a big-VRAM GPU. *)

If[ Environment["THVM_FWD_RECLAIM"] === $Failed, SetEnvironment["THVM_FWD_RECLAIM" -> "1"]];
If[ Environment["THVM_MMAP_NO_WILLNEED"] === $Failed, SetEnvironment["THVM_MMAP_NO_WILLNEED" -> "1"]];
If[ Environment["THVM_MAX_LIVE_BYTES"] === $Failed, SetEnvironment["THVM_MAX_LIVE_BYTES" -> "60000000000"]];
(* CUDA: run the sampler per-op (no cuGraph) -- a PERF choice, not a
   correctness one.  The velocity-net cuGraph replay is now bit-exact (cold ==
   warm, maxAbsDiff 0; the cross-stream hazard where the captured graph ran on
   a separate stream from its default-stream consumers was fixed by launching
   the graph on the default stream, src/backend/cuda/jit.c).  But cuGraph buys
   nothing here: the sampler allocates a fresh z/temb buffer each step, so the
   inputs' device pointers move every call and the cached graph is invalidated
   + re-captured every step instead of replayed -- no launch-overhead saving,
   plus the capture cost (512px BS=2: warm 16.4s both ways, cold 29.9s off vs
   34.0s on).  cuGraph would only pay off once the sampler reuses z/temb in
   place (stable dptr -> graph HITs).  Override with THVM_CUDA_JIT_GRAPH=1. *)
If[ Environment["THVM_CUDA_JIT_GRAPH"] === $Failed, SetEnvironment["THVM_CUDA_JIT_GRAPH" -> "0"]];

Get["WolframInstitute`THVMLink`"];
(* resolve FluxGenerate.wl from the installed paclet so this runs from anywhere
   (e.g. /tmp on a pod), not only the Examples/flux dir. *)
Get[FileNameJoin[{PacletObject["WolframInstitute/THVMLink"]["Location"], "Examples", "flux", "FluxGenerate.wl"}]];
TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);

env[k_, d_] := With[{v = Environment[k]}, If[ v === $Failed || v === None, d, v]];
dev = ToLowerCase @ env["DEV", "cuda"];
seed = ToExpression @ env["SEED", "0"];
side = ToExpression @ env["IMGSIZE", "512"];
imgSize = {side, side};
modelDir = Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b";
If[ !FileExistsQ[FileNameJoin[{modelDir, "transformer", "diffusion_pytorch_model.safetensors"}]],
    pr["SKIP: FLUX.2-klein-4B weights not found under ", modelDir]; Exit[0]];

defaultPrompts = {"a red apple on a wooden table, studio lighting",
                  "a blue bird on a branch, soft morning light"};
prompts = With[{p = Environment["PROMPT"], b = Environment["BATCH"]},
    Which[
        p =!= $Failed && p =!= None, {p},
        b =!= $Failed && b =!= None,
            With[{n = ToExpression[b]}, PadRight[defaultPrompts, n, defaultPrompts]],
        True, defaultPrompts]];

outDir = env["OUTDIR", Environment["HOME"] <> "/.cache/thvm/flux_out"];
If[ !DirectoryQ[outDir], CreateDirectory[outDir]];

pr["FLUX.2-klein-4B  dev=", dev, "  ", side, "x", side, "  batch=", Length[prompts], "  seed=", seed];
pr["prompts: ", prompts];

doRun[tag_] := Module[{t0 = AbsoluteTime[], imgs, dt},
    imgs = FluxGenerate[prompts, "ImageSize" -> imgSize, RandomSeeding -> seed,
                        "Device" -> dev];
    dt = AbsoluteTime[] - t0;
    MapIndexed[Function[{img, idx}, Module[{i = First[idx], path},
        path = FileNameJoin[{outDir, tag <> "_" <> ToString[i] <> ".png"}];
        Export[path, img];
        pr["  ", tag, " img ", i, " -> ", path]]], imgs];
    {dt, ImageData /@ imgs}];

(* COLD: first call builds + caches the session, then generates. *)
{tCold, pxCold} = doRun["cold"];
pr["COLD  = ", Round[tCold, 0.01], " s   (session build: weights + JIT capture + ",
   Length[prompts], "-image sampler + VAE)"];

(* WARM: same settings -> $fxSession hit -> replay only. *)
{tWarm, pxWarm} = doRun["warm"];
pr["WARM  = ", Round[tWarm, 0.01], " s   (cached session; ", Round[tWarm/Length[prompts], 0.01], " s/image)"];

pr["cold/warm pixel-identical: ", Max[Abs[Flatten[pxCold] - Flatten[pxWarm]]] < 1.*^-4];
pr["DONE batch=", Length[prompts], "  cold=", Round[tCold, 0.01], "s  warm=", Round[tWarm, 0.01], "s"];
