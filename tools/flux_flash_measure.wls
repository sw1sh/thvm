(* flux_flash_measure.wls -- measure DiT attention score-buffer footprint at a
   real (tiny) FLUX generation.  Reports per-buffer >30MB table + TMetalBufSummary
   + ImageQ/std for correctness.  SAFE: single small gen, TimeConstrained.

     IS=128 wolframscript -f tools/flux_flash_measure.wls
   Env: IS (image size, default 128), TWO (1 = two prompts for distinctness check).
*)
PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`Examples`"];
TInit[]; TReset[];
pr[a___] := (WriteString["stdout", a, "\n"]; $Output // Flush);

isz = ToExpression[Environment["IS"] /. (x_ /; (x === $Failed || x === None) -> "128")];
two = (Environment["TWO"] === "1");

pr["=== FLUX flash measure: ImageSize ", isz, "  PCONTIG=", Environment["THVM_PCONTIG"], " ==="];

big30 = sum = peak = retained = Null;
imgs = Null;
res = TimeConstrained[
  If[ two,
    imgs = FluxGenerate[{"a cat", "a dog"}, "ImageSize" -> {isz, isz}, RandomSeeding -> 1],
    imgs = {FluxGenerate["a cat", "ImageSize" -> {isz, isz}, RandomSeeding -> 1]}];
  tbl = TMetalBufTableEx[];
  summ = TMetalBufSummary[];
  {imgs, tbl, summ},
  ToExpression[Environment["TC"] /. (x_ /; (x === $Failed || x === None) -> "600")], $TimedOut];

If[ res === $TimedOut, pr["TIMED OUT"]; Quit[1]];
{imgs, tbl, summ} = res;

(* per-buffer table rows: {nbytes, refcount, borrowed, jit_pinned, owns_data, preserved} *)
thr = ToExpression[Environment["THR"] /. (x_ /; (x === $Failed || x === None) -> "30")] * 1*^6;
nonBorrowed = Select[tbl, (#[[3]] == 0 && #[[1]] > thr) &];
big30 = Reverse@Sort[nonBorrowed[[All, 1]]];

pr["--- Correctness ---"];
Do[ With[{im = imgs[[i]]},
    pr["  img[", i, "] ImageQ=", ImageQ[im], "  std=",
       If[ ImageQ[im], N@StandardDeviation@Flatten@ImageData[im], "N/A"]]],
  {i, Length[imgs]}];
If[ two && Length[imgs] == 2 && ImageQ[imgs[[1]]] && ImageQ[imgs[[2]]],
  pr["  maxdiff(cat,dog)=", N@Max@Abs[ImageData[imgs[[1]]] - ImageData[imgs[[2]]]]]];

pr["--- Footprint ---"];
pr["  RetainedBytes/1e9 = ", N[summ["RetainedBytes"]/1*^9]];
pr["  LiveBytes/1e9     = ", N[summ["LiveBytes"]/1*^9]];
pr["  PeakLiveBytes/1e9 = ", N[summ["PeakLiveBytes"]/1*^9]];
pr["  non-borrowed buffers >30MB (count=", Length[big30], "), MB each:"];
pr["    ", N[Take[big30, UpTo[20]]/1*^6]];
pr["  total >30MB non-borrowed bytes/1e9 = ", N[Total[big30]/1*^9]];
pr["--- Buffer-size histogram (non-borrowed >5MB, MB -> count) ---"];
pr["  ", Sort@Counts[Round[N[Select[tbl, (#[[3]] == 0 && #[[1]] > 5*^6) &][[All, 1]]/1*^6], 0.1]]];
