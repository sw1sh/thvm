#!/usr/bin/env wolframscript
(* gen_casc_sample.wls -- generate the CASC-J13 UEQ sample solution.

   Proves BOO001-1 (ternary Boolean algebra: inverse is an involution) with
   TFindProof and renders the resulting ProofObject as a TPTP CNFRefutation,
   writing samples/BOO001-1.FindProof.sol.

   thvm has no ProofObject -> TPTP exporter (only the import side,
   TSZSDerivationToProofObject), so proofObjectToTPTP below walks the
   ProofObject's ProofDataset and emits one cnf(...) line per step: input
   equations as `axiom`, the goal negated as `negated_conjecture`, each derived
   critical-pair / substitution lemma as a `plain` step with an inference()
   citing its parents, and a final `$false` contradiction. Each derived step is
   a genuine status(thm) consequence of its cited parents (thvm's ProofFunction
   verifies the underlying proof), so the derivation is GDV-checkable.

       wolframscript -f gen_casc_sample.wls
*)

scriptDir = DirectoryName[$InputFileName]
repoRoot = FileNameDrop[scriptDir, -3]
PacletDirectoryLoad[FileNameJoin[{repoRoot, "wl", "THVMLink"}]]
Get["WolframInstitute`THVMLink`"]

(* ---- ProofObject -> TPTP CNFRefutation ----------------------------------
   ops  = <|head -> "tptpName", ...|> for the problem's function symbols;
   vars = symbols to render as TPTP variables (uppercased);
   axiomNames[[k]] names the k-th {Axiom, k}; conjectureName names the negated
   conjecture; problem is the SZS header problem name. *)
proofObjectToTPTP[po_, problem_String, axiomNames_List, conjectureName_String,
        ops_Association, vars_List] := Module[{tp, name, ds, lines},
    tp[HoldForm[e_]] := tp[e];
    tp[Equal[l_, r_]] := tp[l] <> " = " <> tp[r];
    tp[h_[args__] /; KeyExistsQ[ops, h]] := ops[h] <> "(" <> StringRiffle[tp /@ {args}, ","] <> ")";
    tp[s_Symbol] := If[ MemberQ[vars, s], ToUpperCase[ToString[s]], ToString[s]];
    tp[e_] := ToString[e];

    name[{"Axiom", k_}] := axiomNames[[k]];
    name[{"CriticalPairLemma", n_}] := "cpl" <> ToString[n];
    name[{"SubstitutionLemma", n_}] := "sl" <> ToString[n];
    name[{"Hypothesis", _}] := conjectureName;

    ds = Normal[po["ProofDataset"]];
    lines = Flatten @ Last @ Reap @ KeyValueMap[
        Function[{key, rec},
            With[{tag = key[[1]], st = rec["Statement"], pf = rec["Proof"]},
                Switch[tag,
                    "Axiom",
                        Sow["cnf(" <> name[key] <> ", axiom, " <> tp[st] <> ")."],
                    "Hypothesis",
                        With[{s = st /. HoldForm[Equal[l_, r_]] :> {tp[l], tp[r]}},
                            Sow["cnf(" <> conjectureName <> ", negated_conjecture, " <> s[[1]] <> " != " <> s[[2]] <> ")."]],
                    "CriticalPairLemma",
                        Sow["cnf(" <> name[key] <> ", plain, " <> tp[st] <> ", inference(sup, [status(thm)], [" <> name[pf["Construct"]] <> ", " <> name[pf["MatchingConstruct"]] <> "]))."],
                    "SubstitutionLemma",
                        Sow["cnf(" <> name[key] <> ", plain, " <> tp[st] <> ", inference(rw, [status(thm)], [" <> name[pf["Input"]] <> ", " <> name[pf["Construct"]] <> "]))."],
                    "Conclusion",
                        Sow["cnf(contradiction, plain, $false, inference(rw, [status(thm)], [" <> conjectureName <> ", " <> name[pf["Construct"]] <> "]))."]
                ]
            ]], ds];
    StringRiffle[
        Join[
            {"% SZS status Unsatisfiable for " <> problem, "% SZS output start CNFRefutation for " <> problem},
            lines,
            {"% SZS output end CNFRefutation for " <> problem}],
        "\n"]
]

(* ---- BOO001-1: ternary Boolean algebra (multiply/3, inverse/1) ---------- *)
ClearAll[mul, inv, a]
axioms = {
    ForAll[{v, w, x, y, z}, mul[mul[v, w, x], y, mul[v, w, z]] == mul[v, w, mul[x, y, z]]],
    ForAll[{x, y}, mul[y, x, x] == x],
    ForAll[{x, y}, mul[x, x, y] == x],
    ForAll[{x, y}, mul[inv[y], y, x] == x],
    ForAll[{x, y}, mul[x, y, inv[y]] == x]
}
po = TFindProof[inv[inv[a]] == a, axioms, TimeConstraint -> 90]
If[ Head[po] =!= ProofObject,
    Print["FAILED to prove BOO001-1: ", po]; Exit[1]
]

sol = proofObjectToTPTP[po, "BOO001-1.p",
    {"associativity", "ternary_multiply_1", "ternary_multiply_2", "left_inverse", "right_inverse"},
    "prove_inverse_is_self_cancelling",
    <|mul -> "multiply", inv -> "inverse"|>,
    {v, w, x, y, z}]

Export[FileNameJoin[{scriptDir, "samples", "BOO001-1.FindProof.sol"}], sol, "Text"]
Print[sol]
