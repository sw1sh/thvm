(* ========================================================
   thvm/atp Twee CLI wrapper
   --------------------------------------------------------
   TTweeProof[problemFile, opts] runs Twee 2.x on a TPTP
   problem file and returns a normalized result Association.

   Twee's --tstp output uses SZS framing (status + Proof
   start/end markers) but the proof BODY is Twee's own
   "Axiom N / Lemma N / Proof: + equation chain" format
   -- it does NOT carry per-step inference DAG metadata
   the way Vampire's --proof tptp does.  TTweeProof captures
   what is available:

       <|
         "Status"      -> "Proved" | "TimedOut" | "Failed",
         "Strategy"    -> "twee",
         "Seconds"     -> Real,
         "ProofLength" -> Integer (count of "Axiom N" +
                          "Lemma N" entries in the proof),
         "Lemmas"      -> {<|Name, Statement|>, ...},  (* parsed *)
         "Axioms"      -> {<|Name, Statement|>, ...},  (* parsed *)
         "RawProof"    -> the proof text after the SZS marker.
       |>

   For Vampire-style full inference-DAG matching, use
   TVampireProof instead (Vampire's --proof tptp emits TPTP
   fof clauses with inference(rule, [], parents) records). *)

BeginPackage["THVMLink`ATP`"]

TTweeProof::usage =
    "TTweeProof[\"path/to/file.p\", opts] runs the local Twee 2.x " <>
    "binary on a TPTP problem file (using --tstp --quiet) and returns " <>
    "a normalized result Association with keys Status, Strategy, " <>
    "Seconds, ProofLength, Lemmas, Axioms, RawProof.  Two-arg form " <>
    "TTweeProof[\"Theory\", \"thm\"] resolves the TPTP file via " <>
    "tools/baselines/vampire_tptp/{Theory}__{thm}.p.  Options: " <>
    "TimeConstraint (default 30), Binary (default Automatic).  Twee " <>
    "does NOT emit per-step inference DAG; for that use TVampireProof.";

TTweeProof::notwee =
    "Twee CLI not found.  Install via `cabal install twee`.";

TTweeProof::badfile =
    "TPTP problem file not found: `1`";

Begin["`Private`"]

Options[TTweeProof] = {
    TimeConstraint -> 30,
    "Binary"       -> Automatic
};

tweeBinary[Automatic] := Module[{paths},
    paths = {"/Users/swish/.cabal/bin/twee",
        "/opt/homebrew/bin/twee", "/usr/local/bin/twee", "twee"};
    SelectFirst[paths, FileExistsQ[#] || # === "twee" &]];
tweeBinary[s_String] := s;

(* Parse Twee's proof body for Axiom / Lemma count.  Each line of
   the proof body that starts with "Axiom N" or "Lemma N"
   contributes a step.  The Goal closer adds one more step. *)
parseTweeProof[proofText_String] :=
    Module[{lines, axEntries, lemEntries, goalSeen},
        lines = StringSplit[proofText, "\n"];
        axEntries = Cases[lines,
            l_String /; StringMatchQ[StringTrim @ l,
                "Axiom " ~~ DigitCharacter.. ~~ " (" ~~ __ ~~ "): " ~~ __] :>
                Module[{m = StringCases[l,
                        "Axiom " ~~ n:DigitCharacter.. ~~ " (" ~~ name__ ~~ "): " ~~ stmt__ :>
                            {n, name, stmt}]},
                    If[ Length[m] > 0,
                        <|"Name" -> "axiom" <> First[m][[1]],
                          "Statement" -> StringTrim @ First[m][[3]]|>,
                        Nothing]]];
        lemEntries = Cases[lines,
            l_String /; StringMatchQ[StringTrim @ l,
                "Lemma " ~~ DigitCharacter.. ~~ ": " ~~ __] :>
                Module[{m = StringCases[l,
                        "Lemma " ~~ n:DigitCharacter.. ~~ ": " ~~ stmt__ :>
                            {n, stmt}]},
                    If[ Length[m] > 0,
                        <|"Name" -> "lemma" <> First[m][[1]],
                          "Statement" -> StringTrim @ First[m][[2]]|>,
                        Nothing]]];
        goalSeen = AnyTrue[lines, StringStartsQ[StringTrim @ #, "Goal "] &];
        <|"Axioms" -> axEntries, "Lemmas" -> lemEntries,
          "ProofLength" -> Length[axEntries] + Length[lemEntries] +
              If[goalSeen, 1, 0]|>];

TTweeProof[theory_String, thm_String, opts:OptionsPattern[]] :=
    Module[{path = FileNameJoin[{
            Directory[], "tools", "baselines", "vampire_tptp",
            theory <> "__" <> thm <> ".p"}]},
        TTweeProof[path, opts]];

TTweeProof[problemFile_String, opts:OptionsPattern[]] /;
        FileExtension[problemFile] === "p" := Module[{
        bin, tc, cmd, out, status, secs, proofBody, parsed},
    bin = tweeBinary[OptionValue["Binary"]];
    If[ MissingQ[bin] || bin === Null,
        Message[TTweeProof::notwee]; Return[$Failed]];
    If[ ! FileExistsQ[problemFile],
        Message[TTweeProof::badfile, problemFile]; Return[$Failed]];
    tc = OptionValue[TimeConstraint];
    cmd = StringJoin[bin, " --tstp --quiet ", problemFile, " 2>&1"];
    {secs, out} = AbsoluteTiming @ RunProcess[
        {"sh", "-c", StringJoin["timeout ", ToString[N[tc + 2]], " ", cmd]},
        "StandardOutput"];
    status = Which[
        StringContainsQ[out, "SZS status Unsatisfiable"]
            || StringContainsQ[out, "SZS status Theorem"], "Proved",
        StringContainsQ[out, "RESULT: Timeout"]
            || StringContainsQ[out, "Time limit"], "TimedOut",
        StringContainsQ[out, "GaveUp"]
            || StringContainsQ[out, "RESULT: Unknown"], "Failed",
        True, "Failed"];
    proofBody = If[ status === "Proved",
        StringCases[out,
            "SZS output start Proof" ~~ body__ ~~ "SZS output end Proof" :>
                StringTrim @ body, 1],
        {}];
    parsed = If[ Length[proofBody] > 0, parseTweeProof[First @ proofBody],
        <|"Axioms" -> {}, "Lemmas" -> {}, "ProofLength" -> 0|>];
    <|"Status" -> status,
      "Strategy" -> "twee",
      "Seconds" -> N @ Round[secs, 0.01],
      "ProofLength" -> parsed["ProofLength"],
      "Axioms" -> parsed["Axioms"],
      "Lemmas" -> parsed["Lemmas"],
      "RawProof" -> If[Length[proofBody] > 0, First @ proofBody, ""]|>
];

End[]
EndPackage[]
