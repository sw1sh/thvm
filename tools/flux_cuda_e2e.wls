#!/usr/bin/env wolframscript
(* flux_cuda_e2e.wls -- FLUX.2-klein-4B text->image generation end to end on a
   fresh PrimeIntellect H100, ENTIRELY through the serviceconnection: no ssh, no
   scp, no prime CLI.  Everything is RemoteEvaluate.

     provision        ServiceExecute[so,"LaunchWolframPod"]          (waits FINISHED)
     weights+paclet   RemoteEvaluate[conf, RunProcess[...]+PacletInstall]   (on the pod)
     generate         RemoteEvaluate[conf, FluxGenerate[...]]        (cold then warm)
     get images       the pixel arrays come back over WSTP -> Export locally
     teardown         ServiceExecute[so,"TerminatePod"]

   The cold run is the session build (weight load + JIT capture + 4-step sampler +
   VAE); the warm run hits the cached $fxSession and just replays -- so a batch
   pays one cold start and one warm sampler per image.  cold_<i>.png / warm_<i>.png
   land in ./flux_out_cuda; they are pixel-identical (same seed) -- only the timing
   differs.

   Run in a CLOUD-CONNECTED kernel ($CloudConnected===True).  Env:
     PROMPT (single) | BATCH (n, def 2) ; IMGSIZE (px, def 512) ; SEED (def 0).
   The published paclet already carries the CUDA bf16 + WMMA tensor-core path. *)

If[ !TrueQ[$CloudConnected], Quiet@CloudConnect[]];
If[ !TrueQ[$CloudConnected],
    Print["ERROR: not cloud-connected. Run CloudConnect[] first (the entitlement + LaunchWolframPod need it)."]; Exit[1]];

pacletRepo = "/Users/swish/src/wolfram/PrimeIntellect";
PacletDirectoryLoad[pacletRepo];
so = ServiceConnect["PrimeIntellect"];
If[ !MatchQ[so, _ServiceObject], Print["ERROR: ServiceConnect[\"PrimeIntellect\"] failed: ", so]; Exit[1]];

env[k_, d_] := With[{v = Environment[k]}, If[ v === $Failed || v === None, d, v]];
batch  = ToExpression @ env["BATCH", "2"];
side   = ToExpression @ env["IMGSIZE", "512"];
seed   = ToExpression @ env["SEED", "0"];
imgSize = {side, side};
hfRepo = env["FLUX_HF_REPO", "black-forest-labs/FLUX.2-klein-4B"];   (* apache-2.0, no token *)
pacletUrl = "https://www.wolframcloud.com/obj/nikm/THVMLink-cuda.paclet";
template = env["WOLFRAM_POD_TEMPLATE", "cmqct4j5z003fvxxj2e2gre02"];
defaultPrompts = {"a red apple on a wooden table, studio lighting",
                  "a blue bird on a branch, soft morning light"};
prompts = With[{p = Environment["PROMPT"]},
    If[ p =!= $Failed && p =!= None, {p}, PadRight[defaultPrompts, batch, defaultPrompts]]];
