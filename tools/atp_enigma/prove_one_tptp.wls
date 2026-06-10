(* Prove one TPTP UEQ problem; emit its proof-essential lemmas as a labelled
   GNN graph dataset (positives only).  Run: wl -t 32 -f prove_one_tptp.wls <problem.p> <out.wxf>
   TPTP_ROOT env points at the TPTP install root so include('Axioms/..') resolves. *)
If[ StringQ[Environment["TPTP_ROOT"]], SetEnvironment["TPTP" -> Environment["TPTP_ROOT"]]];
PacletDirectoryLoad["wl/THVMLink"]; Get["THVMLink`"];
{p, out} = $ScriptCommandLine[[-2 ;; -1]];
id = FileBaseName[p];
dom = StringCases[p, "Problems/" ~~ d : (LetterCharacter ..) ~~ "/" :> d] /. {x_} :> x /. {} -> "?";
po = TimeConstrained[TFindProof[File[p], Method -> "Waldmeister", TimeConstraint -> 26], 28, $Failed];
If[ Head[po] =!= ProofObject, WriteString["stdout", id, ": no-proof\n"]; Exit[0]];
ds = TAtpGraphDataset[po];
If[ ! AssociationQ[ds] || ds["Graphs"] === {}, WriteString["stdout", id, ": empty\n"]; Exit[0]];
plen = Quiet @ Check[po["ProofLength"], Missing[]];
Export[out, <|"Graphs" -> ds["Graphs"], "Labels" -> ds["Labels"],
   "Meta" -> <|"Source" -> "TPTP", "Problem" -> id, "Domain" -> dom, "ProofLength" -> plen|>|>, "WXF"];
WriteString["stdout", id, ": ", Length[ds["Graphs"]], " graphs (pos), plen=", plen, "\n"];
