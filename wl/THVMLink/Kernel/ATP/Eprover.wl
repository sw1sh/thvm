(* thvm/atp E-prover CLI wrapper.

   TEproverProof[problemFile, opts] runs the local E prover binary
   on a TPTP problem file and returns a normalized result Association
   sharing the same shape as TVampireProof + TWaldmeisterProof.

   E's `--proof-object --tstp-format` flags emit SZS-framed TPTP fof
   proof clauses with inference(...) records -- the same shape
   Wolfram`Parser`TPTPImport[..., "SZS"] consumes for Vampire.  So
   the derivation flows through TSZSDerivationToProofObject without
   prover-specific parsing.  E uses inference names like
   "pm" (paramodulation), "rw" (rewriting), "cn" (conjunction),
   "fof_nnf"; these map through $SZSRuleToConstruct's table (any
   unmapped name defaults to SubstitutionLemma). *)

BeginPackage["THVMLink`ATP`", {"Wolfram`Parser`"}]

TEproverProof::usage =
    "TEproverProof[\"path/to/file.p\", opts] runs the local E prover " <>
    "binary on a TPTP problem file (using --auto-schedule " <>
    "--proof-object --tstp-format) and returns a normalized result " <>
    "Association with keys Status, Strategy, Seconds, ProofLength, " <>
    "Inferences, RawSZS.  Two-arg form TEproverProof[\"Theory\", \"thm\"] " <>
    "resolves the TPTP file via tools/baselines/vampire_tptp/{Theory}__{thm}.p.  " <>
    "Options: TimeConstraint (default 30), Binary (default Automatic).  " <>
    "Uses Wolfram`Parser`TPTPImport[..., \"SZS\"] from the " <>
    "Wolfram/WolframParser paclet (declared in BeginPackage so Needs " <>
    "auto-loads it)."

TEproverProof::noeprover =
    "E prover binary not found.  Install via `brew install eprover`."

TEproverProof::badfile =
    "TPTP problem file not found: `1`"

Begin["`Private`"]

Options[TEproverProof] = {
    TimeConstraint -> 30,
    "Binary"       -> Automatic
}

eproverBinary[Automatic] := SelectFirst[
    {"/opt/homebrew/bin/eprover",
     "/usr/local/bin/eprover",
     "eprover"},
    FileExistsQ[#] || # === "eprover" &
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
        If[ MissingQ[bin] || bin === Null,
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
            StringContainsQ[out, "SZS status Theorem"]
                || StringContainsQ[out, "SZS status Unsatisfiable"],
                "Proved",
            StringContainsQ[out, "SZS status ResourceOut"]
                || StringContainsQ[out, "SZS status Timeout"]
                || StringContainsQ[out, "CPU time limit"],
                "TimedOut",
            True,
                "Failed"
        ];
        derivation = If[
            status === "Proved",
            Quiet @ Check[
                Wolfram`Parser`TPTPImport[out, "SZS"],
                $Failed
            ],
            Missing["NoProof"]
        ];
        foldedDerivation = If[
            AssociationQ[derivation] && KeyExistsQ[derivation, "Derivation"],
            derivation["Derivation"],
            {}
        ];
        <|
            "Status"      -> status,
            "Strategy"    -> "eprover-auto-schedule",
            "Seconds"     -> N @ Round[secs, 0.01],
            "ProofLength" -> If[status === "Proved", Length[foldedDerivation], 0],
            "Inferences"  -> foldedDerivation,
            "RawSZS"      -> out
        |>
    ]

End[]
EndPackage[]
