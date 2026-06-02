(* ========================================================
   thvm/atp Vampire CLI wrapper
   --------------------------------------------------------
   TVampireProof[problemFile, opts] runs the vampire binary
   on a TPTP problem file and returns a normalized result
   Association.  Use to validate a thvm preset's behavior
   against the actual Vampire 5.0.1 CLI proof on the same
   problem (the methodology pivot: match results on easy
   cases, then progressively scale up; see commit 7457bf39
   + docs/atp/vampire_case_teardown.md).

   Result shape:
       <|
         "Status"      -> "Proved" | "TimedOut" | "Failed",
         "Strategy"    -> StringQ (winning strategy line,
                          e.g. "lrs+10_1:1_..."),
         "Seconds"     -> Real,
         "ProofLength" -> Integer (parsed SZS derivation
                          steps minus parse-time reorient_
                          equations entries),
         "Inferences"  -> {<|Name, Rule, Parents,
                          Formula|>, ...},  (* the parsed
                          SZS DAG, dropping reorient steps
                          and inlining their parents *)
         "RawSZS"      -> the SZS output text (for
                          debugging / re-parsing).
       |>

   For unsolved problems Status is "Failed" / "TimedOut"
   and the inference fields hold defaults.

   Uses Wolfram`Parser`TPTPImport[..., "SZS"] from
   /Users/swish/src/wolfram/AISkills/examples/WolframParser.
   ======================================================== *)

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
    "convention used by the bench harness).";

TVampireProof::novamp =
    "Vampire CLI not found on PATH.  Install via `brew install vampire`.";

TVampireProof::badfile =
    "TPTP problem file not found: `1`";

TVampireProof::badtptp =
    "Vampire returned a result but TPTPImport could not parse it: `1`";

Begin["`Private`"]

Options[TVampireProof] = {
    TimeConstraint -> 30,
    "Mode"         -> "casc",
    "Binary"       -> Automatic  (* "vampire" or absolute path *)
};

vampireBinary[Automatic] := Module[{paths},
    paths = {"/opt/homebrew/bin/vampire", "/usr/local/bin/vampire",
        "vampire"};
    SelectFirst[paths, FileExistsQ[#] || # === "vampire" &]];
vampireBinary[s_String] := s;

(* Strip parse-time reorient_equations entries: replace any inference
   with rule=reorient_equations by inlining its single parent's name
   in any later inference that references it.  See
   Parse/TPTP.cpp:3701 in Vampire 5.0.1 for why this is a
   preprocessing rewrite, not a saturation step. *)
foldReorients[derivation_List] :=
    Module[{aliases, fold},
        aliases = <||>;
        Scan[Function[step,
            If[ step["Rule"] === "reorient_equations" &&
                    ListQ[step["Parents"]] && Length[step["Parents"]] == 1,
                aliases[step["Name"]] = step["Parents"][[1]]]],
            derivation];
        (* resolve transitive aliases *)
        fold[n_] := If[ KeyExistsQ[aliases, n], fold[aliases[n]], n];
        DeleteCases[
            Map[Function[step,
                If[ KeyExistsQ[step, "Parents"],
                    Append[step, "Parents" -> Map[fold, step["Parents"]]],
                    step]],
                derivation],
            KeyValuePattern["Rule" -> "reorient_equations"]]];

(* Extract the winning strategy string from Vampire's stderr/stdout
   ("% <strategy> on <problem> for (<time>ds)\n% Refutation found.").
   We pick the LAST such line BEFORE "Refutation found". *)
extractStrategy[src_String] :=
    Module[{lines, idx},
        lines = StringSplit[src, "\n"];
        idx = First @ Flatten @ Position[lines,
            l_String /; StringContainsQ[l, "Refutation found"], 1];
        If[ idx === None, Missing["NotFound"],
            Module[{stratLine = SelectFirst[Reverse @ lines[[;; idx - 1]],
                StringContainsQ[#, "% lrs"] || StringContainsQ[#, "% dis"] ||
                    StringContainsQ[#, "% lrs+"] || StringContainsQ[#, "% ott"] ||
                    StringContainsQ[#, "% finite"] &,
                Missing["NotFound"]]},
                If[ MissingQ[stratLine], Missing["NotFound"],
                    StringTrim @ StringReplace[stratLine,
                        {RegularExpression["^% "] -> "",
                         RegularExpression[" on .*"] -> ""}]]]]];

(* Two-arg form: resolve (Theory, thm) to the canonical TPTP
   problem file under tools/baselines/vampire_tptp/.  Mirrors the
   bench harness convention so callers can use the same
   (theory, thm) tuple they use elsewhere. *)
TVampireProof[theory_String, thm_String, opts:OptionsPattern[]] :=
    Module[{path = FileNameJoin[{
            Directory[], "tools", "baselines", "vampire_tptp",
            theory <> "__" <> thm <> ".p"}]},
        TVampireProof[path, opts]];

TVampireProof[problemFile_String, opts:OptionsPattern[]] /;
        FileExtension[problemFile] === "p" := Module[{
        bin, tc, mode, cmd, out, status, derivation,
        strategy, secs, proofLen, foldedDerivation},
    bin = vampireBinary[OptionValue["Binary"]];
    If[ bin === Missing["NotFound"],
        Message[TVampireProof::novamp]; Return[$Failed]];
    If[ ! FileExistsQ[problemFile],
        Message[TVampireProof::badfile, problemFile]; Return[$Failed]];
    tc   = OptionValue[TimeConstraint];
    mode = OptionValue["Mode"];
    cmd = StringJoin[bin, " --mode ", mode, " --proof tptp -t ",
        ToString[N[tc]], " ", problemFile, " 2>&1"];
    {secs, out} = AbsoluteTiming @ RunProcess[
        {"sh", "-c", cmd}, "StandardOutput"];
    status = Which[
        StringContainsQ[out, "SZS status Unsatisfiable"] ||
            StringContainsQ[out, "SZS status Theorem"],   "Proved",
        StringContainsQ[out, "SZS status Timeout"]
            || StringContainsQ[out, "Time limit reached"], "TimedOut",
        True, "Failed"];
    strategy = If[ status === "Proved", extractStrategy[out],
        Missing["NoProof"]];
    derivation = If[ status === "Proved",
        Quiet @ Check[
            Wolfram`Parser`TPTPImport[out, "SZS"], $Failed],
        Missing["NoProof"]];
    foldedDerivation = If[ AssociationQ[derivation] &&
            KeyExistsQ[derivation, "Derivation"],
        foldReorients[derivation["Derivation"]],
        {}];
    proofLen = If[ status === "Proved", Length[foldedDerivation], 0];
    <|"Status" -> status,
      "Strategy" -> strategy,
      "Seconds" -> N @ Round[secs, 0.01],
      "ProofLength" -> proofLen,
      "Inferences" -> foldedDerivation,
      "RawSZS" -> out|>
];

End[]
EndPackage[]
