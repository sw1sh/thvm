#!/usr/bin/env wl
(* flux_session_bench.wls -- the persistent-session 3-call benchmark.
   Loads the paclet ONCE then calls FluxGenerate THREE times in ONE process:
     call 1: FluxGenerate[p1]        (1 image)  target <10s
     call 2: FluxGenerate[p2]        (1 image)  target <3s
     call 3: FluxGenerate[{p1, p2}]  (2 images, BATCH)  target ~3s
   Each call is wrapped in AbsoluteTiming; prints wall time + a pixel-validity
   check (min/max/mean -- must be varied, not flat ~0.495).  The session cache
   (built on call 1, reused by 2/3) is what makes 2/3 warm.

   Memory-safe: THVM_MAX_LIVE_BYTES=30e9 set here (SetEnvironment reaches
   getenv before Get). *)

If[ Environment["THVM_FWD_RECLAIM"] === $Failed, SetEnvironment["THVM_FWD_RECLAIM" -> "1"]];
If[ Environment["THVM_MMAP_NO_WILLNEED"] === $Failed, SetEnvironment["THVM_MMAP_NO_WILLNEED" -> "1"]];
If[ Environment["THVM_MAX_LIVE_BYTES"] === $Failed, SetEnvironment["THVM_MAX_LIVE_BYTES" -> "30000000000"]];
If[ Environment["THVM_FLUX_TIMING"] === $Failed, SetEnvironment["THVM_FLUX_TIMING" -> "1"]];
If[ Environment["THVM_METAL_PSO_CACHE_STATS"] === $Failed, SetEnvironment["THVM_METAL_PSO_CACHE_STATS" -> "1"]];
PacletDirectoryLoad["wl/THVMLink"]; Get["WolframInstitute`THVMLink`Examples`"];
TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);

modelDir = Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b";
If[ !FileExistsQ[FileNameJoin[{modelDir, "transformer", "diffusion_pytorch_model.safetensors"}]],
    pr["SKIP: FLUX.2-klein-4B weights not found under ", modelDir]; Exit[0]];

env[k_, d_] := With[{v = Environment[k]}, If[ v === $Failed || v === None, d, v]];
dev = ToLowerCase @ env["DEV", "metal"];
side = ToExpression @ env["IMGSIZE", "256"];
imgSize = {side, side};
p1 = "a red apple on a table";
p2 = "a blue bird on a branch";

pr["FLUX.2-klein-4B SESSION bench  dev=", dev, "  size=", side, "x", side];

stats[img_] := With[{f = Flatten @ ImageData[img]},
    {Round[Min[f], 0.0001], Round[Max[f], 0.0001], Round[Mean[f], 0.0001],
     AllTrue[f, NumberQ[#] && Abs[#] < Infinity &]}];

run[label_, prompts_] := Module[{dt, imgs, s},
    {dt, imgs} = AbsoluteTiming @ FluxGenerate[prompts,
        "ImageSize" -> imgSize, "Device" -> dev];
    s = stats /@ imgs;
    pr[label, "  ", Round[dt, 0.01], " s"];
    Do[ pr["    img ", i, "  min=", s[[i, 1]], " max=", s[[i, 2]], " mean=", s[[i, 3]], " finite=", s[[i, 4]]],
        {i, Length[s]}];
    {dt, imgs}];

{dt1, imgs1} = run["CALL 1 (1 img, cold)   ", {p1}];
{dt2, imgs2} = run["CALL 2 (1 img, warm)   ", {p2}];
{dt3, imgs3} = run["CALL 3 (2 img, batch)  ", {p1, p2}];

pr[""];
pr["SUMMARY  call1=", Round[dt1, 0.01], "s (<10)  call2=", Round[dt2, 0.01], "s (<3)  call3=", Round[dt3, 0.01], "s (~3)"];

(* batch-vs-sequential parity.  Per-item seed is base+i (i=1-based position), so
   batch[1] (p1, seed+1) must match call1 (p1, seed+1).  batch[2] is p2 at seed+2,
   while call2 was p2 at seed+1 (different noise) -- NOT comparable.  So compare
   only batch[1] vs call1 here; a clean batch==sequential check needs identical
   per-item seeds, run in flux_batch_parity.wls. *)
If[ ListQ[imgs3] && Length[imgs3] === 2 && ImageQ[First[imgs3]],
    Module[{b1 = ImageData @ First[imgs3], s1 = ImageData @ First[imgs1], e1},
        e1 = Max @ Abs[Flatten[b1] - Flatten[s1]];
        pr["PARITY  batch[1] vs seq call1(p1, seed+1) maxabs=", Round[e1, 0.0001]]]];
pr["DONE"];
