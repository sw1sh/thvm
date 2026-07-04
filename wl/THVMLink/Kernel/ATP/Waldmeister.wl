(* thvm/atp Waldmeister CLI wrapper.

   TWaldmeisterProof[problemFile, opts] runs the local Waldmeister
   binary (wmcli) on a WM .pr problem file and returns a
   normalized result Association.

   WM emits its own proof-protocol format (NOT SZS / TPTP fof):
       N : tes-eqn  : LHS = RHS : initial
       N : tes-goal : LHS = RHS : hypothesis
       N : tes-rule : LHS -> RHS : orient(K, x)
       N : tes-eqn  : LHS = RHS : cp(I, side_i, J, side_j)
       N : tes-eqn  : LHS = RHS : tes-red(I, side_i, J, side_j)
       N : tes-goal : LHS = RHS : tes-red(I, side_i, J, side_j)
       N : tes-final: LHS = RHS : K
   So we parse the protocol into a structured derivation list
   compatible with TSZSDerivationToProofObject's input shape.

   Result Association uses the same keys as TVampireProof + the
   same downstream TSZSDerivationToProofObject path. *)

BeginPackage["WolframInstitute`THVMLink`ATP`", {"GeneralUtilities`"}]

SetUsage[TWaldmeisterProof, "TWaldmeisterProof[file$] runs the local Waldmeister wmcli binary on the WM .pr problem file$, parses its proof-protocol output, and returns a normalized Association with keys Status, Strategy, Seconds, ProofLength, Inferences, RawProtocol.
The input must be Waldmeister's own .pr format, not TPTP; a (theory, theorem) two-argument form would need a TPTP-to-pr converter and is not yet supported.
Options: TimeConstraint, \"Binary\", \"MathlinkPath\"."];

TWaldmeisterProof::nowm = "Waldmeister wmcli binary not found."

TWaldmeisterProof::badfile = "Waldmeister .pr file not found: `1`"

Begin["`Private`"]

Options[TWaldmeisterProof] = {TimeConstraint -> 30, "WMCLI" -> Automatic, "Binary" -> Automatic, "MathlinkPath" -> Automatic}

(* Resolve wmcli location: $WMCLI env var > a $PATH scan > a short list of
   common dev-build locations under $HomeDirectory (the wmcli is built
   alongside the Waldmeister source, with no canonical install path, so
   auto-detect the usual spots before giving up).  A truly-absent binary
   yields Missing -> the nowm message fires instead of RunProcess failing
   opaquely on bare "wmcli". *)
wmBinary[Automatic] := Block[{env = Environment["WMCLI"], cands},
    If[StringQ[env] && FileExistsQ[env], Return[env]];
    cands = Join[
        Map[
            dir |-> FileNameJoin[{dir, "wmcli"}],
            StringSplit[Replace[Environment["PATH"], Except[_String] -> ""], ":"]
        ],
        Map[
            rel |-> FileNameJoin[Prepend[rel, $HomeDirectory]],
            {{".local", "bin", "wmcli"}, {"bin", "wmcli"}, {"src", "wolfram", "waldmeister", "wmcli"}}
        ]
    ];
    SelectFirst[cands, FileExistsQ]
]

wmBinary[s_String] := s

(* Frameworks dir lives under $InstallationDirectory (the resolved
   Wolfram install for the current kernel), so the path tracks
   whichever Wolfram product loaded us rather than a hardcoded
   absolute path. *)
wmMathlinkPath[Automatic] := FileNameJoin[{$InstallationDirectory, "Frameworks"}]

wmMathlinkPath[s_String] := s

