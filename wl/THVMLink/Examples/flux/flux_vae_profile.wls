#!/usr/bin/env wl
(* flux_vae_profile.wls -- isolate the VAE decode for per-kernel profiling.
   Builds ONLY the VAE jit (mirrors fxSessionGet's VAE context), decodes a fixed
   random latent, then uses TProfileDelta over ONE warm replay so every reported
   kernel is a VAE kernel.  Prints top kernels by TotalUs with output numel + an
   MSL source fingerprint (groupnorm/silu/conv/upsample/matmul).  IMGSIZE sets the
   grid (gridH=H/16). *)

If[ Environment["THVM_MAX_LIVE_BYTES"] === $Failed, SetEnvironment["THVM_MAX_LIVE_BYTES" -> "30000000000"]];
PacletDirectoryLoad["wl/THVMLink"]; Get["WolframInstitute`THVMLink`Examples`"];
TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);

env[k_, d_] := With[{v = Environment[k]}, If[ v === $Failed || v === None, d, v]];
dev = ToLowerCase @ env["DEV", "metal"];
side = ToExpression @ env["IMGSIZE", "128"];
nreps = ToExpression @ env["NREPS", "6"];
topn = ToExpression @ env["TOPN", "40"];
modelDir = Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b";
vaePath = FileNameJoin[{modelDir, "vae", "diffusion_pytorch_model.safetensors"}];
If[ !FileExistsQ[vaePath], pr["SKIP: VAE weights not found ", vaePath]; Exit[0]];

gridH = Round[side/16];  gridW = Round[side/16];  simg = gridH gridW;
vaeCfg = <|"eps" -> 1.*^-6, "epsBn" -> 1.*^-4, "gridH" -> gridH, "gridW" -> gridW|>;
pr["VAE profile  dev=", dev, "  size=", side, "x", side, "  simg=", simg, "  nreps=", nreps];

fxVaeSub = WolframInstitute`THVMLink`Examples`Private`fxVaeSub;
vaeDecoder = WolframInstitute`THVMLink`Examples`Private`vaeDecoder;
fxVaeKey = WolframInstitute`THVMLink`Examples`Private`fxVaeKey;
wdt = env["VAEDTYPE", If[ dev === "cpu", "f32", "bf16"]];   (* override weight dtype *)
pr["  wdt=", wdt];
loader[vt_] := Module[{cache = <||>}, n |-> Lookup[cache, n,
    cache[n] = TRealize @ TToDevice[TUOpCast[vt[fxVaeKey[n]], wdt], dev]]];
ctx = TContextNew[dev];
vae = TInContext[ctx, Module[{vt, vKeys, wfV, wsubV},
    vt = TSafeTensorLoad[vaePath];  vKeys = Keys[vt];
    wfV = loader[vt];  wsubV = fxVaeSub[wfV, vKeys];
    TJit[Function[lat, TRealize @ vaeDecoder[lat, wfV, wsubV, vaeCfg]]]]];

decode[lat_] := TInContext[ctx, With[{up = TToDevice[TTensorCreate @ NumericArray[lat, "Real32"], dev]},
    vae[TRealize @ If[ wdt === "f32", up, TUOpCast[up, wdt]]]]];

SeedRandom[7];
lat = RandomVariate[NormalDistribution[], {simg, 128}];

{tc, img} = AbsoluteTiming[TRealize @ decode[lat]];
pr["  cold (capture) ", Round[tc, 0.001], " s"];
ts = Table[First @ AbsoluteTiming[TRealize @ decode[lat]], {nreps}];
pr["  warm replays  ", Round[ts, 0.001]];
pr["  warm min=", Round[Min[ts], 0.001], "  median=", Round[Median[ts], 0.001]];

(* GPU exec time vs wall: TMetalGpuTime accumulates the batched-flush GPU span,
   so (wall - gpu_delta) is the CPU encode/replay-loop cost. *)
