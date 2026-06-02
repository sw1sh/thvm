(* thvm/atp Vampire CLI wrapper.

   TVampireProof[problemFile, opts] runs the vampire binary on a
   TPTP problem file and returns a normalized result Association.
   Use it to validate a thvm preset's behavior against the actual
   Vampire 5.0.1 CLI proof on the same problem (the methodology
   pivot: match results on easy cases, then progressively scale up;
   see commit 7457bf39 + docs/atp/vampire_case_teardown.md).

   Result shape:
       <|
         "Status"      -> "Proved" | "TimedOut" | "Failed",
         "Strategy"    -> the winning strategy line ("lrs+10_1:1_..."),
         "Seconds"     -> wall time as a Real,
         "ProofLength" -> SZS derivation step count, minus parse-time
                          reorient_equations entries,
         "Inferences"  -> the parsed SZS DAG with reorient steps
                          inlined into their parents,
         "RawSZS"      -> the SZS output text for re-parsing.
       |>

   For unsolved problems Status is "Failed" / "TimedOut" and the
   inference fields hold defaults.  Uses
   Wolfram`Parser`TPTPImport[..., "SZS"] from
   /Users/swish/src/wolfram/AISkills/examples/WolframParser. *)

BeginPackage["THVMLink`ATP`"]

TVampireProof::usage =
    "TVampireProof[\"path/to/file.p\", opts] runs the local Vampire 5.0.1 " <>
    "binary on a TPTP problem file and returns a normalized result " <>
    "Association with keys Status, Strategy, Seconds, ProofLength, " <>
    "Inferences, RawSZS.  Options: TimeConstraint (seconds, default 30), " <>
    "Mode (default \"casc\").  See docs/atp/vampire_case_teardown.md " <>
    "for the per-token strategy decode and the ProofObject mapping.  " <>
    "TVampireProof[\"Theory\", \"thm\", opts] resolves the TPTP file via " <>
    "tools/baselines/vampire_tptp/{Theory}__{thm}.p (the cwd-relative " <>
    "convention used by the bench harness)."

TVampireProof::novamp =
    "Vampire CLI not found on PATH.  Install via `brew install vampire`."

TVampireProof::badfile =
    "TPTP problem file not found: `1`"

TVampireProof::badtptp =
    "Vampire returned a result but TPTPImport could not parse it: `1`"

Begin["`Private`"]

Options[TVampireProof] = {
    TimeConstraint -> 30,
    "Mode"         -> "casc",
    "Binary"       -> Automatic
}

(* Resolve the vampire binary location: search the common Homebrew /
   /usr/local prefixes, fall back to bare "vampire" so PATH still
   wins if the binary lives elsewhere. *)
vampireBinary[Automatic] := SelectFirst[
    {"/opt/homebrew/bin/vampire", "/usr/local/bin/vampire", "vampire"},
    FileExistsQ[#] || # === "vampire" &
]

vampireBinary[s_String] := s

(* Strip parse-time reorient_equations entries: Vampire records
   equation-flipping done by Parse/TPTP.cpp:3701 as a "plain"
   inference with rule = "reorient_equations" and one parent.  These
   are preprocessing rewrites, NOT saturation steps, so the
   ProofObject builder should inline them into their parents. *)
foldReorients[derivation_List] := Block[{aliases, fold},
    aliases = Association @ Cases[
        derivation,
        s_Association /; s["Rule"] === "reorient_equations"
            && ListQ[s["Parents"]] && Length[s["Parents"]] == 1 :>
            (s["Name"] -> s["Parents"][[1]])
    ];
    fold[n_] := If[ KeyExistsQ[aliases, n], fold[aliases[n]], n];
    DeleteCases[
        Map[
            step |-> If[
                KeyExistsQ[step, "Parents"],
                Append[step, "Parents" -> Map[fold, step["Parents"]]],
                step
            ],
            derivation
        ],
        KeyValuePattern["Rule" -> "reorient_equations"]
    ]
]

(* Extract the winning strategy string from Vampire's output: the
   last "% <strategy> on <problem> for (<time>ds)" line before
   "% Refutation found.".  Returns Missing["NotFound"] when no
   refutation was reported. *)
extractStrategy[src_String] := Block[{lines, idx, stratLine},
    lines = StringSplit[src, "\n"];
    idx = FirstPosition[
        lines,
        l_String /; StringContainsQ[l, "Refutation found"],
        Missing["NotFound"],
        {1}
    ];
    If[ MissingQ[idx],
        Missing["NotFound"],
        stratLine = SelectFirst[
            Reverse @ lines[[ ;; idx[[1]] - 1]],
            StringStartsQ[#, "% "]
                && (StringContainsQ[#, "lrs"] || StringContainsQ[#, "dis"]
                    || StringContainsQ[#, "ott"] || StringContainsQ[#, "finite"]) &,
            Missing["NotFound"]
        ];
        If[ MissingQ[stratLine],
            Missing["NotFound"],
            StringTrim @ StringReplace[
                stratLine,
                {
                    RegularExpression["^% "]    -> "",
                    RegularExpression[" on .*"] -> ""
                }
            ]
        ]
    ]
]

(* Two-arg form: resolve (Theory, thm) to the canonical TPTP problem
   file under tools/baselines/vampire_tptp/.  Mirrors the bench
   harness convention so callers can use the same (theory, thm) tuple
   they use elsewhere. *)
TVampireProof[theory_String, thm_String, opts : OptionsPattern[]] :=
    TVampireProof[
        FileNameJoin[{
            Directory[], "tools", "baselines", "vampire_tptp",
            theory <> "__" <> thm <> ".p"
        }],
        opts
    ]

TVampireProof[problemFile_String, opts : OptionsPattern[]] /;
        FileExtension[problemFile] === "p" :=
    Block[{
        bin, tc, mode, cmd, out, secs, status,
        derivation, foldedDerivation
    },
        bin = vampireBinary[OptionValue["Binary"]];
        If[ MissingQ[bin],
            Message[TVampireProof::novamp];
            Return[$Failed]
        ];
        If[ ! FileExistsQ[problemFile],
            Message[TVampireProof::badfile, problemFile];
            Return[$Failed]
        ];
        tc   = OptionValue[TimeConstraint];
        mode = OptionValue["Mode"];
        cmd = StringJoin[
            bin, " --mode ", mode, " --proof tptp -t ",
            ToString[N[tc]], " ", problemFile, " 2>&1"
        ];
        {secs, out} = AbsoluteTiming @ RunProcess[
            {"sh", "-c", cmd}, "StandardOutput"
        ];
        status = Which[
            StringContainsQ[out, "SZS status Unsatisfiable"]
                || StringContainsQ[out, "SZS status Theorem"],
                "Proved",
            StringContainsQ[out, "SZS status Timeout"]
                || StringContainsQ[out, "Time limit reached"],
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
            foldReorients[derivation["Derivation"]],
            {}
        ];
        <|
            "Status"      -> status,
            "Strategy"    -> If[
                status === "Proved",
                extractStrategy[out],
                Missing["NoProof"]
            ],
            "Seconds"     -> N @ Round[secs, 0.01],
            "ProofLength" -> If[status === "Proved", Length[foldedDerivation], 0],
            "Inferences"  -> foldedDerivation,
            "RawSZS"      -> out
        |>
    ]

End[]
EndPackage[]
