(* Build the GNN graph dataset for one AxiomaticTheory's proved NotableTheorems.
   Run: wl -t 240 -f prove_theory.wls <Theory> <scorer|NONE> <coopRatio> <out.wxf>
   ATP_DATA env names the scratch dir holding proved.wl (default /tmp/enigma). *)
PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"];
dataDir = Replace[Environment["ATP_DATA"], Except[_?StringQ] -> "/tmp/enigma"];
{th, scorer, ratio, out} = $ScriptCommandLine[[-4 ;; -1]];
If[ scorer =!= "NONE",
   SetEnvironment["THVM_ATP_GNN_RERANK_PERIOD" -> "200"];
   SetEnvironment["THVM_ATP_GNN_COOP_RATIO" -> ratio];
   TAtpSetGnnScorer[TAtpLoadGnnScorer[scorer]]];
names = Lookup[Get[FileNameJoin[{dataDir, "proved.wl"}]], th, {}];
conj = Flatten[Values @ KeyTake[AxiomaticTheory[th, "NotableTheorems"], names]];
ax = AxiomaticTheory[th];
If[ conj === {} || Head[ax] =!= List, WriteString["stdout", th, ": SKIP\n"]; Exit[0]];
d = TAtpGraphDataset[conj, ax, TimeConstraint -> 6, MaxSteps -> 20000];
Export[out, <|"Graphs" -> d["Graphs"], "Labels" -> d["Labels"],
   "Meta" -> <|"Source" -> "AxiomaticTheory", "Theory" -> th, "NProofs" -> d["NProofs"]|>|>, "WXF"];
WriteString["stdout", th, ": ", d["NProofs"], " solved, ", Length[d["Graphs"]], " graphs\n"];
