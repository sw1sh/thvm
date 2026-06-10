(* Merge the AxiomaticTheory (ds/) + TPTP (tptp_ds/) graph datasets, balance,
   and train a small GNN scorer.  Run: wl -t 300 -f merge_train.wls <out.safetensors>
   ATP_DATA env names the scratch dir (default /tmp/enigma). *)
PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"]; SeedRandom[7];
dataDir = Replace[Environment["ATP_DATA"], Except[_?StringQ] -> "/tmp/enigma"];
out = Last[$ScriptCommandLine];
files = Join[FileNames["*.wxf", FileNameJoin[{dataDir, "ds"}]],
             FileNames["*.wxf", FileNameJoin[{dataDir, "tptp_ds"}]]];
dss = Import[#, "WXF"] & /@ files;
g = Join @@ (#["Graphs"] & /@ dss); l = Join @@ (#["Labels"] & /@ dss);
ok = Pick[Range[Length[g]], (#["NNodes"] <= 64 &) /@ g]; g = g[[ok]]; l = l[[ok]];
posI = Pick[Range[Length[l]], # == 1 & /@ l]; negI = Pick[Range[Length[l]], # == 0 & /@ l];
sel = Join[RandomSample[posI, Min[800, Length[posI]]], RandomSample[negI, Min[800, Length[negI]]]];
WriteString["stdout", "MERGED: ", Length[files], " files, ", Length[g], " graphs (",
   Length[posI], " pos / ", Length[negI], " neg) -> balanced ", Length[sel], "\n"];
r = TAtpTrainGnn[<|"Graphs" -> g[[sel]], "Labels" -> l[[sel]]|>, "Hidden" -> 16, "Rounds" -> 2, MaxTrainingRounds -> 150];
WriteString["stdout", "TRAIN: AUC=", r["TrainAUC"], " loss ", r["LossStart"], " -> ", r["LossEnd"], "\n"];
TAtpSaveGnnScorer[r["Model"], out]; WriteString["stdout", "SAVED ", out, "\n"];
