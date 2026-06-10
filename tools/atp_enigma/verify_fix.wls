(* Train the bigger GNN on the FULL merged dataset (no subsample) -- the
   minibatched, memory-bounded config (issue sw1sh/thvm#8).  Run:
     wl -t 450 -f verify_fix.wls
   Saves <ATP_DATA>/corpus_big.safetensors. *)
PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"]; SeedRandom[7];
dataDir = Replace[Environment["ATP_DATA"], Except[_?StringQ] -> "/tmp/enigma"];
files = Join[FileNames["*.wxf", FileNameJoin[{dataDir, "ds"}]],
             FileNames["*.wxf", FileNameJoin[{dataDir, "tptp_ds"}]]];
dss = Import[#, "WXF"] & /@ files;
g = Join @@ (#["Graphs"] & /@ dss); l = Join @@ (#["Labels"] & /@ dss);
WriteString["stdout", "FULL DATASET (no subsample): ", Length[g], " graphs, ",
   Count[l, 1], " pos / ", Count[l, 0], " neg, maxNodes=", Max[#["NNodes"] & /@ g], "\n"];
r = TAtpTrainGnn[<|"Graphs" -> g, "Labels" -> l|>,
      "Hidden" -> 32, "Rounds" -> 3, MaxTrainingRounds -> 300, "BatchSize" -> 128, "NodeCap" -> 64];
WriteString["stdout", "BIG-MODEL TRAIN COMPLETED: AUC=", r["TrainAUC"],
   " loss ", r["LossStart"], " -> ", r["LossEnd"], " NPos=", r["NPos"], " NNeg=", r["NNeg"], "\n"];
TAtpSaveGnnScorer[r["Model"], FileNameJoin[{dataDir, "corpus_big.safetensors"}]];
WriteString["stdout", "SAVED corpus_big.safetensors\n"];
