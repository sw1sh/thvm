(* Held-out AUC: split the corpus by FILE (= by problem, no leakage), train,
   score the unseen test graphs with the trained model, report train + TEST AUC.
   Run: wl -t 500 -f eval_heldout.wls <hidden> <rounds> [maxTrainRounds] [seed] *)
PacletDirectoryLoad["wl/THVMLink"]; Get["WolframInstitute`THVMLink`"];
P = "WolframInstitute`THVMLink`ATP`Private`";
fwd = Symbol[P <> "atpGnnForwardLogits"]; tens = Symbol[P <> "atpGnnTensors"]; auc = Symbol[P <> "atpScorerAuc"];
dataDir = Replace[Environment["ATP_DATA"], Except[_?StringQ] -> "/tmp/enigma"];
argv = $ScriptCommandLine;
hH = ToExpression[argv[[2]]]; rR = ToExpression[argv[[3]]];
mtr = If[Length[argv] >= 4, ToExpression[argv[[4]]], 300];
seed = If[Length[argv] >= 5, ToExpression[argv[[5]]], 11];
SeedRandom[seed];
files = RandomSample @ Join[FileNames["*.wxf", FileNameJoin[{dataDir, "ds"}]],
                            FileNames["*.wxf", FileNameJoin[{dataDir, "tptp_ds"}]]];
nTest = Ceiling[Length[files] * 0.2];
testF = Take[files, nTest]; trainF = Drop[files, nTest];
grab[fs_] := Module[{d = Import[#, "WXF"] & /@ fs},
   {Join @@ (#["Graphs"] & /@ d), Join @@ (#["Labels"] & /@ d)}];
{trG, trL} = grab[trainF]; {teG, teL} = grab[testF];
WriteString["stdout", "split seed=", seed, ": ", Length[trainF], " train files (", Length[trG],
   " graphs), ", nTest, " test files (", Length[teG], " graphs)\n"];
r = TAtpTrainGnn[<|"Graphs" -> trG, "Labels" -> trL|>,
      "Hidden" -> hH, "Rounds" -> rR, MaxTrainingRounds -> mtr, "BatchSize" -> 128, "NodeCap" -> 64];
m = r["Model"];
bt = tens[<|"Graphs" -> teG, "Labels" -> teL|>, 64];
tt[a_] := TTensorCreate[a];
logits = fwd[tt[bt["X"]], tt[bt["A"]],
   tt[ArrayReshape[bt["Mask"], {bt["B"], 1, bt["N"]}]],
   tt[ArrayReshape[bt["NNodes"], {bt["B"], 1, 1}]],
   tt /@ m["W1"], tt /@ m["Ws"], tt /@ m["Bh"], tt[m["Wout"]], tt[m["Bout"]],
   bt["B"], bt["N"], 6, hH, rR];
pr = Normal @ TRealize @ logits;
teAUC = auc[pr[[All, 2]] - pr[[All, 1]], teL];
WriteString["stdout", "ARCH h=", hH, " r=", rR, " mtr=", mtr,
   " seed=", seed, ": trainAUC=", r["TrainAUC"], "  TEST_AUC=", teAUC, "\n"];
