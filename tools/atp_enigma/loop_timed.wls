(* Timed A/B (wall + solved) of GNN rerank on a fixed equation-theorem set.
   Run: wl -t 200 -f loop_timed.wls <rerankPeriod> <maxSteps>
   Reads <ATP_DATA>/corpus_big.safetensors. period 1e9 = rerank off. *)
PacletDirectoryLoad["wl/THVMLink"]; Get["WolframInstitute`THVMLink`"];
dataDir = Replace[Environment["ATP_DATA"], Except[_?StringQ] -> "/tmp/enigma"];
{period, ms} = $ScriptCommandLine[[-2 ;; -1]];
m = TAtpLoadGnnScorer[FileNameJoin[{dataDir, "corpus_big.safetensors"}]]; per = ToExpression[period];
bare[c_] := Module[{e = c /. ForAll[_, x_] :> x}, If[ListQ[e] && Length[e] == 1, First[e], e]];
eqQ[c_] := MatchQ[c, (Equal | Inactive[Equal])[_, _]];
set = {"GroupAxioms", "BooleanAxioms", "RingAxioms"};
out = AbsoluteTiming @ Reap[
   Do[ Module[{ax = AxiomaticTheory[k], thms = AxiomaticTheory[k, "NotableTheorems"]},
       Do[ Module[{conj = bare[thms[nm]]},
           If[ eqQ[conj], Sow[TFindProofGnnReranked[conj, ax, m, "RerankPeriod" -> per, MaxSteps -> ToExpression[ms]]]]],
         {nm, Keys[thms]}]], {k, set}]];
tt = out[[1]]; reap = out[[2]];
res = If[reap[[2]] === {}, {}, reap[[2, 1]]];
WriteString["stdout", "period=", period, " ms=" <> ms <> " SOLVED ", Count[res, "PROVED"], "/", Length[res],
   " in ", Round[tt, 0.1], "s (", Round[tt/Max[Length[res], 1], 0.01], "s/thm)\n"];