pr[a___] := (Print[a]; $Output // Flush);

(* ---- provision ---------------------------------------------------------- *)
pr["provisioning H100 (LaunchWolframPod, waits for installationStatus=FINISHED)..."];
{tProv, pid} = AbsoluteTiming @ ServiceExecute[so, "LaunchWolframPod",
    {"GPUType" -> "H100 80GB", "Name" -> "flux-e2e", "CustomTemplateID" -> template,
     "DiskGB" -> 256, "Wait" -> True, "MaxWaitMinutes" -> 14}];
If[ !StringQ[pid], Print["ERROR: LaunchWolframPod -> ", pid]; Exit[1]];
pr["  provisioned ", pid, " in ", Round[tProv, 0.1], " s"];
terminate[] := Quiet @ ServiceExecute[so, "TerminatePod", {"PodID" -> pid}];

result = Catch[
  ent = CreateLicenseEntitlement[<|"StandardKernelLimit" -> 4,
     "LicenseExpiration" -> Quantity[12, "Hours"],
     "EntitlementExpiration" -> Quantity[7, "Days"]|>][[1]];
  conf = ServiceExecute[so, "PodKernelConfiguration", {"PodID" -> pid, "Entitlement" -> ent}];
  If[ !MatchQ[conf, _KernelConfiguration], Throw["PodKernelConfiguration -> " <> ToString[conf]]];

  (* ---- one-time pod setup (weights + nvrtc libs + paclet), all RemoteEvaluate --- *)
  pr["setup on pod: weights (", hfRepo, ") + nvrtc + paclet ..."];
  With[{repo = hfRepo, url = pacletUrl},
    RemoteEvaluate[conf, Module[{gs, mk = "/root/.cache/thvm/flux2-klein-4b"},
       gs = First @ FileNames["targets/x86_64-linux",
              "/root/.Wolfram/Paclets/Repository/WolframGPUSDK-LINUX-*/CUDAResources/Linux-x86-64", Infinity];
       If[ !FileExistsQ[mk <> "/transformer/diffusion_pytorch_model.safetensors"],
           RunProcess[{"bash", "-c", "pip install -q -U 'huggingface_hub[hf_transfer]' >/dev/null 2>&1; "<>
             "HF_HUB_ENABLE_HF_TRANSFER=1 huggingface-cli download "<>repo<>" --local-dir "<>mk<>" 2>&1 | tail -2"}]];
       (* nvrtc dlopens libnvrtc-builtins from the system path -> stage it into the ld cache *)
       RunProcess[{"bash", "-c", "cp "<>gs<>"/lib/libnvrtc* /usr/local/lib/ 2>/dev/null; ldconfig"}];
       PacletInstall[url, ForceVersionInstall -> True];
       "setup-done"]]];

  (* ---- cold + warm in ONE eval so $fxSession persists; pixels come back via WSTP --- *)
  pr["generating: batch=", Length[prompts], "  ", side, "x", side, "  cold then warm ..."];
  res = With[{ps = prompts, sz = imgSize, sd = seed},
    RemoteEvaluate[conf, Module[{gs, loc, gen},
       gs = First @ FileNames["targets/x86_64-linux",
              "/root/.Wolfram/Paclets/Repository/WolframGPUSDK-LINUX-*/CUDAResources/Linux-x86-64", Infinity];
       SetEnvironment["CUDA_PATH" -> gs];
       SetEnvironment["THVM_FWD_RECLAIM" -> "1"];
       SetEnvironment["THVM_MMAP_NO_WILLNEED" -> "1"];
       SetEnvironment["THVM_MAX_LIVE_BYTES" -> "70000000000"];
       loc = PacletObject["WolframInstitute/THVMLink"]["Location"];
       Get["WolframInstitute`THVMLink`"];
       Get[FileNameJoin[{loc, "Examples", "flux", "FluxGenerate.wl"}]];
       (* ToExpression after the Gets so FluxGenerate resolves on the pod *)
       gen = ToExpression["Function[{p,z,s}, AbsoluteTiming@FluxGenerate[p,\"ImageSize\"->z,\"Seed\"->s,\"Device\"->\"cuda\",\"ReturnImages\"->False]]"];
       With[{c = gen[ps, sz, sd], w = gen[ps, sz, sd]},
         {c[[1]], w[[1]], c[[2]], w[[2]]}]]]];
  If[ !MatchQ[res, {_?NumericQ, _?NumericQ, _List, _List}], Throw["generation -> " <> ToString[Short[res]]]];
  res,
  _, (terminate[]; pr["pod terminated (after error)"]; #1) &];

If[ StringQ[result], Print["ERROR: ", result]; terminate[]; Exit[1]];
{tCold, tWarm, pxCold, pxWarm} = result;

(* ---- export the images locally + report -------------------------------- *)
outDir = FileNameJoin[{Directory[], "flux_out_cuda"}];
If[ !DirectoryQ[outDir], CreateDirectory[outDir]];
saveImgs[px_, tag_] := MapIndexed[Function[{p, idx},
    With[{path = FileNameJoin[{outDir, tag <> "_" <> ToString[First[idx]] <> ".png"}]},
      Export[path, Image[Clip[p, {0., 1.}], Interleaving -> False, ColorSpace -> "RGB"]];
      pr["  ", path]]], px];
saveImgs[pxCold, "cold"]; saveImgs[pxWarm, "warm"];

pr["cold/warm pixel-identical: ", Max[Abs[Flatten[pxCold] - Flatten[pxWarm]]] < 1.*^-4];
pr[""];
pr["==== FLUX.2-klein-4B on H100, batch=", Length[prompts], " ", side, "x", side, " ===="];
pr["  PROVISION = ", Round[tProv, 0.1], " s"];
pr["  COLD      = ", Round[tCold, 0.01], " s   (session build: weights + JIT capture + sampler + VAE)"];
pr["  WARM      = ", Round[tWarm, 0.01], " s   (", Round[tWarm/Length[prompts], 0.01], " s/image, cached session)"];
terminate[];
pr["pod terminated."];