(* Parse a single proof-protocol line of the form
       N : <kind> : <body> : <source> [###R K|###E K]
   into an Association.  The trailing `###R K` / `###E K` is the
   rule-or-equation number WM assigns post-orient, NOT a parent, so
   strip it before parent-extraction. *)
parseProofLine[line_String] := Block[{parts, name, kind, body, source, sourceClean, sourceRule, parents},
    parts = StringSplit[line, " : ", 4];
    If[Length[parts] < 4, Return[Nothing]];
    {name, kind, body, source} = StringTrim /@ parts;
    sourceClean = StringTrim @ StringReplace[source, RegularExpression["\\s*###[RE]\\s*\\d+\\s*$"] -> ""];
    sourceRule = Which[
        StringStartsQ[sourceClean, "initial"], "file",
        StringStartsQ[sourceClean, "hypothesis"], "file",
        StringStartsQ[sourceClean, "orient"], "orient",
        StringStartsQ[sourceClean, "cp"], "superposition",
        StringStartsQ[sourceClean, "tes-red"], "forward_demodulation",
        StringMatchQ[sourceClean, DigitCharacter ..],
            (* Bare-digit source is used in TWO distinct WM patterns:
               (a) `tes-eqn : ... : 2` copies equation 2 as a new
                   numbered entry (bookkeeping; the kind disambig is
                   `tes-eqn`).  This is the `equation_copy` rule the
                   $BookkeepingRules table drops on fold.
               (b) `tes-final : ... : 28` is the closing step that
                   cites the prior step that derived the empty clause.
                   This maps to trivial_inequality_removal -> Conclusion. *)
            If[kind === "tes-final", "trivial_inequality_removal", "equation_copy"],
        True, "rewrite"
    ];
    (* Extract the equation-number ARGS of the inference function:
       orient(K, x) -> {K}
       cp(I, side, J, side) -> {I, J}
       tes-red(I, side, J, side) -> {I, J}
       <number> -> {<number>}
       A `side` can carry a subterm-POSITION index (`L.2`, `R.3`); its
       digit is NOT a parent.  Scanning every digit run (the old
       approach) grabbed that position digit AND any un-stripped `###R
       K` suffix, and the spurious entry displaced the real second
       parent -- e.g. `cp(11,L.2,35,L)` parsed to {11,2,35} instead of
       {11,35}, collapsing a superposition's two parents onto one and
       breaking CriticalPairLemma reconstruction.  Split the function's
       arg list on commas and keep only the pure-integer args; a
       bare-number source (no parens) is itself the single parent. *)
    parents = With[{args = StringCases[sourceClean, "(" ~~ a__ ~~ ")" :> a]},
        If[ args === {},
            ToExpression /@ StringCases[sourceClean, n : DigitCharacter .. :> n],
            ToExpression /@ Select[StringTrim /@ StringSplit[First[args], ","],
                StringMatchQ[#, DigitCharacter ..] &]]];
    <|
        "Head" -> "wm",
        "Name" -> name,
        "Role" -> If[StringStartsQ[sourceClean, "hypothesis"], "negated_conjecture", "axiom"],
        "Formula" -> body,
        "Rule" -> sourceRule,
        "Parents" -> Map[ToString, parents]
    |>
]

parseProtocol[raw_String] := Block[{lines},
    (* WM's PROOF PROTOCOL section header is banner-style
       ("P R O O F   P R O T O C O L"), too brittle to scan for
       literally.  Instead scan ALL output lines for the protocol
       shape `<digits> : tes-* : ... : ...`; only the proof
       protocol emits those, so it's a sound filter. *)
    lines = StringSplit[raw, "\n"];
    lines = Select[lines, StringMatchQ[StringTrim[#], DigitCharacter .. ~~ " : tes-" ~~ __] &];
    DeleteCases[Map[parseProofLine, lines], Nothing]
]

TWaldmeisterProof[problemFile_String, opts : OptionsPattern[]] /;
        FileExtension[problemFile] === "pr" :=
    Block[{bin, fwk, cmd, out, secs, status, derivation},
        (* "WMCLI" suboption wins; else "Binary"; else Automatic ->
           $WMCLI env / PATH / dev-location auto-detect in wmBinary. *)
        bin = wmBinary[Replace[OptionValue["WMCLI"], Automatic :> OptionValue["Binary"]]];
        If[ MissingQ[bin],
            Message[TWaldmeisterProof::nowm];
            Return[$Failed]
        ];
        If[ ! FileExistsQ[problemFile],
            Message[TWaldmeisterProof::badfile, problemFile];
            Return[$Failed]
        ];
        fwk = wmMathlinkPath[OptionValue["MathlinkPath"]];
        (* Match FEQ/ELProver: ELProver.c prepends "-auto" so the Automodus
           heuristic picks the goal-directed shortcut (a shorter proof); bare
           wmcli does blind completion and finds a longer proof.  Add -auto so
           Method->"WaldmeisterProcess" matches FindEquationalProof. *)
        cmd = StringJoin["DYLD_FRAMEWORK_PATH='", fwk, "' ", bin, " -auto ", problemFile, " 2>&1"];
        {secs, out} = AbsoluteTiming @ RunProcess[{"sh", "-c", cmd}, "StandardOutput"];
        status = Which[
            StringContainsQ[out, "Waldmeister states: Goal proved"] || StringContainsQ[out, "this proves the goal"],
                "Proved",
            StringContainsQ[out, "Time limit"],
                "TimedOut",
            True,
                "Failed"
        ];
        derivation = If[status === "Proved", parseProtocol[out], {}];
        <|
            "Status" -> status,
            "Strategy" -> "waldmeister-auto",
            "Seconds" -> N @ Round[secs, 0.01],
            "ProofLength" -> Length[derivation],
            "Inferences" -> derivation,
            "RawProtocol" -> out
        |>
    ]

End[]
EndPackage[]