If[ dev =!= "cpu",
    Module[{g0, g1, wall},
        g0 = TInContext[ctx, TMetalGpuTime[]];
        wall = First @ AbsoluteTiming[TRealize @ decode[lat]];
        g1 = TInContext[ctx, TMetalGpuTime[]];
        pr["  one replay: wall=", Round[wall*1000, 0.1], " ms  gpu=",
           Round[(g1["TotalUs"] - g0["TotalUs"])/1000., 0.1], " ms  cpu(encode)=",
           Round[wall*1000 - (g1["TotalUs"] - g0["TotalUs"])/1000., 0.1], " ms  flushes=",
           g1["FlushCount"] - g0["FlushCount"]]]];

(* profile delta over ONE more warm replay (the snapshot pair brackets a single
   decode, so DispatchCount delta == 1 per kernel = the VAE kernels). *)
before = TInContext[ctx, TProfileAll[]];
TInContext[ctx, TRealize @ decode[lat]];
after = TInContext[ctx, TProfileAll[]];
delta = TProfileDelta[before, after];

(* category: classify the kernel by its MSL structure. *)
fingerprint[kid_] := TInContext[ctx, Module[{src = TKernelSource[kid, "Metal"]},
    Which[
        src === "" || src === Missing["NotLowered"], "<no-msl>",
        StringContainsQ[src, "1.0f/sqrt"] || StringContainsQ[src, "rsqrt"], "gn-rstd",
        StringContainsQ[src, "* (int)(9))"] || StringContainsQ[src, "a8 = 0; a8 < 3"], "conv3x3",
        StringContainsQ[src, "/*reduce*/"] && StringContainsQ[src, "for ("], "reduce/matmul",
        StringContainsQ[src, "exp"], "softmax/silu",
        True, "elementwise"]]];

(* TMetalPerOpProfile gives TRUE GPU span (GPUEndTime-GPUStartTime), NOT inflated
   by the per-op commit+wait floor -- the honest per-kernel ranking under PEROP. *)
gpuProf = TInContext[ctx, TMetalPerOpProfile[]];
rows = TInContext[ctx, KeyValueMap[Function[{kid, p},
    <|"kid" -> kid, "us" -> Lookup[gpuProf, kid, <|"GpuUs" -> 0|>]["GpuUs"],
      "fires" -> p["DispatchCount"],
      "numel" -> TKernelInfo[kid]["output_numel"], "route" -> p["DispatchKind"],
      "fp" -> fingerprint[kid]|>], delta]];
rows = ReverseSortBy[rows, #us &];
totalUs = Total[#us & /@ rows];
pr["  delta kernels=", Length[rows], "  sum_us=", totalUs, "  (", Round[totalUs/1000., 0.1], " ms)"];
(* category summary: count + total us per kernel class. *)
byCat = GroupBy[rows, #fp &];
pr["  --- by category (us, count) ---"];
Do[ With[{cat = c, rs = byCat[c]},
    pr["    ", StringPadRight[cat, 16], " us=", StringPadRight[ToString[Total[#us & /@ rs]], 9],
       " n=", Length[rs]]],
    {c, ReverseSortBy[Keys[byCat], Total[#us & /@ byCat[#]] &]}];
pr["  --- top kernels ---"];
pr[StringPadRight["kid", 7], StringPadRight["us", 10], StringPadRight["fires", 7],
   StringPadRight["numel", 11], StringPadRight["route", 12], "fingerprint"];
Do[ With[{r = rows[[i]]},
    pr[StringPadRight[ToString[r["kid"]], 7], StringPadRight[ToString[r["us"]], 10],
       StringPadRight[ToString[r["fires"]], 7], StringPadRight[ToString[r["numel"]], 11],
       StringPadRight[ToString[r["route"]], 12], r["fp"]]],
    {i, Min[topn, Length[rows]]}];
pr["DONE"];
