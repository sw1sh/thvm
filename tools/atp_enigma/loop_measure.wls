(* A/B the GNN-guided reranker on the proved AxiomaticTheory equation theorems:
   solved-within-MaxSteps.  Run: wl -t 700 -f loop_measure.wls <scorer> <rerankPeriod> <maxSteps> <out.wxf>
   period 1e9 = rerank effectively off (baseline). ATP_DATA names the scratch dir. *)
PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"];
dataDir = Replace[Environment["ATP_DATA"], Except[_?StringQ] -> "/tmp/enigma"];
{modelFile, period, budgetS, out} = $ScriptCommandLine[[-4 ;; -1]];
m = TAtpLoadGnnScorer[modelFile];
proved = Get[FileNameJoin[{dataDir, "proved.wl"}]];
budget = ToExpression[budgetS]; per = ToExpression[period];
bare[c_] := Module[{e = c /. ForAll[_, x_] :> x}, If[ListQ[e] && Length[e] == 1, First[e], e]];
eqQ[c_] := MatchQ[c, (Equal | Inactive[Equal])[_, _]];
res = Reap[
   Do[ Module[{ax = AxiomaticTheory[k], thms = AxiomaticTheory[k, "NotableTheorems"]},
       Do[ Module[{conj = bare[thms[nm]], st},
           If[ eqQ[conj],
              st = Quiet @ TimeConstrained[
                 Check[TFindProofGnnReranked[conj, ax, m, "RerankPeriod" -> per, MaxSteps -> budget], "ERR"], 18, "TO"];
              Sow[{k, nm, st}]]],
         {nm, proved[k]}]],
     {k, Keys[proved]}]][[2]];
res = If[res === {}, {}, res[[1]]];
WriteString["stdout", "SOLVED ", Count[res, {_, _, "PROVED"}], "/", Length[res],
   " @MaxSteps=", budget, " period=", period, "\n"];
Export[out, res, "WXF"];
