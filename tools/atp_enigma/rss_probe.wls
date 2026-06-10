(* Probe peak RSS of the minibatched big-model train on the full dataset
   (issue sw1sh/thvm#8 memory check).  Run: wl -t 200 -f rss_probe.wls
   while polling the WolframKernel RSS externally. ATP_DATA names the scratch dir. *)
PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"]; SeedRandom[7];
dataDir = Replace[Environment["ATP_DATA"], Except[_?StringQ] -> "/tmp/enigma"];
files = Join[FileNames["*.wxf", FileNameJoin[{dataDir, "ds"}]],
             FileNames["*.wxf", FileNameJoin[{dataDir, "tptp_ds"}]]];
dss = Import[#, "WXF"] & /@ files;
g = Join @@ (#["Graphs"] & /@ dss); l = Join @@ (#["Labels"] & /@ dss);
WriteString["stdout", "probe on ", Length[g], " graphs\n"];
TAtpTrainGnn[<|"Graphs" -> g, "Labels" -> l|>, "Hidden" -> 32, "Rounds" -> 3,
   MaxTrainingRounds -> 60, "BatchSize" -> 128, "NodeCap" -> 64];
WriteString["stdout", "probe done\n"];
