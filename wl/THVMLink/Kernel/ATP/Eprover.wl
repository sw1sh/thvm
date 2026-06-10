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

BeginPackage["THVMLink`ATP`", {"Wolfram`Parser`"}]

GeneralUtilities`SetUsage[TEproverProof, "TEproverProof[file$, opts$] runs the local E prover binary on the TPTP problem file$ (path ending in .p) and returns a normalized result Association with keys Status, Strategy, Seconds, ProofLength, Inferences, RawSZS.
TEproverProof[\"Theory\", \"thm\"] resolves the TPTP file via tools/baselines/vampire_tptp/{Theory}__{thm}.p.
Options: TimeConstraint, Binary; see the ATP documentation."];

TEproverProof::noeprover =
    "E prover binary not found.  Install via `brew install eprover`."

TEproverProof::badfile =
    "TPTP problem file not found: `1`"

Begin["`Private`"]

Options[TEproverProof] = {
    TimeConstraint -> 30,
    "Binary" -> Automatic
}

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

TEproverProof[theory_String, thm_String, opts : OptionsPattern[]] :=
    TEproverProof[
        FileNameJoin[{
            Directory[], "tools", "baselines", "vampire_tptp",
            theory <> "__" <> thm <> ".p"
        }],
        opts
    ]

TEproverProof[problemFile_String, opts : OptionsPattern[]] /;
        FileExtension[problemFile] === "p" :=
    Block[{
        bin, tc, cmd, out, secs, status,
        derivation, foldedDerivation
    },
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
        cmd = StringJoin[
            bin,
            " --auto-schedule --proof-object --tstp-format",
            " --cpu-limit=", ToString[N[tc]], " ",
            problemFile, " 2>&1"
        ];
        {secs, out} = AbsoluteTiming @ RunProcess[
            {"sh", "-c", cmd}, "StandardOutput"
        ];
        status = Which[
            StringContainsQ[out, "SZS status Theorem"] || StringContainsQ[out, "SZS status Unsatisfiable"],
                "Proved",
            StringContainsQ[out, "SZS status ResourceOut"] || StringContainsQ[out, "SZS status Timeout"] || StringContainsQ[out, "CPU time limit"],
                "TimedOut",
            True,
                "Failed"
        ];
        derivation = If[ status === "Proved",
            Quiet @ Check[
                Wolfram`Parser`TPTPImport[out, "SZS"],
                $Failed
            ],
            Missing["NoProof"]
        ];
        foldedDerivation = If[ AssociationQ[derivation] && KeyExistsQ[derivation, "Derivation"],
            derivation["Derivation"],
            {}
        ];
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
