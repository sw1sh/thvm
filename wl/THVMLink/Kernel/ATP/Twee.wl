(* thvm/atp Twee CLI wrapper.

   TTweeProof[problemFile, opts] runs Twee 2.x on a TPTP problem
   file and returns a normalized result Association.

   Twee's --tstp output uses SZS framing (status + Proof start/end
   markers) but the proof BODY is Twee's own
   "Axiom N / Lemma N / Proof: + equation chain" format; it does
   NOT carry per-step inference DAG metadata the way Vampire's
   --proof tptp does.  TTweeProof captures what is available:

       <|
         "Status"      -> "Proved" | "TimedOut" | "Failed",
         "Strategy"    -> "twee",
         "Seconds"     -> wall time as a Real,
         "ProofLength" -> count of Axiom + Lemma + Goal entries,
         "Lemmas"      -> {<|Name, Statement|>, ...},
         "Axioms"      -> {<|Name, Statement|>, ...},
         "RawProof"    -> the proof text after the SZS marker.
       |>

   For Vampire-style full inference-DAG matching, use TVampireProof
   instead (Vampire's --proof tptp emits TPTP fof clauses with
   inference(rule, [], parents) records). *)

BeginPackage["THVMLink`ATP`"]

GeneralUtilities`SetUsage[TTweeProof, "TTweeProof[file$] runs the local Twee 2.x binary on a TPTP problem file$ (with --tstp --quiet) and returns a normalized result Association with keys Status, Strategy, Seconds, ProofLength, Lemmas, Axioms, RawProof.
TTweeProof[\"Theory\", \"thm\"] resolves the TPTP file via tools/baselines/vampire_tptp/{Theory}__{thm}.p.
Options: TimeConstraint, Binary. Twee emits no per-step inference DAG; for that use TVampireProof."]

TTweeProof::notwee =
    "Twee CLI not found.  Install via `cabal install twee`."

TTweeProof::badfile =
    "TPTP problem file not found: `1`"

Begin["`Private`"]

Options[TTweeProof] = {
    TimeConstraint -> 30,
    "Binary" -> Automatic
}

(* Resolve twee: the cabal/Homebrew/usr-local prefixes first (cabal
   installs to ~/.cabal/bin), then any $PATH dir.  No default, so a
   truly-absent binary yields Missing -> the notwee message fires instead
   of RunProcess failing opaquely on bare "twee". *)
tweeBinary[Automatic] := SelectFirst[
    Join[
        {
            FileNameJoin[{$HomeDirectory, ".cabal", "bin", "twee"}],
            "/opt/homebrew/bin/twee",
            "/usr/local/bin/twee"
        },
        Map[
            dir |-> FileNameJoin[{dir, "twee"}],
            StringSplit[Replace[Environment["PATH"], Except[_String] -> ""], ":"]
        ]
    ],
    FileExistsQ
]

tweeBinary[s_String] := s

(* Parse one Axiom / Lemma proof body line into <|Name, Statement|>.
   Returns Nothing on no-match so Map can flatten. *)
parseAxiomLine[l_String] := Replace[
    StringCases[
        l,
        "Axiom " ~~ n : DigitCharacter .. ~~ " (" ~~ name__ ~~ "): " ~~ stmt__ :>
            <|"Name" -> "axiom" <> n, "Statement" -> StringTrim @ stmt|>
    ],
    {
        {entry_Association, ___} -> entry,
        _ -> Nothing
    }
]

parseLemmaLine[l_String] := Replace[
    StringCases[
        l,
        "Lemma " ~~ n : DigitCharacter .. ~~ ": " ~~ stmt__ :>
            <|"Name" -> "lemma" <> n, "Statement" -> StringTrim @ stmt|>
    ],
    {
        {entry_Association, ___} -> entry,
        _ -> Nothing
    }
]

(* Parse Twee's proof body.  Each "Axiom N (...): ..." line + each
   "Lemma N: ..." line + the (single) "Goal N (...): ..." line
   contribute one step to ProofLength. *)
parseTweeProof[proofText_String] := Block[{lines, axEntries, lemEntries, goalSeen},
    lines = StringSplit[proofText, "\n"];
    axEntries = DeleteCases[Map[parseAxiomLine, lines], Nothing];
    lemEntries = DeleteCases[Map[parseLemmaLine, lines], Nothing];
    goalSeen = AnyTrue[lines, StringStartsQ[StringTrim @ #, "Goal "] &];
    <|
        "Axioms" -> axEntries,
        "Lemmas" -> lemEntries,
        "ProofLength" -> Length[axEntries] + Length[lemEntries] + If[goalSeen, 1, 0]
    |>
]

TTweeProof[theory_String, thm_String, opts : OptionsPattern[]] :=
    TTweeProof[
        FileNameJoin[{
            Directory[], "tools", "baselines", "vampire_tptp",
            theory <> "__" <> thm <> ".p"
        }],
        opts
    ]

TTweeProof[problemFile_String, opts : OptionsPattern[]] /;
        FileExtension[problemFile] === "p" :=
    Block[{bin, tc, cmd, out, secs, status, proofBody, parsed},
        bin = tweeBinary[OptionValue["Binary"]];
        If[ MissingQ[bin],
            Message[TTweeProof::notwee];
            Return[$Failed]
        ];
        If[ ! FileExistsQ[problemFile],
            Message[TTweeProof::badfile, problemFile];
            Return[$Failed]
        ];
        tc = OptionValue[TimeConstraint];
        cmd = StringJoin[
            "timeout ", ToString[N[tc + 2]], " ",
            bin, " --tstp --quiet ", problemFile, " 2>&1"
        ];
        {secs, out} = AbsoluteTiming @ RunProcess[
            {"sh", "-c", cmd}, "StandardOutput"
        ];
        status = Which[
            StringContainsQ[out, "SZS status Unsatisfiable"] || StringContainsQ[out, "SZS status Theorem"],
                "Proved",
            StringContainsQ[out, "RESULT: Timeout"] || StringContainsQ[out, "Time limit"],
                "TimedOut",
            True,
                "Failed"
        ];
        proofBody = If[ status === "Proved",
            StringCases[
                out,
                "SZS output start Proof" ~~ body__ ~~ "SZS output end Proof" :>
                    StringTrim @ body,
                1
            ],
            {}
        ];
        parsed = If[ Length[proofBody] > 0,
            parseTweeProof[First @ proofBody],
            <|"Axioms" -> {}, "Lemmas" -> {}, "ProofLength" -> 0|>
        ];
        <|
            "Status" -> status,
            "Strategy" -> "twee",
            "Seconds" -> N @ Round[secs, 0.01],
            "ProofLength" -> parsed["ProofLength"],
            "Axioms" -> parsed["Axioms"],
            "Lemmas" -> parsed["Lemmas"],
            "RawProof" -> If[Length[proofBody] > 0, First @ proofBody, ""]
        |>
    ]

End[]
EndPackage[]
