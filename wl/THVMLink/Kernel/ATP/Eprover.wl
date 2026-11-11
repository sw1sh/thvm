(* thvm/atp E-prover CLI wrapper.

   TEproverProof[problemFile, opts] runs the local E prover binary
   on a TPTP problem file and returns a normalized result Association
   sharing the same shape as TVampireProof + TWaldmeisterProof.

   E's `--proof-object --tstp-format` flags emit SZS-framed TPTP fof
   proof clauses with inference(...) records, the same shape
   Wolfram`Parser`TPTPImport[..., "SZS"] consumes for Vampire.  So
   the derivation flows through TSZSDerivationToProofObject without
   prover-specific parsing.  E uses inference names like
   "pm" (paramodulation), "rw" (rewriting), "cn" (conjunction),
   "fof_nnf"; these map through $SZSRuleToConstruct's table (any
   unmapped name defaults to SubstitutionLemma). *)

BeginPackage["WolframInstitute`THVMLink`ATP`", {"GeneralUtilities`"}]

SetUsage[TEproverProof, "TEproverProof[file$, opts$] runs the local E prover binary on the TPTP problem file$ (path ending in .p) and returns a normalized result Association with keys Status, Strategy, Seconds, ProofLength, Inferences, RawSZS.
TEproverProof[\"Theory\", \"thm\"] resolves the TPTP file via tools/baselines/vampire_tptp/{Theory}__{thm}.p.
Options: TimeConstraint, Binary; see the ATP documentation."];

TEproverProof::noeprover = "E prover binary not found.  Install via: brew install eprover"

TEproverProof::badfile = "TPTP problem file not found: `1`"

Begin["`Private`"]

Options[TEproverProof] = {TimeConstraint -> 30, "Binary" -> Automatic}

(* E's TSTP proofs differ from Vampire's in two ways the SZS converter does not expect.  E copies
   input clauses under new names with no inference (cnf(c_0_5, axiom, ..., ax3)) and keeps
   identity simplifications; a single-parent step with its parent's formula becomes an
   equation_copy, which the converter folds away.  And E nests inferences
   (inference(rw, _, [inference(spm, _, [c_0_7, c_0_8]), c_0_9])), which arrive as parent strings;
   their parents are the clause names inside. *)
eNormalizeDerivation[steps_List] := Module[{names = Lookup[steps, "Name"], formulas},
    formulas = AssociationThread[names, Lookup[steps, "Formula"]];
    Map[
        s |-> Module[{parents = Flatten[Map[
                p |-> If[ StringStartsQ[p, "inference("],
                    Select[StringCases[p, RegularExpression["[A-Za-z_][A-Za-z0-9_]*"]], MemberQ[names, #] &],
                    p],
                Lookup[s, "Parents", {}]]]},
            Which[
                Length[parents] === 1 && s["Rule"] =!= "file" && Lookup[formulas, First[parents]] === s["Formula"],
                    Join[s, <|"Rule" -> "equation_copy", "Parents" -> parents|>],
                True,
                    Append[s, "Parents" -> DeleteDuplicates[parents]]
            ]],
        steps]
]

(* Resolve eprover: the known prefixes first, then any $PATH dir.  No
   default, so a truly-absent binary yields Missing -> the noeprover
   message fires instead of RunProcess failing opaquely on bare "eprover". *)
eproverBinary[Automatic] := SelectFirst[
    Join[
        {"/opt/homebrew/bin/eprover", "/usr/local/bin/eprover"},
        Map[
            dir |-> FileNameJoin[{dir, "eprover"}],
            StringSplit[Replace[Environment["PATH"], Except[_String] -> ""], ":"]
        ]
    ],
    FileExistsQ
]

eproverBinary[s_String] := s

TEproverProof[theory_String, thm_String, opts : OptionsPattern[]] := TEproverProof[
    FileNameJoin[{Directory[], "tools", "baselines", "vampire_tptp", theory <> "__" <> thm <> ".p"}],
    opts
]

TEproverProof[problemFile_String, opts : OptionsPattern[]] /;
        FileExtension[problemFile] === "p" :=
    Block[{bin, tc, cmd, run, out, secs, status, derivation, foldedDerivation},
        bin = eproverBinary[OptionValue["Binary"]];
        If[ MissingQ[bin],
            Message[TEproverProof::noeprover];
            Return[$Failed]
        ];
        If[ ! FileExistsQ[problemFile],
            Message[TEproverProof::badfile, problemFile];
            Return[$Failed]
        ];
        tc = OptionValue[TimeConstraint];
        cmd = StringJoin[bin, " --auto-schedule --proof-object --tstp-format", " --cpu-limit=", ToString[Ceiling[tc]], " ", problemFile, " 2>&1"];
        run = atpRunLimited[cmd, Ceiling[tc] + 2];
        {secs, out} = {run["Seconds"], run["Output"]};
        status = Which[
            StringContainsQ[out, "SZS status Theorem"] || StringContainsQ[out, "SZS status Unsatisfiable"],
                "Proved",
            (* E's automatic schedule reports GaveUp when its CPU limit runs out, as it does for a
               strategy that gives up early: a GaveUp near the limit is the limit. *)
            run["TimedOut"] || StringContainsQ[out, "SZS status ResourceOut"] || StringContainsQ[out, "SZS status Timeout"] || StringContainsQ[out, "CPU time limit"] ||
                    (StringContainsQ[out, "SZS status GaveUp"] && secs >= 0.9 Ceiling[tc]),
                "TimedOut",
            True,
                "Failed"
        ];
        derivation = If[ status === "Proved",
            Quiet @ Check[tptpImport[out, "SZS"], $Failed],
            Missing["NoProof"]
        ];
        foldedDerivation = If[AssociationQ[derivation] && KeyExistsQ[derivation, "Derivation"], eNormalizeDerivation[derivation["Derivation"]], {}];
        <|
            "Status" -> status,
            "Strategy" -> "eprover-auto-schedule",
            "Seconds" -> N @ Round[secs, 0.01],
            "ProofLength" -> If[status === "Proved", Length[foldedDerivation], 0],
            "Inferences" -> foldedDerivation,
            "RawSZS" -> out
        |>
    ]

End[]
EndPackage[]
