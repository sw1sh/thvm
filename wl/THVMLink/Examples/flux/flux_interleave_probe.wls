#!/usr/bin/env wl
(* Interleave probe: build the full 3-capture session manually (velJit warmup,
   then qwJit + vaeJit), then run the FULL per-call sequence TWICE for the SAME
   prompt and check the qwJit encoding + velJit latent are IDENTICAL across the
   two calls.  Reproduces the session's cross-call inconsistency in isolation. *)
If[ Environment["THVM_FWD_RECLAIM"] === $Failed, SetEnvironment["THVM_FWD_RECLAIM" -> "1"]];
If[ Environment["THVM_MMAP_NO_WILLNEED"] === $Failed, SetEnvironment["THVM_MMAP_NO_WILLNEED" -> "1"]];
If[ Environment["THVM_MAX_LIVE_BYTES"] === $Failed, SetEnvironment["THVM_MAX_LIVE_BYTES" -> "30000000000"]];
PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"];
Get[FileNameJoin[{DirectoryName[$InputFileName], "FluxGenerate.wl"}]];
TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);
md = Environment["HOME"] <> "/.cache/thvm/flux2-klein-4b";
sess = fxSessionGet["metal", {256, 256}, 4, md];
pr["session built"];
(* generate the SAME prompt+seed twice -- the persistent session must produce
   IDENTICAL images (no cross-call JIT corruption). *)
genImg[p_, s_] := First @ FluxGenerate[{p}, "ImageSize"->{256,256}, "Seed"->s, "Device"->"metal", "ReturnImages"->False];
img1 = genImg["a red apple on a table", 0];
img2 = genImg["a red apple on a table", 0];
pr["img1 mean=", Round[Mean[Flatten[img1]],0.0001], "  img2 mean=", Round[Mean[Flatten[img2]],0.0001]];
pr["img1 vs img2 (same prompt+seed) maxabs=", Round[Max[Abs[Flatten[img1]-Flatten[img2]]],0.0001]];
pr["DONE"];
