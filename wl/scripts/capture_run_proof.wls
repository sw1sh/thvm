(* Capture the arguments TFindProof[thm, theory] sends to thvm_wl_atp_run_proof on its first
   schedule entry, without running the proof: the run function is replaced by one that records
   its arguments and throws.  The wire's heap Terms are written in prefix form, so
   bin/atp_replay (wl/THVMLink/CSource/replay.c) can rebuild them in another process.

     CAP_OUT=case.txt CAP_THM=OrAssociativity CAP_THEORY=WolframAxioms \
       wolframscript -f wl/scripts/capture_run_proof.wls *)
PacletDirectoryLoad[FileNameJoin[{DirectoryName[$InputFileName], ".."}]];
Needs["WolframInstitute`THVMLink`"];
out = Environment["CAP_OUT"];
thm = Replace[Environment["CAP_THM"], $Failed -> "OrAssociativity"];
theory = Replace[Environment["CAP_THEORY"], $Failed -> "WolframAxioms"];
toPrefix = WolframInstitute`THVMLink`Private`load["thvm_wl_atp_terms_to_prefix",
    {{"NumericArray", "Constant"}}, "NumericArray"];
WolframInstitute`THVMLink`ATP`Private`$atpRunProofFn = Function[Null, Throw[{##}, "capture"]];
args = Catch[TFindProof[thm, theory], "capture"];
If[ ! ListQ[args], Print["no capture: ", Short[args]]; Exit[1]];
Print["captured ", Length[args], " arguments"];
w = Normal[args[[1]]];
{nG, nAx} = Take[w, 2];
terms = w[[3 ;; 2 + 2 nAx + 2 nG]];
flags = w[[3 + 2 nAx + 2 nG ;;]];
pre = Normal[toPrefix[NumericArray[terms, "Integer64"]]];
tok[l_List] := StringRiffle[Join[{"A", ToString[Length[l]]}, ToString /@ l], " "];
tok[na_NumericArray] := tok[Normal[na]];
tok[r_Real] := ToString[NumberForm[r, {16, 6}]];
tok[x_Integer] := ToString[x];
tok[x_] := (Print["unexpected argument ", Short[x]]; Exit[1]);
Export[out, StringRiffle[{
    "case " <> thm,
    "args " <> StringRiffle[tok /@ ReplacePart[args, 1 -> 0], " "],
    "wire " <> StringRiffle[ToString /@ Join[{nG, nAx, Length[flags]}, flags], " "],
    "terms " <> ToString[Length[pre]] <> " " <> StringRiffle[ToString /@ pre, " "],
    "end"}, "\n"], "Text"];
Print["argument heads: ", Head /@ args];
Print["steps ", args[[2]], " maxlab ", args[[3]], " wall ", args[[4]]];
Exit[0]
