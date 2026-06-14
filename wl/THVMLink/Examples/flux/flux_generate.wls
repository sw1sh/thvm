#!/usr/bin/env wl
(* flux_generate.wls -- runnable end-to-end FLUX.2-klein-4B text->image
   generator.  Tokenizes the prompt(s), runs the Qwen3-4B text encoder, the
   MMDiT velocity net (4-step Euler flow-match, TJit-captured ONCE and shared
   across the batch), and the AutoencoderKLFlux2 decoder, then Exports each
   image to ~/.cache/thvm/flux_out/img_<i>.png.

   The transformer/VAE/encoder weights + the RoPE table + the JIT-captured
   velocity net are built ONCE; a 2-image batch therefore pays one cold start
   and one warm sampler run per image (NOT two cold starts).

   Env overrides:
     PROMPT   single prompt (otherwise the default {apple, bird} batch is used)
     BATCH    number of prompts (truncates/cycles the default list)
     IMGSIZE  square side in px, e.g. "512" (default 256)
     SEED     base seed (default 0)
     DEV      device, "metal" (default) or "cpu"

   Memory-safe run (the user is strict; never exceed 30e9 live bytes):
     THVM_MAX_LIVE_BYTES=30000000000 wl -f flux_generate.wls
   Needs the FLUX.2-klein-4B weights under ~/.cache/thvm/flux2-klein-4b/. *)

PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"];
Get[FileNameJoin[{DirectoryName[$InputFileName], "FluxGenerate.wl"}]];
TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);

modelDir = Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b";
If[ !FileExistsQ[FileNameJoin[{modelDir, "transformer", "diffusion_pytorch_model.safetensors"}]],
    pr["SKIP: FLUX.2-klein-4B weights not found under ", modelDir]; Exit[0]];

env[k_, d_] := With[{v = Environment[k]}, If[ v === $Failed || v === None, d, v]];
dev = ToLowerCase @ env["DEV", "metal"];
seed = ToExpression @ env["SEED", "0"];
side = ToExpression @ env["IMGSIZE", "256"];
imgSize = {side, side};

defaultPrompts = {"a red apple on a table", "a blue bird"};
prompts = With[{p = Environment["PROMPT"], b = Environment["BATCH"]},
    Which[
        p =!= $Failed && p =!= None, {p},
        b =!= $Failed && b =!= None,
            With[{n = ToExpression[b]}, PadRight[defaultPrompts, n, defaultPrompts]],
        True, defaultPrompts]];

outDir = Environment["HOME"] <> "/.cache/thvm/flux_out";
If[ !DirectoryQ[outDir], CreateDirectory[outDir]];

pr["FLUX.2-klein-4B generate  dev=", dev, "  size=", side, "x", side,
   "  batch=", Length[prompts], "  seed=", seed];
pr["prompts: ", prompts];

tStart = AbsoluteTime[];
imgs = FluxGenerate[prompts,
    "ImageSize" -> imgSize, "Seed" -> seed, "Device" -> dev, "ReturnImages" -> False];
tTotal = AbsoluteTime[] - tStart;

(* imgs is a list of {3,H,W} pixel arrays in [0,1].  Export + stats per item. *)
MapIndexed[
    Function[{px, idx},
        Module[{i = First[idx], flat, img, path},
            flat = Flatten[px];
            img = Image[Clip[px, {0., 1.}], Interleaving -> False, ColorSpace -> "RGB"];
            path = FileNameJoin[{outDir, "img_" <> ToString[i] <> ".png"}];
            Export[path, img];
            pr["  img ", i, "  ", prompts[[i]]];
            pr["    -> ", path, "   ", Dimensions[px][[2]], "x", Dimensions[px][[3]]];
            pr["    pixel  min=", Round[Min[flat], 0.0001], "  max=", Round[Max[flat], 0.0001],
               "  mean=", Round[Mean[flat], 0.0001], "  finite=", AllTrue[flat, NumberQ[#] && Abs[#] < Infinity &]]]],
    imgs];

pr["TOTAL ", Round[tTotal, 1], " s for ", Length[prompts], " image(s)  (",
   Round[tTotal/Length[prompts], 1], " s/image avg incl. one cold capture)"];
pr["DONE"];
